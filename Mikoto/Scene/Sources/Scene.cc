//    Copyright 2026 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <filesystem>
#include <functional>
#include <optional>
#include <type_traits>

#include <EASTL/algorithm.h>
#include <EASTL/chrono.h>
#include <EASTL/memory.h>
#include <EASTL/optional.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/type_traits.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/utility.h>

#include <entt/entt.hpp>

#include <glm/glm.hpp>
#include <glm/gtx/compatibility.hpp>

#include <nlohmann/json.hpp>

#include <yaml-cpp/yaml.h>

#include <Animation/AnimationController.hh>
#include <Animation/AnimationSystem.hh>
#include <Animation/Animator.hh>
#include <Animation/KeyframeClip.hh>
#include <Animation/Utility.hh>

#include <Assets/AssetRegistryService.hh>

#include <Audio/AudioService.hh>
#include <Audio/AudioSettings.hh>
#include <Audio/Utility.hh>

#include <Core/Core.hh>
#include <Core/Exception.hh>
#include <Core/Profiler.hh>
#include <Core/Types.hh>

#include <Filesystem/File.hh>
#include <Filesystem/FileService.hh>

#include <Logging/Assert.hh>
#include <Logging/Logger.hh>

#include <Material/PostProcessMaterial.hh>
#include <Material/Utility.hh>

#include <Physics/PhysicsWorld.hh>
#include <Physics/PhysicSystem.hh>
#include <Physics/Shape.hh>

#include <Renderer/Core/SceneRenderer.hh>
#include <Renderer/ForwardRenderer/Passes/AtmosphericScatteringModule.hh>
#include <Renderer/Gui/Canvas.hh>
#include <Renderer/Particle/ParticleEmitter.hh>
#include <Renderer/Particle/ParticleSystem.hh>
#include <Renderer/Rhi/Types.hh>

#include <Scene/Component.hh>
#include <Scene/Entity.hh>
#include <Scene/Scene.hh>
#include <Scene/Utility.hh>

#include <Scripting/Script.hh>
#include <Scripting/ScriptingService.hh>

#include <Text/Utility.hh>

#include <Video/VideoService.hh>

namespace YAML {
    using namespace mikoto::core;

    template<>
    struct convert<mikoto::core::float3> {
        static auto encode( const mikoto::core::float3 &rhs ) -> Node {
            MKT_BEGIN_PROFILER();
            Node node{};
            node.push_back( rhs.x );
            node.push_back( rhs.y );
            node.push_back( rhs.z );
            return node;
        }

        static auto decode( const Node &node, mikoto::core::float3 &rhs ) -> bool {
            MKT_BEGIN_PROFILER();

            if ( !node.IsSequence() || node.size() != mikoto::core::float3::length() ) {
                return false;
            }

            rhs.x = node[0].as<mikoto::core::float3::type::value_type>();
            rhs.y = node[1].as<mikoto::core::float3::type::value_type>();
            rhs.z = node[2].as<mikoto::core::float3::type::value_type>();

            return true;
        }
    };

    template<>
    struct convert<mikoto::core::float4> {
        static auto encode( const mikoto::core::float4 &rhs ) -> Node {
            MKT_BEGIN_PROFILER();
            Node node{};
            node.push_back( rhs.x );
            node.push_back( rhs.y );
            node.push_back( rhs.z );
            node.push_back( rhs.w );
            return node;
        }

        static auto decode( const Node &node, mikoto::core::float4 &rhs ) -> bool {
            MKT_BEGIN_PROFILER();

            if ( !node.IsSequence() || node.size() != glm::vec4::length() ) {
                return false;
            }

            rhs.x = node[0].as<mikoto::core::float4::type::value_type>();
            rhs.y = node[1].as<mikoto::core::float4::type::value_type>();
            rhs.z = node[2].as<mikoto::core::float4::type::value_type>();
            rhs.w = node[3].as<mikoto::core::float4::type::value_type>();

            return true;
        }
    };

    template<>
    struct convert<mikoto::core::float4x4> {
        static auto encode( const mikoto::core::float4x4 &rhs ) -> Node {
            MKT_BEGIN_PROFILER();
            Node node{};
            node.push_back( rhs[0] );
            node.push_back( rhs[1] );
            node.push_back( rhs[2] );
            node.push_back( rhs[3] );
            return node;
        }

        static auto decode( const Node &node, mikoto::core::float4x4 &rhs ) -> bool {
            MKT_BEGIN_PROFILER();

            if ( !node.IsSequence() || node.size() != 4 ) {
                return false;
            }

            rhs[0] = node[0].as<mikoto::core::float4>();
            rhs[1] = node[1].as<mikoto::core::float4>();
            rhs[2] = node[2].as<mikoto::core::float4>();
            rhs[3] = node[3].as<mikoto::core::float4>();

            return true;
        }
    };
}// namespace YAML

namespace mikoto::scene {

    using namespace mikoto::core;
    using namespace mikoto::animation;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;

    template<typename T>
    static auto WritePhysicsValue( T value ) -> YAML::Node {
        YAML::Node node{};

        if constexpr ( std::is_enum_v<T> ) {
            node = static_cast<int>( value );
        }
        else if constexpr ( std::is_arithmetic_v<T> ) {
            node = value;
        }
        else if constexpr ( std::is_same_v<T, eastl::string> ) {
            node = value.c_str();
        }
        else if constexpr ( requires { value.GetPath(); } ) {
            node = value.GetPath().GetC_Str();
        }
        else if constexpr ( std::is_same_v<T, core::quat> ) {
            node.push_back( value.w );
            node.push_back( value.x );
            node.push_back( value.y );
            node.push_back( value.z );
        }
        else if constexpr ( requires { value.x; value.y; } ) {
            node.push_back( value.x );
            node.push_back( value.y );

            if constexpr ( requires { value.z; } ) {
                node.push_back( value.z );
            }
        }
        else if constexpr ( requires { value.begin(); value.end(); } ) {
            node = YAML::Node{ YAML::NodeType::Sequence };

            for ( const auto &item: value ) {
                node.push_back( WritePhysicsValue( item ) );
            }
        }
        else {
            VisitPhysicsFields( value, [&]( const char *name, auto &field ) {
                node[name] = WritePhysicsValue( field );
                return false;
            } );
        }

        return node;
    }

    template<typename T>
    static auto ReadPhysicsValue( const YAML::Node &node, T &value ) -> void {
        if constexpr ( std::is_enum_v<T> ) {
            value = static_cast<T>( node.as<int>() );
        }
        else if constexpr ( std::is_arithmetic_v<T> ) {
            value = node.as<T>();
        }
        else if constexpr ( std::is_same_v<T, eastl::string> ) {
            value = node.as<std::string>().c_str();
        }
        else if constexpr ( requires { value.GetPath(); } ) {
            value = T{ filesystem::Path{ node.as<std::string>() } };
        }
        else if constexpr ( std::is_same_v<T, core::quat> ) {
            value = { node[0].as<float>(), node[1].as<float>(), node[2].as<float>(), node[3].as<float>() };
        }
        else if constexpr ( requires { value.x; value.y; } ) {
            value.x = node[0].as<float>();
            value.y = node[1].as<float>();

            if constexpr ( requires { value.z; } ) {
                value.z = node[2].as<float>();
            }
        }
        else if constexpr ( requires { value.resize( 0 ); } ) {
            if ( !node.IsSequence() || node.size() > 1000000 ) {
                MKT_THROW_RUNTIME_ERROR( "Invalid physics array." );
            }

            value.resize( node.size() );

            for ( size_t i{}; i < value.size(); ++i ) {
                ReadPhysicsValue( node[i], value[i] );
            }
        }
        else if constexpr ( requires { value.begin(); value.end(); } ) {
            if ( !node.IsSequence() || node.size() != value.size() ) {
                MKT_THROW_RUNTIME_ERROR( "Invalid physics axis array." );
            }

            for ( size_t i{}; i < value.size(); ++i ) {
                ReadPhysicsValue( node[i], value[i] );
            }
        }
        else {
            VisitPhysicsFields( value, [&]( const char *name, auto &field ) {
                if ( const auto child{ node[name] } ) {
                    ReadPhysicsValue( child, field );
                    return true;
                }

                return false;
            } );
        }
    }

    static auto SerializePhysicsComponents( const Entity &entity, YAML::Emitter &emitter ) -> void {
        ForEachExtendedPhysicsComponent( [&]<typename T>( const char *name ) {
            if ( entity.HasComponent<T>() ) {
                emitter << YAML::Key << name << YAML::Value << WritePhysicsValue( entity.GetComponent<T>() );
            }
        } );
    }

    static auto DeserializePhysicsComponents( Entity &entity, const YAML::Node &node ) -> void {
        ForEachExtendedPhysicsComponent( [&]<typename T>( const char *name ) {
            if ( const auto child{ node[name] } ) {
                T value{};
                ReadPhysicsValue( child, value );
                entity.AddComponent<T>( eastl::move( value ) );
            }
        } );
    }

    static auto CopyPhysicsComponents( const Entity &source, Entity &destination ) -> void {
        ForEachExtendedPhysicsComponent( [&]<typename T>( const char * ) {
            if ( source.HasComponent<T>() ) {
                auto &copy{ destination.AddComponent<T>( source.GetComponent<T>() ) };

                if constexpr ( std::is_same_v<T, CharacterControllerComponent> ) {
                    copy.mDesiredVelocity = {};
                    copy.mJumpSpeed = 0;
                    copy.mGrounded = false;
                }
            }
        } );
    }

    template<typename T>
    static auto RemapValue( T &value, const std::function<core::u64( core::u64 )> &remap ) -> void {
        if constexpr ( requires { value.begin(); value.end(); } && !std::is_same_v<T, eastl::string> ) {
            for ( auto &item: value ) {
                RemapValue( item, remap );
            }
        }
        else if constexpr ( !std::is_arithmetic_v<T> && !std::is_enum_v<T> && !requires { value.x; } && !requires { value.GetPath(); } && !std::is_same_v<T, eastl::string> ) {
            VisitPhysicsFields( value, [&]( const char *name, auto &field ) {
                using Field = std::remove_cvref_t<decltype( field )>;

                if constexpr ( std::is_same_v<Field, core::u64> ) {
                    if ( std::string_view{ name }.ends_with( "EntityID" ) ) {
                        field = remap( field );
                        return true;
                    }
                }
                else if constexpr ( std::is_same_v<Field, eastl::vector<core::u64>> ) {
                    if ( std::string_view{ name } == "ColliderEntities" ) {
                        for ( auto &id: field ) {
                            id = remap( id );
                        }

                        return true;
                    }
                }
                else {
                    RemapValue( field, remap );
                    return true;
                }

                return false;
            } );
        }
    }

    static auto RemapPhysicsEntities( Entity &entity, const std::function<core::u64( core::u64 )> &remap ) -> void {
        ForEachExtendedPhysicsComponent( [&]<typename T>( const char * ) {
            if ( entity.HasComponent<T>() ) {
                RemapValue( entity.GetComponent<T>(), remap );
            }
        } );
    }

    static auto SerializeVehicleDetails( const VehicleComponent &vehicle, YAML::Emitter &emitter ) -> void {
        emitter << YAML::Key << "VehicleType" << YAML::Value << static_cast<int>( vehicle.GetVehicleType() );
        emitter << YAML::Key << "Wheels" << YAML::Value << WritePhysicsValue( vehicle.GetWheelDescriptions() );
    }

    static auto DeserializeVehicleDetails( VehicleComponent &vehicle, const YAML::Node &node ) -> void {
        const int kind{ node["VehicleType"].as<int>( 0 ) };

        if ( kind < 0 || kind > static_cast<int>( VehicleType::eSixDof ) ) {
            MKT_THROW_RUNTIME_ERROR( "Invalid vehicle type." );
        }

        vehicle.SetVehicleType( static_cast<VehicleType>( kind ) );

        if ( node["Wheels"] ) {
            eastl::vector<VehicleWheelDescription> wheels{};
            ReadPhysicsValue( node["Wheels"], wheels );
            vehicle.SetWheelDescriptions( wheels );
        }
    }


    struct AtmosphereField {
        core::cstr mName{};
        core::f32 renderer::AtmosphereSettings::*mValue{};
    };

    using SceneMaterialIDs = ankerl::unordered_dense::map<const material::SurfaceMaterial *, core::u32>;

    static constexpr eastl::array<AtmosphereField, 14> kAtmosphereFields{ { { "PlanetRadius", &renderer::AtmosphereSettings::mPlanetRadius },
        { "AtmosphereHeight", &renderer::AtmosphereSettings::mAtmosphereHeight },
        { "WorldUnitsPerKilometer", &renderer::AtmosphereSettings::mWorldUnitsPerKilometer },
        { "GroundHeight", &renderer::AtmosphereSettings::mGroundHeight },
        { "RayleighScale", &renderer::AtmosphereSettings::mRayleighScale },
        { "MieScale", &renderer::AtmosphereSettings::mMieScale },
        { "OzoneScale", &renderer::AtmosphereSettings::mOzoneScale },
        { "MieAnisotropy", &renderer::AtmosphereSettings::mMieAnisotropy },
        { "GroundAlbedo", &renderer::AtmosphereSettings::mGroundAlbedo },
        { "SunElevation", &renderer::AtmosphereSettings::mSunElevation },
        { "SunAzimuth", &renderer::AtmosphereSettings::mSunAzimuth },
        { "SunIlluminance", &renderer::AtmosphereSettings::mSunIlluminance },
        { "SunAngularRadius", &renderer::AtmosphereSettings::mSunAngularRadius },
        { "AerialDistance", &renderer::AtmosphereSettings::mAerialDistance } } };

    static auto SerializeAtmosphere( YAML::Emitter &output, const renderer::AtmosphereSettings &settings ) -> void {
        output << YAML::BeginMap;

        for ( const auto &field: kAtmosphereFields ) {
            output << YAML::Key << field.mName << YAML::Value << settings.*( field.mValue );
        }

        output << YAML::Key << "UseDirectionalLight" << YAML::Value << settings.mUseDirectionalLight;
        output << YAML::Key << "AerialPerspective" << YAML::Value << settings.mAerialPerspective;
        output << YAML::EndMap;
    }

    static auto DeserializeAtmosphere( const YAML::Node &node ) -> renderer::AtmosphereSettings {
        renderer::AtmosphereSettings settings{};

        if ( !node ) {
            return settings;
        }

        if ( !node.IsMap() ) {
            throw YAML::RepresentationException( node.Mark(), "Atmosphere must be a map." );
        }

        for ( const auto &field: kAtmosphereFields ) {
            if ( const auto value{ node[field.mName] } ) {
                settings.*( field.mValue ) = value.as<core::f32>();
            }
        }

        settings.mUseDirectionalLight = node["UseDirectionalLight"].as<bool>( settings.mUseDirectionalLight );
        settings.mAerialPerspective = node["AerialPerspective"].as<bool>( settings.mAerialPerspective );
        settings.Sanitize();

        return settings;
    }


    static auto SerializeAudioSettings( const audio::SourceSettings &value ) -> YAML::Node {
        MKT_BEGIN_PROFILER();
        YAML::Node node;
        node["Volume"] = value.mVolume;
        node["Pan"] = value.mPan;
        constexpr std::array<core::cstr, 5> names{ "Volume", "SpatialBlend", "Spread", "LowPass", "Reverb" };

        for ( size_t i{}; i < value.mCurves.size(); ++i ) {
            const auto &curve{ value.mCurves[i] };
            auto target{ node["DistanceCurves"][names[i]] };
            target["Enabled"] = curve.mEnabled;
            target["Interpolation"] = curve.mInterpolation == audio::CurveInterpolation::eLinear ? "Linear" : "Smooth";

            for ( core::u32 p{}; p < curve.mCount; ++p ) {
                YAML::Node point;
                point.push_back( curve.mPoints[p].x );
                point.push_back( curve.mPoints[p].y );
                target["Points"].push_back( point );
            }
        }

        node["Pitch"] = value.mPitch;
        node["Doppler"] = value.mDoppler;
        node["MinDistance"] = value.mMinDistance;
        node["MaxDistance"] = value.mMaxDistance;
        node["Rolloff"] = value.mRolloff;
        node["Muted"] = value.mMuted;
        node["Looping"] = value.mLooping;
        node["Spatialized"] = value.mSpatialized;
        node["Attenuation"] = static_cast<int>( value.mAttenuation );
        return node;
    }


    static auto Vector( const core::float3 &value ) -> nlohmann::json {
        MKT_BEGIN_PROFILER();
        return { value.x, value.y, value.z };
    }


    static auto Vector( const nlohmann::json &value ) -> core::float3 {
        MKT_BEGIN_PROFILER();

        if ( !value.is_array() || value.size() != 3 ) {
            MKT_THROW_RUNTIME_ERROR( "Expected an IK vector." );
        }

        const core::float3 result{ value[0].get<core::f32>(), value[1].get<core::f32>(), value[2].get<core::f32>() };
        return result;
    }


    static auto Scalar( const nlohmann::json &value, core::f32 minimum, core::f32 maximum ) -> core::f32 {
        MKT_BEGIN_PROFILER();
        const core::f32 result{ value.get<core::f32>() };

        if ( result < minimum || result > maximum ) {
            MKT_THROW_RUNTIME_ERROR( "Invalid animator setting." );
        }

        return result;
    }


    static auto DeserializeAudioSettings( const YAML::Node &node ) -> audio::SourceSettings {
        MKT_BEGIN_PROFILER();
        audio::SourceSettings value;
        const auto read = [&]( const char *key, float &target ) {
            if ( node[key] ) {
                target = node[key].as<float>();
            }
        };
        read( "Volume", value.mVolume );
        read( "Pan", value.mPan );
        constexpr std::array<core::cstr, 5> names{ "Volume", "SpatialBlend", "Spread", "LowPass", "Reverb" };

        if ( const auto curves{ node["DistanceCurves"] } ) {
            for ( size_t i{}; i < value.mCurves.size(); ++i ) {
                if ( const auto curve{ curves[names[i]] } ) {
                    auto &target{ value.mCurves[i] };
                    target.mEnabled = curve["Enabled"].as<bool>( false );
                    // Older scenes used linear interpolation exclusively.
                    const auto interpolation{ curve["Interpolation"].as<std::string>( "Linear" ) };

                    if ( interpolation != "Linear" && interpolation != "Smooth" ) {
                        MKT_THROW_RUNTIME_ERROR( "Invalid audio curve interpolation." );
                    }

                    target.mInterpolation = interpolation == "Linear" ? audio::CurveInterpolation::eLinear : audio::CurveInterpolation::eSmooth;

                    if ( const auto points{ curve["Points"] }; points && points.IsSequence() && points.size() >= 2 ) {
                        target.mCount = core::u32( std::min( points.size(), size_t( audio::DistanceCurve::kMaxPoints ) ) );

                        for ( core::u32 p{}; p < target.mCount; ++p ) {
                            target.mPoints[p] = { points[p][0].as<float>(), points[p][1].as<float>() };
                        }
                    }
                }
            }
        }

        read( "Pitch", value.mPitch );
        read( "Doppler", value.mDoppler );
        read( "MinDistance", value.mMinDistance );
        read( "MaxDistance", value.mMaxDistance );
        read( "Rolloff", value.mRolloff );

        if ( node["Muted"] ) {
            value.mMuted = node["Muted"].as<bool>();
        }

        if ( node["Looping"] ) {
            value.mLooping = node["Looping"].as<bool>();
        }

        if ( node["Spatialized"] ) {
            value.mSpatialized = node["Spatialized"].as<bool>();
        }

        if ( node["Attenuation"] ) {
            const int model = node["Attenuation"].as<int>();

            if ( model < 0 || model > 3 ) {
                MKT_THROW_RUNTIME_ERROR( "Invalid audio attenuation model." );
            }

            value.mAttenuation = static_cast<audio::Attenuation>( model );
        }

        return audio::SanitizeAudio( value );
    }


    static auto CanvasVector( core::float2 value ) -> YAML::Node {
        YAML::Node n;
        n.push_back( value.x );
        n.push_back( value.y );
        return n;
    }


    static auto CanvasScalar( const YAML::Node &n, float fallback = 0 ) -> float {
        float value = n ? n.as<float>() : fallback;
        return value;
    }


    static auto CanvasVector( const YAML::Node &n, core::float2 fallback = {} ) -> core::float2 {
        if ( !n ) {
            return fallback;
        }

        if ( !n.IsSequence() || n.size() != 2 ) {
            MKT_THROW_RUNTIME_ERROR( "Canvas vector must contain two numbers." );
        }

        return { CanvasScalar( n[0] ), CanvasScalar( n[1] ) };
    }


    static auto SerializeCanvas( const renderer::Canvas &canvas ) -> YAML::Node {
        YAML::Node n;
        n["Version"] = 2;
        n["Overlay"] = canvas.IsOverlay();
        n["ReferenceSize"] = CanvasVector( canvas.GetReferenceSize() );
        n["WorldScale"] = canvas.GetWorldScale();
        n["SortOrder"] = canvas.GetSortOrder();
        n["Elements"] = YAML::Node( YAML::NodeType::Sequence );

        for ( const auto &command: canvas.GetCommands() ) {
            YAML::Node c;
            c["Id"] = command.mId;
            c["Parent"] = command.mParentId;
            c["Name"] = command.mName.c_str();
            c["Interactable"] = command.mInteractable;
            c["Type"] = static_cast<int>( command.mType );
            c["Visible"] = command.mVisible;
            c["Enabled"] = command.mEnabled;
            const auto &t = command.mTransform;
            c["Layout"] = static_cast<int>( t.mLayoutMode );
            c["UniformResize"] = t.mUniformResize;
            c["AnchorMin"] = CanvasVector( t.mAnchorMin );
            c["AnchorMax"] = CanvasVector( t.mAnchorMax );
            c["Pivot"] = CanvasVector( t.mPivot );
            c["Position"] = CanvasVector( t.mAnchoredPosition );
            c["Size"] = CanvasVector( t.mSizeDelta );
            c["Scale"] = CanvasVector( t.mScale );
            c["Rotation"] = t.mRotation;
            c["Edges"].push_back( t.mEdges.mLeft );
            c["Edges"].push_back( t.mEdges.mTop );
            c["Edges"].push_back( t.mEdges.mRight );
            c["Edges"].push_back( t.mEdges.mBottom );
            c["Color"].push_back( command.mColor.mR );
            c["Color"].push_back( command.mColor.mG );
            c["Color"].push_back( command.mColor.mB );
            c["Color"].push_back( command.mColor.mA );
            c["Start"] = CanvasVector( command.mStart );
            c["End"] = CanvasVector( command.mEnd );
            c["Text"] = command.mText.c_str();
            c["FontSize"] = command.mFontSize;
            c["Thickness"] = command.mThickness;
            c["Rounding"] = command.mRounding;
            c["Value"] = command.mValue;

            if ( !command.GetTextureAsset().GetRef().GetPath().IsEmpty() ) {
                c["TexturePath"] = command.GetTextureAsset().GetRef().GetPath().GetC_Str();
            }

            if ( !command.mFont.IsEmpty() ) {
                c["FontPath"] = command.mFont->GetPath().GetC_Str();
            }

            n["Elements"].push_back( c );
        }

        return n;
    }


    static auto DeserializeCanvas( const YAML::Node &n ) -> renderer::Canvas {
        renderer::Canvas result;
        const auto version{ n["Version"].as<core::u32>( 1 ) };

        if ( version < 1 || version > 2 ) {
            MKT_THROW_RUNTIME_ERROR( "Unsupported Canvas version." );
        }

        result.SetOverlay( n["Overlay"].as<bool>( true ) );
        result.SetReferenceSize( CanvasVector( n["ReferenceSize"], { 1920, 1080 } ) );
        result.SetWorldScale( CanvasScalar( n["WorldScale"], 0.001f ) );
        result.SetSortOrder( n["SortOrder"].as<int>( 0 ) );
        const auto elements = n["Elements"];

        if ( !elements ) {
            return result;
        }

        if ( !elements.IsSequence() || elements.size() > 65536 ) {
            MKT_THROW_RUNTIME_ERROR( "Invalid Canvas element list." );
        }

        for ( const auto &c: elements ) {
            renderer::CanvasCommand command;

            if ( version >= 2 ) {
                command.mId = c["Id"].as<renderer::CanvasElementId>();
                command.mParentId = c["Parent"].as<renderer::CanvasElementId>( 0 );

                if ( !command.mId ) {
                    MKT_THROW_RUNTIME_ERROR( "Canvas widget requires a nonzero ID." );
                }
            }

            command.mName = c["Name"].as<std::string>( "" ).c_str();
            const int type = c["Type"].as<int>();
            const int layout = c["Layout"].as<int>( 0 );

            if ( type < 0 || type > static_cast<int>( renderer::CanvasCommandType::eSlider ) || layout < 0 || layout > 2 ) {
                MKT_THROW_RUNTIME_ERROR( "Invalid Canvas element type or layout." );
            }

            command.mType = static_cast<renderer::CanvasCommandType>( type );
            command.mInteractable = c["Interactable"].as<bool>( version >= 2 || command.mType >= renderer::CanvasCommandType::eButton );
            command.mVisible = c["Visible"].as<bool>( true );
            command.mEnabled = c["Enabled"].as<bool>( true );
            auto &t = command.mTransform;
            t.mLayoutMode = static_cast<renderer::CanvasLayoutMode>( layout );
            t.mUniformResize = c["UniformResize"].as<bool>( false );
            t.mAnchorMin = CanvasVector( c["AnchorMin"] );
            t.mAnchorMax = CanvasVector( c["AnchorMax"] );
            t.mPivot = CanvasVector( c["Pivot"], { 0.5f, 0.5f } );
            t.mAnchoredPosition = CanvasVector( c["Position"] );
            t.mSizeDelta = CanvasVector( c["Size"] );
            t.mScale = CanvasVector( c["Scale"], { 1, 1 } );
            t.mRotation = CanvasScalar( c["Rotation"] );

            if ( const auto edges = c["Edges"] ) {
                if ( !edges.IsSequence() || edges.size() != 4 ) {
                    MKT_THROW_RUNTIME_ERROR( "Canvas edges require four numbers." );
                }

                t.mEdges = { CanvasScalar( edges[0] ), CanvasScalar( edges[1] ), CanvasScalar( edges[2] ), CanvasScalar( edges[3] ) };
            }

            if ( const auto color = c["Color"] ) {
                if ( !color.IsSequence() || color.size() != 4 ) {
                    MKT_THROW_RUNTIME_ERROR( "Canvas color requires four numbers." );
                }

                command.mColor = { CanvasScalar( color[0] ), CanvasScalar( color[1] ), CanvasScalar( color[2] ), CanvasScalar( color[3] ) };
            }

            command.mStart = CanvasVector( c["Start"] );
            command.mEnd = CanvasVector( c["End"] );
            // Legacy lines stored endpoints in canvas coordinates. New widgets
            // store them locally so parenting does not apply translation twice.
            if ( version == 1 && command.mType == renderer::CanvasCommandType::eLine ) {
                const auto origin{ t.Resolve( result.GetReferenceSize() ).mPosition };
                command.mStart -= origin;
                command.mEnd -= origin;
            }

            command.mText = c["Text"].as<std::string>( "" ).c_str();
            command.mFontSize = glm::clamp( CanvasScalar( c["FontSize"], 24 ), 1.0f, 2048.0f );
            command.mThickness = glm::max( 0.0f, CanvasScalar( c["Thickness"] ) );
            command.mRounding = glm::max( 0.0f, CanvasScalar( c["Rounding"] ) );
            command.mValue = glm::clamp( CanvasScalar( c["Value"] ), 0.0f, 1.0f );
            const auto texture = c["TexturePath"].as<std::string>( "" );
            const auto font = c["FontPath"].as<std::string>( "" );

            if ( !texture.empty() ) {
                command.SetTexture( asset::AssetRegistryService::Get()->LoadAssetHandle<asset::Texture>( filesystem::Path{ texture.c_str() }, rhi::TextureDimension::eTexture2D ) );
            }

            if ( !font.empty() ) {
                command.mFont = asset::AssetRegistryService::Get()->LoadAsset<renderer::Font>( filesystem::Path{ font.c_str() } );
            }

            result.GetEditableCommands().push_back( command );
        }
        // Reject ambiguous IDs, missing parents and cycles at the load boundary.
        // Transient hover/focus/capture state deliberately never enters YAML.
        if ( !result.IsHierarchyValid() ) {
            MKT_THROW_RUNTIME_ERROR( "Invalid Canvas widget hierarchy." );
        }

        return result;
    }

    static auto SerializeLight( const LightComponent &component ) -> YAML::Node {
        MKT_BEGIN_PROFILER();
        YAML::Node node;
        node["Type"] = static_cast<int>( component.GetActiveType() );
        const auto write = [&]( const auto &light ) {
            YAML::Node item;

            for ( int i = 0; i < 3; ++i ) {
                item["Color"].push_back( light.GetColor()[i] );
            }

            item["Intensity"] = light.GetIntensity();
            item["CastShadows"] = light.IsShadowCaster();

            if constexpr ( requires { light.GetSourceRadius(); } ) {
                item["SourceRadius"] = light.GetSourceRadius();
            }

            if constexpr ( requires { light.GetAngularRadius(); } ) {
                item["AngularRadius"] = light.GetAngularRadius();
            }

            if constexpr ( requires { light.GetRadius(); } ) {
                item["Radius"] = light.GetRadius();
            }

            if constexpr ( requires { light.GetRange(); } ) {
                item["Range"] = light.GetRange();
            }

            if constexpr ( requires { light.GetDirection(); } ) {
                for ( int i = 0; i < 3; ++i ) {
                    item["Direction"].push_back( light.GetDirection()[i] );
                }
            }

            if constexpr ( requires { light.GetAngle(); } ) {
                item["Angle"] = light.GetAngle();
                item["Softness"] = light.GetSoftness();
            }

            if constexpr ( requires { light.GetSize(); light.IsTwoSided(); } ) {
                item["Shape"] = static_cast<int>( light.GetShape() );
                item["Size"].push_back( light.GetSize().x );
                item["Size"].push_back( light.GetSize().y );
                item["TwoSided"] = light.IsTwoSided();
                item["UseMaterialEmission"] = light.IsMaterialEmissionEnabled();
                item["EmissionSource"] = light.GetEmissionSourceEntityID();
            }

            return item;
        };
        node["Directional"] = write( component.Get<renderer::DirectionalLight>() );
        node["Point"] = write( component.Get<renderer::PointLight>() );
        node["Spot"] = write( component.Get<renderer::SpotLight>() );
        node["Area"] = write( component.Get<renderer::AreaLight>() );
        return node;
    }


    static auto DeserializeLight( const YAML::Node &node ) -> LightComponent {
        MKT_BEGIN_PROFILER();
        LightComponent result;

        if ( node["Type"] ) {
            const int type = node["Type"].as<int>();

            if ( type < 0 || type > 4 ) {
                MKT_THROW_RUNTIME_ERROR( "Invalid light type." );
            }

            result.SetActiveType( static_cast<renderer::LightType>( type ) );
        }

        const auto scalar = []( const YAML::Node &value ) {
            const float f = value.as<float>();
            return f;
        };
        const auto vector = [&]( const YAML::Node &value ) {
            if ( !value.IsSequence() || value.size() != 3 ) {
                MKT_THROW_RUNTIME_ERROR( "Light vectors require three numbers." );
            }

            return core::float3{ scalar( value[0] ), scalar( value[1] ), scalar( value[2] ) };
        };
        const auto read = [&]( auto &light, const YAML::Node &item ) {
            if ( !item ) {
                return;
            }

            if ( item["Color"] ) {
                light.SetColor( vector( item["Color"] ) );
            }

            if ( item["Intensity"] ) {
                light.SetIntensity( scalar( item["Intensity"] ) );
            }

            if ( item["CastShadows"] ) {
                light.SetIsShadowCaster( item["CastShadows"].as<bool>() );
            }

            if constexpr ( requires { light.SetSourceRadius( 0.0f ); } ) {
                if ( item["SourceRadius"] ) {
                    light.SetSourceRadius( scalar( item["SourceRadius"] ) );
                }
            }

            if constexpr ( requires { light.SetAngularRadius( 0.0f ); } ) {
                if ( item["AngularRadius"] ) {
                    light.SetAngularRadius( scalar( item["AngularRadius"] ) );
                }
            }

            if constexpr ( requires { light.GetRadius(); } ) {
                if ( item["Radius"] ) {
                    light.SetRadius( scalar( item["Radius"] ) );
                }
            }

            if constexpr ( requires { light.GetDirection(); } ) {
                if ( item["Direction"] ) {
                    light.SetDirection( vector( item["Direction"] ) );
                }
            }

            if constexpr ( requires { light.GetAngle(); } ) {
                if ( item["Angle"] ) {
                    light.SetAngle( scalar( item["Angle"] ) );
                }

                if ( item["Softness"] ) {
                    light.SetSoftness( scalar( item["Softness"] ) );
                }
            }

            if constexpr ( requires { light.GetSize(); light.IsTwoSided(); } ) {
                if ( const auto shape{ item["Shape"] } ) {
                    const int value{ shape.as<int>() };

                    if ( value < 0 || value > static_cast<int>( renderer::AreaLightShape::eTube ) ) {
                        MKT_THROW_RUNTIME_ERROR( "Invalid area light shape." );
                    }

                    light.SetShape( static_cast<renderer::AreaLightShape>( value ) );
                }

                if ( item["Range"] ) {
                    light.SetRange( scalar( item["Range"] ) );
                }

                if ( const auto size{ item["Size"] } ) {
                    if ( !size.IsSequence() || size.size() != 2 ) {
                        MKT_THROW_RUNTIME_ERROR( "Area light size requires two numbers." );
                    }

                    light.SetSize( { scalar( size[0] ), scalar( size[1] ) } );
                }

                if ( item["TwoSided"] ) {
                    light.SetTwoSided( item["TwoSided"].as<bool>() );
                }
                if ( item["UseMaterialEmission"] ) {
                    light.SetMaterialEmissionEnabled( item["UseMaterialEmission"].as<bool>() );
                }
                if ( item["EmissionSource"] ) {
                    light.SetEmissionSourceEntityID( item["EmissionSource"].as<core::u64>() );
                }
            }
        };

        read( result.Get<renderer::DirectionalLight>(), node["Directional"] );
        read( result.Get<renderer::PointLight>(), node["Point"] );
        read( result.Get<renderer::SpotLight>(), node["Spot"] );
        read( result.Get<renderer::AreaLight>(), node["Area"] );
        return result;
    }

    static auto PhysicsVector( const core::float3 &value ) -> YAML::Node {
        MKT_BEGIN_PROFILER();
        YAML::Node result;
        result.push_back( value.x );
        result.push_back( value.y );
        result.push_back( value.z );
        return result;
    }


    static auto ReadPhysicsVector( const YAML::Node &node ) -> core::float3 {
        MKT_BEGIN_PROFILER();

        if ( !node.IsSequence() || node.size() != 3 ) {
            MKT_THROW_RUNTIME_ERROR( "Expected a three-component physics vector." );
        }

        core::float3 result{ node[0].as<float>(), node[1].as<float>(), node[2].as<float>() };
        return result;
    }


    static auto ReadPhysicsScalar( const YAML::Node &node, float minimum, float maximum ) -> float {
        MKT_BEGIN_PROFILER();
        const float value = node.as<float>();

        if ( value < minimum || value > maximum ) {
            MKT_THROW_RUNTIME_ERROR( "Physics scalar is out of range." );
        }

        return value;
    }


    static auto SerializeRigidBody( const RigidBodyComponent &rb ) -> YAML::Node {
        MKT_BEGIN_PROFILER();
        YAML::Node node{};
        node["BodyType"] = static_cast<int>( rb.GetBodyType() );
        node["Mass"] = rb.GetMass();
        node["Friction"] = rb.GetFriction();
        node["Restitution"] = rb.GetRestitution();
        node["UseGravity"] = rb.UseGravity();
        node["LinearVelocity"] = PhysicsVector( rb.GetLinearVelocity() );
        node["AngularVelocity"] = PhysicsVector( rb.GetAngularVelocity() );
        return node;
    }


    static auto DeserializeRigidBody( const YAML::Node &node ) -> RigidBodyComponent {
        MKT_BEGIN_PROFILER();
        RigidBodyComponent rb{};

        if ( node["BodyType"] ) {
            const int type = node["BodyType"].as<int>();

            if ( type < 0 || type > 2 ) {
                MKT_THROW_RUNTIME_ERROR( "Invalid rigid body type." );
            }

            rb.SetBodyType( static_cast<RigidBodyComponent::BodyType>( type ) );
        }

        if ( node["Mass"] ) {
            rb.SetMass( ReadPhysicsScalar( node["Mass"], 1, RigidBodyComponent::GetMaxBodyMass() ) );
        }

        if ( node["Friction"] ) {
            rb.SetFriction( ReadPhysicsScalar( node["Friction"], 0, 1 ) );
        }

        if ( node["Restitution"] ) {
            rb.SetRestitution( ReadPhysicsScalar( node["Restitution"], 0, 1 ) );
        }

        if ( node["UseGravity"] ) {
            rb.SetUseGravity( node["UseGravity"].as<bool>() );
        }

        if ( node["LinearVelocity"] ) {
            rb.SetLinearVelocity( ReadPhysicsVector( node["LinearVelocity"] ) );
        }

        if ( node["AngularVelocity"] ) {
            rb.SetAngularVelocity( ReadPhysicsVector( node["AngularVelocity"] ) );
        }

        return rb;
    }

    template<typename Collider>
    static YAML::Node SerializeCollider( const Collider &collider ) {
        MKT_BEGIN_PROFILER();
        YAML::Node node;
        node["Offset"] = PhysicsVector( collider.GetOffset() );
        node["Trigger"] = collider.IsTrigger();

        if constexpr ( std::is_same_v<Collider, BoxColliderComponent> ) {
            node["HalfExtents"] = PhysicsVector( collider.GetHalfExtents() );
        }

        if constexpr ( std::is_same_v<Collider, SphereColliderComponent> || std::is_same_v<Collider, CapsuleColliderComponent> ) {
            node["Radius"] = collider.GetRadius();
        }

        if constexpr ( std::is_same_v<Collider, CapsuleColliderComponent> ) {
            node["HalfHeight"] = collider.GetHalfHeight();
        }

        if constexpr ( std::is_same_v<Collider, MeshColliderComponent> ) {
            node["Convex"] = collider.IsConvex();
        }

        return node;
    }

    template<typename Collider>
    static Collider DeserializeCollider( const YAML::Node &node ) {
        MKT_BEGIN_PROFILER();
        Collider collider;

        if ( node["Offset"] ) {
            collider.SetOffset( ReadPhysicsVector( node["Offset"] ) );
        }

        if ( node["Trigger"] ) {
            collider.SetTrigger( node["Trigger"].as<bool>() );
        }

        if constexpr ( std::is_same_v<Collider, BoxColliderComponent> ) {
            if ( node["HalfExtents"] ) {
                const auto extents = ReadPhysicsVector( node["HalfExtents"] );

                if ( glm::any( glm::lessThan( extents, core::float3{ 0.001f } ) ) ) {
                    MKT_THROW_RUNTIME_ERROR( "Box half extents must be at least 0.001." );
                }

                collider.SetHalfExtents( extents );
            }
        }

        if constexpr ( std::is_same_v<Collider, SphereColliderComponent> || std::is_same_v<Collider, CapsuleColliderComponent> ) {
            if ( node["Radius"] ) {
                collider.SetRadius( ReadPhysicsScalar( node["Radius"], 0.001f, FLT_MAX ) );
            }
        }

        if constexpr ( std::is_same_v<Collider, CapsuleColliderComponent> ) {
            if ( node["HalfHeight"] ) {
                collider.SetHalfHeight( ReadPhysicsScalar( node["HalfHeight"], 0, FLT_MAX ) );
            }
        }

        if constexpr ( std::is_same_v<Collider, MeshColliderComponent> ) {
            if ( node["Convex"] ) {
                collider.SetConvex( node["Convex"].as<bool>() );
            }
        }

        return collider;
    }

    // A scene stores file references while retaining material sharing within that scene.
    // Materials without a source file receive a companion .mktmtl asset.
    class SceneMaterialFiles {
    public:
        explicit SceneMaterialFiles( const filesystem::Path &sceneFile, bool copyMaterials = false )
            : mSceneFile{ sceneFile }, mCopyMaterials{ copyMaterials } {
        }

        auto Save( const material::MaterialHandle &material ) -> filesystem::Path {
            if ( const auto found{ mSaved.find( material.GetPtr() ) }; found != mSaved.end() ) {
                return found->second;
            }

            auto source{ mCopyMaterials ? material::CloneMaterial( *material ) : material };
            auto path{ material::GetMaterialSourcePath( *source ) };

            if ( path.IsEmpty() ) {
                path = CreatePath();
            }

            source->Serialize( path );
            mSaved.emplace( material.GetPtr(), path );
            return path;
        }

        auto Load( const YAML::Node &reference ) -> material::MaterialHandle {
            const filesystem::Path path{ reference["MaterialPath"].as<std::string>().c_str() };
            const eastl::string key{ path.ToAbsolute().GetC_Str() };

            if ( const auto found{ mLoaded.find( key ) }; found != mLoaded.end() ) {
                return found->second;
            }

            auto material{ material::LoadMaterial( filesystem::FileHandle::New( path ) ) };
            mLoaded.emplace( key, material );
            return material;
        }

        auto ImportLegacy( const YAML::Node &data, const char *type ) -> material::MaterialHandle {
            // Migrate the old envelope unchanged; only the material interprets its properties.
            YAML::Node root{};

            if ( data["Version"] && data["Material"] ) {
                root = YAML::Clone( data );
            }
            else {
                root["Version"] = 1;
                root["Type"] = type;
                root["Name"] = data["Name"].as<std::string>( type );
                root["Material"] = YAML::Clone( data );
            }

            const auto path{ CreatePath() };
            YAML::Emitter emitter{};
            emitter << root;

            if ( !emitter.good() ) {
                MKT_THROW_RUNTIME_ERROR( "Could not migrate embedded scene material." );
            }

            filesystem::FileHandle file{ filesystem::FileService::Get()->CreateNewFile( path ) };

            if ( file.IsEmpty() ) {
                MKT_THROW_RUNTIME_ERROR( "Cannot open migrated scene material for writing." );
            }

            file->Write( emitter.c_str() );
            return material::LoadMaterial( file );
        }

    private:
        auto CreatePath() const -> filesystem::Path {
            const auto scenePath{ mSceneFile.ToAbsolute().GetPathTyped<std::filesystem::path>() };
            const auto directory{ scenePath.parent_path() / ( scenePath.stem().string() + ".materials" ) };
            std::filesystem::create_directories( directory );
            const auto name{ mCopyMaterials ? string::Format( "Material_{}.mktmtl", mSaved.size() )
                                            : string::Format( "{}.mktmtl", core::as<core::u64>( math::random::Guid{} ) ) };
            return filesystem::Path{ ( directory / name.c_str() ).string().c_str() };
        }

        filesystem::Path mSceneFile{};
        bool mCopyMaterials{};
        ankerl::unordered_dense::map<const material::IMaterial *, filesystem::Path> mSaved{};
        ankerl::unordered_dense::map<eastl::string, material::MaterialHandle> mLoaded{};
    };

#define MKT_SERIALIZE_COMPONENT_IF_PRESENT( TYPE, KEY_NAME )       \
    if ( root->HasComponent<TYPE>() ) {                            \
        emitter << YAML::Key << KEY_NAME << YAML::Value;           \
        SerializeComponent( root->GetComponent<TYPE>(), emitter ); \
    }

    static auto operator<<( YAML::Emitter &out, const mikoto::core::float4 &v ) -> YAML::Emitter & {
        MKT_BEGIN_PROFILER();
        out << YAML::Flow;
        out << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
        return out;
    }

    static auto operator<<( YAML::Emitter &out, const mikoto::core::float3 &v ) -> YAML::Emitter & {
        MKT_BEGIN_PROFILER();
        out << YAML::Flow;
        out << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
        return out;
    }

    static auto operator<<( YAML::Emitter &emitter, const mikoto::core::float4x4 &matrix ) -> YAML::Emitter & {
        MKT_BEGIN_PROFILER();
        emitter << YAML::Flow << YAML::BeginSeq;

        emitter << matrix[0];
        emitter << matrix[1];
        emitter << matrix[2];
        emitter << matrix[3];

        emitter << YAML::EndSeq;
        return emitter;
    }

    static auto SerializeComponent( const TransformComponent &transform, YAML::Emitter &emitter ) -> void {
        MKT_BEGIN_PROFILER();
        const auto &position{ transform.GetTranslation() };
        const auto &rotation{ transform.GetRotation() };
        const auto &scale{ transform.GetScale() };
        const auto &uniformScale{ transform.HasUniformScale() };

        emitter << YAML::BeginMap;
        emitter << YAML::Key << "Position" << YAML::Value << position;
        emitter << YAML::Key << "Rotation" << YAML::Value << rotation;
        emitter << YAML::Key << "Scale" << YAML::Value << scale;
        emitter << YAML::Key << "Uniform Scale" << YAML::Value << uniformScale;

        // TRS alone cannot preserve a mirrored or sheared imported/gizmo basis.
        emitter << YAML::Key << "Local Transform" << YAML::Value << transform.GetTransform();

        emitter << YAML::EndMap;
    }

    static auto SerializeComponent( const TagComponent &tag, YAML::Emitter &emitter ) -> void {
        MKT_BEGIN_PROFILER();
        emitter << YAML::BeginMap;
        emitter << YAML::Key << "Name" << YAML::Value << tag.GetTag().c_str();
        emitter << YAML::Key << "Visibility" << YAML::Value << tag.IsActive();
        emitter << YAML::EndMap;
    }

    static auto SerializeComponent( const MeshComponent &meshComponent, YAML::Emitter &emitter ) -> void {
        MKT_BEGIN_PROFILER();
        emitter << YAML::BeginMap;

        emitter << YAML::Key << "MeshIndex" << YAML::Value << meshComponent.GetMeshIndex();
        emitter << YAML::Key << "ModelPath" << YAML::Value << meshComponent.GetModelPath().GetC_Str();

        emitter << YAML::EndMap;
    }

    static auto SerializeComponent( const LightComponent &lightComponent, YAML::Emitter &emitter ) -> void {
        MKT_BEGIN_PROFILER();
        emitter << SerializeLight( lightComponent );
    }

    static auto SerializeComponent( const VideoComponent &component, YAML::Emitter &emitter ) -> void {
        YAML::Node node{};
        const auto &playback{ component.GetPlaybackSettings() };
        node["ClipPath"] = component.GetClip().GetPath().GetC_Str();
        node["PlayOnStart"] = playback.mPlayOnStart;
        node["Looping"] = playback.mLooping;
        node["AudioEnabled"] = playback.mAudioEnabled;
        node["OutputMap"] = static_cast<core::u32>( component.GetOutputMap() );
        node["Audio"] = SerializeAudioSettings( playback.mAudio );
        emitter << node;
    }

    static auto SerializeComponent( const TextureRenderComponent &component, YAML::Emitter &emitter, const Scene &scene ) -> void {
        YAML::Node node{};
        node["Resolution"] = core::as<core::u32>( component.GetResolution() );
        node["RenderRate"] = core::as<core::u32>( component.GetRenderRate() );
        node["RenderPath"] = core::as<core::u32>( component.GetRenderPath() );
        node["OutputMap"] = core::as<core::u32>( component.GetRenderOutput() );
        node["CameraEntityID"] = core::u64{};

        for ( const auto [entity, camera, tag]: scene.GetRegistry().view<CameraComponent, TagComponent>().each() ) {
            if ( camera.HasCamera() && ( component.GetCameraEntityID() ? tag.GetGuid() == component.GetCameraEntityID() : &camera.GetCamera() == component.GetCamera() ) ) {
                node["CameraEntityID"] = tag.GetGuid();
                break;
            }
        }

        emitter << node;
    }

    static auto SerializeComponent( const AudioSourceComponent &audioComponent, YAML::Emitter &emitter ) -> void {
        MKT_BEGIN_PROFILER();
        auto node = SerializeAudioSettings( audioComponent.GetSettings() );
        node["Output"] = audioComponent.mOutputID;
        const auto clip = audioComponent.GetClip();
        node["ClipPath"] = clip && clip->GetFile() ? clip->GetFile()->GetPath().GetC_Str() : "";
        emitter << node;
    }

    static auto SerializeComponent( const CloudInteractionComponent &component, YAML::Emitter &emitter ) -> void {
        emitter << YAML::BeginMap
                << YAML::Key << "Enabled" << YAML::Value << component.IsEnabled()
                << YAML::Key << "Shape" << YAML::Value << core::as<core::u32>( component.GetShape() )
                << YAML::Key << "Dimensions" << YAML::Value << component.GetDimensions()
                << YAML::Key << "Strength" << YAML::Value << component.GetStrength()
                << YAML::Key << "Feather" << YAML::Value << component.GetFeather() << YAML::EndMap;
    }

    template<typename T>
    static auto SerializeVolumeComponent( const T &component, YAML::Emitter &emitter, SceneMaterialFiles &materials ) -> void {
        emitter << YAML::BeginMap;
        emitter << YAML::Key << "Shape" << YAML::Value << core::as<core::u32>( component.GetShape() );
        emitter << YAML::Key << "Dimensions" << YAML::Value << component.GetDimensions();
        emitter << YAML::Key << "HorizonRadius" << YAML::Value << component.GetHorizonRadius();
        emitter << YAML::Key << "DensityMultiplier" << YAML::Value << component.GetDensityMultiplier();
        emitter << YAML::Key << "CastShadows" << YAML::Value << component.IsShadowCaster();
        emitter << YAML::Key << "ReceiveShadows" << YAML::Value << component.IsShadowReceiver();

        if ( const auto material{ component.GetMaterial() } ) {
            emitter << YAML::Key << "MaterialPath" << YAML::Value << materials.Save( material ).GetC_Str();
        }

        if constexpr ( std::is_same_v<T, CloudComponent> ) {
            emitter << YAML::Key << "BottomAltitude" << YAML::Value << component.GetBottomAltitude();
            emitter << YAML::Key << "LayerThickness" << YAML::Value << component.GetLayerThickness();
            emitter << YAML::Key << "WindDirection" << YAML::Value << YAML::Flow << YAML::BeginSeq
                    << component.GetWindDirection().x << component.GetWindDirection().y << YAML::EndSeq;
            emitter << YAML::Key << "WindSpeed" << YAML::Value << component.GetWindSpeed();
            const auto &lightning{ component.GetLightning() };
            emitter << YAML::Key << "Lightning" << YAML::Value << YAML::BeginMap
                    << YAML::Key << "Enabled" << YAML::Value << lightning.mEnabled
                    << YAML::Key << "Automatic" << YAML::Value << lightning.mAutomatic
                    << YAML::Key << "Position" << YAML::Value << lightning.mPosition
                    << YAML::Key << "Color" << YAML::Value << lightning.mColor
                    << YAML::Key << "Intensity" << YAML::Value << lightning.mIntensity
                    << YAML::Key << "Radius" << YAML::Value << lightning.mRadius
                    << YAML::Key << "Interval" << YAML::Value << lightning.mInterval
                    << YAML::Key << "Duration" << YAML::Value << lightning.mDuration
                    << YAML::Key << "Seed" << YAML::Value << lightning.mSeed
                    << YAML::Key << "BoltsEnabled" << YAML::Value << lightning.mBoltsEnabled
                    << YAML::Key << "BoltLength" << YAML::Value << lightning.mBoltLength
                    << YAML::Key << "BoltWidth" << YAML::Value << lightning.mBoltWidth
                    << YAML::Key << "BoltBranches" << YAML::Value << lightning.mBoltBranches << YAML::EndMap;
        }
        emitter << YAML::EndMap;
    }

    template<typename T>
    static auto ReadVolumeComponent( const YAML::Node &node, SceneMaterialFiles &materials ) -> T {
        T result{};
        result.SetShape( core::as<decltype( result.GetShape() )>( node["Shape"].as<core::u32>( 0 ) ) );

        if ( node["Dimensions"] ) {
            result.SetDimensions( ReadPhysicsVector( node["Dimensions"] ) );
        }

        result.SetHorizonRadius( node["HorizonRadius"].as<float>( result.GetHorizonRadius() ) );
        result.SetDensityMultiplier( node["DensityMultiplier"].as<float>( 1 ) );
        result.SetCastShadows( node["CastShadows"].as<bool>( true ) );
        result.SetReceiveShadows( node["ReceiveShadows"].as<bool>( true ) );

        if ( node["MaterialPath"] ) {
            auto material{ materials.Load( node ) };
            if ( !dynamic_cast<material::VolumeMaterial *>( material.GetPtr() ) ) {
                MKT_THROW_RUNTIME_ERROR( "Cloud and fog components require a volume material." );
            }
            result.SetMaterial( material.As<material::VolumeMaterial>() );
        }

        if constexpr ( std::is_same_v<T, CloudComponent> ) {
            result.SetBottomAltitude( node["BottomAltitude"].as<float>( result.GetBottomAltitude() ) );
            result.SetLayerThickness( node["LayerThickness"].as<float>( result.GetLayerThickness() ) );
            result.SetWindSpeed( node["WindSpeed"].as<float>( result.GetWindSpeed() ) );
            if ( const auto light{ node["Lightning"] } ) {
                auto settings{ result.GetLightning() };
                settings.mEnabled = light["Enabled"].as<bool>( false );
                settings.mAutomatic = light["Automatic"].as<bool>( true );
                if ( light["Position"] ) {
                    settings.mPosition = light["Position"].as<core::float3>();
                }
                if ( light["Color"] ) {
                    settings.mColor = light["Color"].as<core::float3>();
                }
                settings.mIntensity = light["Intensity"].as<float>( settings.mIntensity );
                settings.mRadius = light["Radius"].as<float>( settings.mRadius );
                settings.mInterval = light["Interval"].as<float>( settings.mInterval );
                settings.mDuration = light["Duration"].as<float>( settings.mDuration );
                settings.mSeed = light["Seed"].as<core::u32>( settings.mSeed );
                settings.mBoltsEnabled = light["BoltsEnabled"].as<bool>( settings.mBoltsEnabled );
                settings.mBoltLength = light["BoltLength"].as<float>( settings.mBoltLength );
                settings.mBoltWidth = light["BoltWidth"].as<float>( settings.mBoltWidth );
                settings.mBoltBranches = light["BoltBranches"].as<core::u32>( settings.mBoltBranches );
                result.SetLightning( settings );
            }

            if ( const auto wind{ node["WindDirection"] } ) {
                if ( !wind.IsSequence() || wind.size() != 2 ) {
                    MKT_THROW_RUNTIME_ERROR( "Cloud wind direction needs two components." );
                }
                result.SetWindDirection( { wind[0].as<float>(), wind[1].as<float>() } );
            }
        }
        return result;
    }

    static auto SerializeComponent( const ParticleSimulatorComponent &component, YAML::Emitter &emitter ) -> void {
        if ( const auto &effect{ component.GetEffect() } ) {
            effect->Serialize( emitter );
        }
        else {
            emitter << YAML::BeginMap << YAML::Key << "Version" << YAML::Value << 1
                    << YAML::Key << "Emitters" << YAML::Value << YAML::BeginSeq << YAML::EndSeq << YAML::EndMap;
        }
    }

    static auto SerializeComponent( const AudioListenerComponent &listener, YAML::Emitter &emitter ) -> void {
        MKT_BEGIN_PROFILER();
        emitter << YAML::BeginMap << YAML::Key << "Enabled" << YAML::Value << listener.IsEnabled() << YAML::EndMap;
    }

    static auto ReadLandscapeColor( const YAML::Node &node, const char *key, core::float3 fallback ) -> core::float3 {
        const auto color{ node[key] };

        if ( !color ) {
            return fallback;
        }

        if ( !color.IsSequence() || color.size() != 3 ) {
            MKT_THROW_RUNTIME_ERROR( string::Format( "Terrain {} must contain three RGB values.", key ) );
        }

        return { color[0].as<float>(), color[1].as<float>(), color[2].as<float>() };
    }

    static auto SerializeComponent( const LandscapeComponent &landscape, YAML::Emitter &emitter ) -> void {
        const auto world{ landscape.GetWorldDescription() };
        emitter << YAML::BeginMap;
        emitter << YAML::Key << "Origin" << YAML::Value << YAML::Flow << YAML::BeginSeq << world.mOrigin.x << world.mOrigin.y << YAML::EndSeq;
        emitter << YAML::Key << "QuadsPerSection" << YAML::Value << world.mQuadsPerSection;
        emitter << YAML::Key << "SectionsPerComponent" << YAML::Value << world.mSectionsPerComponent;
        emitter << YAML::Key << "ComponentsX" << YAML::Value << world.mComponentsX;
        emitter << YAML::Key << "ComponentsY" << YAML::Value << world.mComponentsY;
        emitter << YAML::Key << "VertexSpacing" << YAML::Value << world.mVertexSpacing;
        emitter << YAML::Key << "HeightScale" << YAML::Value << world.mHeightScale;
        emitter << YAML::Key << "BaseElevation" << YAML::Value << world.mBaseElevation;
        emitter << YAML::Key << "LowColor" << YAML::Value << YAML::Flow << YAML::BeginSeq << world.mLowColor.x << world.mLowColor.y << world.mLowColor.z << YAML::EndSeq;
        emitter << YAML::Key << "MiddleColor" << YAML::Value << YAML::Flow << YAML::BeginSeq << world.mMiddleColor.x << world.mMiddleColor.y << world.mMiddleColor.z << YAML::EndSeq;
        emitter << YAML::Key << "HighColor" << YAML::Value << YAML::Flow << YAML::BeginSeq << world.mHighColor.x << world.mHighColor.y << world.mHighColor.z << YAML::EndSeq;
        emitter << YAML::Key << "EnableEditLayers" << YAML::Value << world.mEnableEditLayers;
        emitter << YAML::Key << "ShowGrid" << YAML::Value << landscape.IsGridVisible();
        emitter << YAML::Key << "ShowVertices" << YAML::Value << landscape.AreVerticesVisible();
        const auto &surface{ landscape.GetSurfaceSettings() };
        emitter << YAML::Key << "Surface" << YAML::Value << YAML::BeginMap;
        emitter << YAML::Key << "BlendWidth" << YAML::Value << surface.mBlendWidth;
        emitter << YAML::Key << "HeightBlendWidth" << YAML::Value << surface.mHeightBlendWidth;
        emitter << YAML::Key << "HeightBlend" << YAML::Value << surface.mHeightBlendEnabled;
        emitter << YAML::Key << "ElevationDebug" << YAML::Value << surface.mShowElevationColors;
        const auto &grass{ surface.mGrass };
        emitter << YAML::Key << "Grass" << YAML::Value << YAML::BeginMap;
        emitter << YAML::Key << "Enabled" << YAML::Value << grass.mEnabled;
        emitter << YAML::Key << "UseCustomMesh" << YAML::Value << grass.mUseCustomMesh;
        emitter << YAML::Key << "ModelPath" << YAML::Value << grass.mMesh.GetPath().GetC_Str();
        emitter << YAML::Key << "MeshIndex" << YAML::Value << grass.mMeshIndex;
        emitter << YAML::Key << "MeshWeight" << YAML::Value << grass.mMeshWeight;
        emitter << YAML::Key << "MeshVariants" << YAML::Value << YAML::BeginSeq;
        for ( const auto &variant: grass.mMeshVariants ) {
            emitter << YAML::BeginMap
                    << YAML::Key << "ModelPath" << YAML::Value << variant.mMesh.GetPath().GetC_Str()
                    << YAML::Key << "MeshIndex" << YAML::Value << variant.mMeshIndex
                    << YAML::Key << "Weight" << YAML::Value << variant.mWeight
                    << YAML::Key << "HeightScale" << YAML::Value << variant.mHeightScale
                    << YAML::Key << "WidthScale" << YAML::Value << variant.mWidthScale << YAML::EndMap;
        }
        emitter << YAML::EndSeq;
        emitter << YAML::Key << "OcclusionEnabled" << YAML::Value << grass.mOcclusionEnabled;
        emitter << YAML::Key << "ClumpStrength" << YAML::Value << grass.mClumpStrength;
        emitter << YAML::Key << "HeightVariation" << YAML::Value << grass.mHeightVariation;
        emitter << YAML::Key << "FarLodDistance" << YAML::Value << grass.mFarLodDistance;
        emitter << YAML::Key << "MinDensity" << YAML::Value << grass.mMinDensity;
        emitter << YAML::Key << "WidthCompensation" << YAML::Value << grass.mWidthCompensation;
        emitter << YAML::Key << "WindFrequency" << YAML::Value << grass.mWindFrequency;
        emitter << YAML::Key << "WindFlutter" << YAML::Value << grass.mWindFlutter;
        emitter << YAML::Key << "WindDistance" << YAML::Value << grass.mWindDistance;
        emitter << YAML::Key << "Translucency" << YAML::Value << grass.mTranslucency;
        emitter << YAML::Key << "TranslucencyPower" << YAML::Value << grass.mTranslucencyPower;
        emitter << YAML::Key << "NormalRounding" << YAML::Value << grass.mNormalRounding;
        emitter << YAML::Key << "DryAmount" << YAML::Value << grass.mDryAmount;
        emitter << YAML::Key << "ColorVariation" << YAML::Value << grass.mColorVariation;
        emitter << YAML::Key << "Layer" << YAML::Value << grass.mLayer;
        emitter << YAML::Key << "Seed" << YAML::Value << grass.mSeed;
        emitter << YAML::Key << "Density" << YAML::Value << grass.mDensity;
        emitter << YAML::Key << "Height" << YAML::Value << grass.mHeight;
        emitter << YAML::Key << "Width" << YAML::Value << grass.mWidth;
        emitter << YAML::Key << "Bend" << YAML::Value << grass.mBend;
        emitter << YAML::Key << "ClumpSize" << YAML::Value << grass.mClumpSize;
        emitter << YAML::Key << "ViewDistance" << YAML::Value << grass.mViewDistance;
        emitter << YAML::Key << "LodDistance" << YAML::Value << grass.mLodDistance;
        emitter << YAML::Key << "MaxSlope" << YAML::Value << grass.mMaxSlope;
        emitter << YAML::Key << "MinElevation" << YAML::Value << grass.mMinElevation;
        emitter << YAML::Key << "MaxElevation" << YAML::Value << grass.mMaxElevation;
        emitter << YAML::Key << "WindSpeed" << YAML::Value << grass.mWindSpeed;
        emitter << YAML::Key << "WindStrength" << YAML::Value << grass.mWindStrength;
        emitter << YAML::Key << "AmbientOcclusion" << YAML::Value << grass.mAmbientOcclusion;
        emitter << YAML::Key << "SelfShadow" << YAML::Value << grass.mSelfShadow;
        emitter << YAML::Key << "CastShadows" << YAML::Value << grass.mCastShadows;
        emitter << YAML::Key << "ContactShadows" << YAML::Value << grass.mContactShadows;
        emitter << YAML::Key << "ShadowDensity" << YAML::Value << grass.mShadowDensity;
        emitter << YAML::Key << "Roughness" << YAML::Value << grass.mRoughness;
        emitter << YAML::Key << "WindDirection" << YAML::Value << YAML::Flow << YAML::BeginSeq << grass.mWindDirection.x << grass.mWindDirection.y << YAML::EndSeq;
        for ( const auto &[name, color]: { std::pair{ "RootColor", grass.mRootColor }, std::pair{ "MidColor", grass.mMidColor },
                  std::pair{ "TipColor", grass.mTipColor }, std::pair{ "DryColor", grass.mDryColor } } ) {
            emitter << YAML::Key << name << YAML::Value << YAML::Flow << YAML::BeginSeq << color.x << color.y << color.z << YAML::EndSeq;
        }
        emitter << YAML::EndMap;
        emitter << YAML::Key << "Layers" << YAML::Value << YAML::BeginSeq;
        for ( const auto &layer: surface.mLayers ) {
            emitter << YAML::BeginMap;
            emitter << YAML::Key << "Name" << YAML::Value << layer.mName.c_str();
            emitter << YAML::Key << "Tint" << YAML::Value << YAML::Flow << YAML::BeginSeq << layer.mTint.x << layer.mTint.y << layer.mTint.z << YAML::EndSeq;
            emitter << YAML::Key << "UVScale" << YAML::Value << YAML::Flow << YAML::BeginSeq << layer.mUVScale.x << layer.mUVScale.y << YAML::EndSeq;
            emitter << YAML::Key << "HeightRange" << YAML::Value << YAML::Flow << YAML::BeginSeq << layer.mHeightRange.x << layer.mHeightRange.y << YAML::EndSeq;
            emitter << YAML::Key << "Roughness" << YAML::Value << layer.mRoughness;
            emitter << YAML::Key << "NormalStrength" << YAML::Value << layer.mNormalStrength;
            emitter << YAML::Key << "SlopeInfluence" << YAML::Value << layer.mSlopeInfluence;
            // Nested TexturePath keys use the scene's existing relative-path walker.
            for ( const auto &[key, reference]: { std::pair{ "Albedo", layer.mAlbedo },
                      std::pair{ "Normal", layer.mNormal }, std::pair{ "Height", layer.mHeight } } ) {
                emitter << YAML::Key << key << YAML::Value << YAML::BeginMap
                        << YAML::Key << "TexturePath" << YAML::Value << reference.GetPath().GetAbsolute().c_str() << YAML::EndMap;
            }
            emitter << YAML::EndMap;
        }
        emitter << YAML::EndSeq << YAML::EndMap;
        if ( const auto edited{ landscape.GetSculptedHeightmap() } ) {
            eastl::vector<core::f32> heights{};
            heights.reserve( core::usize{ edited->mWidth } * edited->mHeight );
            for ( core::u32 y{}; y < edited->mHeight; ++y ) {
                for ( core::u32 x{}; x < edited->mWidth; ++x ) {
                    heights.push_back( edited->Sample( { float( x ) / ( edited->mWidth - 1 ), float( y ) / ( edited->mHeight - 1 ) } ) );
                }
            }
            emitter << YAML::Key << "SculptedHeightmap" << YAML::Value << YAML::BeginMap
                    << YAML::Key << "Width" << YAML::Value << edited->mWidth
                    << YAML::Key << "Height" << YAML::Value << edited->mHeight
                    << YAML::Key << "Pixels" << YAML::Value << YAML::Binary{ reinterpret_cast<const unsigned char *>( heights.data() ), heights.size() * sizeof( core::f32 ) } << YAML::EndMap;
        }
        if ( const auto painted{ landscape.GetPaintMap() } ) {
            emitter << YAML::Key << "PaintMap" << YAML::Value << YAML::BeginMap
                    << YAML::Key << "Width" << YAML::Value << painted->mWidth
                    << YAML::Key << "Height" << YAML::Value << painted->mHeight
                    << YAML::Key << "Weights" << YAML::Value << YAML::Binary{ reinterpret_cast<const unsigned char *>( painted->mWeights.data() ), painted->mWeights.size() * sizeof( core::float4 ) } << YAML::EndMap;
        }
        // TexturePath participates in the existing scene-relative asset path conversion.
        emitter << YAML::Key << "TexturePath" << YAML::Value << landscape.GetHeightmapReference().GetPath().GetAbsolute().c_str();

        if ( landscape.GetNoiseGeneration() ) {
            const auto noise{ landscape.GetNoiseSettings() };
            emitter << YAML::Key << "GeneratedNoise" << YAML::Value << YAML::BeginMap;
            emitter << YAML::Key << "Frequency" << YAML::Value << noise.mFBM.mFrequency;
            emitter << YAML::Key << "Amplitude" << YAML::Value << noise.mFBM.mAmplitude;
            emitter << YAML::Key << "Persistence" << YAML::Value << noise.mFBM.mPersistence;
            emitter << YAML::Key << "Lacunarity" << YAML::Value << noise.mFBM.mLacunarity;
            emitter << YAML::Key << "Octaves" << YAML::Value << noise.mFBM.mOctaves;
            emitter << YAML::Key << "PerlinFrequency" << YAML::Value << noise.mPerlinFrequency;
            emitter << YAML::Key << "PerlinAmplitude" << YAML::Value << noise.mPerlinAmplitude;
            emitter << YAML::Key << "PerlinBlend" << YAML::Value << noise.mPerlinBlend;
            emitter << YAML::Key << "Offset" << YAML::Value << YAML::Flow << YAML::BeginSeq << noise.mOffset.x << noise.mOffset.y << YAML::EndSeq;
            emitter << YAML::EndMap;
        }

        emitter << YAML::EndMap;
    }

    static auto SerializeComponent( const WaterRenderComponent &water, YAML::Emitter &emitter, SceneMaterialFiles &materialFiles ) -> void {
        const auto size{ water.GetPlaneDimensions() }, resolution{ water.GetPlaneResolution() };
        const auto debugColor{ water.GetDebugPreviewColor() };
        const auto &appearance{ water.GetAppearance() };
        emitter << YAML::BeginMap
                << YAML::Key << "Shape" << YAML::Value << static_cast<core::u32>( water.GetShape() )
                << YAML::Key << "Dimensions" << YAML::Value << YAML::Flow << YAML::BeginSeq << size.x << size.y << YAML::EndSeq
                << YAML::Key << "Resolution" << YAML::Value << YAML::Flow << YAML::BeginSeq << resolution.x << resolution.y << YAML::EndSeq
                << YAML::Key << "ShowGrid" << YAML::Value << water.IsGridVisible()
                << YAML::Key << "ShowVertices" << YAML::Value << water.AreVerticesVisible()
                << YAML::Key << "DebugPreviewColor" << YAML::Value << YAML::Flow << YAML::BeginSeq << debugColor.x << debugColor.y << debugColor.z << YAML::EndSeq
                << YAML::Key << "FoamAmount" << YAML::Value << water.GetFoamAmount()
                << YAML::Key << "Appearance" << YAML::Value << YAML::BeginMap
                << YAML::Key << "InfiniteOcean" << YAML::Value << appearance.mInfiniteOcean
                << YAML::Key << "DepthBasedRefraction" << YAML::Value << appearance.mDepthBasedRefractionEnabled
                << YAML::Key << "ShoreFoam" << YAML::Value << appearance.mShoreFoamEnabled
                << YAML::Key << "UnderwaterScattering" << YAML::Value << appearance.mUnderwaterScatteringEnabled
                << YAML::Key << "Caustics" << YAML::Value << appearance.mCausticsEnabled
                << YAML::Key << "ShoreFoamDepth" << YAML::Value << appearance.mShoreFoamDepth
                << YAML::Key << "ShoreFoamAmount" << YAML::Value << appearance.mShoreFoamAmount
                << YAML::Key << "FoamLifetime" << YAML::Value << appearance.mFoamLifetime
                << YAML::Key << "WaveDepth" << YAML::Value << appearance.mWaveDepth
                << YAML::Key << "ShoreWaveSpeed" << YAML::Value << appearance.mShoreWaveSpeed
                << YAML::Key << "Scattering" << YAML::Value << YAML::Flow << YAML::BeginSeq << appearance.mScattering.x << appearance.mScattering.y << appearance.mScattering.z << YAML::EndSeq
                << YAML::Key << "Anisotropy" << YAML::Value << appearance.mAnisotropy
                << YAML::Key << "MaximumDepth" << YAML::Value << appearance.mMaximumDepth
                << YAML::Key << "CausticsStrength" << YAML::Value << appearance.mCausticsStrength
                << YAML::Key << "CausticsScale" << YAML::Value << appearance.mCausticsScale
                << YAML::Key << "CausticsDepth" << YAML::Value << appearance.mCausticsDepth
                << YAML::EndMap
                << YAML::Key << "MaterialPath" << YAML::Value << materialFiles.Save( water.GetMaterial() ).GetC_Str() << YAML::EndMap;
    }

    static auto SerializeComponent( const WaterSimulationComponent &water, YAML::Emitter &emitter ) -> void {
        const auto &fft{ water.GetFftSettings() };

        emitter << YAML::BeginMap
                << YAML::Key << "Type" << YAML::Value << static_cast<core::u32>( water.GetType() )
                << YAML::Key << "TimeScale" << YAML::Value << water.GetTimeScale()
                << YAML::Key << "Steepness" << YAML::Value << water.GetSteepness()
                << YAML::Key << "FFT" << YAML::Value << YAML::BeginMap
                << YAML::Key << "PatchLength" << YAML::Value << fft.mPatchLength
                << YAML::Key << "WindSpeed" << YAML::Value << fft.mWindSpeed
                << YAML::Key << "WindDirection" << YAML::Value << fft.mWindDirection
                << YAML::Key << "Amplitude" << YAML::Value << fft.mAmplitude
                << YAML::Key << "Choppiness" << YAML::Value << fft.mChoppiness
                << YAML::Key << "Seed" << YAML::Value << fft.mSeed << YAML::EndMap
                << YAML::Key << "Waves" << YAML::Value << YAML::BeginSeq;

        for ( const auto &wave: water.GetWaves() ) {
            emitter << YAML::BeginMap
                    << YAML::Key << "Direction" << YAML::Value << YAML::Flow << YAML::BeginSeq << wave.mDirection.x << wave.mDirection.y << YAML::EndSeq
                    << YAML::Key << "Amplitude" << YAML::Value << wave.mAmplitude
                    << YAML::Key << "Wavelength" << YAML::Value << wave.mWavelength
                    << YAML::Key << "Speed" << YAML::Value << wave.mSpeed
                    << YAML::Key << "PhaseOffset" << YAML::Value << wave.mPhaseOffset << YAML::EndMap;
        }

        emitter << YAML::EndSeq << YAML::EndMap;
    }

    static auto SerializeComponent( const AudioMixerComponent &mixer, YAML::Emitter &emitter ) -> void {
        emitter << YAML::BeginMap << YAML::Key << "Volume" << YAML::Value << mixer.GetVolume()
                << YAML::Key << "Muted" << YAML::Value << mixer.IsMuted() << YAML::EndMap;
    }

    static auto SerializeComponent( const AudioMixerGroupComponent &group, YAML::Emitter &emitter ) -> void {
        emitter << YAML::BeginMap << YAML::Key << "Mixer" << YAML::Value << group.mMixerID
                << YAML::Key << "Output" << YAML::Value << group.mOutputID
                << YAML::Key << "Volume" << YAML::Value << group.mVolume
                << YAML::Key << "Muted" << YAML::Value << group.mMuted << YAML::EndMap;
    }

    static auto SerializeComponent( const VehicleComponent &vehicle, YAML::Emitter &emitter ) -> void {
        emitter << YAML::BeginMap;
        emitter << YAML::Key << "HalfTrack" << YAML::Value << vehicle.mHalfTrack;
        emitter << YAML::Key << "HalfWheelbase" << YAML::Value << vehicle.mHalfWheelbase;
        emitter << YAML::Key << "WheelRadius" << YAML::Value << vehicle.mWheelRadius;
        emitter << YAML::Key << "WheelWidth" << YAML::Value << vehicle.mWheelWidth;
        emitter << YAML::Key << "SuspensionMin" << YAML::Value << vehicle.mSuspensionMin;
        emitter << YAML::Key << "SuspensionMax" << YAML::Value << vehicle.mSuspensionMax;
        emitter << YAML::Key << "MaxSteerDegrees" << YAML::Value << vehicle.mMaxSteerDegrees;
        emitter << YAML::Key << "EngineTorque" << YAML::Value << vehicle.mEngineTorque;
        emitter << YAML::Key << "WheelAnchorHeight" << YAML::Value << vehicle.mWheelAnchorHeight;
        emitter << YAML::Key << "CenterOfMassOffsetY" << YAML::Value << vehicle.mCenterOfMassOffsetY;
        emitter << YAML::Key << "SuspensionFrequency" << YAML::Value << vehicle.mSuspensionFrequency;
        emitter << YAML::Key << "SuspensionDamping" << YAML::Value << vehicle.mSuspensionDamping;
        emitter << YAML::Key << "MaxPitchRollDegrees" << YAML::Value << vehicle.mMaxPitchRollDegrees;
        emitter << YAML::Key << "LongitudinalImpulseScale" << YAML::Value << vehicle.mLongitudinalImpulseScale;
        emitter << YAML::Key << "FourWheelDrive" << YAML::Value << vehicle.mFourWheelDrive;
        emitter << YAML::Key << "CylinderWheelCast" << YAML::Value << vehicle.mCylinderWheelCast;
        SerializeVehicleDetails( vehicle, emitter );
        emitter << YAML::EndMap;
    }

    static auto SerializeComponent( const BridgeSegmentComponent &segment, YAML::Emitter &emitter ) -> void {
        emitter << YAML::BeginMap << YAML::Key << "Index" << YAML::Value << segment.mIndex << YAML::EndMap;
    }

    static auto SerializeComponent( const VehicleWheelComponent &wheel, YAML::Emitter &emitter ) -> void {
        emitter << YAML::BeginMap << YAML::Key << "Index" << YAML::Value << wheel.mIndex;
        emitter << YAML::Key << "FollowSpin" << YAML::Value << wheel.mFollowSpin;
        emitter << YAML::Key << "VisualCorrection" << YAML::Value << YAML::Flow << YAML::BeginSeq;

        for ( core::u32 column{}; column < 4; ++column ) {
            for ( core::u32 row{}; row < 4; ++row ) {
                emitter << wheel.mVisualCorrection[column][row];
            }
        }

        emitter << YAML::EndSeq << YAML::EndMap;
    }


    static auto SerializeComponent( const RigidBodyComponent &physicsComponent, YAML::Emitter &emitter ) -> void {
        MKT_BEGIN_PROFILER();
        emitter << SerializeRigidBody( physicsComponent );
    }


    static auto SerializeComponent( const MeshColliderComponent &physicsComponent, YAML::Emitter &emitter ) -> void {
        MKT_BEGIN_PROFILER();
        emitter << SerializeCollider( physicsComponent );
    }


    static auto SerializeComponent( const BoxColliderComponent &collider, YAML::Emitter &emitter ) -> void {
        MKT_BEGIN_PROFILER();
        emitter << SerializeCollider( collider );
    }


    static auto SerializeComponent( const SphereColliderComponent &collider, YAML::Emitter &emitter ) -> void {
        MKT_BEGIN_PROFILER();
        emitter << SerializeCollider( collider );
    }


    static auto SerializeComponent( const CapsuleColliderComponent &collider, YAML::Emitter &emitter ) -> void {
        MKT_BEGIN_PROFILER();
        emitter << SerializeCollider( collider );
    }


    static auto SerializeComponent( const CameraComponent &cameraComponent, YAML::Emitter &emitter ) -> void {
        MKT_BEGIN_PROFILER();
        emitter << YAML::BeginMap;

        if ( cameraComponent.HasCamera() ) {
            const auto &camera{ cameraComponent.GetCamera() };
            emitter << YAML::Key << "Projection" << YAML::Value << static_cast<core::i32>( camera.GetProjectionType() );
            emitter << YAML::Key << "Fov" << YAML::Value << camera.GetFOV();
            emitter << YAML::Key << "Near" << YAML::Value << camera.GetNearPlane();
            emitter << YAML::Key << "Far" << YAML::Value << camera.GetFarPlane();
            emitter << YAML::Key << "OrthographicHeight" << YAML::Value << camera.GetOrthographicHeight();
            emitter << YAML::Key << "Main" << YAML::Value << cameraComponent.IsMainCamera();
            emitter << YAML::Key << "FixedAspect" << YAML::Value << cameraComponent.IsAspectRatioFixed();
            emitter << YAML::Key << "Viewport" << YAML::Value << mikoto::core::float3{ camera.GetViewPort().first, camera.GetViewPort().second, 0 };
            emitter << YAML::Key << "ClearFlags" << YAML::Value << static_cast<core::i32>( cameraComponent.GetClearFlags() );
            emitter << YAML::Key << "ClearColor" << YAML::Value << mikoto::core::float4{ cameraComponent.GetClearColor() };
            emitter << YAML::Key << "Gamma" << YAML::Value << cameraComponent.GetGamma();
            emitter << YAML::Key << "Exposure" << YAML::Value << cameraComponent.GetExposure();
            emitter << YAML::Key << "SkyboxBlur" << YAML::Value << cameraComponent.GetSkyboxBlur();

            const auto &settings{ camera.GetDepthOfFieldSettings() };
            emitter << YAML::Key << "DepthOfField" << YAML::Value << YAML::BeginMap;
            emitter << YAML::Key << "Enabled" << YAML::Value << settings.mEnabled;
            emitter << YAML::Key << "Aperture" << YAML::Value << settings.mAperture;
            emitter << YAML::Key << "FocalLength" << YAML::Value << settings.mFocalLength;
            emitter << YAML::Key << "FocusDistance" << YAML::Value << settings.mFocusDistance;
            emitter << YAML::Key << "MaxBlur" << YAML::Value << settings.mMaxBlur;
            emitter << YAML::Key << "MaxAperture" << YAML::Value << settings.mMaxAperture;
            emitter << YAML::Key << "SensorWidth" << YAML::Value << settings.mSensorWidth;
            emitter << YAML::Key << "UsePhysicalLens" << YAML::Value << settings.mUsePhysicalLens;
            emitter << YAML::Key << "BladeCount" << YAML::Value << settings.mBladeCount;
            emitter << YAML::Key << "BladeCurvature" << YAML::Value << settings.mBladeCurvature;
            emitter << YAML::Key << "BladeRotation" << YAML::Value << settings.mBladeRotation;
            emitter << YAML::EndMap;

            const auto &vignette{ camera.GetVignetteSettings() };
            emitter << YAML::Key << "Vignette" << YAML::Value << YAML::BeginMap;
            emitter << YAML::Key << "Enabled" << YAML::Value << vignette.mEnabled;
            emitter << YAML::Key << "Shape" << YAML::Value << core::as<core::u32>( vignette.mShape );
            emitter << YAML::Key << "Center" << YAML::Value << YAML::Flow << YAML::BeginSeq
                    << vignette.mCenter.x << vignette.mCenter.y << YAML::EndSeq;
            emitter << YAML::Key << "HalfSize" << YAML::Value << YAML::Flow << YAML::BeginSeq
                    << vignette.mHalfSize.x << vignette.mHalfSize.y << YAML::EndSeq;
            emitter << YAML::Key << "Feather" << YAML::Value << vignette.mFeather;
            emitter << YAML::Key << "Strength" << YAML::Value << vignette.mStrength;
            emitter << YAML::Key << "Color" << YAML::Value << vignette.mColor;
            emitter << YAML::Key << "AspectCorrect" << YAML::Value << vignette.mAspectCorrect;
            emitter << YAML::EndMap;

            emitter << YAML::Key << "FilmGrain" << YAML::Value << YAML::BeginMap;
            emitter << YAML::Key << "Amount" << YAML::Value << camera.GetFilmGrainSettings().mAmount;
            emitter << YAML::EndMap;
        }

        emitter << YAML::EndMap;
    }


    static auto SerializeComponent( const CanvasComponent &component, YAML::Emitter &emitter ) -> void {
        emitter << SerializeCanvas( component.GetCanvas() );
    }


    static auto SerializeComponent( const TextComponent &textComponent, YAML::Emitter &emitter, SceneMaterialFiles &materialFiles ) -> void {
        MKT_BEGIN_PROFILER();
        emitter << YAML::BeginMap;

        emitter << YAML::Key << "Contents" << YAML::Value << textComponent.GetContents().c_str();
        emitter << YAML::Key << "Color" << YAML::Value << textComponent.GetColor();
        emitter << YAML::Key << "OutlineWidth" << YAML::Value << textComponent.GetOutlineWidth();
        emitter << YAML::Key << "OutlineColor" << YAML::Value << textComponent.GetOutlineColor();

        if ( const auto material{ textComponent.GetMaterial() } ) {
            emitter << YAML::Key << "MaterialPath" << YAML::Value << materialFiles.Save( material ).GetC_Str();
        }

        emitter << YAML::Key << "IsWorldText" << YAML::Value << textComponent.IsWorldText();
        emitter << YAML::Key << "Size" << YAML::Value << textComponent.GetSize();
        emitter << YAML::Key << "Spacing" << YAML::Value << textComponent.GetSpacing();
        emitter << YAML::Key << "WorldSize" << YAML::Value << textComponent.GetSize( true );
        emitter << YAML::Key << "ScreenSize" << YAML::Value << textComponent.GetSize( false );
        emitter << YAML::Key << "WorldSpacing" << YAML::Value << textComponent.GetSpacing( true );
        emitter << YAML::Key << "ScreenSpacing" << YAML::Value << textComponent.GetSpacing( false );

        const auto &screen = textComponent.GetScreenPlacement();
        emitter << YAML::Key << "ScreenPosition" << YAML::Value << mikoto::core::float3{ screen.mPosition, 0 };
        emitter << YAML::Key << "ScreenScale" << YAML::Value << mikoto::core::float3{ screen.mScale, 1 };
        emitter << YAML::Key << "ScreenRotation" << YAML::Value << screen.mRotation;
        emitter << YAML::Key << "LineSpacing" << YAML::Value << textComponent.GetLineSpacing();
        emitter << YAML::Key << "Alignment" << YAML::Value << static_cast<int>( textComponent.GetAlignment() );
        emitter << YAML::Key << "Backend" << YAML::Value << static_cast<int>( textComponent.GetBackend() );

        if ( textComponent.HasFont() ) {
            emitter << YAML::Key << "FontPath" << YAML::Value << textComponent.GetFont()->GetPath().GetC_Str();
        }

        emitter << YAML::EndMap;
    }


    static auto SerializeComponent( const SkyLightComponent &sky, YAML::Emitter &emitter, SceneMaterialFiles &materialFiles ) -> void {
        emitter << YAML::BeginMap
                << YAML::Key << "Enabled" << YAML::Value << sky.IsEnabled()
                << YAML::Key << "Source" << YAML::Value << core::as<core::u32>( sky.GetSource() )
                << YAML::Key << "Intensity" << YAML::Value << sky.GetIntensity()
                << YAML::Key << "OcclusionEnabled" << YAML::Value << sky.IsOcclusionEnabled()
                << YAML::Key << "OcclusionDistance" << YAML::Value << sky.GetOcclusionDistance()
                << YAML::Key << "OcclusionSamplesPerFrame" << YAML::Value << sky.GetOcclusionSamplesPerFrame();

        if ( sky.GetEnvironmentMaterial() ) {
            emitter << YAML::Key << "MaterialPath" << YAML::Value << materialFiles.Save( sky.GetEnvironmentMaterial() ).GetC_Str();
        }

        emitter << YAML::EndMap;
    }

    static auto SerializeComponent( SkyboxMaterialComponent component, YAML::Emitter &emitter, SceneMaterialFiles &materialFiles ) -> void {
        MKT_BEGIN_PROFILER();
        YAML::Node node{};

        if ( auto material{ component.GetMaterial() } ) {
            node["MaterialPath"] = materialFiles.Save( material ).GetC_Str();
        }

        emitter << node;
    }

    static auto SerializeComponent( const PostProcessMaterialComponent &component, YAML::Emitter &emitter, SceneMaterialFiles &materialFiles ) -> void {
        YAML::Node node{};
        node["CameraEntityID"] = component.GetCameraEntityID();

        if ( const auto material{ component.GetMaterial() } ) {
            node["MaterialPath"] = materialFiles.Save( material ).GetC_Str();
        }

        emitter << node;
    }

    static auto DecodeRigSettings( const nlohmann::json &data, Animator &animator, core::u32 rig, eastl::string &error ) -> bool {
        MKT_BEGIN_PROFILER();

        try {
            if ( !data.at( "aim" ).is_array() || !data.at( "two_bone" ).is_array() ||
                 data.at( "aim" ).size() > 32 || data.at( "two_bone" ).size() > 32 ) {
                MKT_THROW_RUNTIME_ERROR( "Unsupported animator settings." );
            }

            const auto model{ animator.GetModel() };

            if ( model.IsEmpty() || !model->HasArmature() ) {
                MKT_THROW_RUNTIME_ERROR( "Animator has no skeleton." );
            }

            const core::i32 count{ model->GetSkeleton( rig )->GetOzzSkeleton()->num_joints() };
            const auto joint{ [count]( const nlohmann::json &value ) -> core::i32 {
                if ( !value.is_number_integer() || value.get<core::f64>() < -1 || value.get<core::f64>() >= count ) {
                    MKT_THROW_RUNTIME_ERROR( "IK joint is outside the skeleton." );
                }

                return value.get<core::i32>();
            } };
            eastl::vector<AimIKConstraint> aims{};
            eastl::vector<TwoBoneIKConstraint> chains{};

            for ( const auto &value: data.at( "aim" ) ) {
                AimIKConstraint aim{};
                aim.mJoint = joint( value.at( "joint" ) );
                aim.mEnabled = value.at( "enabled" ).get<bool>();
                aim.mTarget = Vector( value.at( "target" ) );
                aim.mPole = Vector( value.at( "pole" ) );
                aim.mForward = Vector( value.at( "forward" ) );
                aim.mUp = Vector( value.at( "up" ) );
                aim.mOffset = Vector( value.at( "offset" ) );
                aim.mWeight = Scalar( value.at( "weight" ), 0, 1 );
                aim.mTwist = Scalar( value.at( "twist" ), -1000000, 1000000 );
                aims.push_back( aim );
            }

            for ( const auto &value: data.at( "two_bone" ) ) {
                TwoBoneIKConstraint chain{};
                chain.mStart = joint( value.at( "start" ) );
                chain.mMiddle = joint( value.at( "middle" ) );
                chain.mEnd = joint( value.at( "end" ) );
                chain.mEnabled = value.at( "enabled" ).get<bool>();
                chain.mTarget = Vector( value.at( "target" ) );
                chain.mPole = Vector( value.at( "pole" ) );
                chain.mMiddleAxis = Vector( value.at( "axis" ) );
                chain.mWeight = Scalar( value.at( "weight" ), 0, 1 );
                chain.mTwist = Scalar( value.at( "twist" ), -1000000, 1000000 );
                chain.mSoften = Scalar( value.at( "soften" ), 0, 1 );
                chains.push_back( chain );
            }
            // Publish only after every entry validates; a failed load leaves the
            // previous animator settings intact.
            animator.GetAimConstraints( rig ) = eastl::move( aims );
            animator.GetTwoBoneConstraints( rig ) = eastl::move( chains );
            error.clear();
            return true;
        } catch ( const std::exception &exception ) {
            error = exception.what();
            return false;
        }
    }


    static auto EncodeRigSettings( const Animator &animator, core::u32 rig ) -> nlohmann::json {
        MKT_BEGIN_PROFILER();
        nlohmann::json data{ { "aim", nlohmann::json::array() }, { "two_bone", nlohmann::json::array() } };

        for ( const auto &aim: animator.GetAimConstraints( rig ) ) {
            data["aim"].push_back( { { "joint", aim.mJoint }, { "enabled", aim.mEnabled }, { "target", Vector( aim.mTarget ) }, { "pole", Vector( aim.mPole ) }, { "forward", Vector( aim.mForward ) }, { "up", Vector( aim.mUp ) }, { "offset", Vector( aim.mOffset ) }, { "weight", aim.mWeight }, { "twist", aim.mTwist } } );
        }

        for ( const auto &chain: animator.GetTwoBoneConstraints( rig ) ) {
            data["two_bone"].push_back( { { "start", chain.mStart }, { "middle", chain.mMiddle }, { "end", chain.mEnd }, { "enabled", chain.mEnabled }, { "target", Vector( chain.mTarget ) }, { "pole", Vector( chain.mPole ) }, { "axis", Vector( chain.mMiddleAxis ) }, { "weight", chain.mWeight }, { "twist", chain.mTwist }, { "soften", chain.mSoften } } );
        }

        return data;
    }


    static auto EncodeAnimatorSettings( const Animator &animator ) -> nlohmann::json {
        nlohmann::json data{ { "version", 2 }, { "blend", animator.GetTransitionDuration() }, { "rigs", nlohmann::json::array() } };
        const auto model{ animator.GetModel() };

        for ( core::u32 rig{}; model && rig < model->GetRigs().size(); ++rig ) {
            auto settings{ EncodeRigSettings( animator, rig ) };
            settings["name"] = model->GetRigs()[rig].mName.c_str();
            data["rigs"].push_back( eastl::move( settings ) );
        }

        return data;
    }


    static auto DecodeAnimatorSettings( const nlohmann::json &data, Animator &animator, eastl::string &error ) -> bool {
        try {
            // Parse into private pose state first. Invalid data for any rig must
            // not partially replace the settings on a live character.
            Animator pending{ animator.GetModel() };
            const auto model{ animator.GetModel() };

            if ( !model || !model->HasArmature() ) {
                MKT_THROW_RUNTIME_ERROR( "Animator has no skeleton." );
            }

            const core::u32 count{ static_cast<core::u32>( model->GetRigs().size() ) };
            pending.SetTransitionDuration( Scalar( data.at( "blend" ), 0, 30 ) );

            if ( data.at( "version" ) == 1 ) {
                if ( !DecodeRigSettings( data, pending, 0, error ) ) {
                    return false;
                }
                // Legacy projects only authored settings for their single rig.
                for ( core::u32 rig{ 1 }; rig < count; ++rig ) {
                    pending.GetAimConstraints( rig ) = animator.GetAimConstraints( rig );
                    pending.GetTwoBoneConstraints( rig ) = animator.GetTwoBoneConstraints( rig );
                }
            }
            else if ( data.at( "version" ) == 2 && data.at( "rigs" ).is_array() && data.at( "rigs" ).size() == count ) {
                for ( core::u32 rig{}; rig < count; ++rig ) {
                    const auto &settings{ data.at( "rigs" ).at( rig ) };

                    if ( settings.at( "name" ) != model->GetRigs()[rig].mName.c_str() ) {
                        MKT_THROW_RUNTIME_ERROR( "Animator settings refer to a different skeleton." );
                    }

                    if ( !DecodeRigSettings( settings, pending, rig, error ) ) {
                        return false;
                    }
                }
            }
            else {
                MKT_THROW_RUNTIME_ERROR( "Unsupported animator rig settings." );
            }

            animator.SetTransitionDuration( pending.GetTransitionDuration() );

            for ( core::u32 rig{}; rig < count; ++rig ) {
                animator.GetAimConstraints( rig ) = eastl::move( pending.GetAimConstraints( rig ) );
                animator.GetTwoBoneConstraints( rig ) = eastl::move( pending.GetTwoBoneConstraints( rig ) );
            }

            error.clear();
            return true;
        } catch ( const std::exception &exception ) {
            error = exception.what();
            return false;
        }
    }

    static auto SerializeComponent( const AnimatorComponent &component, YAML::Emitter &emitter ) -> void {
        MKT_BEGIN_PROFILER();
        YAML::Node node{};

        node["ID"] = component.GetAnimatorID();

        if ( auto *animator{ animation::AnimationSystem::Get()->GetAnimator( component.GetAnimatorID() ) } ) {
            node["ModelPath"] = animator->GetModel()->GetPath().GetC_Str();

            if ( const auto *clip{ animator->GetCurrentAnimation() } ) {
                node["Clip"] = clip->GetName().c_str();
            }

            node["Playing"] = animator->IsPlaying();
            node["ControllerPlaying"] = animator->GetControllerPlayback().IsPlaying();
            node["Controller"] = animator->GetController().EncodeController().dump();
            node["Settings"] = EncodeAnimatorSettings( *animator ).dump();
        }

        emitter << node;
    }

    static auto SerializeComponent( const SkinnedMeshRenderer &component, YAML::Emitter &emitter ) -> void {
        MKT_BEGIN_PROFILER();
        YAML::Node node;
        node["AnimatorID"] = component.GetAnimatorID();
        emitter << node;
    }

    static auto SerializeComponent( const ScriptComponent &scriptComponent, YAML::Emitter &emitter ) -> void {
        MKT_BEGIN_PROFILER();
        emitter << YAML::BeginMap;
        emitter << YAML::Key << "ScriptPath" << YAML::Value << scriptComponent.GetFilePath().GetC_Str();
        emitter << YAML::Key << "Language" << YAML::Value << ( scriptComponent.GetLanguage() == scripting::ScriptLanguage::eNative ? "Cpp" : scriptComponent.GetLanguage() == scripting::ScriptLanguage::eCSharp ? "CSharp"
                                                                                                                                                                                                                 : "Lua" );
        emitter << YAML::Key << "ClassName" << YAML::Value << scriptComponent.GetClassName().c_str();
        emitter << YAML::Key << "Properties" << YAML::Value << scriptComponent.GetProperties().c_str();
        emitter << YAML::EndMap;
    }

    static auto SerializeComponent( const KeyframeAnimationComponent &component, YAML::Emitter &emitter ) -> void {
        emitter << component.mClip.EncodeKeyframeClip();
    }

    static auto SerializeNode( YAML::Emitter &emitter, const Entity *root, const Scene &scene,
        SceneMaterialIDs &materialIDs, YAML::Node &materials, SceneMaterialFiles &materialFiles ) -> void {
        MKT_BEGIN_PROFILER();

        if ( root == nullptr ) {
            return;
        }

        emitter << YAML::BeginMap;
        emitter << YAML::Key << "Type" << YAML::Value << static_cast<int>( root->GetType() );

        // Document references are remapped to fresh runtime GUIDs on load.
        emitter << YAML::Key << "ID" << YAML::Value << root->GetComponent<TagComponent>().GetGuid();

        if ( root->HasComponent<GroupComponent>() ) {
            emitter << YAML::Key << "GroupComponent" << YAML::Value << YAML::BeginMap;
            emitter << YAML::Key << "Members" << YAML::Value << YAML::BeginSeq;

            for ( const auto member: root->GetComponent<GroupComponent>().GetChildren() ) {
                emitter << member;
            }

            emitter << YAML::EndSeq << YAML::EndMap;
        }

        // Tag and Transform components are always present
        emitter << YAML::Key << "TagComponent" << YAML::Value;
        SerializeComponent( root->GetComponent<TagComponent>(), emitter );

        emitter << YAML::Key << "TransformComponent" << YAML::Value;
        const auto *preview{
            root->HasComponent<KeyframeAnimationComponent>()
                ? &root->GetComponent<KeyframeAnimationComponent>()
                : nullptr
        };

        if ( preview && preview->IsPreviewing() ) {
            // Saving/Play snapshots must retain the authored pose, not whichever
            // frame happened to be under the sequencer's preview playhead.
            auto transform{ root->GetComponent<TransformComponent>() };
            transform.SetTransform( preview->GetRestTransform() );
            SerializeComponent( transform, emitter );
        }
        else {
            SerializeComponent( root->GetComponent<TransformComponent>(), emitter );
        }

        if ( const auto pending{ scene.GetModelCreationProgress( root->GetComponent<TagComponent>().GetGuid() ) } ) {
            // Keep the existing file key so saved pending imports remain compatible.
            emitter << YAML::Key << "StreamedModelComponent" << YAML::Value << YAML::BeginMap;
            emitter << YAML::Key << "ModelPath" << YAML::Value << pending->mReference.GetPath().GetC_Str();
            emitter << YAML::Key << "NextMesh" << YAML::Value << pending->mNextMesh;
            emitter << YAML::EndMap;
        }

        MKT_SERIALIZE_COMPONENT_IF_PRESENT( MeshComponent, "MeshComponent" );

        if ( root->HasComponent<MaterialComponent>() ) {
            const auto source{ root->GetComponent<MaterialComponent>().GetMaterial() };

            if ( const auto *physical{ dynamic_cast<const material::SurfaceMaterial *>( source.GetPtr() ) } ) {
                const auto [slot, added]{ materialIDs.try_emplace( physical, static_cast<core::u32>( materialIDs.size() ) ) };

                if ( added ) {
                    YAML::Node reference{};
                    reference["MaterialPath"] = materialFiles.Save( source ).GetC_Str();
                    materials.push_back( reference );
                }

                emitter << YAML::Key << "MaterialComponent" << YAML::Value << YAML::BeginMap;
                emitter << YAML::Key << "Reference" << YAML::Value << slot->second;
                emitter << YAML::EndMap;
            }
        }

        if ( root->HasComponent<SkyLightComponent>() ) {
            emitter << YAML::Key << "SkyLightComponent" << YAML::Value;
            SerializeComponent( root->GetComponent<SkyLightComponent>(), emitter, materialFiles );
        }

        if ( root->HasComponent<SkyboxMaterialComponent>() ) {
            emitter << YAML::Key << "SkyboxMaterialComponent" << YAML::Value;
            SerializeComponent( root->GetComponent<SkyboxMaterialComponent>(), emitter, materialFiles );
        }

        if ( root->HasComponent<PostProcessMaterialComponent>() ) {
            emitter << YAML::Key << "PostProcessMaterialComponent" << YAML::Value;
            SerializeComponent( root->GetComponent<PostProcessMaterialComponent>(), emitter, materialFiles );
        }

        MKT_SERIALIZE_COMPONENT_IF_PRESENT( AnimatorComponent, "AnimatorComponent" );
        MKT_SERIALIZE_COMPONENT_IF_PRESENT( KeyframeAnimationComponent, "KeyframeAnimationComponent" );
        MKT_SERIALIZE_COMPONENT_IF_PRESENT( SkinnedMeshRenderer, "SkinnedMeshRenderer" );

        if ( root->HasComponent<LightComponent>() ) {
            auto light{ root->GetComponent<LightComponent>() };

            if ( preview && preview->IsPreviewing() ) {
                preview->RestoreLightProperties( light );
            }

            emitter << YAML::Key << "LightComponent" << YAML::Value;
            SerializeComponent( light, emitter );
        }

        MKT_SERIALIZE_COMPONENT_IF_PRESENT( AudioSourceComponent, "AudioSourceComponent" );
        MKT_SERIALIZE_COMPONENT_IF_PRESENT( VideoComponent, "VideoComponent" );

        if ( root->HasComponent<TextureRenderComponent>() ) {
            emitter << YAML::Key << "TextureRenderComponent" << YAML::Value;
            SerializeComponent( root->GetComponent<TextureRenderComponent>(), emitter, scene );
        }

        MKT_SERIALIZE_COMPONENT_IF_PRESENT( AudioMixerComponent, "AudioMixerComponent" );
        MKT_SERIALIZE_COMPONENT_IF_PRESENT( AudioMixerGroupComponent, "AudioMixerGroupComponent" );
        MKT_SERIALIZE_COMPONENT_IF_PRESENT( AudioListenerComponent, "AudioListenerComponent" );
        MKT_SERIALIZE_COMPONENT_IF_PRESENT( ParticleSimulatorComponent, "ParticleSimulatorComponent" );
        MKT_SERIALIZE_COMPONENT_IF_PRESENT( CloudInteractionComponent, "CloudInteractionComponent" );

        if ( root->HasComponent<CloudComponent>() ) {
            emitter << YAML::Key << "CloudComponent" << YAML::Value;
            SerializeVolumeComponent( root->GetComponent<CloudComponent>(), emitter, materialFiles );
        }
        if ( root->HasComponent<FogComponent>() ) {
            emitter << YAML::Key << "FogComponent" << YAML::Value;
            SerializeVolumeComponent( root->GetComponent<FogComponent>(), emitter, materialFiles );
        }

        if ( root->HasComponent<WaterRenderComponent>() ) {
            emitter << YAML::Key << "WaterRenderComponent" << YAML::Value;
            SerializeComponent( root->GetComponent<WaterRenderComponent>(), emitter, materialFiles );
        }

        MKT_SERIALIZE_COMPONENT_IF_PRESENT( WaterSimulationComponent, "WaterSimulationComponent" );
        MKT_SERIALIZE_COMPONENT_IF_PRESENT( LandscapeComponent, "LandscapeComponent" );
        MKT_SERIALIZE_COMPONENT_IF_PRESENT( CameraComponent, "CameraComponent" );
        MKT_SERIALIZE_COMPONENT_IF_PRESENT( ScriptComponent, "ScriptComponent" );

        if ( root->HasComponent<TextComponent>() ) {
            emitter << YAML::Key << "TextComponent" << YAML::Value;
            SerializeComponent( root->GetComponent<TextComponent>(), emitter, materialFiles );
        }

        MKT_SERIALIZE_COMPONENT_IF_PRESENT( CanvasComponent, "CanvasComponent" );

        MKT_SERIALIZE_COMPONENT_IF_PRESENT( VehicleComponent, "VehicleComponent" );
        MKT_SERIALIZE_COMPONENT_IF_PRESENT( VehicleWheelComponent, "VehicleWheelComponent" );
        MKT_SERIALIZE_COMPONENT_IF_PRESENT( BridgeSegmentComponent, "BridgeSegmentComponent" );
        MKT_SERIALIZE_COMPONENT_IF_PRESENT( RigidBodyComponent, "RigidBodyComponent" );
        MKT_SERIALIZE_COMPONENT_IF_PRESENT( BoxColliderComponent, "BoxColliderComponent" );
        MKT_SERIALIZE_COMPONENT_IF_PRESENT( SphereColliderComponent, "SphereColliderComponent" );
        MKT_SERIALIZE_COMPONENT_IF_PRESENT( CapsuleColliderComponent, "CapsuleColliderComponent" );
        MKT_SERIALIZE_COMPONENT_IF_PRESENT( MeshColliderComponent, "MeshColliderComponent" );

        SerializePhysicsComponents( *root, emitter );

        // I do not serialize the relationship component the hierarchy is stored explicitly by the nesting of the nodes in the YAML file.
        // So if an entity has children they will be nested under it in the YAML file and if it does not have children it will just be a leaf node.
        // Entity Unique ID is a runtime property that is established when it is created.
        emitter << YAML::Key << "Children" << YAML::Value << YAML::BeginSeq;

        for ( const auto &childID: root->GetComponent<RelationComponent>().GetChildren() ) {
            SerializeNode( emitter, scene.FindByID( childID ), scene, materialIDs, materials, materialFiles );
        }

        emitter << YAML::EndSeq;
        emitter << YAML::EndMap;
    }

    static auto OnSkyboxMaterialAdded( entt::registry &reg, entt::entity e ) -> void {
        MKT_BEGIN_PROFILER();
        auto &sbComponent{ reg.get<SkyboxMaterialComponent>( e ) };
        sbComponent.SetMaterial( asset::AssetRegistryService::Get()->CreateMaterial( material::SkyboxMaterialDescription{} ) );
    }

    static auto OnMeshRendererAdded( entt::registry &reg, entt::entity e ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !reg.any_of<MaterialComponent>( e ) ) {
            reg.emplace<MaterialComponent>( e );
        }

        MaterialComponent &materialComponent{ reg.get<MaterialComponent>( e ) };

        if ( materialComponent.HasMaterial() ) {
            return;
        }

        MeshComponent &meshComponent{ reg.get<MeshComponent>( e ) };

        if ( const asset::MeshNode *meshNode{ meshComponent.GetMesh() } ) {
            // Material is internal as it comes from the mesh itself
            // these kind of materials do not need to be serialized
            materialComponent.SetIsInternal( true );
            materialComponent.SetMaterial( asset::AssetRegistryService::Get()->CreateMaterial( meshNode->GetProperties() ) );
        }
        else {
            materialComponent.SetIsInternal( false );
            materialComponent.SetMaterial( asset::AssetRegistryService::Get()->CreateMaterial( material::SurfaceMaterialDescription{} ) );
        }
    }

    static auto OnAudioListenerAdded( entt::registry &reg, entt::entity e ) -> void {
        MKT_BEGIN_PROFILER();
        auto &transformComponent{ reg.get<TransformComponent>( e ) };
        auto &listenerComponent{ reg.get<AudioListenerComponent>( e ) };
        listenerComponent.SetListener( audio::AudioService::Get()->CreateListener( transformComponent.GetTranslation() ) );
    }

    static auto OnAudioListenerRemoved( entt::registry &reg, entt::entity e ) -> void {
        MKT_BEGIN_PROFILER();
        audio::AudioService::Get()->ReleaseListener( reg.get<AudioListenerComponent>( e ).GetListenerPtr() );
    }

    static auto OnAnimatorRemoved( entt::registry &reg, entt::entity entity ) -> void {
        MKT_BEGIN_PROFILER();
        // The owning component controls the animator lifetime, including scene
        // unload and failed Play deserialization, not only the Stop button.
        animation::AnimationSystem::Get()->UnregisterAnimation( reg.get<AnimatorComponent>( entity ).GetAnimatorID() );
    }

    static auto OnScriptRemoved( entt::registry &reg, entt::entity entity ) -> void {
        MKT_BEGIN_PROFILER();

        if ( auto script{ reg.get<ScriptComponent>( entity ).GetHandle() } ) {
            script->Stop();

            if ( auto *service{ scripting::ScriptingService::GetPtr() } ) {
                if ( auto *lua{ service->GetLuaScriptingService() } ) {
                    lua->ReleaseEntityScripts( script->GetEntity() );
                }
            }
        }
    }

    auto Scene::ConvertSceneAssetPaths( YAML::Node node, const filesystem::Path &sceneFile, bool relative ) -> void {
        MKT_BEGIN_PROFILER();
        const auto directory{ std::filesystem::absolute( sceneFile.GetPathTyped<std::filesystem::path>() ).parent_path() };

        // Paths are relative to the scene file, never the process working directory.
        // Moving a whole project therefore preserves its asset references.
        core::usize visited{};
        const auto visit = [&]( auto &&self, YAML::Node value, core::u32 depth ) -> void {
            // A saved 80^3 instancing demo contains over a million YAML values.
            // Keep the nesting guard while allowing the complete stress scene.
            if ( depth > 256 || ++visited > 16000000 ) {
                MKT_THROW_RUNTIME_ERROR( "Scene document is too deeply nested or too large." );
            }

            if ( value.IsSequence() ) {
                for ( auto child: value ) {
                    self( self, child, depth + 1 );
                }
            }
            else if ( value.IsMap() ) {
                for ( auto entry: value ) {
                    const auto key{ entry.first.as<std::string>() };
                    const eastl::string_view name{ key.c_str() };

                    if ( name == "MaterialPath" || name == "ModelPath" ||
                         name == "FontPath" || name == "ClipPath" || name == "ScriptPath" ||
                         name == "TexturePath" || name == "SkeletonPath" || name == "HeightmapPath" ) {
                        const auto text{ entry.second.as<std::string>( "" ) };

                        if ( text.empty() ) {
                            continue;
                        }

                        auto path{ std::filesystem::u8path( text ) };

                        if ( relative ) {
                            path = std::filesystem::absolute( path ).lexically_normal();
                            const auto local{ path.lexically_relative( directory ) };

                            if ( !local.empty() ) {
                                path = local;
                            }
                        }
                        else if ( path.is_relative() ) {
                            path = ( directory / path ).lexically_normal();
                        }

                        const auto utf8{ path.generic_u8string() };
                        value[key] = std::string{ reinterpret_cast<const char *>( utf8.data() ), utf8.size() };
                    }
                    else {
                        self( self, entry.second, depth + 1 );
                    }
                }
            }
        };

        visit( visit, node, 0 );
    }

    auto Scene::Serialize( const filesystem::Path &filename ) const -> void {
        MKT_BEGIN_PROFILER();
        filesystem::FileHandle file{ filesystem::FileService::Get()->CreateNewFile( filename ) };

        if ( file.IsEmpty() ) {
            MKT_THROW_RUNTIME_ERROR( "Cannot open scene file for writing." );
        }

        file->Write( SerializeText( filename ) );
    }

    auto Scene::SerializeText( const filesystem::Path &filename, bool copyMaterials ) const -> eastl::string {
        MKT_BEGIN_PROFILER();
        YAML::Emitter emitter{};
        SceneMaterialIDs materialIDs{};
        SceneMaterialFiles materialFiles{ filename, copyMaterials };
        YAML::Node materials{ YAML::NodeType::Sequence };

        emitter << YAML::BeginMap;
        emitter << YAML::Key << "Version" << YAML::Value << 5;

        emitter << YAML::Key << "Scene" << YAML::Value << GetName().data();
        emitter << YAML::Key << "EnvironmentLightingSource" << YAML::Value << static_cast<core::i32>( mEnvironmentLightingSource );
        emitter << YAML::Key << "Atmosphere" << YAML::Value;
        SerializeAtmosphere( emitter, mAtmosphereSettings );
        emitter << YAML::Key << "ToneMapping" << YAML::Value << static_cast<core::i32>( mToneMapping );
        emitter << YAML::Key << "Objects" << YAML::Value << YAML::BeginSeq;

        for ( const auto &root: mRootEntities ) {
            SerializeNode( emitter, root, *this, materialIDs, materials, materialFiles );
        }

        emitter << YAML::EndSeq;
        emitter << YAML::Key << "Materials" << YAML::Value << materials;
        emitter << YAML::EndMap;

        auto document{ YAML::Load( emitter.c_str() ) };
        ConvertSceneAssetPaths( document, filename, true );
        document["RelativeAssetPaths"] = true;
        YAML::Emitter output;
        output << document;
        return eastl::string{ output.c_str() };
    }

    auto Scene::Serialize( filesystem::FileHandle file ) const -> void {
        MKT_BEGIN_PROFILER();

        if ( file.IsEmpty() ) {
            ( "No scene file selected." );
        }

        auto contents{ SerializeText( file->GetPath() ) };
        file->Write( contents.data(), contents.size() );
    }

    auto Scene::Deserialize( filesystem::FileHandle file ) -> void {
        MKT_BEGIN_PROFILER();

        if ( file.IsEmpty() ) {
            return;
        }

        auto &scene{ *this };
        struct Record {
            YAML::Node mPhysicsNode{};
            core::u64 mFileID{};
            eastl::vector<core::u64> mGroupMembers{};
            core::i32 mParent{ -1 };
            EntityType mType{ EntityType::eEmpty };
            eastl::string mName;
            bool mActive{ true };
            TransformComponent mTransform{};
            asset::ModelHandle mModel{};
            core::i32 mMeshIndex{};
            filesystem::Path mStreamingModelPath{};
            core::usize mNextStreamedMesh{};

            eastl::optional<TextComponent> mText{};
            eastl::optional<CanvasComponent> mCanvas{};
            eastl::optional<LightComponent> mLight{};
            eastl::optional<SkyLightComponent> mSkyLight{};
            eastl::optional<BoxColliderComponent> mBox{};
            eastl::optional<AudioSourceComponent> mAudio{};
            eastl::optional<VideoComponent> mVideo{};
            eastl::optional<TextureRenderComponent> mTextureRender{};
            core::u64 mTextureRenderCameraEntityID{};
            eastl::optional<AudioMixerComponent> mAudioMixer{};
            eastl::optional<AudioMixerGroupComponent> mAudioGroup{};
            eastl::optional<MeshColliderComponent> mMesh{};
            eastl::optional<VehicleComponent> mVehicle{};
            eastl::optional<VehicleWheelComponent> mVehicleWheel{};
            eastl::optional<BridgeSegmentComponent> mBridgeSegment{};
            eastl::optional<RigidBodyComponent> mRigidBody{};
            eastl::optional<SphereColliderComponent> mSphere{};
            eastl::optional<AudioListenerComponent> mListener{};
            asset::ParticleEffectHandle mParticleEffect{};
            eastl::optional<CloudComponent> mCloud{};
            eastl::optional<CloudInteractionComponent> mCloudInteraction{};
            eastl::optional<FogComponent> mFog{};
            eastl::optional<WaterRenderComponent> mWaterRender{};
            eastl::optional<WaterSimulationComponent> mWaterSimulation{};
            eastl::optional<LandscapeComponent> mLandscape{};
            eastl::optional<CapsuleColliderComponent> mCapsule{};
            eastl::optional<DepthOfFieldSettings> mCameraDepthOfField{};
            eastl::optional<VignetteSettings> mCameraVignette{};
            FilmGrainSettings mCameraFilmGrain{};

            filesystem::Path mScriptPath{};
            bool mHasScript{};
            scripting::ScriptLanguage mScriptLanguage{ scripting::ScriptLanguage::eLua };
            eastl::string mScriptClass{};
            eastl::string mScriptProperties{ "{}" };

            YAML::Node mCameraNode{};
            material::MaterialHandle mMaterial{};
            material::MaterialHandle mPostProcessMaterial{};
            core::u64 mPostProcessCameraEntityID{};
            YAML::Node mAnimatorNode{};

            asset::ModelHandle mAnimatorModel{};
            material::MaterialHandle mSkybox{};

            core::u64 mSkinAnimatorID{};
            eastl::optional<animation::AnimationController> mController{};
            eastl::optional<animation::KeyframeClip> mKeyframeClip{};
        };

        try {
            auto data = YAML::Load( file->GetContentsString().c_str() );

            if ( data["RelativeAssetPaths"].as<bool>( false ) ) {
                ConvertSceneAssetPaths( data, file->GetPath(), false );
            }

            if ( !data.IsMap() || !data["Scene"] || !data["Objects"].IsSequence() ) {
                MKT_THROW_RUNTIME_ERROR( "Scene requires Scene and Objects fields." );
            }

            const int version{ data["Version"].as<int>( 1 ) };

            if ( version < 1 || version > 5 ) {
                MKT_THROW_RUNTIME_ERROR( "Unsupported scene version." );
            }

            SceneMaterialFiles materialFiles{ file->GetPath() };
            eastl::vector<material::MaterialHandle> materials{};

            if ( version >= 2 ) {
                const auto table{ data["Materials"] };

                if ( !table.IsSequence() ) {
                    MKT_THROW_RUNTIME_ERROR( "Scene requires a material table." );
                }

                materials.reserve( table.size() );

                for ( const auto &entry: table ) {
                    if ( !entry.IsMap() ) {
                        MKT_THROW_RUNTIME_ERROR( "Invalid scene material definition." );
                    }

                    auto source{ version >= 3
                                     ? materialFiles.Load( entry )
                                     : materialFiles.ImportLegacy( entry, "SurfaceMaterial" ) };

                    if ( !dynamic_cast<material::SurfaceMaterial *>( source.GetPtr() ) ) {
                        MKT_THROW_RUNTIME_ERROR( "Mesh component requires a surface material." );
                    }

                    materials.push_back( source );
                }
            }

            const auto name = data["Scene"].as<std::string>();
            const core::i32 environmentSource{
                data["EnvironmentLightingSource"] ? data["EnvironmentLightingSource"].as<core::i32>() : 0
            };
            const auto atmosphereSettings{ DeserializeAtmosphere( data["Atmosphere"] ) };
            // Older scene files keep the established ACES default. Validate
            // before Clear(), so malformed settings cannot discard this scene.
            const core::i32 toneMapping{ data["ToneMapping"] ? data["ToneMapping"].as<core::i32>() : 0 };

            if ( toneMapping < 0 || toneMapping >= static_cast<core::i32>( renderer::ToneMapper::eCount ) ) {
                MKT_THROW_RUNTIME_ERROR( "Invalid scene tone-mapping operator" );
            }

            if ( environmentSource < 0 || environmentSource > static_cast<core::i32>(
                                                                  EnvironmentLightingSource::eAtmosphere ) ) {
                MKT_THROW_RUNTIME_ERROR( "Invalid environment lighting source." );
            }

            eastl::vector<Record> records{};
            auto parse = [&]( auto &&self, const YAML::Node &object, int parent, int depth ) -> void {
                if ( depth == 129 || records.size() == 100000 ) {
                    MKT_CORE_LOGGER_WARN( "Scene hierarchy is too large." );
                }

                Record record{
                    .mParent = parent
                };
                record.mPhysicsNode = object;

                if ( version >= 5 ) {
                    record.mFileID = object["ID"].as<core::u64>();

                    if ( record.mFileID == 0 ) {
                        MKT_THROW_RUNTIME_ERROR( "Entity ID must be nonzero." );
                    }
                }

                const auto tag{ object["TagComponent"] };
                record.mName = tag["Name"].as<std::string>().c_str();

                if ( tag["Visibility"] ) {
                    record.mActive = tag["Visibility"].as<bool>();
                }

                if ( object["Type"] ) {
                    // Preserve the numeric IDs used by existing scene files.
                    const auto type{ object["Type"].as<int>() };

                    if ( type < 0 || type >= static_cast<int>( EntityType::eCount ) ) {
                        MKT_THROW_RUNTIME_ERROR(
                            string::Format( "Invalid entity type {} for '{}'.", type, record.mName ) );
                    }

                    record.mType = core::as<EntityType>( type );
                }

                if ( const auto group{ object["GroupComponent"] } ) {
                    if ( record.mType != EntityType::eGrouping || !group["Members"].IsSequence() || version < 5 ) {
                        MKT_THROW_RUNTIME_ERROR( "Invalid group component." );
                    }

                    for ( const auto &member: group["Members"] ) {
                        record.mGroupMembers.push_back( member.as<core::u64>() );
                    }
                }

                if ( const auto transform = object["TransformComponent"] ) {
                    if ( const auto matrix{ transform["Local Transform"] } ) {
                        const auto local{ matrix.as<core::float4x4>() };

                        if ( local[0][3] != 0.0f || local[1][3] != 0.0f || local[2][3] != 0.0f || local[3][3] != 1.0f ) {
                            MKT_THROW_RUNTIME_ERROR( "Local transforms must be affine." );
                        }

                        record.mTransform.SetTransform( local );
                    }
                    else {
                        // Legacy files store editable fields only. Do not guess their
                        // units or use the possibly stale world-transform cache.
                        record.mTransform.SetTranslation( ReadPhysicsVector( transform["Position"] ) );
                        record.mTransform.SetRotation( ReadPhysicsVector( transform["Rotation"] ) );
                        record.mTransform.SetScale( ReadPhysicsVector( transform["Scale"] ) );
                    }

                    if ( transform["Uniform Scale"] ) {
                        record.mTransform.SetUniformSale( transform["Uniform Scale"].as<bool>() );
                    }
                }

                if ( const auto pending{ object["StreamedModelComponent"] } ) {
                    record.mStreamingModelPath = filesystem::Path{ pending["ModelPath"].as<std::string>().c_str() };
                    record.mNextStreamedMesh = pending["NextMesh"].as<core::usize>( 0 );
                }

                if ( const auto mesh = object["MeshComponent"] ) {
                    const auto path = mesh["ModelPath"].as<std::string>();
                    record.mMeshIndex = mesh["MeshIndex"].as<int>();

                    if ( !path.empty() ) {
                        record.mModel = asset::AssetRegistryService::Get()->LoadAsset<asset::Model>( filesystem::Path{
                            path.c_str() } );

                        if ( record.mModel.IsEmpty() || record.mMeshIndex < 0 || static_cast<core::usize>( record.mMeshIndex ) >= record.mModel->GetMeshNodeCount() ) {
                            MKT_THROW_RUNTIME_ERROR( "Scene mesh could not be loaded." );
                        }
                    }
                }

                // A missing key on a const YAML node is an invalid handle.
                // Assigning that handle throws; entities without optional
                // material/animator components must retain the empty defaults.
                if ( const auto materialNode{ object["MaterialComponent"] } ) {
                    if ( version >= 2 ) {
                        const auto reference{ materialNode["Reference"].as<core::u32>() };

                        if ( reference >= materials.size() ) {
                            MKT_THROW_RUNTIME_ERROR( "Scene references a missing material." );
                        }

                        record.mMaterial = materials[reference];
                    }
                    else if ( materialNode.size() ) {
                        // Legacy inline materials are independent: matching
                        // values alone do not establish shared authoring intent.
                        record.mMaterial = materialFiles.ImportLegacy( materialNode, "SurfaceMaterial" );
                    }
                }

                if ( const auto interaction{ object["CloudInteractionComponent"] } ) {
                    auto &result{ record.mCloudInteraction.emplace() };
                    result.SetEnabled( interaction["Enabled"].as<bool>( true ) );
                    result.SetShape( core::as<renderer::CloudInteractionShape>( interaction["Shape"].as<core::u32>( 0 ) ) );
                    if ( interaction["Dimensions"] ) {
                        result.SetDimensions( interaction["Dimensions"].as<core::float3>() );
                    }
                    result.SetStrength( interaction["Strength"].as<float>( 1 ) );
                    result.SetFeather( interaction["Feather"].as<float>( .2f ) );
                }
                if ( const auto cloud{ object["CloudComponent"] } ) {
                    record.mCloud = ReadVolumeComponent<CloudComponent>( cloud, materialFiles );
                }
                if ( const auto fog{ object["FogComponent"] } ) {
                    record.mFog = ReadVolumeComponent<FogComponent>( fog, materialFiles );
                }

                if ( const auto postProcess{ object["PostProcessMaterialComponent"] } ) {
                    record.mPostProcessMaterial = materialFiles.Load( postProcess );
                    record.mPostProcessCameraEntityID = postProcess["CameraEntityID"].as<core::u64>( 0 );

                    if ( !dynamic_cast<material::PostProcessMaterial *>( record.mPostProcessMaterial.GetPtr() ) ) {
                        MKT_THROW_RUNTIME_ERROR( "Post-process component requires a post-process material." );
                    }
                }

                if ( const auto properties{ object["SkyLightComponent"] } ) {
                    auto &sky{ record.mSkyLight.emplace() };
                    sky.SetEnabled( properties["Enabled"].as<bool>( true ) );
                    const auto source{ properties["Source"].as<core::u32>( 0 ) };
                    if ( source > core::as<core::u32>( SkyLightSource::eEnvironmentMap ) ) {
                        MKT_THROW_RUNTIME_ERROR( "Invalid skylight source." );
                    }
                    sky.SetSource( core::as<SkyLightSource>( source ) );
                    sky.SetIntensity( properties["Intensity"].as<float>( 1.0f ) );
                    sky.SetOcclusionEnabled( properties["OcclusionEnabled"].as<bool>( true ) );
                    sky.SetOcclusionDistance( properties["OcclusionDistance"].as<float>( 50.0f ) );
                    sky.SetOcclusionSamplesPerFrame( properties["OcclusionSamplesPerFrame"].as<core::u32>( 2 ) );
                    if ( properties["MaterialPath"] ) {
                        sky.SetEnvironmentMaterial( materialFiles.Load( properties ) );
                    }
                }

                if ( const auto skybox{ object["SkyboxMaterialComponent"] }; skybox && skybox.size() ) {
                    if ( version >= 3 ) {
                        record.mSkybox = materialFiles.Load( skybox );
                    }
                    else {
                        auto properties{ YAML::Clone( skybox ) };
                        properties["SkyboxType"] = skybox["Type"].as<core::i32>( 1 );
                        record.mSkybox = materialFiles.ImportLegacy( properties, "SkyboxMaterial" );
                    }

                    if ( !dynamic_cast<material::SkyboxMaterial *>( record.mSkybox.GetPtr() ) ) {
                        MKT_THROW_RUNTIME_ERROR( "Skybox component requires a skybox material." );
                    }
                }

                if ( const auto animatorNode{ object["AnimatorComponent"] } ) {
                    record.mAnimatorNode = animatorNode;
                }

                if ( const auto node{ record.mAnimatorNode }; node && node["ModelPath"] ) {
                    record.mAnimatorModel = asset::AssetRegistryService::Get()->LoadAsset<asset::Model>( filesystem::Path{
                        node["ModelPath"].as<std::string>().c_str() } );

                    if ( record.mAnimatorModel.IsEmpty() ) {
                        MKT_THROW_RUNTIME_ERROR( "Animator model could not be loaded." );
                    }

                    if ( node["Controller"] ) {
                        eastl::string error;
                        auto &controller{ record.mController.emplace() };

                        if ( !controller.DecodeController( nlohmann::json::parse( node["Controller"].as<std::string>() ), error ) ) {
                            MKT_THROW_RUNTIME_ERROR( error.c_str() );
                        }
                    }
                }

                if ( const auto node{ object["SkinnedMeshRenderer"] } ) {
                    record.mSkinAnimatorID = node["AnimatorID"].as<core::u64>();
                }

                if ( const auto cameraNode{ object["CameraComponent"] } ) {
                    record.mCameraNode = cameraNode;
                    const core::f32 nearPlane{ cameraNode["Near"].as<core::f32>( 0.1f ) };
                    const core::f32 farPlane{ cameraNode["Far"].as<core::f32>( 1000 ) };

                    if ( nearPlane <= 0 || farPlane <= nearPlane ) {
                        MKT_THROW_RUNTIME_ERROR( "Invalid camera depth range." );
                    }

                    auto &settings{ record.mCameraDepthOfField.emplace() };

                    if ( const auto grain{ cameraNode["FilmGrain"] } ) {
                        record.mCameraFilmGrain.mAmount = grain["Amount"].as<core::f32>( record.mCameraFilmGrain.mAmount );
                    }

                    if ( const auto depthOfField{ cameraNode["DepthOfField"] } ) {
                        settings.mEnabled = depthOfField["Enabled"].as<bool>( settings.mEnabled );
                        settings.mAperture = depthOfField["Aperture"].as<core::f32>( settings.mAperture );
                        settings.mFocalLength = depthOfField["FocalLength"].as<core::f32>( settings.mFocalLength );
                        settings.mFocusDistance = depthOfField["FocusDistance"].as<core::f32>( settings.mFocusDistance );
                        settings.mMaxBlur = depthOfField["MaxBlur"].as<core::f32>( settings.mMaxBlur );
                        settings.mMaxAperture = depthOfField["MaxAperture"].as<core::f32>(
                            glm::min( settings.mMaxAperture, settings.mAperture ) );
                        settings.mSensorWidth = depthOfField["SensorWidth"].as<core::f32>( settings.mSensorWidth );
                        settings.mUsePhysicalLens = depthOfField["UsePhysicalLens"].as<bool>( settings.mUsePhysicalLens );
                        settings.mBladeCount = depthOfField["BladeCount"].as<core::u32>( settings.mBladeCount );
                        settings.mBladeCurvature = depthOfField["BladeCurvature"].as<core::f32>(
                            settings.mBladeCurvature );
                        settings.mBladeRotation = depthOfField["BladeRotation"].as<core::f32>( settings.mBladeRotation );
                    }

                    auto &vignetteSettings{ record.mCameraVignette.emplace() };

                    if ( const auto vignette{ cameraNode["Vignette"] } ) {
                        vignetteSettings.mEnabled = vignette["Enabled"].as<bool>( vignetteSettings.mEnabled );
                        vignetteSettings.mShape = core::as<VignetteShape>( vignette["Shape"].as<core::u32>( 0 ) );
                        const auto readPair{ []( const YAML::Node &node, core::float2 fallback ) -> core::float2 {
                            if ( !node ) {
                                return fallback;
                            }

                            if ( !node.IsSequence() || node.size() != 2 ) {
                                MKT_THROW_RUNTIME_ERROR( "Expected a two-component vignette parameter." );
                            }

                            return { node[0].as<core::f32>(), node[1].as<core::f32>() };
                        } };
                        vignetteSettings.mCenter = readPair( vignette["Center"], vignetteSettings.mCenter );
                        vignetteSettings.mHalfSize = readPair( vignette["HalfSize"], vignetteSettings.mHalfSize );
                        vignetteSettings.mFeather = vignette["Feather"].as<core::f32>( vignetteSettings.mFeather );
                        vignetteSettings.mStrength = vignette["Strength"].as<core::f32>( vignetteSettings.mStrength );

                        if ( const auto color{ vignette["Color"] } ) {
                            vignetteSettings.mColor = ReadPhysicsVector( color );
                        }

                        vignetteSettings.mAspectCorrect = vignette["AspectCorrect"].as<bool>( vignetteSettings.mAspectCorrect );
                    }
                }

                if ( const auto node = object["VehicleComponent"] ) {
                    VehicleComponent vehicle{};
                    vehicle.mHalfTrack = node["HalfTrack"].as<core::f32>( vehicle.mHalfTrack );
                    vehicle.mHalfWheelbase = node["HalfWheelbase"].as<core::f32>( vehicle.mHalfWheelbase );
                    vehicle.mWheelRadius = node["WheelRadius"].as<core::f32>( vehicle.mWheelRadius );
                    vehicle.mWheelWidth = node["WheelWidth"].as<core::f32>( vehicle.mWheelWidth );
                    vehicle.mSuspensionMin = node["SuspensionMin"].as<core::f32>( vehicle.mSuspensionMin );
                    vehicle.mSuspensionMax = node["SuspensionMax"].as<core::f32>( vehicle.mSuspensionMax );
                    vehicle.mMaxSteerDegrees = node["MaxSteerDegrees"].as<core::f32>( vehicle.mMaxSteerDegrees );
                    vehicle.mEngineTorque = node["EngineTorque"].as<core::f32>( vehicle.mEngineTorque );
                    vehicle.mWheelAnchorHeight = node["WheelAnchorHeight"].as<core::f32>( vehicle.mWheelAnchorHeight );
                    vehicle.mCenterOfMassOffsetY = node["CenterOfMassOffsetY"].as<core::f32>(
                        vehicle.mCenterOfMassOffsetY );
                    vehicle.mSuspensionFrequency = node["SuspensionFrequency"].as<core::f32>(
                        vehicle.mSuspensionFrequency );
                    vehicle.mSuspensionDamping = node["SuspensionDamping"].as<core::f32>( vehicle.mSuspensionDamping );
                    vehicle.mMaxPitchRollDegrees = node["MaxPitchRollDegrees"].as<core::f32>(
                        vehicle.mMaxPitchRollDegrees );
                    vehicle.mLongitudinalImpulseScale = node["LongitudinalImpulseScale"].as<core::f32>(
                        vehicle.mLongitudinalImpulseScale );
                    vehicle.mFourWheelDrive = node["FourWheelDrive"].as<bool>( vehicle.mFourWheelDrive );
                    vehicle.mCylinderWheelCast = node["CylinderWheelCast"].as<bool>( vehicle.mCylinderWheelCast );
                    DeserializeVehicleDetails( vehicle, node );

                    if ( !vehicle.IsValid() ) {
                        MKT_THROW_RUNTIME_ERROR( "Invalid vehicle settings." );
                    }

                    record.mVehicle = vehicle;
                }

                if ( const auto node = object["BridgeSegmentComponent"] ) {
                    record.mBridgeSegment = BridgeSegmentComponent{ node["Index"].as<core::u32>() };
                }

                if ( const auto node = object["VehicleWheelComponent"] ) {
                    VehicleWheelComponent wheel{};
                    wheel.mIndex = node["Index"].as<core::u32>();
                    wheel.mFollowSpin = node["FollowSpin"].as<bool>( true );
                    const auto matrix{ node["VisualCorrection"] };

                    if ( !matrix.IsSequence() || matrix.size() != 16 ) {
                        MKT_THROW_RUNTIME_ERROR( "Invalid vehicle wheel binding." );
                    }

                    for ( core::u32 column{}; column < 4; ++column ) {
                        for ( core::u32 row{}; row < 4; ++row ) {
                            const core::f32 value{ matrix[column * 4 + row].as<core::f32>() };
                            wheel.mVisualCorrection[column][row] = value;
                        }
                    }

                    record.mVehicleWheel = wheel;
                }

                if ( const auto node = object["RigidBodyComponent"] ) {
                    record.mRigidBody = DeserializeRigidBody( node );
                }

                if ( const auto node = object["BoxColliderComponent"] ) {
                    record.mBox = DeserializeCollider<BoxColliderComponent>( node );
                }

                if ( const auto node = object["SphereColliderComponent"] ) {
                    record.mSphere = DeserializeCollider<SphereColliderComponent>( node );
                }

                if ( const auto node = object["CapsuleColliderComponent"] ) {
                    record.mCapsule = DeserializeCollider<CapsuleColliderComponent>( node );
                }

                if ( const auto node = object["MeshColliderComponent"] ) {
                    record.mMesh = DeserializeCollider<MeshColliderComponent>( node );
                }

                if ( int( record.mBox.has_value() ) + int( record.mSphere.has_value() ) + int( record.mCapsule.has_value() ) +
                         int( record.mMesh.has_value() ) >
                     1 ) {
                    MKT_THROW_RUNTIME_ERROR( "Each entity can have only one collider." );
                }

                if ( const auto text = object["TextComponent"] ) {
                    record.mText.emplace( text["Contents"].as<std::string>().c_str(), text["Size"].as<float>(),
                        text["Spacing"].as<float>(), text["IsWorldText"].as<bool>() );

                    if ( text["WorldSize"] ) {
                        record.mText->SetSize( text["WorldSize"].as<float>(), true );
                    }

                    if ( text["ScreenSize"] ) {
                        record.mText->SetSize( text["ScreenSize"].as<float>(), false );
                    }

                    if ( text["WorldSpacing"] ) {
                        record.mText->SetSpacing( text["WorldSpacing"].as<float>(), true );
                    }

                    if ( text["ScreenSpacing"] ) {
                        record.mText->SetSpacing( text["ScreenSpacing"].as<float>(), false );
                    }

                    auto &screenPlacement = record.mText->GetScreenPlacement();

                    if ( text["ScreenPosition"] ) {
                        screenPlacement.mPosition = mikoto::core::float2{ ReadPhysicsVector( text["ScreenPosition"] ) };

                        if ( text["ScreenScale"] ) {
                            screenPlacement.mScale = mikoto::core::float2{ ReadPhysicsVector( text["ScreenScale"] ) };
                        }

                        if ( text["ScreenRotation"] ) {
                            screenPlacement.mRotation = ReadPhysicsScalar( text["ScreenRotation"], -FLT_MAX, FLT_MAX );
                        }
                    }
                    else if ( !record.mText->IsWorldText() ) {
                        // Older screen text used the entity's accumulated transform.
                        // Migrate its 2D placement once; world Z no longer clips it.
                        auto model = record.mTransform.GetTransform();

                        for ( int ancestor = parent; ancestor >= 0; ancestor = records[ancestor].mParent ) {
                            model = records[ancestor].mTransform.GetTransform() * model;
                        }

                        text::SetScreenTextModel( screenPlacement, model );
                    }

                    if ( text["Color"] ) {
                        record.mText->SetColor( text["Color"].as<mikoto::core::float4>() );
                    }

                    if ( text["MaterialPath"] || text["Material"] ) {
                        auto loaded{ text["MaterialPath"] ? materialFiles.Load( text ) : materialFiles.ImportLegacy( text["Material"], "TextMaterial" ) };

                        if ( !dynamic_cast<material::TextMaterial *>( loaded.GetPtr() ) ) {
                            MKT_THROW_RUNTIME_ERROR( "Text component requires a text material." );
                        }

                        record.mText->SetMaterial( loaded.As<material::TextMaterial>() );
                    }

                    // Older scenes retain the default disabled outline.
                    if ( text["OutlineWidth"] ) {
                        record.mText->SetOutlineWidth( text["OutlineWidth"].as<float>() );
                    }

                    if ( text["OutlineColor"] ) {
                        record.mText->SetOutlineColor( text["OutlineColor"].as<mikoto::core::float4>() );
                    }

                    if ( text["LineSpacing"] ) {
                        record.mText->SetLineSpacing( text["LineSpacing"].as<float>() );
                    }

                    if ( text["Alignment"] ) {
                        const int alignment = text["Alignment"].as<int>();

                        if ( alignment < 0 || alignment > 2 ) {
                            MKT_THROW_RUNTIME_ERROR( "Invalid text alignment." );
                        }

                        record.mText->SetAlignment( static_cast<TextAlignment>( alignment ) );
                    }

                    if ( text["FontPath"] ) {
                        const auto path = text["FontPath"].as<std::string>();

                        if ( !path.empty() ) {
                            record.mText->SetFont(
                                asset::AssetRegistryService::Get()->LoadAsset<renderer::Font>( filesystem::Path{ path.c_str() } ) );
                        }
                    }

                    if ( text["Backend"] ) {
                        const int backend = text["Backend"].as<int>();

                        if ( backend < 0 || backend > 1 ) {
                            MKT_THROW_RUNTIME_ERROR( "Invalid text backend." );
                        }

                        record.mText->SetBackend( static_cast<TextBackend>( backend ) );
                    }
                }

                if ( const auto light = object["LightComponent"] ) {
                    record.mLight = DeserializeLight( light );
                }

                if ( const auto canvas = object["CanvasComponent"] ) {
                    record.mCanvas.emplace();
                    record.mCanvas->GetCanvas() = DeserializeCanvas( canvas );
                }

                if ( const auto video{ object["VideoComponent"] } ) {
                    auto &component{ record.mVideo.emplace() };
                    component.SetClip( asset::AssetRef<asset::VideoClip>{ filesystem::Path{ video["ClipPath"].as<std::string>( "" ).c_str() } } );
                    video::PlaybackSettings settings{};
                    settings.mPlayOnStart = video["PlayOnStart"].as<bool>( true );
                    settings.mLooping = video["Looping"].as<bool>( false );
                    settings.mAudioEnabled = video["AudioEnabled"].as<bool>( true );

                    if ( video["Audio"] ) {
                        settings.mAudio = DeserializeAudioSettings( video["Audio"] );
                    }

                    const auto output{ video["OutputMap"].as<core::u32>( 0 ) };

                    if ( output > static_cast<core::u32>( renderer::VideoOutputMap::eEmissive ) ) {
                        MKT_THROW_RUNTIME_ERROR( "Invalid video material map." );
                    }

                    component.SetPlaybackSettings( settings );
                    component.SetOutputMap( static_cast<renderer::VideoOutputMap>( output ) );
                }

                if ( const auto capture{ object["TextureRenderComponent"] } ) {
                    const auto resolution{ capture["Resolution"].as<core::u32>( core::as<core::u32>( rhi::RenderResolution::e1080P ) ) };
                    const auto rate{ capture["RenderRate"].as<core::u32>( 0 ) };
                    const auto path{ capture["RenderPath"].as<core::u32>( 0 ) };
                    const auto output{ capture["OutputMap"].as<core::u32>( 0 ) };

                    if ( resolution > core::as<core::u32>( rhi::RenderResolution::e3120P ) ||
                         rate > core::as<core::u32>( TextureRenderRate::e120FPS ) ||
                         path > core::as<core::u32>( TextureRenderPath::ePathTracing ) ||
                         output > core::as<core::u32>( TextureRenderOutput::eEmissive ) ) {
                        MKT_THROW_RUNTIME_ERROR( "Invalid texture render settings." );
                    }

                    auto &component{ record.mTextureRender.emplace() };
                    component.SetResolution( core::as<rhi::RenderResolution>( resolution ) );
                    component.SetRenderRate( core::as<TextureRenderRate>( rate ) );
                    component.SetRenderPath( core::as<TextureRenderPath>( path ) );
                    component.SetRenderOutput( core::as<TextureRenderOutput>( output ) );
                    record.mTextureRenderCameraEntityID = capture["CameraEntityID"].as<core::u64>( 0 );
                }

                if ( const auto audio = object["AudioSourceComponent"] ) {
                    const auto settings = DeserializeAudioSettings( audio );
                    const auto path = audio["ClipPath"] ? audio["ClipPath"].as<std::string>() : std::string{};
                    record.mAudio.emplace( filesystem::Path{ path.c_str() } );
                    record.mAudio->SetSettings( settings );
                    record.mAudio->mOutputID = audio["Output"].as<core::u64>( 0 );
                }

                if ( const auto mixer{ object["AudioMixerComponent"] } ) {
                    auto &component{ record.mAudioMixer.emplace() };
                    component.SetVolume( audio::AudioValue( mixer["Volume"].as<float>( 1 ), 0, 20 ) );
                    component.SetMute( mixer["Muted"].as<bool>( false ) );
                }

                if ( const auto group{ object["AudioMixerGroupComponent"] } ) {
                    auto &component{ record.mAudioGroup.emplace() };
                    component.mMixerID = group["Mixer"].as<core::u64>( 0 );
                    component.mOutputID = group["Output"].as<core::u64>( 0 );
                    component.mVolume = audio::AudioValue( group["Volume"].as<float>( 1 ), 0, 20 );
                    component.mMuted = group["Muted"].as<bool>( false );
                }

                if ( const auto listener = object["AudioListenerComponent"] ) {
                    record.mListener.emplace();

                    if ( listener["Enabled"] ) {
                        record.mListener->SetEnabled( listener["Enabled"].as<bool>() );
                    }
                }

                if ( const auto effect{ object["ParticleSimulatorComponent"] } ) {
                    record.mParticleEffect = asset::ParticleEffect::Deserialize( effect );
                }
                else if ( const auto legacyEmitter{ object["ParticleEmitterComponent"] } ) {
                    record.mParticleEffect = asset::ParticleEffect::Deserialize( legacyEmitter );
                }

                if ( const auto node{ object["LandscapeComponent"] } ) {
                    renderer::WorldDescription world{};

                    if ( const auto origin{ node["Origin"] } ) {
                        world.mOrigin = { origin[0].as<float>(), origin[1].as<float>() };
                    }

                    world.mQuadsPerSection = node["QuadsPerSection"].as<core::u32>( world.mQuadsPerSection );
                    world.mSectionsPerComponent = node["SectionsPerComponent"].as<core::u32>( world.mSectionsPerComponent );
                    world.mComponentsX = node["ComponentsX"].as<core::u32>( world.mComponentsX );
                    world.mComponentsY = node["ComponentsY"].as<core::u32>( world.mComponentsY );
                    world.mVertexSpacing = node["VertexSpacing"].as<float>( world.mVertexSpacing );
                    world.mHeightScale = node["HeightScale"].as<float>( world.mHeightScale );
                    world.mBaseElevation = node["BaseElevation"].as<float>( world.mBaseElevation );
                    world.mLowColor = ReadLandscapeColor( node, "LowColor", world.mLowColor );
                    world.mMiddleColor = ReadLandscapeColor( node, "MiddleColor", world.mMiddleColor );
                    world.mHighColor = ReadLandscapeColor( node, "HighColor", world.mHighColor );
                    world.mEnableEditLayers = node["EnableEditLayers"].as<bool>( false );

                    if ( !renderer::IsTerrainDescriptionValid( world ) ) {
                        MKT_THROW_RUNTIME_ERROR( "Invalid terrain dimensions or grid exceeds 4,194,304 vertices." );
                    }

                    auto &landscape{ record.mLandscape.emplace() };
                    landscape.SetComponentCount( 1, 1 );
                    landscape.SetTerrainSection( world.mQuadsPerSection );
                    landscape.SetSectionsPerComponent( world.mSectionsPerComponent );
                    landscape.SetComponentCount( world.mComponentsX, world.mComponentsY );
                    landscape.SetOrigin( world.mOrigin );
                    landscape.SetVertexSpacing( world.mVertexSpacing );
                    landscape.SetHeightScale( world.mHeightScale );
                    landscape.SetBaseElevation( world.mBaseElevation );
                    landscape.SetLowColor( world.mLowColor );
                    landscape.SetMiddleColor( world.mMiddleColor );
                    landscape.SetHighColor( world.mHighColor );
                    landscape.SetEnableEditLayers( world.mEnableEditLayers );
                    landscape.SetShowGrid( node["ShowGrid"].as<bool>( false ) );
                    landscape.SetShowVertices( node["ShowVertices"].as<bool>( false ) );

                    if ( const auto generated{ node["GeneratedNoise"] } ) {
                        renderer::TerrainNoiseSettings noise{};
                        noise.mFBM.mFrequency = generated["Frequency"].as<float>( noise.mFBM.mFrequency );
                        noise.mFBM.mAmplitude = generated["Amplitude"].as<float>( noise.mFBM.mAmplitude );
                        noise.mFBM.mPersistence = generated["Persistence"].as<float>( noise.mFBM.mPersistence );
                        noise.mFBM.mLacunarity = generated["Lacunarity"].as<float>( noise.mFBM.mLacunarity );
                        noise.mFBM.mOctaves = generated["Octaves"].as<core::u32>( noise.mFBM.mOctaves );
                        noise.mPerlinFrequency = generated["PerlinFrequency"].as<float>( noise.mPerlinFrequency );
                        noise.mPerlinAmplitude = generated["PerlinAmplitude"].as<float>( noise.mPerlinAmplitude );
                        noise.mPerlinBlend = generated["PerlinBlend"].as<float>( noise.mPerlinBlend );

                        if ( const auto offset{ generated["Offset"] }; offset && offset.IsSequence() && offset.size() == 2 ) {
                            noise.mOffset = { offset[0].as<float>(), offset[1].as<float>() };
                        }

                        if ( !noise.IsValid() ) {
                            MKT_THROW_RUNTIME_ERROR( "Invalid terrain noise parameters." );
                        }

                        landscape.SetGeneratedNoise( noise );
                    }
                    else if ( const auto path{ node["TexturePath"].as<std::string>( "" ) }; !path.empty() ) {
                        // Keep an unavailable source editable instead of preventing scene loading.
                        landscape.SetHeightmap( asset::AssetRef<asset::Texture>{ filesystem::Path{ path.c_str() } } );
                    }
                    if ( const auto source{ node["Surface"] }; source && source.IsMap() ) {
                        auto surface{ landscape.GetSurfaceSettings() };
                        surface.mBlendWidth = glm::clamp( source["BlendWidth"].as<float>( .1f ), .001f, 1.0f );
                        surface.mHeightBlendWidth = glm::clamp( source["HeightBlendWidth"].as<float>( .2f ), .001f, 1.0f );
                        surface.mHeightBlendEnabled = source["HeightBlend"].as<bool>( true );
                        surface.mShowElevationColors = source["ElevationDebug"].as<bool>( false );
                        if ( const auto layers{ source["Layers"] }; layers && layers.IsSequence() ) {
                            for ( core::u32 index{}; index < glm::min<core::u32>( core::as<core::u32>( layers.size() ), renderer::kTerrainSurfaceLayerCount ); ++index ) {
                                const auto layer{ layers[index] };
                                auto &target{ surface.mLayers[index] };
                                target.mName = layer["Name"].as<std::string>( target.mName.c_str() ).c_str();
                                target.mTint = ReadLandscapeColor( layer, "Tint", target.mTint );
                                if ( const auto scale{ layer["UVScale"] }; scale && scale.IsSequence() && scale.size() == 2 ) {
                                    target.mUVScale = glm::clamp( core::float2{ scale[0].as<float>(), scale[1].as<float>() }, core::float2{ .00001f }, core::float2{ 100 } );
                                }
                                if ( const auto range{ layer["HeightRange"] }; range && range.IsSequence() && range.size() == 2 ) {
                                    target.mHeightRange = { range[0].as<float>(), range[1].as<float>() };
                                }
                                target.mRoughness = glm::clamp( layer["Roughness"].as<float>( .8f ), .04f, 1.0f );
                                target.mNormalStrength = glm::clamp( layer["NormalStrength"].as<float>( 1 ), 0.0f, 4.0f );
                                target.mSlopeInfluence = glm::clamp( layer["SlopeInfluence"].as<float>( 0 ), 0.0f, 1.0f );
                                const auto texture{ [&]( core::cstr key ) {
                                    const auto value{ layer[key] };
                                    return asset::AssetRef<asset::Texture>{ filesystem::Path{ value ? value["TexturePath"].as<std::string>( "" ).c_str() : "" } };
                                } };
                                target.mAlbedo = texture( "Albedo" );
                                target.mNormal = texture( "Normal" );
                                target.mHeight = texture( "Height" );
                            }
                        }
                        if ( const auto value{ source["Grass"] }; value && value.IsMap() ) {
                            auto &grass{ surface.mGrass };
                            grass.mEnabled = value["Enabled"].as<bool>( false );
                            grass.mLayer = glm::min( value["Layer"].as<core::u32>( 1 ), renderer::kTerrainSurfaceLayerCount - 1 );
                            grass.mSeed = value["Seed"].as<core::u32>( 721 );
                            grass.mDensity = glm::clamp( value["Density"].as<float>( grass.mDensity ), .1f, 100.0f );
                            grass.mHeight = glm::clamp( value["Height"].as<float>( grass.mHeight ), .05f, 4.0f );
                            grass.mWidth = glm::clamp( value["Width"].as<float>( grass.mWidth ), .002f, .3f );
                            grass.mBend = glm::clamp( value["Bend"].as<float>( grass.mBend ), 0.0f, 1.5f );
                            grass.mClumpSize = glm::clamp( value["ClumpSize"].as<float>( grass.mClumpSize ), .2f, 20.0f );
                            grass.mViewDistance = glm::clamp( value["ViewDistance"].as<float>( grass.mViewDistance ), 5.0f, renderer::kMaxGrassViewDistance );
                            grass.mLodDistance = glm::clamp( value["LodDistance"].as<float>( grass.mLodDistance ), 1.0f, grass.mViewDistance );
                            grass.mMaxSlope = glm::clamp( value["MaxSlope"].as<float>( grass.mMaxSlope ), 0.0f, 80.0f );
                            grass.mMinElevation = glm::clamp( value["MinElevation"].as<float>( grass.mMinElevation ), -10000.0f, 10000.0f );
                            grass.mMaxElevation = glm::clamp( value["MaxElevation"].as<float>( grass.mMaxElevation ), -10000.0f, 10000.0f );
                            grass.mWindSpeed = glm::clamp( value["WindSpeed"].as<float>( grass.mWindSpeed ), 0.0f, 30.0f );
                            grass.mWindStrength = glm::clamp( value["WindStrength"].as<float>( grass.mWindStrength ), 0.0f, 1.5f );
                            grass.mAmbientOcclusion = glm::clamp( value["AmbientOcclusion"].as<float>( grass.mAmbientOcclusion ), 0.0f, 1.0f );
                            grass.mSelfShadow = glm::clamp( value["SelfShadow"].as<float>( grass.mSelfShadow ), 0.0f, 1.0f );
                            grass.mCastShadows = value["CastShadows"].as<bool>( grass.mCastShadows );
                            grass.mContactShadows = value["ContactShadows"].as<bool>( grass.mContactShadows );
                            grass.mShadowDensity = glm::clamp( value["ShadowDensity"].as<float>( grass.mShadowDensity ), 0.0f, 2.0f );
                            grass.mRoughness = glm::clamp( value["Roughness"].as<float>( grass.mRoughness ), .25f, 1.0f );
                            if ( const auto direction{ value["WindDirection"] }; direction && direction.IsSequence() && direction.size() == 2 ) {
                                grass.mWindDirection = { direction[0].as<float>(), direction[1].as<float>() };
                            }
                            grass.mRootColor = ReadLandscapeColor( value, "RootColor", grass.mRootColor );
                            grass.mTipColor = ReadLandscapeColor( value, "TipColor", grass.mTipColor );
                            grass.mUseCustomMesh = value["UseCustomMesh"].as<bool>( false );
                            grass.mMesh = asset::AssetRef<asset::Model>{ filesystem::Path{ value["ModelPath"].as<std::string>( "" ).c_str() } };
                            grass.mMeshIndex = value["MeshIndex"].as<core::u32>( 0 );
                            grass.mMeshWeight = glm::clamp( value["MeshWeight"].as<float>( 1 ), 0.0f, 10.0f );
                            if ( const auto variants{ value["MeshVariants"] }; variants && variants.IsSequence() ) {
                                for ( core::usize i{}; i < glm::min<core::usize>( variants.size(), 7 ); ++i ) {
                                    const auto variant{ variants[i] };
                                    if ( variant.IsMap() ) {
                                        grass.mMeshVariants.push_back( { asset::AssetRef<asset::Model>{ filesystem::Path{ variant["ModelPath"].as<std::string>( "" ).c_str() } },
                                            variant["MeshIndex"].as<core::u32>( 0 ),
                                            glm::clamp( variant["Weight"].as<float>( 1 ), 0.0f, 10.0f ),
                                            glm::clamp( variant["HeightScale"].as<float>( 1 ), .1f, 4.0f ),
                                            glm::clamp( variant["WidthScale"].as<float>( 1 ), .1f, 4.0f ) } );
                                    }
                                }
                            }
                            grass.mOcclusionEnabled = value["OcclusionEnabled"].as<bool>( true );
                            grass.mMidColor = ReadLandscapeColor( value, "MidColor", grass.mMidColor );
                            grass.mDryColor = ReadLandscapeColor( value, "DryColor", grass.mDryColor );
                            grass.mClumpStrength = glm::clamp( value["ClumpStrength"].as<float>( grass.mClumpStrength ), 0.0f, 4.0f );
                            grass.mHeightVariation = glm::clamp( value["HeightVariation"].as<float>( grass.mHeightVariation ), 0.0f, 1.0f );
                            grass.mFarLodDistance = glm::clamp( value["FarLodDistance"].as<float>( grass.mFarLodDistance ), grass.mLodDistance, grass.mViewDistance );
                            grass.mMinDensity = glm::clamp( value["MinDensity"].as<float>( grass.mMinDensity ), .01f, 1.0f );
                            grass.mWidthCompensation = glm::clamp( value["WidthCompensation"].as<float>( grass.mWidthCompensation ), 0.0f, 1.0f );
                            grass.mWindFrequency = glm::clamp( value["WindFrequency"].as<float>( grass.mWindFrequency ), .001f, 1.0f );
                            grass.mWindFlutter = glm::clamp( value["WindFlutter"].as<float>( grass.mWindFlutter ), 0.0f, 1.0f );
                            grass.mWindDistance = glm::clamp( value["WindDistance"].as<float>( grass.mWindDistance ), 1.0f, renderer::kMaxGrassViewDistance );
                            grass.mTranslucency = glm::clamp( value["Translucency"].as<float>( grass.mTranslucency ), 0.0f, 1.0f );
                            grass.mTranslucencyPower = glm::clamp( value["TranslucencyPower"].as<float>( grass.mTranslucencyPower ), 1.0f, 8.0f );
                            grass.mNormalRounding = glm::clamp( value["NormalRounding"].as<float>( grass.mNormalRounding ), 0.0f, .5f );
                            grass.mDryAmount = glm::clamp( value["DryAmount"].as<float>( grass.mDryAmount ), 0.0f, 1.0f );
                            grass.mColorVariation = glm::clamp( value["ColorVariation"].as<float>( grass.mColorVariation ), 0.0f, 1.0f );
                        }
                        landscape.SetSurfaceSettings( surface );
                    }
                    const auto readPixels{ [&]( const YAML::Node &source, core::cstr key, core::usize stride ) {
                        const auto width{ source["Width"].as<core::u32>( 0 ) };
                        const auto height{ source["Height"].as<core::u32>( 0 ) };
                        if ( width < 2 || height < 2 || width > 2049 || height > 2049 ) {
                            MKT_THROW_RUNTIME_ERROR( "Invalid terrain edit map dimensions." );
                        }
                        auto bytes{ source[key].as<YAML::Binary>() };
                        if ( bytes.size() != core::usize{ width } * height * stride ) {
                            MKT_THROW_RUNTIME_ERROR( "Incomplete terrain edit map." );
                        }
                        return bytes;
                    } };
                    if ( const auto edited{ node["SculptedHeightmap"] } ) {
                        const auto bytes{ readPixels( edited, "Pixels", sizeof( core::f32 ) ) };
                        eastl::vector<core::f32> heights( bytes.size() / sizeof( core::f32 ) );
                        std::memcpy( heights.data(), bytes.data(), bytes.size() );
                        landscape.SetSculptedHeightmap( renderer::CreateTerrainHeightmap( edited["Width"].as<core::u32>(), edited["Height"].as<core::u32>(), heights ) );
                    }
                    if ( const auto painted{ node["PaintMap"] } ) {
                        const auto bytes{ readPixels( painted, "Weights", sizeof( core::float4 ) ) };
                        auto map{ eastl::make_shared<renderer::TerrainPaintMap>() };
                        map->mWidth = painted["Width"].as<core::u32>();
                        map->mHeight = painted["Height"].as<core::u32>();
                        map->mWeights.resize( bytes.size() / sizeof( core::float4 ) );
                        std::memcpy( map->mWeights.data(), bytes.data(), bytes.size() );
                        for ( auto &weight: map->mWeights ) {
                            if ( !std::isfinite( weight.x ) || !std::isfinite( weight.y ) || !std::isfinite( weight.z ) || !std::isfinite( weight.w ) ) {
                                MKT_THROW_RUNTIME_ERROR( "Invalid terrain layer weights." );
                            }
                            weight = glm::max( weight, core::float4{ 0 } );
                            const float total{ glm::dot( weight, core::float4{ 1 } ) };
                            weight = total > 1e-6f ? weight / total : core::float4{ 1, 0, 0, 0 };
                        }
                        landscape.SetPaintMap( map );
                    }
                }

                if ( const auto node{ object["WaterRenderComponent"] } ) {
                    auto &water{ record.mWaterRender.emplace() };
                    const auto shape{ node["Shape"].as<core::u32>( 0 ) };

                    if ( shape > static_cast<core::u32>( WaterShape::eCustom ) ) {
                        MKT_THROW_RUNTIME_ERROR( "Invalid water shape." );
                    }

                    water.SetShape( static_cast<WaterShape>( shape ) );

                    if ( const auto size{ node["Dimensions"] } ) {
                        water.SetPlaneDimensions( { size[0].as<float>(), size[1].as<float>() } );
                    }

                    if ( const auto resolution{ node["Resolution"] } ) {
                        water.SetPlaneResolution( { resolution[0].as<float>(), resolution[1].as<float>() } );
                    }

                    water.SetShowGrid( node["ShowGrid"].as<bool>( true ) );
                    water.SetShowVertices( node["ShowVertices"].as<bool>( true ) );
                    water.SetDebugPreviewColor( node["DebugPreviewColor"].as<core::float3>( water.GetDebugPreviewColor() ) );
                    // Existing scenes retain their original foam-free appearance.
                    water.SetFoamAmount( node["FoamAmount"].as<float>( 0.0f ) );

                    auto appearance{ water.GetAppearance() };

                    // Older scenes retain their authored thickness and foam-free coast.
                    if ( const auto properties{ node["Appearance"] } ) {
                        appearance.mInfiniteOcean = properties["InfiniteOcean"].as<bool>( false );
                        appearance.mDepthBasedRefractionEnabled = properties["DepthBasedRefraction"].as<bool>( appearance.mDepthBasedRefractionEnabled );
                        appearance.mShoreFoamEnabled = properties["ShoreFoam"].as<bool>( appearance.mShoreFoamEnabled );
                        appearance.mUnderwaterScatteringEnabled = properties["UnderwaterScattering"].as<bool>( appearance.mUnderwaterScatteringEnabled );
                        appearance.mCausticsEnabled = properties["Caustics"].as<bool>( appearance.mCausticsEnabled );
                        appearance.mShoreFoamDepth = properties["ShoreFoamDepth"].as<core::f32>( appearance.mShoreFoamDepth );
                        appearance.mShoreFoamAmount = properties["ShoreFoamAmount"].as<core::f32>( appearance.mShoreFoamAmount );
                        appearance.mFoamLifetime = properties["FoamLifetime"].as<core::f32>( appearance.mFoamLifetime );
                        appearance.mWaveDepth = properties["WaveDepth"].as<core::f32>( appearance.mWaveDepth );
                        appearance.mShoreWaveSpeed = properties["ShoreWaveSpeed"].as<core::f32>( appearance.mShoreWaveSpeed );
                        appearance.mScattering = properties["Scattering"].as<core::float3>( appearance.mScattering );
                        appearance.mAnisotropy = properties["Anisotropy"].as<core::f32>( appearance.mAnisotropy );
                        appearance.mMaximumDepth = properties["MaximumDepth"].as<core::f32>( appearance.mMaximumDepth );
                        appearance.mCausticsStrength = properties["CausticsStrength"].as<core::f32>( appearance.mCausticsStrength );
                        appearance.mCausticsScale = properties["CausticsScale"].as<core::f32>( appearance.mCausticsScale );
                        appearance.mCausticsDepth = properties["CausticsDepth"].as<core::f32>( appearance.mCausticsDepth );
                    }
                    else {
                        appearance.mDepthBasedRefractionEnabled = false;
                        appearance.mShoreFoamEnabled = false;
                        appearance.mUnderwaterScatteringEnabled = false;
                        appearance.mCausticsEnabled = false;
                    }

                    water.SetAppearance( appearance );

                    if ( node["MaterialPath"] ) {
                        water.SetMaterial( materialFiles.Load( node ) );
                    }
                }

                if ( const auto node{ object["WaterSimulationComponent"] } ) {
                    auto &water{ record.mWaterSimulation.emplace() };
                    const auto type{ node["Type"].as<core::u32>( core::as<core::u32>( water.GetType() ) ) };

                    if ( type > static_cast<core::u32>( WaterSimulationType::eFastFourierTransform ) ) {
                        MKT_THROW_RUNTIME_ERROR( "Invalid water simulation type." );
                    }

                    water.SetType( static_cast<WaterSimulationType>( type ) );
                    water.SetTimeScale( node["TimeScale"].as<float>( 1.f ) );
                    water.SetSteepness( node["Steepness"].as<float>( 0.5f ) );

                    // Scenes saved before FFT support use the component defaults.
                    if ( const auto fft{ node["FFT"] } ) {
                        const auto &defaults{ water.GetFftSettings() };
                        water.SetFftSettings( WaterSimulationComponent::FftSettings{
                            .mPatchLength = fft["PatchLength"].as<core::f32>( defaults.mPatchLength ),
                            .mWindSpeed = fft["WindSpeed"].as<core::f32>( defaults.mWindSpeed ),
                            .mWindDirection = fft["WindDirection"].as<core::f32>( defaults.mWindDirection ),
                            .mAmplitude = fft["Amplitude"].as<core::f32>( defaults.mAmplitude ),
                            .mChoppiness = fft["Choppiness"].as<core::f32>( defaults.mChoppiness ),
                            .mSeed = fft["Seed"].as<core::u32>( defaults.mSeed ) } );
                    }

                    if ( const auto waves{ node["Waves"] } ) {
                        if ( !waves.IsSequence() ) {
                            MKT_THROW_RUNTIME_ERROR( "Water waves must be a sequence." );
                        }

                        water.ClearWaves();

                        for ( const auto &value: waves ) {
                            WaterSimulationComponent::WaterWave wave{};

                            if ( const auto direction{ value["Direction"] } ) {
                                wave.mDirection = { direction[0].as<float>(), direction[1].as<float>() };
                            }

                            wave.mAmplitude = value["Amplitude"].as<float>( wave.mAmplitude );
                            wave.mWavelength = value["Wavelength"].as<float>( wave.mWavelength );
                            wave.mSpeed = value["Speed"].as<float>( wave.mSpeed );
                            wave.mPhaseOffset = value["PhaseOffset"].as<float>( wave.mPhaseOffset );
                            water.AddWave( wave );
                        }
                    }
                }

                if ( const auto keys{ object["KeyframeAnimationComponent"] } ) {
                    record.mKeyframeClip.emplace().DecodeKeyframeClip( keys );
                }

                if ( const auto script{ object["ScriptComponent"] } ) {
                    record.mHasScript = true;
                    const auto language{ script["Language"].as<std::string>( "Lua" ) };

                    if ( language != "Lua" && language != "CSharp" && language != "Cpp" ) {
                        MKT_THROW_RUNTIME_ERROR( "Unknown scene script language." );
                    }

                    record.mScriptLanguage = language == "Cpp"
                                                 ? scripting::ScriptLanguage::eNative
                                             : language == "CSharp"
                                                 ? scripting::ScriptLanguage::eCSharp
                                                 : scripting::ScriptLanguage::eLua;
                    record.mScriptClass = script["ClassName"].as<std::string>( "" ).c_str();
                    record.mScriptProperties = script["Properties"].as<std::string>( "{}" ).c_str();
                    record.mScriptPath = filesystem::Path{ script["ScriptPath"].as<std::string>( "" ).c_str() };

                    if ( record.mScriptLanguage == scripting::ScriptLanguage::eLua && !record.mScriptPath.IsEmpty() && !record.mScriptPath.Exists() ) {
                        MKT_THROW_RUNTIME_ERROR( "Scene script could not be found." );
                    }
                }

                const int index = static_cast<int>( records.size() );
                records.push_back( eastl::move( record ) );

                if ( const auto children = object["Children"] ) {
                    if ( !children.IsSequence() ) {
                        MKT_THROW_RUNTIME_ERROR( "Children must be a sequence." );
                    }

                    for ( const auto &child: children ) {
                        self( self, child, index, depth + 1 );
                    }
                }
            };

            for ( const auto &object: data["Objects"] ) {
                parse( parse, object, -1, 0 );
            }

            // Finish parsing and asset validation before changing the live scene.
            ankerl::unordered_dense::map<core::u64, core::usize> fileEntities;
            eastl::vector<core::i32> groupParents( records.size(), -1 );

            for ( core::usize index{}; index < records.size(); ++index ) {
                const auto &record{ records[index] };

                if ( version >= 5 && !fileEntities.emplace( record.mFileID, index ).second ) {
                    MKT_THROW_RUNTIME_ERROR( "Duplicate scene entity ID." );
                }

                if ( record.mParent >= 0 && records[record.mParent].mType == EntityType::eGrouping ) {
                    MKT_THROW_RUNTIME_ERROR( "Groups cannot be transform parents; use group membership." );
                }
            }

            for ( core::usize index{}; index < records.size(); ++index ) {
                for ( const auto member: records[index].mGroupMembers ) {
                    const auto found{ fileEntities.find( member ) };

                    if ( found == fileEntities.end() || found->second == index || groupParents[found->second] != -1 ) {
                        MKT_THROW_RUNTIME_ERROR( "Missing, duplicate or self-referencing group member." );
                    }

                    groupParents[found->second] = core::as<core::i32>( index );
                }
            }
            // Validate the combined dependency graph before Clear(), including
            // cycles hidden by organizational membership in the outliner.
            eastl::vector<eastl::vector<core::usize>> dependents( records.size() );
            eastl::vector<core::u32> dependencies( records.size(), 0 );
            eastl::vector<core::usize> ready;

            for ( core::usize index{}; index < records.size(); ++index ) {
                for ( const auto parent: { records[index].mParent, groupParents[index] } ) {
                    if ( parent >= 0 ) {
                        dependents[parent].push_back( index );
                        ++dependencies[index];
                    }
                }

                if ( dependencies[index] == 0 ) {
                    ready.push_back( index );
                }
            }

            core::usize visited{};

            while ( !ready.empty() ) {
                const auto index{ ready.back() };
                ready.pop_back();
                ++visited;

                for ( const auto child: dependents[index] ) {
                    if ( --dependencies[child] == 0 ) {
                        ready.push_back( child );
                    }
                }
            }

            if ( visited != records.size() ) {
                MKT_THROW_RUNTIME_ERROR( "Cyclic group hierarchy." );
            }

            scene.SetState( SceneState::eIdle );
            scene.Clear();
            scene.SetEnvironmentLightingSource( core::as<EnvironmentLightingSource>( environmentSource ) );
            scene.SetAtmosphereSettings( atmosphereSettings );
            scene.SetToneMapping( core::as<renderer::ToneMapper>( toneMapping ) );
            scene.SetName( name.c_str() );

            eastl::vector<Entity *> entities{};
            ankerl::unordered_dense::map<core::u64, core::u64> animatorIDs{};
            struct PendingAnimators {
                eastl::vector<core::u64> mIDs{};
                bool mCommitted{};

                ~PendingAnimators() {
                    MKT_BEGIN_PROFILER();
                    // Some animators are registered before their owning entity
                    // exists. Roll them back if any later load operation fails.
                    if ( !mCommitted ) {
                        for ( const core::u64 id: mIDs ) {
                            animation::AnimationSystem::Get()->UnregisterAnimation( id );
                        }
                    }
                }
            } pendingAnimators{};
            pendingAnimators.mIDs.reserve( records.size() );

            // Runtime animator IDs are process-local. Rebuild them once per model
            // instance and reconnect every skinned child to the new owner.
            for ( const auto &record: records ) {
                if ( !record.mAnimatorModel.IsEmpty() ) {
                    const core::u64 savedID{ record.mAnimatorNode["ID"].as<core::u64>() };

                    if ( savedID == 0 || animatorIDs.contains( savedID ) ) {
                        MKT_THROW_RUNTIME_ERROR( "Scene contains an invalid or duplicate animator ID." );
                    }

                    const core::u64 id{ animation::AnimationSystem::Get()->RegisterAnimation( record.mAnimatorModel ) };
                    pendingAnimators.mIDs.push_back( id );
                    animatorIDs.emplace( savedID, id );

                    auto *animator{ animation::AnimationSystem::Get()->GetAnimator( id ) };

                    if ( !animator ) {
                        ( "Unable to register the scene animator" );
                    }

                    if ( record.mController ) {
                        animator->GetController() = *record.mController;
                    }

                    if ( record.mAnimatorNode["Settings"] ) {
                        eastl::string error{};
                        const auto settings{ nlohmann::json::parse( record.mAnimatorNode["Settings"].as<std::string>() ) };

                        if ( !DecodeAnimatorSettings( settings, *animator, error ) ) {
                            ( error );
                        }
                    }

                    if ( record.mAnimatorNode["Clip"] ) {
                        animator->SetCurrentAnimation( record.mAnimatorNode["Clip"].as<std::string>().c_str() );
                    }

                    if ( record.mAnimatorNode["ControllerPlaying"].as<bool>( false ) ) {
                        ( void )animator->PlayController();
                    }
                    else if ( record.mAnimatorNode["Playing"].as<bool>( false ) ) {
                        animator->PlayCurrentAnimation();
                    }
                }
            }

            for ( const auto &record: records ) {
                EntityCreateInfo info;
                info.mName = record.mName;
                info.mEntityType = record.mType;
                info.mRoot = record.mParent < 0 ? nullptr : entities.at( record.mParent );
                auto *entity = scene.CreateEntityDefault( info );

                entities.push_back( entity );

                if ( !record.mStreamingModelPath.IsEmpty() ) {
                    ResumeModelCreation( entity->GetComponent<TagComponent>().GetGuid(),
                        asset::AssetRef<asset::Model>{ record.mStreamingModelPath }, record.mNextStreamedMesh );
                }

                if ( !record.mAnimatorModel.IsEmpty() ) {
                    entity->AddComponent<AnimatorComponent>( animatorIDs.at( record.mAnimatorNode["ID"].as<core::u64>() ) );
                }

                if ( record.mSkinAnimatorID ) {
                    const auto found{ animatorIDs.find( record.mSkinAnimatorID ) };

                    if ( found == animatorIDs.end() ) {
                        MKT_THROW_RUNTIME_ERROR( "Skin references a missing animator." );
                    }

                    entity->AddComponent<SkinnedMeshRenderer>( found->second );
                }

                if ( record.mMaterial ) {
                    entity->AddComponent<MaterialComponent>( record.mMaterial );
                }

                if ( !record.mModel.IsEmpty() ) {
                    scene.SetupMeshComponent( entity, record.mModel, record.mMeshIndex );
                }

                if ( record.mSkyLight ) {
                    entity->AddComponent<SkyLightComponent>( *record.mSkyLight );
                }

                if ( record.mSkybox ) {
                    entity->AddComponent<SkyboxMaterialComponent>().SetMaterial( record.mSkybox );
                }

                if ( record.mPostProcessMaterial ) {
                    entity->AddComponent<PostProcessMaterialComponent>( record.mPostProcessMaterial )
                        .SetCameraEntityID( record.mPostProcessCameraEntityID );
                }

                entity->GetComponent<TransformComponent>() = record.mTransform;
                entity->GetComponent<TagComponent>().SetActive( record.mActive );

                if ( record.mCameraDepthOfField ) {
                    auto &cameraComponent{ entity->AddComponent<CameraComponent>() };
                    auto &camera{ cameraComponent.GetCamera() };
                    const auto &node{ record.mCameraNode };
                    camera.SetClipPlanes( node["Near"].as<core::f32>( 0.1f ), node["Far"].as<core::f32>( 1000 ) );
                    camera.SetFieldOfView( node["Fov"].as<core::f32>( 45 ) );
                    camera.SetOrthographicHeight( node["OrthographicHeight"].as<core::f32>( 20 ) );
                    camera.SetProjectionType( node["Projection"].as<core::i32>( 1 ) == 0
                                                  ? ProjectionType::eOrthographic
                                                  : ProjectionType::ePerspective );

                    if ( node["Viewport"] ) {
                        const auto viewport{ ReadPhysicsVector( node["Viewport"] ) };

                        if ( viewport.x > 0 && viewport.y > 0 ) {
                            camera.SetViewportSize( viewport.x, viewport.y );
                        }
                    }

                    cameraComponent.SetMainCamera( node["Main"].as<bool>( true ) );
                    cameraComponent.SetFixedAspectRatio( node["FixedAspect"].as<bool>( false ) );
                    cameraComponent.SetGamma( node["Gamma"].as<core::f32>( 2.0f ) );
                    cameraComponent.SetExposure( node["Exposure"].as<core::f32>( 2.0f ) );
                    cameraComponent.SetSkyboxBlur( node["SkyboxBlur"].as<core::f32>( 0.3f ) );
                    cameraComponent.SetClearFlags( static_cast<CameraClearFlags>( node["ClearFlags"].as<core::i32>( 0 ) ) );

                    if ( const auto color{ node["ClearColor"] }; color && color.IsSequence() && color.size() == 4 ) {
                        cameraComponent.SetClearColor( mikoto::core::float4{
                            color[0].as<core::f32>(), color[1].as<core::f32>(), color[2].as<core::f32>(),
                            color[3].as<core::f32>() } );
                    }

                    camera.SetDepthOfFieldSettings( *record.mCameraDepthOfField );
                    camera.SetFilmGrainSettings( record.mCameraFilmGrain );

                    if ( record.mCameraVignette ) {
                        camera.SetVignetteSettings( *record.mCameraVignette );
                    }
                }

                if ( record.mHasScript ) {
                    // Keep missing C# assemblies editable: a project may be opened
                    // before its managed scripts have been built on this machine.
                    entity->AddComponent<ScriptComponent>( record.mScriptPath, record.mScriptLanguage,
                              record.mScriptClass )
                        .SetProperties( record.mScriptProperties );
                }

                if ( record.mText ) {
                    entity->AddComponent<TextComponent>( *record.mText );
                }

                if ( record.mCanvas ) {
                    entity->AddComponent<CanvasComponent>( *record.mCanvas );
                }

                if ( record.mAudio ) {
                    entity->AddComponent<AudioSourceComponent>( eastl::move( *record.mAudio ) );
                }

                if ( record.mVideo ) {
                    entity->AddComponent<VideoComponent>( *record.mVideo );
                }

                if ( record.mTextureRender ) {
                    entity->AddComponent<TextureRenderComponent>( eastl::move( *record.mTextureRender ) );
                }

                if ( record.mAudioMixer ) {
                    entity->AddComponent<AudioMixerComponent>( eastl::move( *record.mAudioMixer ) );
                }

                if ( record.mAudioGroup ) {
                    entity->AddComponent<AudioMixerGroupComponent>( *record.mAudioGroup );
                }

                if ( record.mListener ) {
                    entity->AddComponent<AudioListenerComponent>( *record.mListener );
                }

                if ( record.mWaterRender ) {
                    entity->AddComponent<WaterRenderComponent>( *record.mWaterRender );
                }

                if ( record.mWaterSimulation ) {
                    entity->AddComponent<WaterSimulationComponent>( *record.mWaterSimulation );
                }

                if ( record.mLandscape ) {
                    entity->AddComponent<LandscapeComponent>( *record.mLandscape );
                }

                if ( record.mParticleEffect ) {
                    entity->AddComponent<ParticleSimulatorComponent>().SetEffect( record.mParticleEffect );
                }

                if ( record.mCloudInteraction ) {
                    entity->AddComponent<CloudInteractionComponent>( *record.mCloudInteraction );
                }
                if ( record.mCloud ) {
                    entity->AddComponent<CloudComponent>( *record.mCloud );
                }
                if ( record.mFog ) {
                    entity->AddComponent<FogComponent>( *record.mFog );
                }

                if ( record.mLight ) {
                    entity->AddComponent<LightComponent>( *record.mLight );
                }

                if ( record.mKeyframeClip ) {
                    entity->AddComponent<KeyframeAnimationComponent>().mClip = eastl::move( *record.mKeyframeClip );
                }

                if ( record.mBox ) {
                    entity->AddComponent<BoxColliderComponent>( *record.mBox );
                }

                if ( record.mSphere ) {
                    entity->AddComponent<SphereColliderComponent>( *record.mSphere );
                }

                if ( record.mCapsule ) {
                    entity->AddComponent<CapsuleColliderComponent>( *record.mCapsule );
                }

                if ( record.mMesh ) {
                    entity->AddComponent<MeshColliderComponent>( *record.mMesh );
                }

                if ( record.mVehicle ) {
                    entity->AddComponent<VehicleComponent>( *record.mVehicle );
                }

                if ( record.mVehicleWheel ) {
                    entity->AddComponent<VehicleWheelComponent>( *record.mVehicleWheel );
                }

                if ( record.mBridgeSegment ) {
                    entity->AddComponent<BridgeSegmentComponent>( *record.mBridgeSegment );
                }

                if ( record.mRigidBody ) {
                    entity->AddComponent<RigidBodyComponent>( *record.mRigidBody );

                    if ( !record.mBox && !record.mSphere && !record.mCapsule && !record.mMesh ) {
                        entity->RemoveComponent<BoxColliderComponent>();
                    }
                }

                DeserializePhysicsComponents( *entity, record.mPhysicsNode );
            }

            // References may point forward in the document, so reconnect only after allocation.
            for ( core::usize index{}; index < entities.size(); ++index ) {
                auto *entity{ entities[index] };

                if ( entity->HasComponent<TextureRenderComponent>() ) {
                    const auto found{ fileEntities.find( records[index].mTextureRenderCameraEntityID ) };

                    if ( found != fileEntities.end() && entities[found->second]->HasComponent<CameraComponent>() ) {
                        const auto &camera{ entities[found->second]->GetComponent<CameraComponent>() };
                        entity->GetComponent<TextureRenderComponent>().SetCamera( camera.HasCamera() ? &camera.GetCamera() : nullptr,
                            entities[found->second]->GetComponent<TagComponent>().GetGuid() );
                    }
                }

                if ( entity->HasComponent<PostProcessMaterialComponent>() ) {
                    auto &component{ entity->GetComponent<PostProcessMaterialComponent>() };
                    const auto found{ fileEntities.find( component.GetCameraEntityID() ) };
                    // A removed preset camera disables grain rather than invalidating the scene.
                    component.SetCameraEntityID( found == fileEntities.end() ? 0
                                                                             : entities[found->second]->GetComponent<TagComponent>().GetGuid() );
                }

                if ( entity->HasComponent<LightComponent>() ) {
                    auto &area{ entity->GetComponent<LightComponent>().Get<renderer::AreaLight>() };
                    const auto found{ fileEntities.find( area.GetEmissionSourceEntityID() ) };
                    area.SetEmissionSourceEntityID( found == fileEntities.end() ? 0 : entities[found->second]->GetComponent<TagComponent>().GetGuid() );
                }

                RemapPhysicsEntities( *entity, [&]( core::u64 id ) -> core::u64 {
                    if ( id == 0 ) {
                        return 0;
                    }

                    const auto found{ fileEntities.find( id ) };

                    if ( found == fileEntities.end() ) {
                        MKT_THROW_RUNTIME_ERROR( "Physics component references a missing entity." );
                    }

                    return entities[found->second]->GetComponent<TagComponent>().GetGuid();
                } );
            }

            for ( core::usize index{}; index < records.size(); ++index ) {
                if ( groupParents[index] >= 0 ) {
                    if ( !scene.SetEntityGroup( entities[index]->GetComponent<TagComponent>().GetGuid(),
                             entities[groupParents[index]]->GetComponent<TagComponent>().GetGuid() ) ) {
                        MKT_THROW_RUNTIME_ERROR( "Cannot restore group membership." );
                    }
                }
            }

            scene.UpdateWorldTransformations();

            if ( scene.mPhysicsWorld ) {
                scene.mPhysicsWorld->Synchronize();
            }

            pendingAnimators.mCommitted = true;
        } catch ( const std::exception &error ) {
            MKT_CORE_LOGGER_ERROR( "Cannot load scene '{}': {}", file->GetPath().GetC_Str(), error.what() );
            throw;
        }
    }

    auto Scene::Deserialize( const filesystem::Path &filename ) -> void {
        MKT_BEGIN_PROFILER();

        if ( filename.IsEmpty() || !std::filesystem::is_regular_file( filename.GetPathTyped<std::filesystem::path>() ) ) {
            MKT_THROW_RUNTIME_ERROR( "Scene file does not exist." );
        }

        Deserialize( filesystem::FileHandle::New( filename ) );
    }

    auto SceneSerializer::Serialize( const core::ISerializable *serializable, const filesystem::Path &saveFilePath ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !serializable || saveFilePath.IsEmpty() ) {
            return;
        }

        serializable->Serialize( saveFilePath );
    }

    auto SceneSerializer::Deserialize( const filesystem::Path &saveFilePath ) -> core::Ref<core::ISerializable> {
        MKT_BEGIN_PROFILER();

        if ( saveFilePath.IsEmpty() ) {
            return core::Ref<core::ISerializable>::CreateEmpty();
        }

        core::Ref<core::ISerializable> result{ core::Ref<Scene>::New() };
        result->Deserialize( saveFilePath );
        return result;
    }

    // https://skypjack.github.io/entt/index.html


    auto Scene::UpdateWorldTransforms( eastl::span<const core::usize> parents,
        eastl::span<const mikoto::core::float4x4> localTransforms,
        eastl::span<mikoto::core::float4x4> worldTransforms,
        eastl::span<core::u8> dirty ) -> core::usize {
        assert( parents.size() == localTransforms.size() );
        assert( parents.size() == worldTransforms.size() );
        assert( parents.size() == dirty.size() );

        core::usize updated{};

        for ( core::usize index{}; index < parents.size(); ++index ) {
            const auto parent{ parents[index] };
            assert( parent == kNoTransformParent || parent < index );
            const bool parentChanged{ parent != kNoTransformParent && dirty[parent] != 0 };
            dirty[index] = dirty[index] != 0 || parentChanged;

            if ( !dirty[index] ) {
                continue;
            }

            worldTransforms[index] = parent == kNoTransformParent
                                         ? localTransforms[index]
                                         : worldTransforms[parent] * localTransforms[index];
            ++updated;
        }

        return updated;
    }

    Scene::Scene( const eastl::string_view name )
        : mName{ name }, mHierarchySignal{ eastl::make_shared<HierarchySignal>() } {
        mRegistry.on_construct<TransformComponent>().connect<&Scene::OnTransformStorageChanged>( this );
        mRegistry.on_destroy<TransformComponent>().connect<&Scene::OnTransformStorageChanged>( this );
        MKT_BEGIN_PROFILER();

        // Renderer
        mRegistry.on_construct<MeshComponent>().connect<&OnMeshRendererAdded>();

        mRegistry.on_construct<ParticleSimulatorComponent>().connect<&Scene::OnParticleSimulatorAdded>( this );

        mRegistry.on_construct<SkyboxMaterialComponent>().connect<&OnSkyboxMaterialAdded>();

        // Animation
        mRegistry.on_destroy<AnimatorComponent>().connect<&OnAnimatorRemoved>();

        // Audio
        mRegistry.on_construct<AudioListenerComponent>().connect<&OnAudioListenerAdded>();
        mRegistry.on_destroy<AudioListenerComponent>().connect<&OnAudioListenerRemoved>();

        // Script
        mRegistry.on_construct<ScriptComponent>().connect<&Scene::OnScriptAdded>( this );
        mRegistry.on_destroy<ScriptComponent>().connect<&OnScriptRemoved>();

        // Physics
        mRegistry.on_construct<RigidBodyComponent>().connect<&Scene::OnRigidBodyAdded>( this );
        mRegistry.on_construct<MeshColliderComponent>().connect<&Scene::OnColliderAdded<MeshColliderComponent>>( this );
        mRegistry.on_construct<BoxColliderComponent>().connect<&Scene::OnColliderAdded<BoxColliderComponent>>( this );
        mRegistry.on_construct<SphereColliderComponent>().connect<&Scene::OnColliderAdded<SphereColliderComponent>>( this );
        mRegistry.on_construct<CapsuleColliderComponent>().connect<&Scene::OnColliderAdded<CapsuleColliderComponent>>( this );

        ForEachExtendedPhysicsComponent( [&]<typename T>( const char * ) {
            if constexpr ( requires { T::GetColliderType(); } ) {
                mRegistry.on_construct<T>().template connect<&Scene::OnColliderAdded<T>>( this );
                mRegistry.on_destroy<T>().template connect<&Scene::OnColliderRemoved>( this );
            }
        } );
        mRegistry.on_destroy<RigidBodyComponent>().connect<&Scene::OnRigidBodyRemoved>( this );
        mRegistry.on_destroy<MeshColliderComponent>().connect<&Scene::OnColliderRemoved>( this );
        mRegistry.on_destroy<BoxColliderComponent>().connect<&Scene::OnColliderRemoved>( this );
        mRegistry.on_destroy<SphereColliderComponent>().connect<&Scene::OnColliderRemoved>( this );
        mRegistry.on_destroy<CapsuleColliderComponent>().connect<&Scene::OnColliderRemoved>( this );

        // Component callbacks must be connected before the world creates entity bodies.
        physics::PhysicsWorldCreateInfo spec{
            .mScene = this,
            .mGravity = physics::PhysicsWorld::GetGravityFor( physics::GravityBody::eEarth )
        };
        mPhysicsWorld = physics::PhysicSystem::Get()->CreatePhysicsWorld( spec );

        if ( auto *particles{ renderer::ParticleSystem::GetPtr() } ) {
            particles->RegisterScene( *this );
        }
    }

    auto Scene::OnParticleSimulatorAdded( entt::registry &, entt::entity ) -> void {
        MKT_BEGIN_PROFILER();

        // Clear unregisters the scene. Reusing it, including deserialization,
        // reconnects particle updates as soon as an emitter is added again.
        if ( auto *particles{ renderer::ParticleSystem::GetPtr() } ) {
            particles->RegisterScene( *this );
        }
    }

    auto Scene::OnRigidBodyAdded( entt::registry &reg, entt::entity e ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !HasCollider( reg, e ) ) {
            reg.emplace_or_replace<BoxColliderComponent>( e );
        }

        const TagComponent &tag{ reg.get<TagComponent>( e ) };
        Entity *entity{ FindByID( tag.GetGuid() ) };
        mPhysicsWorld->AddRigidBody( entity );

        ++mStatistics.mRigidBodyCount;
    }

    template<typename Collider>
    auto Scene::OnColliderAdded( entt::registry &reg, entt::entity e ) -> void {
        MKT_BEGIN_PROFILER();
        // One authoritative collider per entity. Replacing it is explicit and deterministic.
        ForEachColliderComponent( [&]<typename T>() {
            if constexpr ( !std::is_same_v<Collider, T> ) {
                reg.remove<T>( e );
            }
        } );

        ++mStatistics.mColliderCount;
    }

    auto Scene::OnRigidBodyRemoved( entt::registry &reg, entt::entity e ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mPhysicsWorld ) {
            mPhysicsWorld->RemoveEntityBody( e );
        }

        if ( mStatistics.mRigidBodyCount > 0 ) {
            --mStatistics.mRigidBodyCount;
        }
        // On the next synchronization the remaining collider becomes a static body.
    }

    auto Scene::OnColliderRemoved( entt::registry &reg, entt::entity e ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mPhysicsWorld ) {
            mPhysicsWorld->RemoveEntityBody( e );
        }

        if ( mStatistics.mColliderCount > 0 ) {
            --mStatistics.mColliderCount;
        }
    }

    auto Scene::OnScriptAdded( entt::registry &registry, entt::entity handle ) -> void {
        MKT_BEGIN_PROFILER();
        const auto language{ registry.get<ScriptComponent>( handle ).GetLanguage() };
        scripting::ScriptHandle script{};
        Entity *entity{ FindByHandle( handle ) };
        const filesystem::Path path{ registry.get<ScriptComponent>( handle ).GetFilePath() };

        if ( entity ) {
            if ( path.IsEmpty() && mSceneState != SceneState::ePlaying ) {
                script = scripting::ScriptingService::Get()->CreateScript( language, entity, mSceneState != SceneState::ePlaying );
            }
            else if ( !path.IsEmpty() && mSceneState == SceneState::ePlaying ) {
                script = scripting::ScriptingService::Get()->LoadScript( language, path, entity, false );
            }
        }
        // Script loading can execute user code. Its component address is not
        // stable across that call if user code changes the registry.
        if ( registry.valid( handle ) ) {
            if ( auto *component{ registry.try_get<ScriptComponent>( handle ) } ) {
                component->SetScript( script );
            }
        }
    }


    auto Scene::GetSceneStats() const -> const SceneStatistics & {
        MKT_BEGIN_PROFILER();
        return mStatistics;
    }


    auto Scene::SetName( const eastl::string_view name ) -> void {
        MKT_BEGIN_PROFILER();
        mName = name;
    }


    auto Scene::GetName() const -> eastl::string_view {
        MKT_BEGIN_PROFILER();
        return mName;
    }

    auto Scene::GetEntities() const -> const ankerl::unordered_dense::map<core::u64, eastl::unique_ptr<Entity>> & {
        MKT_BEGIN_PROFILER();
        return mEntities;
    }

    auto Scene::Clear() -> void {
        MKT_BEGIN_PROFILER();

        if ( auto *scripting{ scripting::ScriptingService::GetPtr() } ) {
            if ( auto *dotnet{ scripting->GetDotnetScriptingService() } ) {
                dotnet->Stop( *this );
            }
        }

        if ( mPhysicsWorld ) {
            mPhysicsWorld->ClearBodies();
        }

        mRegistry.clear();

        // Clear is also used by the destructor; neither path may leave a
        // dangling scene registration in the particle service.
        if ( auto *particles{ renderer::ParticleSystem::GetPtr() } ) {
            particles->UnRegisterScene( *this );
        }

        mEntities.clear();
        mRootEntities.clear();

        // Discard cached transforms and geometry together with their entity identities.
        mTransformEntities.clear();
        mTransformParents.clear();
        mLocalTransforms.clear();
        mWorldTransforms.clear();
        mTransformDirty.clear();
        mGeometryCache.reset();
        mImportedMaterials.clear();

        mHighlightedEntities.clear();
        mNextHighlightedEntities.clear();
        mHighlightPending.clear();
        mHighlightRoot = 0;
        mHighlightRoots.clear();

        NotifyHierarchyChanged( true );
        mStatistics = {};
        ++mCommandGeneration;

        if ( !mProcessingEntityCommands ) {
            mPendingEntityCommands.clear();
        }

        std::lock_guard lock{ mCommandQueueMutex };
        mEntityCommands.clear();
    }

    Scene::~Scene() {
        MKT_BEGIN_PROFILER();
        Clear();

        if ( mPhysicsWorld ) {
            physics::PhysicSystem::Get()->RemovePhysicsWorld( this );
        }
    }


    auto Scene::GetPhysicsWorld() const -> physics::PhysicsWorld * {
        MKT_BEGIN_PROFILER();
        return mPhysicsWorld;
    }

    auto Scene::GetRootEntities() const -> const ankerl::unordered_dense::set<Entity *> & {
        MKT_BEGIN_PROFILER();
        return mRootEntities;
    }


    auto Scene::GetEntityCount() const -> core::usize {
        MKT_BEGIN_PROFILER();
        return mEntities.size();
    }


    auto Scene::GetRegistry() -> entt::registry & {
        MKT_BEGIN_PROFILER();
        return mRegistry;
    }

    auto Scene::GetRegistry() const -> const entt::registry & {
        MKT_BEGIN_PROFILER();
        return mRegistry;
    }

    auto Scene::DispatchPhysicsEvents() -> void {
        if ( !mPhysicsWorld ) {
            return;
        }

        // Drain even in Simulate/Idle so those modes cannot retain a stale event batch.
        const auto events{ mPhysicsWorld->GetContactEvents() };
        if ( events.empty() ) {
            return;
        }
        const auto generation{ mCommandGeneration };
        for ( const auto &event: events ) {
            for ( const auto id: { event.mEntityA, event.mEntityB } ) {
                if ( !IsPlaying() || generation != mCommandGeneration ) {
                    return;
                }

                const auto otherID{ id == event.mEntityA ? event.mEntityB : event.mEntityA };
                auto *entity{ FindByID( id ) };
                auto *other{ FindByID( otherID ) };
                if ( !entity || !other || !entity->GetComponent<TagComponent>().IsActive() ||
                     !entity->HasComponent<ScriptComponent>() ) {
                    continue;
                }

                // A callback may queue deletion of either participant or a parent.
                // Resolve again for each recipient and never hold registry references
                // across user code. The retained script handle outlives the invocation.
                const auto pendingRemoval{ [&]( Entity *candidate ) {
                    std::lock_guard lock{ mCommandQueueMutex };
                    for ( core::usize remaining{ mEntities.size() }; candidate && remaining; --remaining ) {
                        const auto candidateID{ candidate->GetComponent<TagComponent>().GetGuid() };
                        for ( const auto *commands: { &mEntityCommands, &mPendingEntityCommands } ) {
                            for ( const auto &command: *commands ) {
                                if ( command.mType == EntityCommand::Type::eDestroy && command.mEntityID == candidateID ) {
                                    return true;
                                }
                            }
                        }
                        const auto parent{ candidate->GetComponent<RelationComponent>().GetParent() };
                        candidate = parent ? FindByID( *parent ) : nullptr;
                    }
                    return false;
                } };
                if ( pendingRemoval( entity ) || pendingRemoval( other ) ) {
                    continue;
                }

                auto script{ entity->GetComponent<ScriptComponent>().GetHandle() };
                if ( script ) {
                    script->OnPhysicsEvent( event, *other );
                }
            }
        }
        UpdateWorldTransformations();
    }

    auto Scene::UpdatePlaying( core::f64 timeStep ) -> void {
        MKT_BEGIN_PROFILER();

        // Script callbacks can add components or queue entity removal. Snapshot
        // handles, then resolve each one again instead of retaining view references.
        mScriptUpdateEntities.clear();

        for ( const auto handle: mRegistry.view<ScriptComponent, TagComponent>() ) {
            mScriptUpdateEntities.push_back( handle );
        }

        for ( const auto entity: mScriptUpdateEntities ) {
            if ( !mRegistry.valid( entity ) ) {
                continue;
            }

            const auto *tag{ mRegistry.try_get<TagComponent>( entity ) };
            auto *script{ mRegistry.try_get<ScriptComponent>( entity ) };

            if ( tag && tag->IsActive() && script ) {
                // Newly added or reassigned components load through the same language factory.
                if ( script->GetHandle().IsEmpty() ) {
                    OnScriptAdded( mRegistry, entity );
                    script = mRegistry.valid( entity ) ? mRegistry.try_get<ScriptComponent>( entity ) : nullptr;
                }

                scripting::ScriptHandle handle{ script ? script->GetHandle() : scripting::ScriptHandle{} };

                if ( handle ) {
                    handle->Update( timeStep );
                }
            }
        }
    }

    auto Scene::IsPlaying() const -> bool {
        MKT_BEGIN_PROFILER();
        return mSceneState == SceneState::ePlaying;
    }

    auto Scene::SetState( SceneState state ) -> void {
        MKT_BEGIN_PROFILER();

        if ( state == SceneState::eIdle ) {
            mVolumeTime = 0;
            // Workspace changes also request Idle on an already-idle scene.
            // Restore previews before it is hidden or used as the Play source.
            for ( auto [handle, animation]: mRegistry.view<KeyframeAnimationComponent>().each() ) {
                if ( animation.IsPreviewing() ) {
                    if ( auto *entity{ FindByHandle( handle ) } ) {
                        animation.Stop( *entity );
                    }
                }
            }
        }

        if ( state == SceneState::eIdle || state != mSceneState ) {
            // Also stop editor-owned audio when the workspace is hidden or
            // cloned for Play; only the destination scene should remain audible.
            for ( auto [entity, video]: mRegistry.view<VideoComponent>().each() ) {
                video.ReleasePlayback();
            }
        }

        if ( state == mSceneState ) {
            return;
        }

        if ( mSceneState == SceneState::ePlaying ) {

            for ( auto [entity, script]: mRegistry.view<ScriptComponent>().each() ) {
                if ( auto handle{ script.GetHandle() } ) {
                    handle->Stop();
                }
            }

            scripting::ScriptingService::Get()->GetDotnetScriptingService()->Stop( *this );
        }

        mSceneState = state;

        for ( auto [handle, animation]: mRegistry.view<KeyframeAnimationComponent>().each() ) {
            if ( auto *entity{ FindByHandle( handle ) } ) {
                animation.Stop( *entity );

                if ( state == SceneState::ePlaying && animation.mClip.mPlayOnStart ) {
                    animation.Play( *entity, false );
                }
            }
        }

        if ( state != SceneState::eIdle ) {
            physics::PhysicSystem::Get()->SetSimulationTarget( this );
        }

        if ( state != SceneState::ePlaying ) {
            return;
        }

        if ( mPhysicsWorld ) {
            mPhysicsWorld->RestartContactNotifications();
        }

        eastl::fixed_vector<entt::entity, 32> scripts{};

        for ( const auto handle: mRegistry.view<ScriptComponent>() ) {
            scripts.push_back( handle );
        }

        for ( const auto handle: scripts ) {
            if ( !mRegistry.valid( handle ) ) {
                continue;
            }

            auto *script{ mRegistry.try_get<ScriptComponent>( handle ) };

            if ( !script || script->GetFilePath().IsEmpty() ) {
                continue;
            }

            const filesystem::Path path{ script->GetFilePath() };
            auto loaded{ scripting::ScriptingService::Get()->LoadScript( script->GetLanguage(), path, FindByHandle( handle ), false ) };
            // Loading can execute Lua initialization, which can change ECS storage.
            // Reacquire the component instead of retaining a reference across it.
            if ( mRegistry.valid( handle ) ) {
                if ( auto *current{ mRegistry.try_get<ScriptComponent>( handle ) } ) {
                    current->SetScript( loaded );

                    if ( loaded ) {
                        loaded->Start( *this );
                    }
                }
            }
        }
    }

    auto Scene::UpdateAudioListenerAndSources( core::f32 timeStep ) -> void {
        MKT_BEGIN_PROFILER();
        auto *device{ audio::AudioService::Get()->GetDevice() };
        auto mixers{ mRegistry.view<AudioMixerComponent, TagComponent>() };
        auto groups{ mRegistry.view<AudioMixerGroupComponent, TagComponent>() };

        for ( auto [handle, component, tag]: mixers.each() ) {
            if ( auto *mixer{ component.GetMixer( device ) } ) {
                mixer->SetVolume( component.GetVolume(), component.IsMuted() || !tag.IsActive() );
                mixer->BeginUpdate();
            }
        }

        for ( auto [handle, group, tag]: groups.each() ) {
            auto *owner{ FindByID( group.mMixerID ) };

            if ( owner && owner->HasComponent<AudioMixerComponent>() ) {
                if ( auto *mixer{ owner->GetComponent<AudioMixerComponent>().GetMixer( device ) } ) {
                    // A failed bus allocation falls back to the device when sources are routed below.
                    ( void )mixer->UpdateGroup( tag.GetGuid(), group.mVolume, group.mMuted || !tag.IsActive() );
                }
            }
        }

        for ( auto [handle, group, tag]: groups.each() ) {
            auto *owner{ FindByID( group.mMixerID ) };

            if ( owner && owner->HasComponent<AudioMixerComponent>() ) {
                if ( auto *mixer{ owner->GetComponent<AudioMixerComponent>().GetMixer( device ) } ) {
                    if ( !mixer->RouteGroup( tag.GetGuid(), group.mOutputID ) ) {
                        mixer->RouteGroup( tag.GetGuid(), 0 );
                    }
                }
            }
        }

        for ( auto [handle, component, tag]: mixers.each() ) {
            if ( auto *mixer{ component.GetMixer( device ) } ) {
                mixer->EndUpdate();
            }
        }
        // Update listeners
        auto viewListeners{ mRegistry.view<TransformComponent, AudioListenerComponent, TagComponent>() };

        for ( const auto &entity: viewListeners ) {
            TransformComponent &transformComponent{ mRegistry.get<TransformComponent>( entity ) };
            AudioListenerComponent &audioListenerComponent{ mRegistry.get<AudioListenerComponent>( entity ) };

            if ( audioListenerComponent.IsActive() ) {
                auto &listener{ audioListenerComponent.GetListener() };
                const auto &world{ transformComponent.GetWorldTransform() };
                const bool active{ audioListenerComponent.IsEnabled() && mRegistry.get<TagComponent>( entity ).IsActive() };
                listener.UpdatePosition( mikoto::core::float3{ world[3] }, active ? timeStep : 0.0f );

                const core::float3 forward{ -core::float3{ world[2] } };
                const core::float3 up{ core::float3{ world[1] } };

                listener.SetOrientation( forward.x, forward.y, forward.z, up.x, up.y, up.z );
                listener.SetEnabled( active );
            }
        }

        // Update sources
        auto viewSources{ mRegistry.view<TransformComponent, AudioSourceComponent, TagComponent>() };

        for ( const auto &entity: viewSources ) {
            TransformComponent &transformComponent{ mRegistry.get<TransformComponent>( entity ) };
            AudioSourceComponent &audioSourceComponent{ mRegistry.get<AudioSourceComponent>( entity ) };

            audio::AudioSourceHandle source{ audioSourceComponent.GetSource() };

            if ( !source.IsEmpty() ) {
                auto *output{ FindByID( audioSourceComponent.mOutputID ) };
                core::u64 groupID{};

                if ( output && output->HasComponent<AudioMixerGroupComponent>() ) {
                    groupID = output->GetComponent<TagComponent>().GetGuid();
                    output = FindByID( output->GetComponent<AudioMixerGroupComponent>().mMixerID );
                }

                auto *mixer{ output && output->HasComponent<AudioMixerComponent>() ? output->GetComponent<AudioMixerComponent>().GetMixer( device ) : nullptr };

                if ( !source->SetOutput( mixer, mixer ? groupID : 0 ) ) {
                    source->SetOutput( nullptr );
                }

                const bool active = mRegistry.get<TagComponent>( entity ).IsActive();
                source->UpdatePosition( mikoto::core::float3{ transformComponent.GetWorldTransform()[3] }, active ? timeStep : 0.0f );

                if ( !active ) {
                    source->Stop();
                }
            }
        }
    }

    auto Scene::Update( renderer::ISceneRenderer *renderer ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !renderer ) {
            return;
        }

        // Scene data owns the environment; it must also reach the renderer
        // when the inspector is closed, scripts change it, or Play swaps scenes.
        const auto environment{ GetEnvironmentMaterial() };

        renderer->SetSkyboxMaterial( environment );
        renderer->SetEnvironmentLightingEnabled( GetEnvironmentLightingSource() != EnvironmentLightingSource::eSceneLightsOnly );
        renderer->SetTonemapType( mToneMapping );
        renderer->SetAmbientScale( GetEnvironmentIntensity() );
    }

    auto Scene::SetToneMapping( renderer::ToneMapper type ) -> void {
        MKT_BEGIN_PROFILER();
        const core::i32 value{ static_cast<core::i32>( type ) };
        mToneMapping = value >= 0 && value < static_cast<core::i32>( renderer::ToneMapper::eCount )
                           ? type
                           : renderer::ToneMapper::eAces;
    }

    auto Scene::GetToneMapping() const -> renderer::ToneMapper {
        MKT_BEGIN_PROFILER();
        return mToneMapping;
    }

    auto Scene::GetEnvironmentLightingSource() const -> EnvironmentLightingSource {
        MKT_BEGIN_PROFILER();
        if ( const auto *sky{ GetSkyLight() } ) {
            return sky->GetSource() == SkyLightSource::eAtmosphere ? EnvironmentLightingSource::eAtmosphere : EnvironmentLightingSource::eEnvironmentMap;
        }

        return mEnvironmentLightingSource;
    }

    auto Scene::SetAtmosphereSettings( const renderer::AtmosphereSettings &settings ) -> void {
        mAtmosphereSettings = settings;
        mAtmosphereSettings.Sanitize();
    }

    auto Scene::GetAtmosphereSettings() const -> const renderer::AtmosphereSettings & {
        return mAtmosphereSettings;
    }

    auto Scene::OnViewPortResize( const core::f32 width, const core::f32 height ) -> void {
        MKT_BEGIN_PROFILER();
        // Resize non-fixed aspect ratio cameras
        for ( const auto &entity: mRegistry.view<CameraComponent>() ) {
            CameraComponent &cameraComponent{ mRegistry.get<CameraComponent>( entity ) };
            cameraComponent.OnViewportResize( width, height );
        }
    }

    auto Scene::IsSimulating() const -> bool {
        MKT_BEGIN_PROFILER();
        return mSceneState == SceneState::eSimulating;
    }

    auto Scene::Update( core::f32 timeStep ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        ProcessPendingCommands();

        // Idle players start with silent posters; Inspector controls can advance them explicitly.
        if ( auto *videos{ video::VideoService::GetPtr() }; videos && videos->IsInitialized() ) {
            videos->Update();

            for ( auto [entity, video, tag]: mRegistry.view<VideoComponent, TagComponent>().each() ) {
                const auto *transform{ mRegistry.try_get<TransformComponent>( entity ) };
                const core::float3 position{ transform ? core::float3{ transform->GetWorldTransform()[3] } : core::float3{} };
                video.UpdatePlayback( timeStep, !IsPlaying(), tag.IsActive(), position );
            }
        }

        if ( mSceneState != SceneState::eIdle ) {
            // One scene clock keeps wind identical in Game and Scene viewports.
            mVolumeTime += glm::max( timeStep, 0.0f );
            physics::PhysicSystem::Get()->SetSimulationTarget( this );

            if ( mSceneState == SceneState::ePlaying ) {
                UpdatePlaying( timeStep );
            }
        }

        // Keyframed properties are applied after scripts and before world matrices,
        // physics synchronization, and audio. Preview uses the same evaluator as
        // Play; it never schedules a second update through AnimationSystem.
        for ( auto [handle, animation, tag]:
            mRegistry.view<KeyframeAnimationComponent, TagComponent>().each() ) {
            if ( tag.IsActive() && ( IsPlaying() || ( mSceneState == SceneState::eIdle && animation.IsPreviewing() ) ) ) {
                if ( auto *entity{ FindByHandle( handle ) } ) {
                    animation.Update( *entity, timeStep );
                }
            }
        }

        // Simulate runs physics and particles; Play also executes scripts. Publish their
        // local-transform edits before physics synchronization and spatial audio.
        // PhysicSystem owns the actual simulation step, avoiding a double step here.
        UpdateWorldTransformations();
        UpdateMeshResidency();

        if ( mPhysicsWorld ) {
            mPhysicsWorld->Synchronize();
        }

        UpdateAudioListenerAndSources( timeStep );

        // Preview water in idle as well as Play/Simulate. Renderer passes only read
        // this clock, so extra viewports never advance the simulation twice.
        for ( auto [entity, water, tag]: mRegistry.view<WaterSimulationComponent, TagComponent>().each() ) {
            if ( tag.IsActive() ) {
                water.Update( timeStep );
            }
        }

        // Once everything is synchronized, capture additional views without advancing the scene.
        auto captures{ mRegistry.view<TextureRenderComponent, TagComponent>() };

        if ( captures.begin() == captures.end() ) {
            return;
        }

        // Legacy pointer-only references are resolved once, then upgraded to scene entity IDs.
        ankerl::unordered_dense::map<const Camera *, entt::entity> cameras{};

        for ( const auto [entity, camera, transform, tag]: mRegistry.view<CameraComponent, TransformComponent, TagComponent>().each() ) {
            if ( camera.HasCamera() ) {
                cameras.emplace( &camera.GetCamera(), entity );
            }
        }

        for ( auto [entity, capture, tag]: captures.each() ) {
            if ( !tag.IsActive() || ( !capture.GetCamera() && !capture.GetCameraEntityID() ) ) {
                continue;
            }

            auto *cameraEntity{ capture.GetCameraEntityID() ? FindByID( capture.GetCameraEntityID() ) : nullptr };

            if ( !capture.GetCameraEntityID() ) {
                const auto found{ cameras.find( capture.GetCamera() ) };
                cameraEntity = found == cameras.end() ? nullptr : FindByHandle( found->second );
            }

            if ( !cameraEntity || !cameraEntity->HasComponent<CameraComponent>() ||
                 !cameraEntity->GetComponent<CameraComponent>().HasCamera() ) {
                capture.SetCamera( nullptr );
                continue;
            }

            if ( !cameraEntity->GetComponent<TagComponent>().IsActive() ) {
                continue;
            }

            const auto &camera{ cameraEntity->GetComponent<CameraComponent>() };
            capture.SetCamera( &camera.GetCamera(), cameraEntity->GetComponent<TagComponent>().GetGuid() );

            const auto *material{ mRegistry.try_get<MaterialComponent>( entity ) };
            capture.Update( *this, camera, cameraEntity->GetComponent<TransformComponent>().GetWorldTransform(),
                material ? material->GetMaterial() : material::MaterialHandle{}, timeStep );
        }
    }

    auto Scene::GetEnvironmentIntensity() const -> core::f32 {
        const auto *sky{ GetSkyLight() };
        const core::f32 intensity{ sky ? ( sky->IsEnabled() ? sky->GetIntensity() : 0.0f ) : 1.0f };
        const auto source{ GetEnvironmentLightingSource() };
        if ( source == EnvironmentLightingSource::eSceneLightsOnly ) {
            return 0.0f;
        }
        const auto environment{ source == EnvironmentLightingSource::eEnvironmentMap ? GetEnvironmentMaterial() : material::MaterialHandle{} };
        return intensity * ( environment ? environment.As<material::SkyboxMaterial>()->GetAmbientScale() : 1.0f );
    }

    auto Scene::GetSkyLight() const -> const SkyLightComponent * {
        const SkyLightComponent *selected{};
        core::u64 selectedID{};
        for ( const auto [entity, sky, tag]: mRegistry.view<SkyLightComponent, TagComponent>().each() ) {
            if ( tag.IsActive() && ( !selected || tag.GetGuid() < selectedID ) ) {
                selected = &sky;
                selectedID = tag.GetGuid();
            }
        }

        return selected;
    }

    auto Scene::SetEnvironmentLightingSource( EnvironmentLightingSource source ) -> void {
        MKT_BEGIN_PROFILER();
        mEnvironmentLightingSource = source;
    }

    auto Scene::GetEnvironmentMaterial() const -> material::MaterialHandle {
        MKT_BEGIN_PROFILER();
        if ( const auto *sky{ GetSkyLight() }; sky && sky->GetSource() == SkyLightSource::eEnvironmentMap && sky->GetEnvironmentMaterial() ) {
            return sky->GetEnvironmentMaterial();
        }

        material::MaterialHandle selected;
        core::u64 selectedID{};
        bool selectedComplete{};

        for ( auto [entity, skybox, tag]: mRegistry.view<SkyboxMaterialComponent, TagComponent>().each() ) {
            if ( !tag.IsActive() || !skybox.GetMaterial() ) {
                continue;
            }

            auto candidate{ skybox.GetMaterial().As<material::SkyboxMaterial>() };
            const bool complete{ candidate->HasRequiredTextures() };
            // Prefer a usable map, then use GUID order so save/load and ECS
            // storage reordering cannot silently pick a different environment.
            if ( !selected || ( complete && !selectedComplete ) ||
                 ( complete == selectedComplete && tag.GetGuid() < selectedID ) ) {
                selected = candidate;
                selectedID = tag.GetGuid();
                selectedComplete = complete;
            }
        }

        return selected;
    }

    auto Scene::SetupMeshComponent( Entity *entity, asset::ModelHandle model, core::i32 index ) -> void {
        MKT_BEGIN_PROFILER();

        MKT_ASSERT( index > -1, "Index must be a positive integer" );
        MKT_ASSERT( entity != nullptr, "Entity cannot bet null" );
        MKT_ASSERT( !model.IsEmpty(), "Model cannot be empty" );

        entity->AddComponent<MeshComponent>( model, index );

        asset::MeshNode &meshNode{ model->GetMeshNode( index ) };

        // Meshes have their own transformation from whatever modeling tool they were used
        // We need to apply it to the mesh local transform in order to properly place it in our world
        TransformComponent &transform{ entity->GetComponent<TransformComponent>() };
        transform.SetTransform( meshNode.GetTransform() );

        if ( !entity->HasComponent<MaterialComponent>() ) {
            auto &imported{ mImportedMaterials[model.GetPtr()] };
            imported.mModel = model;

            // Procedural meshes without a source material ID remain distinct.
            const core::u64 key{ meshNode.GetMaterialSourceIndex() == ~core::u32{} ? ( core::u64{ 1 } << 32 ) | static_cast<core::u32>( index ) : meshNode.GetMaterialSourceIndex() };
            auto &material{ imported.mMaterials[key] };

            if ( material.IsEmpty() ) {
                material = asset::AssetRegistryService::Get()->CreateMaterial( meshNode.GetProperties() );
            }

            entity->AddComponent<MaterialComponent>( material );
        }
    }

    auto Scene::FindByID( const core::u64 uniqueID ) -> Entity * {
        MKT_BEGIN_PROFILER();

        const auto it{ mEntities.find( uniqueID ) };
        return it != mEntities.end() ? it->second.get() : nullptr;
    }

    auto Scene::FindFirstByName( const eastl::string_view name ) -> Entity * {
        MKT_BEGIN_PROFILER();
        const auto it{ eastl::find_if( mEntities.begin(), mEntities.end(), [&]( auto &pair ) -> bool {
            const auto &entity{ pair.second };

            // All entities have tag component you can only create
            // them from the CreateEntity method
            const auto &tag{ entity->template GetComponent<TagComponent>() };
            return tag.GetTag() == name;
        } ) };

        return it != mEntities.end() ? it->second.get() : nullptr;
    }

    auto Scene::FindFirstByName( const eastl::string_view name ) const -> const Entity * {
        MKT_BEGIN_PROFILER();

        const auto it{ eastl::find_if( mEntities.begin(), mEntities.end(), [&]( auto &pair ) -> bool {
            const auto &entity{ pair.second };

            // All entities have tag component you can only create
            // them from the CreateEntity method
            const auto &tag{ entity->template GetComponent<TagComponent>() };
            return tag.GetTag() == name;
        } ) };

        return it != mEntities.end() ? it->second.get() : nullptr;
    }

    auto Scene::ExistsByName( const eastl::string_view name ) -> bool {
        MKT_BEGIN_PROFILER();
        return FindFirstByName( name ) != nullptr;
    }

    auto Scene::CreateEntityDefault( const EntityCreateInfo &info ) -> Entity * {
        MKT_BEGIN_PROFILER();

        if ( info.mRoot && info.mRoot->mRegistry != &mRegistry ) {
            MKT_THROW_RUNTIME_ERROR( "Entity parent belongs to another scene" );
        }

        Entity *parent{ info.mRoot && !info.mRoot->HasComponent<GroupComponent>() ? info.mRoot : nullptr };

        eastl::unique_ptr<Entity> entity{ new Entity{ mRegistry, info.mEntityType } };

        Entity *result{ entity.get() };
        const entt::entity handle{ result->Get() };

        core::u64 id{};
        bool registered{};

        try {
            if ( parent ) {
                result->AddComponent<RelationComponent>( eastl::make_optional( parent->GetComponent<TagComponent>().GetGuid() ) );
            }
            else {
                result->AddComponent<RelationComponent>();
            }

            result->AddComponent<HighlightComponent>();
            result->AddComponent<TagComponent>( info.mName );
            result->AddComponent<TransformComponent>( mikoto::core::float3{ 0.0f }, mikoto::core::float3{ 1.0f }, mikoto::core::float3{ 0.0f } );

            // TODO: improve
            do {
                id = result->GetComponent<TagComponent>().GetGuid();

                if ( mEntities.contains( id ) ) {
                    result->GetComponent<TagComponent>().GetGuidObject().Recreate();
                    id = result->GetComponent<TagComponent>().GetGuid();
                }
                else {
                    break;
                }
            } while ( true );

            result->GetComponent<TagComponent>().mScene = this;
            auto &relation{ result->GetComponent<RelationComponent>() };

            relation.mScene = this;
            relation.mEntityID = id;

            if ( info.mEntityType == EntityType::eGrouping ) {
                auto &group{ result->AddComponent<GroupComponent>() };
                group.mScene = this;
                group.mEntityID = id;
            }

            // Publish one owner before component callbacks run. A GUID collision
            // must never leak a registry handle or leave a dangling root pointer.
            const auto [it, inserted]{ mEntities.try_emplace( id ) };

            if ( !inserted ) {
                MKT_THROW_RUNTIME_ERROR( "Entity GUID collision" );
            }

            it->second = eastl::move( entity );
            registered = true;

            if ( parent ) {
                parent->GetComponent<RelationComponent>().mChildrenIDs.insert( id );

                // Newly created descendants inherit selection immediately,
                // including meshes published by deferred model creation.
                if ( mHighlightedEntities.contains( info.mRoot->GetComponent<TagComponent>().GetGuid() ) ) {
                    mHighlightedEntities.insert( id );
                    result->GetComponent<HighlightComponent>().SetHighlighted( true );
                }
            }
            else {
                mRootEntities.emplace( result );
            }

            if ( info.mRoot && !parent ) {
                ( void )SetEntityGroup( id, info.mRoot->GetComponent<TagComponent>().GetGuid() );
            }

            // Creation already maintains highlight membership incrementally.
            mTransformOrderDirty = true;
            mHierarchySignal->publish( true );
            return result;

        } catch ( ... ) {
            if ( registered && parent ) {
                parent->GetComponent<RelationComponent>().mChildrenIDs.erase( id );
            }

            if ( registered ) {
                ( void )SetEntityGroup( id, 0 );
            }

            mRootEntities.erase( result );
            mHighlightedEntities.erase( id );
            mRegistry.destroy( handle );

            if ( registered ) {
                mEntities.erase( id );
            }

            throw;
        }
    }

    auto Scene::CreateEntitySingle( const EntityCreateInfo &createInfo ) -> Entity * {
        MKT_BEGIN_PROFILER_NAMED();

        if ( !createInfo.mModel.IsEmpty() && createInfo.mModel->GetMeshNodeCount() == 0 ) {
            MKT_CORE_LOGGER_ERROR( "Cannot create entity from model with no meshes: {}", createInfo.mModel->GetPath().GetC_Str() );
            return nullptr;
        }

        Entity *result{ CreateEntityDefault( createInfo ) };

        const core::u64 guid{ result->GetComponent<TagComponent>().GetGuid() };

        try {

            if ( createInfo.mEntityType == EntityType::eLight ) {
                result->AddComponent<LightComponent>( createInfo.mLightType );
            }

            if ( createInfo.mEntityType == EntityType::eCanvas ) {
                result->AddComponent<CanvasComponent>();
            }

            if ( createInfo.mEntityType == EntityType::eWater ) {
                result->AddComponent<WaterRenderComponent>();
                result->AddComponent<WaterSimulationComponent>();
            }

            if ( createInfo.mEntityType == EntityType::eTerrain ) {
                result->AddComponent<LandscapeComponent>();
            }

            if ( createInfo.mEntityType == EntityType::eText ) {
                auto &text{ result->AddComponent<TextComponent>(
                    createInfo.mInitialContents,
                    createInfo.mTextSize,
                    createInfo.mTextSpacing,
                    createInfo.mIsWorldText ) };

                if ( !createInfo.mFont.IsEmpty() ) {
                    text.SetFont( createInfo.mFont );
                }
            }


            // in root model is not empty, we create the children for this entity each children well hold a mesh
            if ( !createInfo.mModel.IsEmpty() ) {
                result->SetType( EntityType::eMesh );

                if ( createInfo.mModel->GetMeshNodeCount() > 1 ) {
                    core::u64 animatorID{};

                    if ( createInfo.mModel->IsSkinned() ) {
                        animatorID = animation::AnimationSystem::Get()->RegisterAnimation( createInfo.mModel );
                        result->AddComponent<AnimatorComponent>( animatorID );
                    }

                    for ( core::usize index{}; index < createInfo.mModel->GetMeshNodeCount(); index++ ) {
                        AddSingleEntityWithRoot( result, createInfo.mModel, index, animatorID );
                    }
                }
                else {
                    if ( createInfo.mModel->IsSkinned() ) {
                        core::u64 animatorID{ animation::AnimationSystem::Get()->RegisterAnimation( createInfo.mModel ) };
                        result->AddComponent<AnimatorComponent>( animatorID );
                        result->AddComponent<SkinnedMeshRenderer>( animatorID );
                    }

                    SetupMeshComponent( result, createInfo.mModel, 0 );
                }
            }
        } catch ( ... ) {
            ( void )DestroyEntitySingle( guid );
            throw;
        }

        return result;
    }

    auto Scene::PushEntity( eastl::string_view name ) -> void {
        MKT_BEGIN_PROFILER();
        const EntityCreateInfo info{
            .mRoot = nullptr,
            .mName = eastl::string{ name },
        };

        PushEntity( info );
    }

    auto Scene::CreateEntity( eastl::string_view name ) -> Entity * {
        MKT_BEGIN_PROFILER();
        const EntityCreateInfo info{
            .mRoot{ nullptr },
            .mName{ name },
            .mModel{ asset::ModelHandle::CreateEmpty() }
        };

        return CreateEntity( info );
    }

    auto Scene::DestroyEntitySingle( core::u64 entityID ) -> bool {
        MKT_BEGIN_PROFILER();
        Entity *entity{ FindByID( entityID ) };

        if ( !entity ) {
            return false;
        }

        // Deleting an organizational folder dissolves it, not its contents.
        if ( entity->HasComponent<GroupComponent>() ) {
            const auto members{ entity->GetComponent<GroupComponent>().GetChildren() };
            const auto enclosingGroup{ GetEntityGroup( entityID ) };

            for ( const auto member: members ) {
                ( void )SetEntityGroup( member, enclosingGroup );
            }
        }

        ( void )SetEntityGroup( entityID, 0 );

        const auto &relation{ entity->GetComponent<RelationComponent>() };
        const auto parentID{ relation.GetParent() };
        // Children erase themselves from this relation, so traverse a snapshot.
        const auto children{ relation.GetChildren() };

        for ( const core::u64 childID: children ) {
            ( void )DestroyEntitySingle( childID );
        }

        if ( parentID ) {
            if ( auto *parent{ FindByID( *parentID ) } ) {
                parent->GetComponent<RelationComponent>().mChildrenIDs.erase( entityID );
            }
        }
        // Managed OnDestroy can still read transforms here, before any component is removed.
        if ( entity->HasComponent<ScriptComponent>() ) {
            if ( auto script{ entity->GetComponent<ScriptComponent>().GetHandle() } ) {
                script->Stop();
            }
        }

        // Removal callbacks still need the owning Entity. Dense-map iterators
        // cannot be kept across recursive erase operations; the pointer is stable.
        mRegistry.destroy( entity->Get() );
        mRootEntities.erase( entity );
        mHighlightedEntities.erase( entityID );

        if ( mHighlightRoot == entityID ) {
            mHighlightRoot = 0;
        }

        mEntities.erase( entityID );
        mTransformOrderDirty = true;
        mHierarchySignal->publish( true );
        return true;
    }

    auto Scene::ReparentEntity( const core::u64 entityID, const core::u64 parentID ) -> bool {
        auto *entity{ FindByID( entityID ) };
        auto *parent{ parentID ? FindByID( parentID ) : nullptr };

        if ( !entity || ( parentID && !parent ) ||
             ( parent && parent->HasComponent<GroupComponent>() ) || WouldCreateHierarchyCycle( entityID, parentID ) ) {
            return false;
        }

        auto &relation{ entity->GetComponent<RelationComponent>() };

        if ( relation.mParent.value_or( 0 ) == parentID ) {
            return true;
        }
        // Reject cycles before mutating either endpoint. The step limit also
        // prevents an already malformed ancestor chain from trapping the editor.
        core::usize remaining{ mEntities.size() };

        for ( auto *ancestor{ parent }; ancestor; ) {
            if ( ancestor == entity || remaining-- == 0 ) {
                return false;
            }

            const auto id{ ancestor->GetComponent<RelationComponent>().GetParent() };
            ancestor = id ? FindByID( *id ) : nullptr;
        }
        // Allocate the new membership before erasing the old one.
        if ( parent ) {
            parent->GetComponent<RelationComponent>().mChildrenIDs.insert( entityID );
        }
        else {
            mRootEntities.insert( entity );
        }

        if ( auto *oldParent{ FindByID( relation.mParent.value_or( 0 ) ) } ) {
            oldParent->GetComponent<RelationComponent>().mChildrenIDs.erase( entityID );
        }
        else if ( !relation.mParent ) {
            mRootEntities.erase( entity );
        }

        relation.mParent = parentID ? eastl::make_optional( parentID ) : eastl::nullopt;
        NotifyHierarchyChanged( true );
        return true;
    }

    auto Scene::CreateGroup( eastl::string_view name, Entity *container ) -> Entity * {
        EntityCreateInfo info{};
        info.mName = name;
        info.mRoot = container;
        info.mEntityType = EntityType::eGrouping;
        return CreateEntity( info );
    }

    auto Scene::GetEntityGroup( core::u64 entityID ) const -> core::u64 {
        const auto *entity{ FindByID( entityID ) };
        return entity ? entity->GetComponent<RelationComponent>().mGroup.value_or( 0 ) : 0;
    }

    auto Scene::GetHierarchyParent( core::u64 entityID ) const -> core::u64 {
        const auto *entity{ FindByID( entityID ) };

        if ( !entity ) {
            return 0;
        }

        const auto &relation{ entity->GetComponent<RelationComponent>() };
        return relation.mGroup.value_or( relation.mParent.value_or( 0 ) );
    }

    auto Scene::WouldCreateHierarchyCycle( core::u64 entityID, core::u64 parentID ) const -> bool {
        eastl::vector<core::u64> pending{ parentID };
        ankerl::unordered_dense::set<core::u64> visited;

        while ( !pending.empty() ) {
            const auto id{ pending.back() };
            pending.pop_back();

            if ( !id ) {
                continue;
            }

            if ( id == entityID ) {
                return true;
            }

            if ( !visited.insert( id ).second ) {
                continue;
            }

            if ( const auto *ancestor{ FindByID( id ) } ) {
                const auto &relation{ ancestor->GetComponent<RelationComponent>() };
                // Check both graphs so removing a folder cannot expose a hidden cycle.
                if ( relation.mParent ) {
                    pending.push_back( *relation.mParent );
                }

                if ( relation.mGroup ) {
                    pending.push_back( *relation.mGroup );
                }
            }
        }

        return false;
    }

    auto Scene::SetEntityGroup( core::u64 entityID, core::u64 groupID ) -> bool {
        auto *entity{ FindByID( entityID ) };
        auto *group{ groupID ? FindByID( groupID ) : nullptr };

        if ( !entity || ( groupID && ( !group || !group->HasComponent<GroupComponent>() ) ) ||
             WouldCreateHierarchyCycle( entityID, groupID ) ) {
            return false;
        }

        auto &relation{ entity->GetComponent<RelationComponent>() };

        if ( relation.mGroup.value_or( 0 ) == groupID ) {
            return true;
        }

        if ( group ) {
            group->GetComponent<GroupComponent>().mChildren.insert( entityID );
        }

        if ( auto *previous{ FindByID( relation.mGroup.value_or( 0 ) ) } ) {
            previous->GetComponent<GroupComponent>().mChildren.erase( entityID );
        }

        relation.mGroup = groupID ? eastl::make_optional( groupID ) : eastl::nullopt;
        // Organizational edits do not invalidate the transform traversal.
        mHierarchySignal->publish( true );
        return true;
    }

    auto Scene::MoveEntityInHierarchy( core::u64 entityID, core::u64 targetID ) -> bool {
        auto *entity{ FindByID( entityID ) };
        auto *target{ targetID ? FindByID( targetID ) : nullptr };

        if ( !entity || ( targetID && !target ) || WouldCreateHierarchyCycle( entityID, targetID ) ) {
            return false;
        }

        if ( target && target->HasComponent<GroupComponent>() ) {
            return SetEntityGroup( entityID, targetID );
        }
        // Un-grouping an item at the root only removes its folder membership.
        // Its independently authored transform parent remains intact.
        if ( !targetID && GetEntityGroup( entityID ) ) {
            return SetEntityGroup( entityID, 0 );
        }

        UpdateWorldTransformations();
        const auto parentWorld{ target ? target->GetComponent<TransformComponent>().GetWorldTransform() : core::float4x4{ 1.0f } };
        const auto determinant{ glm::determinant( parentWorld ) };

        if ( glm::abs( determinant ) <= 1e-6f ) {
            return false;
        }

        auto local{ glm::inverse( parentWorld ) * entity->GetComponent<TransformComponent>().GetWorldTransform() };
        local[0][3] = local[1][3] = local[2][3] = 0.0f;
        local[3][3] = 1.0f;

        if ( !ReparentEntity( entityID, targetID ) ) {
            return false;
        }

        ( void )SetEntityGroup( entityID, 0 );
        entity->GetComponent<TransformComponent>().SetTransform( local );
        return true;
    }

    auto Scene::SetLocalTransform( core::u64 entityID, const core::float4x4 &local, bool transformChildren ) -> bool {
        auto *entity{ FindByID( entityID ) };

        if ( !entity ) {
            return false;
        }

        if ( local[0][3] != 0.0f || local[1][3] != 0.0f || local[2][3] != 0.0f || local[3][3] != 1.0f ) {
            return false;
        }

        auto &transform{ entity->GetComponent<TransformComponent>() };

        if ( transform.GetTransform() == local ) {
            return true;
        }

        eastl::vector<eastl::pair<TransformComponent *, core::float4x4>> children;
        const auto &childIDs{ entity->GetComponent<RelationComponent>().GetChildren() };

        if ( !transformChildren && !childIDs.empty() ) {
            const auto determinant{ glm::determinant( local ) };

            if ( determinant == 0.0f ) {
                return false;
            }
            // Ancestor transforms cancel out, so no possibly stale world cache is needed.
            const auto compensation{ glm::inverse( local ) * transform.GetTransform() };
            children.reserve( childIDs.size() );

            for ( const auto childID: childIDs ) {
                auto *child{ FindByID( childID ) };

                if ( !child ) {
                    return false;
                }

                auto &childTransform{ child->GetComponent<TransformComponent>() };
                auto corrected{ compensation * childTransform.GetTransform() };
                corrected[0][3] = corrected[1][3] = corrected[2][3] = 0.0f;
                corrected[3][3] = 1.0f;
                children.emplace_back( &childTransform, corrected );
            }
        }
        // Validate the entire edit before committing. Keeping direct children fixed
        // also keeps their descendants fixed, without altering hierarchy membership.
        transform.SetTransform( local );

        for ( auto &[child, corrected]: children ) {
            child->SetTransform( corrected );
        }

        return true;
    }

    auto Scene::GetHierarchySignal() const -> eastl::shared_ptr<HierarchySignal> {
        return mHierarchySignal;
    }

    auto Scene::NotifyHierarchyChanged( bool structureChanged ) -> void {
        mHighlightsDirty |= structureChanged;
        mTransformOrderDirty |= structureChanged;
        mHierarchySignal->publish( structureChanged );
    }

    auto Scene::SetHighlightRoot( const core::u64 uniqueID ) -> void {
        SetHighlightRoots( uniqueID ? eastl::vector<core::u64>{ uniqueID } : eastl::vector<core::u64>{} );
    }

    auto Scene::SetHighlightRoots( const eastl::vector<core::u64> &roots ) -> void {
        MKT_BEGIN_PROFILER();
        const auto uniqueID{ roots.empty() ? 0 : roots.front() };
        if ( mHighlightRoots == roots && !mHighlightsDirty ) {
            return;
        }

        // Discover new membership while retaining shared descendants' flags;
        // only the symmetric difference of the two sets is written.
        // Scratch storage is retained between changes to avoid allocation churn.
        mNextHighlightedEntities.clear();
        mHighlightPending.clear();

        mHighlightRoots = roots;
        for ( const auto id: roots ) {
            if ( id && FindByID( id ) ) {
                mHighlightPending.push_back( id );
            }
        }
        while ( !mHighlightPending.empty() ) {
            const core::u64 id{ mHighlightPending.back() };
            mHighlightPending.pop_back();
            auto *entity{ FindByID( id ) };

            if ( !entity || !mNextHighlightedEntities.insert( id ).second ) {
                continue;
            }

            if ( mHighlightedEntities.insert( id ).second ) {
                entity->GetComponent<HighlightComponent>().SetHighlighted( true );
            }

            for ( const core::u64 child: entity->GetComponent<RelationComponent>().GetChildren() ) {
                mHighlightPending.push_back( child );
            }
        }

        for ( auto it{ mHighlightedEntities.begin() }; it != mHighlightedEntities.end(); ) {
            const core::u64 id{ *it };

            if ( !mNextHighlightedEntities.contains( id ) ) {
                if ( auto *entity{ FindByID( id ) } ) {
                    entity->GetComponent<HighlightComponent>().SetHighlighted( false );
                }

                it = mHighlightedEntities.erase( it );
            }
            else {
                ++it;
            }
        }

        mHighlightRoot = mHighlightedEntities.contains( uniqueID ) ? uniqueID : 0;
        mHighlightsDirty = false;
    }

    auto Scene::WorkerDestroyEntity( core::u64 entityID ) -> void {
        MKT_BEGIN_PROFILER();
        RemoveEntity( entityID );
    }

    auto Scene::AddSingleEntityWithRoot( Entity *root, asset::ModelHandle model, core::i32 index, core::u64 animatorID ) -> void {
        MKT_BEGIN_PROFILER();
        eastl::string name{ model->GetMeshNode( index ).GetName() };

        if ( name.empty() ) {
            name = string::Format( "{} ({})", model->GetName(), index );
        }

        const EntityCreateInfo entityCreateInfo{
            .mRoot = root,
            .mName = name.c_str()
        };

        if ( Entity * child{ CreateEntity( entityCreateInfo ) } ) {
            if ( !model.IsEmpty() && model->IsSkinned() ) {
                child->AddComponent<SkinnedMeshRenderer>( animatorID );
            }

            SetupMeshComponent( child, model, index );
        }
    }

    auto Scene::RemoveEntity( core::u64 uniqueID ) -> void {
        MKT_BEGIN_PROFILER();
        const std::lock_guard lock{ mCommandQueueMutex };
        mEntityCommands.push_back( { .mType = EntityCommand::Type::eDestroy, .mEntityID = uniqueID } );
    }

    auto Scene::FindByHandle( entt::entity handle ) -> Entity * {
        MKT_BEGIN_PROFILER();
        // Resolve through the registry's sparse set and GUID hash table instead
        // of scanning every entity for script and physics callbacks.
        if ( !mRegistry.valid( handle ) ) {
            return nullptr;
        }

        const auto *tag{ mRegistry.try_get<TagComponent>( handle ) };
        return tag ? FindByID( tag->GetGuid() ) : nullptr;
    }

    auto Scene::FindByID( const core::u64 uniqueID ) const -> const Entity * {
        MKT_BEGIN_PROFILER();
        const auto it{ mEntities.find( uniqueID ) };
        return it != mEntities.end() ? it->second.get() : nullptr;
    }

    auto Scene::ExistsByID( const core::u64 uniqueID ) -> bool {
        MKT_BEGIN_PROFILER();
        return FindByID( uniqueID ) != nullptr;
    }

    auto Scene::DuplicateEntity( Entity *other ) -> Entity * {
        MKT_BEGIN_PROFILER();

        if ( !other || other->mRegistry != &mRegistry || !other->IsValid() ) {
            return nullptr;
        }

        const auto parentID{ other->GetComponent<RelationComponent>().GetParent() };
        Entity *parent{ parentID ? FindByID( *parentID ) : nullptr };
        ankerl::unordered_dense::map<core::u64, core::u64> animatorIDs{};
        ankerl::unordered_dense::map<core::u64, core::u64> entityIDs{};
        // Capture the original closure before creating anything. A grouped member
        // may retain a transform parent that is also being cloned; temporary new
        // children must not be mistaken for additional source entities.
        ankerl::unordered_dense::set<core::u64> sourceEntities;
        eastl::vector<core::u64> pending{ other->GetComponent<TagComponent>().GetGuid() };

        while ( !pending.empty() ) {
            const auto id{ pending.back() };
            pending.pop_back();
            const auto *source{ FindByID( id ) };

            if ( !source || !sourceEntities.insert( id ).second ) {
                continue;
            }

            for ( const auto child: source->GetComponent<RelationComponent>().GetChildren() ) {
                pending.push_back( child );
            }

            if ( source->HasComponent<GroupComponent>() ) {
                for ( const auto member: source->GetComponent<GroupComponent>().GetChildren() ) {
                    pending.push_back( member );
                }
            }
        }

        auto duplicate{ [&]( auto &&self, Entity *source, Entity *root ) -> Entity * {
            const auto sourceID{ source->GetComponent<TagComponent>().GetGuid() };

            if ( const auto found{ entityIDs.find( sourceID ) }; found != entityIDs.end() ) {
                return FindByID( found->second );
            }

            EntityCreateInfo info{};
            info.mName = source->GetComponent<TagComponent>().GetTag();
            info.mRoot = root;
            info.mEntityType = source->GetType();

            Entity *result{ CreateEntityDefault( info ) };
            const core::u64 id{ result->GetComponent<TagComponent>().GetGuid() };

            try {
                entityIDs.emplace( sourceID, id );
                result->GetComponent<TransformComponent>() = source->GetComponent<TransformComponent>();
                result->GetComponent<TagComponent>().SetActive( source->GetComponent<TagComponent>().IsActive() );

                const auto copy{ [&]<typename Component>() {
                    if ( source->HasComponent<Component>() ) {
                        result->AddComponent<Component>( source->GetComponent<Component>() );
                    }
                } };

                copy.template operator()<MeshComponent>();

                if ( const auto pending{ GetModelCreationProgress( source->GetComponent<TagComponent>().GetGuid() ) } ) {
                    ResumeModelCreation( id, pending->mReference, pending->mNextMesh );
                }

                copy.template operator()<MaterialComponent>();
                copy.template operator()<LightComponent>();
                copy.template operator()<CameraComponent>();
                copy.template operator()<TextComponent>();
                copy.template operator()<CanvasComponent>();
                copy.template operator()<PostProcessMaterialComponent>();
                copy.template operator()<AudioSourceComponent>();
                copy.template operator()<VideoComponent>();
                copy.template operator()<TextureRenderComponent>();
                copy.template operator()<AudioMixerComponent>();
                copy.template operator()<AudioMixerGroupComponent>();
                copy.template operator()<AudioListenerComponent>();
                // Instance copying shares the recipe but starts with no live particles.
                copy.template operator()<ParticleSimulatorComponent>();
                copy.template operator()<CloudComponent>();
                copy.template operator()<CloudInteractionComponent>();
                copy.template operator()<FogComponent>();
                copy.template operator()<WaterRenderComponent>();
                copy.template operator()<WaterSimulationComponent>();
                copy.template operator()<LandscapeComponent>();
                copy.template operator()<KeyframeAnimationComponent>();

                if ( source->HasComponent<KeyframeAnimationComponent>() ) {
                    const auto &animation{ source->GetComponent<KeyframeAnimationComponent>() };

                    if ( animation.IsPreviewing() ) {
                        result->GetComponent<TransformComponent>().SetTransform( animation.GetRestTransform() );

                        if ( result->HasComponent<LightComponent>() ) {
                            animation.RestoreLightProperties( result->GetComponent<LightComponent>() );
                        }
                    }
                }

                if ( source->HasComponent<SkyLightComponent>() ) {
                    result->AddComponent<SkyLightComponent>( source->GetComponent<SkyLightComponent>() );
                }

                if ( source->HasComponent<SkyboxMaterialComponent>() ) {
                    auto &skybox{ result->AddComponent<SkyboxMaterialComponent>() };
                    skybox.SetMaterial( source->GetComponent<SkyboxMaterialComponent>().GetMaterial() );
                }

                if ( source->HasComponent<ScriptComponent>() ) {
                    // Rebind a fresh script instance to the duplicate, never copy the
                    // original entity's runtime script handle.
                    const auto &script{ source->GetComponent<ScriptComponent>() };
                    result->AddComponent<ScriptComponent>( script.GetFilePath(), script.GetLanguage(), script.GetClassName() )
                        .SetProperties( script.GetProperties() );
                }

                core::u64 sourceAnimator{};

                if ( source->HasComponent<AnimatorComponent>() ) {
                    sourceAnimator = source->GetComponent<AnimatorComponent>().GetAnimatorID();
                }
                else if ( source->HasComponent<SkinnedMeshRenderer>() ) {
                    sourceAnimator = source->GetComponent<SkinnedMeshRenderer>().GetAnimatorID();
                }

                if ( sourceAnimator ) {
                    auto *animations{ animation::AnimationSystem::GetPtr() };
                    auto *original{ animations->GetAnimator( sourceAnimator ) };

                    if ( original ) {
                        auto mapped{ animatorIDs.find( sourceAnimator ) };
                        core::u64 animatorID{};

                        if ( mapped != animatorIDs.end() ) {
                            animatorID = mapped->second;
                        }
                        else {
                            animatorID = animations->RegisterAnimation( original->GetModel() );

                            try {
                                result->AddComponent<AnimatorComponent>( animatorID );
                            } catch ( ... ) {
                                animations->UnregisterAnimation( animatorID );
                                throw;
                            }

                            auto *animator{ animations->GetAnimator( animatorID ) };

                            if ( !animator ) {
                                MKT_THROW_RUNTIME_ERROR( "Cannot duplicate animator" );
                            }

                            animator->GetController() = original->GetController();
                            animator->SetTransitionDuration( original->GetTransitionDuration() );
                            animator->GetAimConstraints() = original->GetAimConstraints();
                            animator->GetTwoBoneConstraints() = original->GetTwoBoneConstraints();

                            for ( const auto &[name, clip]: original->GetAnimationList() ) {
                                if ( clip.get() == original->GetCurrentAnimation() ) {
                                    animator->SetCurrentAnimation( name );
                                    break;
                                }
                            }

                            if ( original->GetControllerPlayback().IsPlaying() ) {
                                ( void )animator->PlayController();
                            }
                            else if ( original->IsPlaying() ) {
                                animator->PlayCurrentAnimation();
                            }

                            animator->EvaluatePose();
                            animatorIDs.emplace( sourceAnimator, animatorID );
                        }

                        if ( source->HasComponent<SkinnedMeshRenderer>() ) {
                            result->AddComponent<SkinnedMeshRenderer>( animatorID );
                        }
                    }
                }

                copy.template operator()<BoxColliderComponent>();
                copy.template operator()<SphereColliderComponent>();
                copy.template operator()<CapsuleColliderComponent>();
                copy.template operator()<MeshColliderComponent>();
                CopyPhysicsComponents( *source, *result );
                // Copy shapes before bodies so the default collider cannot replace them.
                copy.template operator()<RigidBodyComponent>();
                copy.template operator()<VehicleComponent>();
                copy.template operator()<VehicleWheelComponent>();
                copy.template operator()<BridgeSegmentComponent>();

                if ( result->HasComponent<VehicleComponent>() ) {
                    auto &vehicle{ result->GetComponent<VehicleComponent>() };
                    vehicle.mThrottle = vehicle.mSteer = vehicle.mBrake = vehicle.mHandBrake = 0.0f;
                }

                if ( source->HasComponent<RigidBodyComponent>() &&
                     !source->HasAnyOfComponents<BoxColliderComponent, SphereColliderComponent, CapsuleColliderComponent, MeshColliderComponent>() ) {
                    result->RemoveComponent<BoxColliderComponent>();
                }

                const auto children{ source->GetComponent<RelationComponent>().GetChildren() };

                for ( const core::u64 child: children ) {
                    if ( !sourceEntities.contains( child ) ) {
                        continue;
                    }

                    if ( auto *entity{ FindByID( child ) } ) {
                        self( self, entity, result );
                    }
                }

                if ( source->HasComponent<GroupComponent>() ) {
                    const auto members{ source->GetComponent<GroupComponent>().GetChildren() };

                    for ( const auto member: members ) {
                        if ( auto *entity{ FindByID( member ) } ) {
                            // Keep an external transform parent until all internal IDs can be remapped.
                            self( self, entity, FindByID( entity->GetComponent<RelationComponent>().GetParent().value_or( 0 ) ) );
                        }
                    }
                }

                return result;
            } catch ( ... ) {
                ( void )DestroyEntitySingle( id );
                throw;
            }
        } };

        try {
            auto *result{ duplicate( duplicate, other, parent ) };

            for ( const auto &[source, destination]: entityIDs ) {
                const auto originalParent{ FindByID( source )->GetComponent<RelationComponent>().GetParent().value_or( 0 ) };

                if ( const auto found{ entityIDs.find( originalParent ) }; found != entityIDs.end() ) {
                    ( void )ReparentEntity( destination, found->second );
                }

                auto *copy{ FindByID( destination ) };
                RemapPhysicsEntities( *copy, [&]( core::u64 id ) {
                    const auto found{ entityIDs.find( id ) };
                    return found == entityIDs.end() ? id : found->second;
                } );
                const auto remap = [&]( core::u64 &id ) {
                    if ( const auto found{ entityIDs.find( id ) }; found != entityIDs.end() ) {
                        id = found->second;
                    }
                };

                if ( copy->HasComponent<PostProcessMaterialComponent>() ) {
                    auto &component{ copy->GetComponent<PostProcessMaterialComponent>() };
                    auto cameraID{ component.GetCameraEntityID() };
                    remap( cameraID );
                    component.SetCameraEntityID( cameraID );
                }

                if ( copy->HasComponent<TextureRenderComponent>() ) {
                    auto &component{ copy->GetComponent<TextureRenderComponent>() };
                    auto cameraID{ component.GetCameraEntityID() };
                    remap( cameraID );
                    auto *cameraEntity{ FindByID( cameraID ) };

                    if ( cameraEntity && cameraEntity->HasComponent<CameraComponent>() ) {
                        const auto &camera{ cameraEntity->GetComponent<CameraComponent>() };
                        component.SetCamera( camera.HasCamera() ? &camera.GetCamera() : nullptr, cameraID );
                    }
                }

                if ( copy->HasComponent<LightComponent>() ) {
                    auto &area{ copy->GetComponent<LightComponent>().Get<renderer::AreaLight>() };
                    auto sourceID{ area.GetEmissionSourceEntityID() };
                    remap( sourceID );
                    area.SetEmissionSourceEntityID( sourceID );
                }

                if ( copy->HasComponent<AudioSourceComponent>() ) {
                    remap( copy->GetComponent<AudioSourceComponent>().mOutputID );
                }

                if ( copy->HasComponent<AudioMixerGroupComponent>() ) {
                    auto &group{ copy->GetComponent<AudioMixerGroupComponent>() };
                    remap( group.mMixerID );
                    remap( group.mOutputID );
                }
            }

            for ( const auto &[source, destination]: entityIDs ) {
                const auto originalGroup{ GetEntityGroup( source ) };

                if ( originalGroup ) {
                    const auto found{ entityIDs.find( originalGroup ) };
                    ( void )SetEntityGroup( destination, found != entityIDs.end() ? found->second : originalGroup );
                }
            }

            return result;
        } catch ( ... ) {
            for ( const auto &[source, destination]: entityIDs ) {
                ( void )DestroyEntitySingle( destination );
            }

            throw;
        }
    }


    auto Scene::CreateEntity( const EntityCreateInfo &createInfo ) -> Entity * {
        MKT_BEGIN_PROFILER();
        return CreateEntitySingle( createInfo );
    }


    auto Scene::PushEntity( const EntityCreateInfo &createInfo ) -> void {
        MKT_BEGIN_PROFILER();
        EntityCommand command{ .mCreateInfo = createInfo };

        // The caller keeps the parent alive while enqueueing. Only its ID crosses
        // the deferred boundary; resolve it again when the scene drains the queue.
        if ( createInfo.mRoot ) {
            if ( createInfo.mRoot->mRegistry != &mRegistry ) {
                MKT_THROW_RUNTIME_ERROR( "Entity parent belongs to another scene" );
            }

            command.mParentID = createInfo.mRoot->GetComponent<TagComponent>().GetGuid();
        }

        command.mCreateInfo.mRoot = nullptr;
        const std::lock_guard lock{ mCommandQueueMutex };
        mEntityCommands.push_back( eastl::move( command ) );
    }


    auto Scene::CreateEntity( Entity *root, eastl::string_view name ) -> Entity * {
        MKT_BEGIN_PROFILER();
        const EntityCreateInfo info{
            .mRoot{ root },
            .mName{ name },
            .mModel{ asset::ModelHandle::CreateEmpty() }
        };

        return CreateEntity( info );
    }


    auto Scene::UpdateStreamingView( const void *owner, const Camera &camera, bool retainDistant ) const -> void {
        MKT_BEGIN_PROFILER();

        auto view{ eastl::find_if( mStreamingViews.begin(), mStreamingViews.end(),
            [owner]( const StreamingView &candidate ) {
                return candidate.mOwner == owner;
            } ) };

        if ( view == mStreamingViews.end() ) {
            mStreamingViews.emplace_back();
            view = mStreamingViews.end() - 1;
        }

        // Keep some margin beyond the far plane to avoid dropping meshes at the
        // visible boundary. Views that retain distant geometry bypass this range.
        constexpr core::f32 rangeMargin{ 1.25f };
        const core::f32 range{ retainDistant ? std::numeric_limits<core::f32>::max() : camera.GetFarPlane() * rangeMargin };

        view->mOwner = owner;
        view->mPosition = camera.GetPosition();
        view->mRange = range;
        view->mLastSeen = std::chrono::steady_clock::now();
    }


    auto Scene::IsMeshResidencyRequired( entt::entity entity, const MeshComponent &mesh,
        const TransformComponent &transform, const TagComponent &tag ) const -> bool {
        // Without a live view, retain meshes. Physics and animation also need
        // their data independently of visibility, including inactive entities.
        if ( mStreamingViews.empty() ||
             mRegistry.any_of<MeshColliderComponent, RigidBodyComponent, SkinnedMeshRenderer>( entity ) ) {
            return true;
        }

        if ( const auto *highlight{ mRegistry.try_get<HighlightComponent>( entity ) } ) {
            if ( highlight->IsHighlighted() ) {
                return true;
            }
        }

        if ( !tag.IsActive() ) {
            return false;
        }

        // Transform the local bounds into a conservative world-space sphere.
        // Absolute basis vectors account for rotation and non-uniform scale.
        const auto &world{ transform.GetWorldTransform() };
        const auto &localExtent{ mesh.GetStreamingExtent() };
        const core::float3 center{ world * core::float4{ mesh.GetStreamingCenter(), 1.0f } };

        const core::float3 worldExtent{
            glm::abs( core::float3{ world[0] } ) * localExtent.x +
            glm::abs( core::float3{ world[1] } ) * localExtent.y +
            glm::abs( core::float3{ world[2] } ) * localExtent.z
        };
        const core::f32 radius{ glm::length( worldExtent ) };

        // A mesh stays resident if any active view needs it.
        for ( const auto &view: mStreamingViews ) {
            if ( glm::distance( center, view.mPosition ) <= view.mRange + radius ) {
                return true;
            }
        }

        return false;
    }


    auto Scene::UpdateMeshResidency() -> void {
        MKT_BEGIN_PROFILER();

        constexpr std::chrono::milliseconds updateInterval{ 250 };
        constexpr std::chrono::seconds viewTimeout{ 2 };
        const auto now{ std::chrono::steady_clock::now() };

        if ( now - mResidencyUpdate < updateInterval ) {
            return;
        }

        mResidencyUpdate = now;

        // Views refresh their timestamp when rendered. Remove views belonging
        // to closed or inactive panels before calculating the remaining demand.
        eastl::erase_if( mStreamingViews, [now, viewTimeout]( const StreamingView &view ) {
            return now - view.mLastSeen > viewTimeout;
        } );

        ankerl::unordered_dense::set<const asset::Model *> residentModels{};
        const auto meshes{ mRegistry.view<MeshComponent, TransformComponent, TagComponent>() };

        for ( auto [entity, mesh, transform, tag]: meshes.each() ) {
            const bool requiresResidency{ IsMeshResidencyRequired( entity, mesh, transform, tag ) };
            mesh.UpdateResidency( requiresResidency );

            if ( const auto model{ mesh.GetModel() } ) {
                residentModels.insert( model.GetPtr() );
            }

            // Texture residency follows the same demand as the owning mesh.
            auto *materialComponent{ mRegistry.try_get<MaterialComponent>( entity ) };

            if ( !materialComponent ) {
                continue;
            }

            if ( auto *surfaceMaterial{ dynamic_cast<material::SurfaceMaterial *>( materialComponent->GetMaterial().GetPtr() ) } ) {
                surfaceMaterial->UpdateTextureResidency( requiresResidency );
            }
        }

        // An emitting rectangle can light visible receivers while its own
        // mesh is culled, or have a material without any visible mesh at all.
        for ( auto [entity, light, tag]: mRegistry.view<LightComponent, TagComponent>().each() ) {
            if ( !light.IsTypeActive( renderer::LightType::eAreaLight ) ) {
                continue;
            }
            const auto &area{ light.Get<renderer::AreaLight>() };
            const auto *selected{ FindByID( area.GetEmissionSourceEntityID() ) };
            const auto sourceEntity{ area.GetEmissionSourceEntityID() ? ( selected ? selected->Get() : entt::null ) : entity };
            const auto *source{ mRegistry.try_get<MaterialComponent>( sourceEntity ) };
            if ( auto *surface{ source ? dynamic_cast<material::SurfaceMaterial *>( source->GetMaterial().GetPtr() ) : nullptr } ) {
                surface->UpdateTextureResidency( tag.IsActive() && area.GetShape() == renderer::AreaLightShape::eRectangle &&
                                                 area.IsMaterialEmissionEnabled() );
            }
        }

        // The material-sharing cache must not keep a model alive after its last
        // mesh unloads, otherwise the streaming system cannot evict the model.
        for ( auto entry{ mImportedMaterials.begin() }; entry != mImportedMaterials.end(); ) {
            if ( !residentModels.contains( entry->first ) ) {
                entry = mImportedMaterials.erase( entry );
            }
            else {
                ++entry;
            }
        }
    }


    auto Scene::GetModelCreationProgress( core::u64 rootID ) const -> eastl::optional<ModelCreationProgress> {
        MKT_BEGIN_PROFILER();

        // Workers only append to the incoming queue. Active commands belong to
        // the scene thread, where the inspector and serialization read them.
        const std::lock_guard lock{ mCommandQueueMutex };

        for ( const auto *commands: { &mPendingEntityCommands, &mEntityCommands } ) {
            for ( const auto &command: *commands ) {
                if ( command.mType != EntityCommand::Type::eCreate || command.mModelRootID != rootID ) {
                    continue;
                }

                auto reference{ command.mCreateInfo.mModelAsset };

                if ( !reference.GetID() && command.mCreateInfo.mModel ) {
                    reference = asset::AssetRef<asset::Model>{ command.mCreateInfo.mModel->GetPath() };
                }

                return ModelCreationProgress{
                    .mError = command.mModelRequest.GetError(),
                    .mNextMesh = command.mNextMesh,
                    .mReference = reference,
                    .mState = command.mCreateInfo.mModel ? asset::AssetState::eReady : command.mModelRequest.GetState(),
                };
            }
        }

        return eastl::nullopt;
    }


    auto Scene::ResumeModelCreation( core::u64 rootID, const asset::AssetRef<asset::Model> &reference, core::usize nextMesh ) -> void {
        MKT_BEGIN_PROFILER();

        EntityCommand command{};
        command.mCreateInfo.mModelAsset = reference;
        command.mModelRootID = rootID;
        command.mNextMesh = nextMesh;

        const std::lock_guard lock{ mCommandQueueMutex };
        mEntityCommands.push_back( eastl::move( command ) );
    }


    auto Scene::CollectEntityCommands() -> void {
        MKT_BEGIN_PROFILER();

        // Drain new work every frame, even while older imports are waiting.
        const std::lock_guard lock{ mCommandQueueMutex };

        // Appends, mEntityCommands might not be
        // empty we cannot simply swap
        for ( auto &command: mEntityCommands ) {
            mPendingEntityCommands.push_back( eastl::move( command ) );
        }

        mEntityCommands.clear();
    }


    auto Scene::ProcessDestroyCommands() -> void {
        MKT_BEGIN_PROFILER();
        const auto generation{ mCommandGeneration };

        // Deletions must not wait behind an import or its entity creation budget.
        for ( core::usize index{}; index < mPendingEntityCommands.size(); ) {
            if ( mPendingEntityCommands[index].mType != EntityCommand::Type::eDestroy ) {
                ++index;
                continue;
            }

            const auto id{ mPendingEntityCommands[index].mEntityID };
            mPendingEntityCommands.erase( mPendingEntityCommands.begin() + index );
            ( void )DestroyEntitySingle( id );

            if ( generation != mCommandGeneration ) {
                return;
            }
        }

        // Drop requests and model references for deleted roots or parents now,
        // even if this frame has no time left for creating entities.
        eastl::erase_if( mPendingEntityCommands,
            [this]( const EntityCommand &command ) {
                return command.mModelRootID ? !FindByID( command.mModelRootID ) : command.mParentID && !FindByID( command.mParentID );
            } );
    }


    auto Scene::ResolveCommandModel( EntityCommand &command ) -> asset::ModelHandle {
        MKT_BEGIN_PROFILER();

        if ( command.mCreateInfo.mModel ) {
            return command.mCreateInfo.mModel;
        }

        // A retry invalidates the registry entry. Start a new request while
        // preserving the root and any mesh children already created.
        if ( command.mModelRequest.GetState() == asset::AssetState::eUnloaded ) {
            command.mModelRequest = asset::AssetRegistryService::Get()->Request<asset::Model>( command.mCreateInfo.mModelAsset );
            command.mFailureReported = false;
        }

        if ( command.mModelRequest.GetState() == asset::AssetState::eFailed && !command.mFailureReported ) {
            MKT_CORE_LOGGER_ERROR( "Cannot stream model '{}': {}", command.mCreateInfo.mModelAsset.GetPath().GetC_Str(), command.mModelRequest.GetError() );
            command.mFailureReported = true;
        }

        return command.mModelRequest.GetIfReady();
    }


    auto Scene::ProcessCreateCommand( EntityCommand &command, core::usize &created ) -> bool {
        MKT_BEGIN_PROFILER();

        const bool expandModel{
            command.mCreateInfo.mModelAsset.GetID() ||
            ( command.mCreateInfo.mModel && command.mCreateInfo.mModel->GetMeshNodeCount() > 1 )
        };

        if ( !expandModel ) {
            auto info{ command.mCreateInfo };
            info.mRoot = command.mParentID ? FindByID( command.mParentID ) : nullptr;

            if ( !command.mParentID || info.mRoot ) {
                ( void )CreateEntitySingle( info );
                ++created;
            }

            return true;
        }

        // Publish a selectable root immediately. The command owns the loading
        // request and expansion cursor until all of its mesh children exist.
        if ( !command.mModelRootID ) {
            auto info{ command.mCreateInfo };
            info.mRoot = command.mParentID ? FindByID( command.mParentID ) : nullptr;

            if ( command.mParentID && !info.mRoot ) {
                return true;
            }

            info.mModel.Reset();
            auto *root{ CreateEntitySingle( info ) };

            command.mModelRootID = root->GetComponent<TagComponent>().GetGuid();

            ++created;
            return false;
        }

        auto *root{ FindByID( command.mModelRootID ) };

        if ( !root ) {
            return true;
        }

        const auto model{ ResolveCommandModel( command ) };

        if ( !model ) {
            return false;
        }

        root->SetType( EntityType::eMesh );

        if ( model->IsSkinned() ) {
            if ( !root->HasComponent<AnimatorComponent>() ) {
                root->AddComponent<AnimatorComponent>( animation::AnimationSystem::Get()->RegisterAnimation( model ) );
            }

            command.mAnimatorID = root->GetComponent<AnimatorComponent>().GetAnimatorID();
        }

        // Create one child per visit so ready imports share the frame budget.
        if ( command.mNextMesh < model->GetMeshNodeCount() ) {
            AddSingleEntityWithRoot( root, model, core::as<core::i32>( command.mNextMesh ), command.mAnimatorID );
            ++command.mNextMesh;
            ++created;
        }

        return command.mNextMesh >= model->GetMeshNodeCount();
    }

    auto Scene::ProcessPendingCommands() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( mProcessingEntityCommands ) {
            return;
        }

        mProcessingEntityCommands = true;
        struct Guard {
            bool &mProcessing;
            ~Guard() {
                mProcessing = false;
            }
        } guard{ mProcessingEntityCommands };

        const auto generation{ mCommandGeneration };

        CollectEntityCommands();
        ProcessDestroyCommands();

        if ( generation != mCommandGeneration ) {
            mPendingEntityCommands.clear();
            return;
        }

        constexpr core::usize maximumEntities{ 128 };
        constexpr std::chrono::milliseconds creationBudget{ 2 };
        const auto start{ std::chrono::steady_clock::now() };
        core::usize created{};

        // Visit waiting requests once, but keep advancing ready commands while
        // the shared budget permits. A pending load never blocks the next command.
        while ( !mPendingEntityCommands.empty() ) {
            const auto previousCount{ created };

            for ( core::usize index{}; index < mPendingEntityCommands.size(); ) {
                auto &command{ mPendingEntityCommands[index] };
                bool complete{};

                try {
                    complete = ProcessCreateCommand( command, created );
                } catch ( ... ) {
                    // Retire a failed command before cleanup can invoke callbacks.
                    const auto rootID{ command.mModelRootID };
                    mPendingEntityCommands.erase( mPendingEntityCommands.begin() + index );

                    if ( rootID && generation == mCommandGeneration ) {
                        ( void )DestroyEntitySingle( rootID );
                    }

                    if ( generation != mCommandGeneration ) {
                        mPendingEntityCommands.clear();
                    }

                    throw;
                }

                if ( generation != mCommandGeneration ) {
                    mPendingEntityCommands.clear();
                    return;
                }

                if ( complete ) {
                    mPendingEntityCommands.erase( mPendingEntityCommands.begin() + index );
                }
                else {
                    ++index;
                }

                if ( created >= maximumEntities || std::chrono::steady_clock::now() - start >= creationBudget ) {
                    return;
                }
            }

            if ( created == previousCount ) {
                break;
            }
        }
    }

    auto Scene::WorkerCreateEntity( const EntityCreateInfo &info ) -> void {
        MKT_BEGIN_PROFILER();
        PushEntity( info );
    }

    auto EntityCreateInfo::SetName( eastl::string_view name ) -> EntityCreateInfo & {
        MKT_BEGIN_PROFILER();
        this->mName = name;
        return *this;
    }

    auto EntityCreateInfo::SetRoot( Entity *root ) -> EntityCreateInfo & {
        MKT_BEGIN_PROFILER();
        this->mRoot = root;
        return *this;
    }

    auto EntityCreateInfo::SetModel( asset::ModelHandle modelMesh ) -> EntityCreateInfo & {
        MKT_BEGIN_PROFILER();
        this->mModel = modelMesh;
        return *this;
    }

    auto Scene::OnTransformStorageChanged( entt::registry &, entt::entity ) -> void {
        mTransformOrderDirty = true;
    }

    auto Scene::RebuildTransformOrder() -> void {
        MKT_BEGIN_PROFILER();
        mTransformEntities.clear();
        mTransformParents.clear();
        mLocalTransforms.clear();
        mWorldTransforms.clear();
        mTransformDirty.clear();

        const auto count{ mEntities.size() };
        mTransformEntities.reserve( count );
        mTransformParents.reserve( count );
        mLocalTransforms.reserve( count );
        mWorldTransforms.reserve( count );
        mTransformDirty.reserve( count );

        // The growing array is also a breadth-first queue. Hierarchy traversal
        // and GUID lookups only happen when its structure changes.
        ankerl::unordered_dense::set<entt::entity> visited{};
        visited.reserve( count );
        const auto append = [&]( Entity *entity, core::usize parent ) {
            if ( !entity || !mRegistry.valid( entity->Get() ) || !visited.insert( entity->Get() ).second ) {
                return;
            }

            mTransformEntities.push_back( entity->Get() );
            mTransformParents.push_back( parent );
            auto *transform{ mRegistry.try_get<TransformComponent>( entity->Get() ) };
            mLocalTransforms.push_back( transform ? transform->mTransform : mikoto::core::float4x4{ 1.0f } );
            mWorldTransforms.emplace_back( 1.0f );
            mTransformDirty.push_back( 1 );
        };

        for ( auto *root: mRootEntities ) {
            append( root, kNoTransformParent );
        }

        for ( core::usize index{}; index < mTransformEntities.size(); ++index ) {
            if ( const auto *relation{ mRegistry.try_get<RelationComponent>( mTransformEntities[index] ) } ) {
                for ( const auto child: relation->GetChildren() ) {
                    append( FindByID( child ), index );
                }
            }
        }

        mTransformOrderDirty = false;
    }

    auto Scene::UpdateWorldTransformations() -> void {
        MKT_BEGIN_PROFILER();
        const bool rebuilt{ mTransformOrderDirty };

        if ( rebuilt ) {
            RebuildTransformOrder();
        }

        // Gather local edits. Never cache component addresses: EnTT can move
        // components when another entity is removed or storage is sorted.
        bool anyDirty{ rebuilt };

        for ( core::usize index{}; index < mTransformEntities.size(); ++index ) {
            auto *transform{ mRegistry.try_get<TransformComponent>( mTransformEntities[index] ) };
            mTransformDirty[index] = rebuilt || ( transform && transform->mTransformDirty );
            anyDirty |= mTransformDirty[index] != 0;

            if ( transform && mTransformDirty[index] ) {
                mLocalTransforms[index] = transform->mTransform;
            }
        }

        if ( !anyDirty ) {
            return;
        }

        const auto count{ mTransformEntities.size() };
        const auto updated{ UpdateWorldTransforms(
            { mTransformParents.data(), count }, { mLocalTransforms.data(), count },
            { mWorldTransforms.data(), count }, { mTransformDirty.data(), count } ) };

        if ( updated == 0 ) {
            return;
        }

        // Preserve the existing component-facing world matrix API, publishing
        // only changed results. Publishing must not mark local data dirty again.
        for ( core::usize index{}; index < count; ++index ) {
            if ( mTransformDirty[index] ) {
                if ( auto *transform{ mRegistry.try_get<TransformComponent>( mTransformEntities[index] ) } ) {
                    if ( transform->mWorldTransform != mWorldTransforms[index] ) {
                        transform->mWorldTransform = mWorldTransforms[index];
                        transform->mWorldRevision = TransformComponent::NextWorldRevision();
                    }

                    transform->mTransformDirty = false;
                }
            }
        }
    }

    auto Scene::GetGeometryCache() const -> eastl::shared_ptr<renderer::SceneGeometryCache> & {
        return mGeometryCache;
    }
}// namespace mikoto::scene
