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


#include <cmath>
#include <span>
#include <string>
#include <variant>
#include <cassert>
#include <cstddef>
#include <filesystem>
#include <string_view>
#include <type_traits>

#include <EASTL/utility.h>
#include <EASTL/sort.h>
#include <EASTL/vector.h>
#include <EASTL/algorithm.h>
#include <EASTL/fixed_vector.h>

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/glm_element_traits.hpp>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/compatibility.hpp>

#include <ozz/base/log.h>
#include <ozz/base/maths/math_ex.h>
#include <ozz/base/containers/set.h>
#include <ozz/base/maths/simd_math.h>
#include <ozz/base/containers/vector.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/offline/raw_skeleton.h>
#include <ozz/animation/offline/tools/import2ozz.h>
#include <ozz/animation/offline/raw_animation_utils.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Logging/Logger.hh>

#include <Threading/Utility.hh>

#include <Filesystem/FileSystem.hh>
#include <Filesystem/FileService.hh>

#include <Assets/Utility.hh>
#include <Assets/GLTFImporter.hh>
#include <Assets/AssetRegistryService.hh>

#include <Material/SurfaceMaterial.hh>

#include <Animation/Utility.hh>
#include <Animation/SkinningBuilder.hh>

//---------------------------------------------------------------------------//
// Initial gltf2ozz implementation author: Alexander Dzhoganov                //
// https://github.com/guillaumeblanc/ozz-animation/pull/70                    //
//----------------------------------------------------------------------------//

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 4702 )// unreachable code
#pragma warning( disable : 4267 )// conversion from 'core::usize' to 'type'
#endif                           // _MSC_VER

#ifdef _MSC_VER
#pragma warning( pop )
#endif// _MSC_VER

namespace mikoto::asset {

    using namespace mikoto::asset;
    using namespace mikoto::core;
    using namespace mikoto::animation;
    using namespace mikoto::renderer;
    using namespace mikoto::material;
    using namespace mikoto::renderer::rhi;

    MKT_NODISCARD static auto LoadGltfDocument( const filesystem::Path& path ) -> fastgltf::Asset {
        fastgltf::Parser parser{ fastgltf::Extensions::KHR_materials_unlit |
                                 fastgltf::Extensions::KHR_materials_clearcoat |
                                 fastgltf::Extensions::KHR_materials_emissive_strength |
                                 fastgltf::Extensions::KHR_mesh_quantization |
                                 fastgltf::Extensions::KHR_materials_pbrSpecularGlossiness };
        auto data{ fastgltf::GltfDataBuffer::FromPath( path.GetPathTyped<std::filesystem::path>() ) };

        if ( data.error() != fastgltf::Error::None ) {
            MKT_THROW_RUNTIME_ERROR( string::Format( "Cannot read glTF document: {}", fastgltf::getErrorMessage( data.error() ) ) );
        }

        auto parsed{ parser.loadGltf( data.get(), path.GetDirectory().GetPathTyped<std::filesystem::path>(),
            fastgltf::Options::LoadExternalBuffers ) };

        if ( parsed.error() != fastgltf::Error::None ) {
            MKT_THROW_RUNTIME_ERROR( string::Format( "fastgltf: {}", fastgltf::getErrorMessage( parsed.error() ) ) );
        }

        return eastl::move( parsed.get() );
    }

    MKT_NODISCARD static auto GetLoadedBytes( const fastgltf::DataSource &source ) -> std::span<const std::byte> {
        return std::visit( []( const auto& data ) -> std::span<const std::byte> {
            if constexpr ( requires { data.bytes; } ) {
                return { data.bytes.data(), data.bytes.size() };
            }

            return {};
        }, source );
    }

    MKT_NODISCARD static auto GetBufferViewBytes( const fastgltf::Asset &model, core::usize index ) -> std::span<const std::byte> {
        const auto& view{ model.bufferViews.at( index ) };
        const auto bytes{ GetLoadedBytes( model.buffers.at( view.bufferIndex ).data ) };

        if ( view.byteOffset > bytes.size() || view.byteLength > bytes.size() - view.byteOffset ) {
            MKT_THROW_RUNTIME_ERROR( "glTF buffer view exceeds its loaded buffer." );
        }

        return bytes.subspan( view.byteOffset, view.byteLength );
    }

    MKT_NODISCARD static auto IsIndexComponentType( fastgltf::ComponentType type ) -> bool {
        return type == fastgltf::ComponentType::UnsignedByte ||
               type == fastgltf::ComponentType::UnsignedShort ||
               type == fastgltf::ComponentType::UnsignedInt;
    }

    // Use fastgltf's accessor readers for strides, normalization and sparse overrides.
    // https://github.com/spnda/fastgltf/blob/v1.0.0/include/fastgltf/tools.hpp
    static auto ValidateAccessor( const fastgltf::Asset& model, const fastgltf::Accessor& accessor,
        fastgltf::AccessorType type ) -> void {
        if ( accessor.type != type || fastgltf::validateAccessorBounds( model, accessor ) != fastgltf::Error::None ) {
            MKT_THROW_RUNTIME_ERROR( "Invalid glTF accessor type or bounds." );
        }

        if ( accessor.bufferViewIndex ) {
            // The upstream validator also accepts buffers whose bytes are not loaded.
            static_cast<void>( GetBufferViewBytes( model, *accessor.bufferViewIndex ) );
        }

        if ( const auto& sparse{ accessor.sparse } ) {
            // The upstream bounds check covers the base accessor. Check both sparse ranges.
            const auto fits{ [&]( core::usize view, core::usize offset, core::usize stride ) {
                const auto bytes{ GetBufferViewBytes( model, view ) };
                return stride != 0 && offset <= bytes.size() && sparse->count <= ( bytes.size() - offset ) / stride;
            } };

            if ( sparse->count > accessor.count || !IsIndexComponentType( sparse->indexComponentType ) ||
                 !fits( sparse->indicesBufferView, sparse->indicesByteOffset, fastgltf::getComponentByteSize( sparse->indexComponentType ) ) ||
                 !fits( sparse->valuesBufferView, sparse->valuesByteOffset, fastgltf::getElementByteSize( type, accessor.componentType ) ) ) {
                MKT_THROW_RUNTIME_ERROR( "Invalid glTF sparse accessor bounds." );
            }
        }
    }

    MKT_NODISCARD static auto GetLocalTransform( const fastgltf::Node &node ) -> core::float4x4 {
        if ( const auto* matrix{ std::get_if<fastgltf::math::nmat4x4>( &node.transform ) } ) {
            return glm::make_mat4( matrix->data() );
        }

        const auto& trs{ std::get<fastgltf::TRS>( node.transform ) };
        const core::float3 translation{ trs.translation[0], trs.translation[1], trs.translation[2] };
        const core::float3 scale{ trs.scale[0], trs.scale[1], trs.scale[2] };
        const core::quat rotation{ trs.rotation[3], trs.rotation[0], trs.rotation[1], trs.rotation[2] };

        return glm::translate( core::float4x4{ 1.0f }, translation ) *
               glm::mat4_cast( rotation ) *
               glm::scale( core::float4x4{ 1.0f }, scale );
    }

    MKT_NODISCARD static auto GetAnimationDuration( const fastgltf::Accessor &input ) -> core::f32 {
        if ( !input.max || input.max->size() != 1 || !input.max->isType<double>() ) {
            MKT_THROW_RUNTIME_ERROR( "Animation timestamps require one floating-point maximum value." );
        }

        return core::as<core::f32>( input.max->get<double>( 0 ) );
    }

    template<typename _VectorType>
    MKT_NODISCARD static auto FixupNames( _VectorType& _data, const char* _pretty_name,
        const char* _prefix_name ) -> bool {
        MKT_BEGIN_PROFILER();
        ozz::set<std::string> names;

        for ( core::usize i = 0; i < _data.size(); ++i ) {
            bool renamed = false;
            typename _VectorType::const_reference data = _data[i];

            std::string name( data.name.c_str() );

            // Fixes unnamed animations.
            if ( name.length() == 0 ) {
                renamed = true;
                name = _prefix_name;
                name += std::to_string( i );
            }

            // Fixes duplicated names, while it has duplicates
            for ( auto it = names.find( name ); it != names.end(); it = names.find( name ) ) {
                renamed = true;
                name += "_";
                name += std::to_string( i );
            }

            // Update names index.
            if ( !names.insert( name ).second ) {
                assert( false && "Algorithm must ensure no duplicated animation names." );
            }

            if ( renamed ) {
                ozz::log::LogV() << _pretty_name << " #" << i << " with name \""
                                 << data.name << "\" was renamed to \"" << name
                                 << "\" in order to avoid duplicates." << std::endl;

                _data[i].name = name;
            }
        }

        return true;
    }

    template<typename T>
    static auto ReadAnimationValues( const fastgltf::Asset& model, const fastgltf::Accessor& accessor ) -> eastl::vector<T> {
        using Element = 
            std::conditional_t<std::is_same_v<T, ozz::math::Quaternion>, fastgltf::math::fvec4,
            std::conditional_t<std::is_same_v<T, ozz::math::Float3>, fastgltf::math::fvec3, core::f32>>;

        ValidateAccessor( model, accessor, fastgltf::ElementTraits<Element>::type );

        eastl::vector<T> values{};
        values.reserve( accessor.count );

        fastgltf::iterateAccessor<Element>( model, accessor, [&]( const Element& value ) {
            if constexpr ( std::is_same_v<T, ozz::math::Quaternion> ) {
                values.emplace_back( value[0], value[1], value[2], value[3] );
            }
            else if constexpr ( std::is_same_v<T, ozz::math::Float3> ) {
                values.emplace_back( value[0], value[1], value[2] );
            }
            else {
                values.push_back( value );
            }
        } );

        return values;
    }

    // Samples a linear animation channel
    // There is an exact mapping between gltf and ozz keyframes so we just copy
    // everything over.
    template<typename _KeyframesType>
    MKT_NODISCARD static auto SampleLinearChannel( const fastgltf::Asset &_model,
        const fastgltf::Accessor& _output,
        const ozz::span<const float>& _timestamps,
        _KeyframesType* _keyframes ) -> bool {
        MKT_BEGIN_PROFILER();
        const core::usize gltf_keys_count = _output.count;

        if ( gltf_keys_count == 0 ) {
            _keyframes->clear();
            return true;
        }

        typedef typename _KeyframesType::value_type::Value ValueType;
        const auto values{ ReadAnimationValues<ValueType>( _model, _output ) };

        if ( _timestamps.size() != gltf_keys_count ) {
            ozz::log::Err() << "gltf format error, inconsistent number of keys."
                            << std::endl;
            return false;
        }

        _keyframes->reserve( _output.count );

        for ( core::usize i = 0; i < _output.count; ++i ) {
            const typename _KeyframesType::value_type key{ _timestamps[i], values[i] };
            _keyframes->push_back( key );
        }

        return true;
    }

    // Samples a step animation channel
    // There are twice-1 as many ozz keyframes as gltf keyframes
    template<typename _KeyframesType>
    MKT_NODISCARD static auto SampleStepChannel( const fastgltf::Asset &_model,
        const fastgltf::Accessor& _output,
        const ozz::span<const float>& _timestamps,
        _KeyframesType* _keyframes ) {
        MKT_BEGIN_PROFILER();
        const core::usize gltf_keys_count = _output.count;

        if ( gltf_keys_count == 0 ) {
            _keyframes->clear();
            return true;
        }

        typedef typename _KeyframesType::value_type::Value ValueType;
        const auto values{ ReadAnimationValues<ValueType>( _model, _output ) };

        if ( _timestamps.size() != gltf_keys_count ) {
            ozz::log::Err() << "gltf format error, inconsistent number of keys."
                            << std::endl;
            return false;
        }

        // A step is created with 2 consecutive keys. Last step is a single key.
        core::usize numKeyframes = gltf_keys_count * 2 - 1;
        _keyframes->resize( numKeyframes );

        for ( core::usize i = 0; i < _output.count; i++ ) {
            typename _KeyframesType::reference key = _keyframes->at( i * 2 );
            key.time = _timestamps[i];
            key.value = values[i];

            if ( i < _output.count - 1 ) {
                typename _KeyframesType::reference next_key = _keyframes->at( i * 2 + 1 );
                next_key.time = nexttowardf( _timestamps[i + 1], 0.f );
                next_key.value = values[i];
            }
        }

        return true;
    }

    // Samples a hermite spline in the form
    // p(t) = (2t^3 - 3t^2 + 1)p0 + (t^3 - 2t^2 + t)m0 + (-2t^3 + 3t^2)p1 + (t^3 -
    // t^2)m1 where t is a value between 0 and 1 p0 is the starting point at t = 0
    // m0 is the scaled starting tangent at t = 0
    // p1 is the ending point at t = 1
    // m1 is the scaled ending tangent at t = 1
    // p(t) is the resulting point value
    template<typename T>
    MKT_NODISCARD static auto SampleHermiteSpline( float _alpha, const T &p0, const T &m0, const T &p1,
        const T& m1 ) -> T {
        MKT_BEGIN_PROFILER();
        assert( _alpha >= 0.f && _alpha <= 1.f );

        const float t1 = _alpha;
        const float t2 = _alpha * _alpha;
        const float t3 = t2 * _alpha;

        // a = 2t^3 - 3t^2 + 1
        const float a = 2.0f * t3 - 3.0f * t2 + 1.0f;
        // b = t^3 - 2t^2 + t
        const float b = t3 - 2.0f * t2 + t1;
        // c = -2t^3 + 3t^2
        const float c = -2.0f * t3 + 3.0f * t2;
        // d = t^3 - t^2
        const float d = t3 - t2;

        // p(t) = a * p0 + b * m0 + c * p1 + d * m1
        T pt = p0 * a + m0 * b + p1 * c + m1 * d;
        return pt;
    }

    // Samples a cubic-spline channel
    // the number of keyframes is determined from the animation duration and given
    // sample rate
    template<typename _KeyframesType>
    MKT_NODISCARD static auto SampleCubicSplineChannel( const fastgltf::Asset &_model,
        const fastgltf::Accessor& _output,
        const ozz::span<const float>& _timestamps,
        float _sampling_rate, float _duration,
        _KeyframesType* _keyframes ) -> bool {
        MKT_BEGIN_PROFILER();
        ( void )_duration;

        core::usize gltf_keys_count = _output.count / 3;

        if ( gltf_keys_count == 0 ) {
            _keyframes->clear();
            return true;
        }

        typedef typename _KeyframesType::value_type::Value ValueType;
        const auto values{ ReadAnimationValues<ValueType>( _model, _output ) };

        if ( _output.count % 3 != 0 ||
             _timestamps.size() != gltf_keys_count ) {
            ozz::log::Err() << "gltf format error, inconsistent number of keys."
                            << std::endl;
            return false;
        }

        if ( gltf_keys_count == 1 ) {
            _keyframes->assign( 1, { _timestamps[0], values[1] } );
            return true;
        }

        // Iterate keyframes at _sampling_rate steps, between first and last time
        // stamps.
        ozz::animation::offline::FixedRateSamplingTime fixed_it(
            _timestamps[gltf_keys_count - 1] - _timestamps[0], _sampling_rate );
        _keyframes->resize( fixed_it.num_keys() );
        core::usize cubic_key0 = 0;

        for ( core::usize k = 0; k < fixed_it.num_keys(); ++k ) {
            const float time = fixed_it.time( k ) + _timestamps[0];

            // Creates output key.
            typename _KeyframesType::value_type key;
            key.time = time;

            // Makes sure time is in between the correct cubic keyframes.
            while ( cubic_key0 + 2 < gltf_keys_count && _timestamps[cubic_key0 + 1] < time ) {
                cubic_key0++;
            }

            assert( _timestamps[cubic_key0] <= time &&
                    time <= _timestamps[cubic_key0 + 1] );

            // Interpolate cubic key
            const float t0 = _timestamps[cubic_key0];    // keyframe before time
            const float t1 = _timestamps[cubic_key0 + 1];// keyframe after time
            const float alpha = ( time - t0 ) / ( t1 - t0 );
            const ValueType& p0 = values[cubic_key0 * 3 + 1];
            const ValueType m0 = values[cubic_key0 * 3 + 2] * ( t1 - t0 );
            const ValueType& p1 = values[( cubic_key0 + 1 ) * 3 + 1];
            const ValueType m1 = values[( cubic_key0 + 1 ) * 3] * ( t1 - t0 );
            key.value = SampleHermiteSpline( alpha, p0, m0, p1, m1 );

            // Pushes interpolated key.
            _keyframes->at( k ) = key;
        }

        return true;
    }

    template<typename _KeyframesType>
    MKT_NODISCARD static auto SampleChannel( const fastgltf::Asset &_model,
        fastgltf::AnimationInterpolation _interpolation,
        const fastgltf::Accessor& _output,
        const ozz::span<const float>& _timestamps,
        float _sampling_rate, float _duration,
        _KeyframesType* _keyframes ) -> bool {
        MKT_BEGIN_PROFILER();
        bool valid = false;

        if ( _interpolation == fastgltf::AnimationInterpolation::Linear ) {
            valid = SampleLinearChannel( _model, _output, _timestamps, _keyframes );
        }
        else if ( _interpolation == fastgltf::AnimationInterpolation::Step ) {
            valid = SampleStepChannel( _model, _output, _timestamps, _keyframes );
        }
        else if ( _interpolation == fastgltf::AnimationInterpolation::CubicSpline ) {
            valid = SampleCubicSplineChannel( _model, _output, _timestamps,
                _sampling_rate, _duration, _keyframes );
        }
        else {
            ozz::log::Err() << "Invalid or unknown interpolation type." << std::endl;
            valid = false;
        }

        // Check if sorted (increasing time, might not be stricly increasing).
        if ( valid ) {
            valid = eastl::is_sorted( _keyframes->begin(), _keyframes->end(),
                []( typename _KeyframesType::const_reference _a,
                    typename _KeyframesType::const_reference _b ) {
                return _a.time < _b.time;
            } );

            if ( !valid ) {
                ozz::log::Log()
                    << "gltf format error, keyframes are not sorted in increasing order."
                    << std::endl;
            }
        }

        // Remove keyframes with strictly equal times, keeping the first one.
        if ( valid ) {
            auto new_end = eastl::unique( _keyframes->begin(), _keyframes->end(),
                []( typename _KeyframesType::const_reference _a,
                    typename _KeyframesType::const_reference _b ) {
                return _a.time == _b.time;
            } );

            if ( new_end != _keyframes->end() ) {
                _keyframes->erase( new_end, _keyframes->end() );

                ozz::log::Log() << "gltf format error, keyframe times are not unique. "
                                   "Imported data were modified to remove keyframes at "
                                   "consecutive equivalent times."
                                << std::endl;
            }
        }

        return valid;
    }

    MKT_NODISCARD static auto CreateNodeTransform(
        const fastgltf::Node &_node,
        ozz::math::Transform *_transform ) -> bool {
        MKT_BEGIN_PROFILER();
        *_transform = ozz::math::Transform::identity();

        if ( const auto *matrix{ std::get_if<fastgltf::math::nmat4x4>( &_node.transform ) } ) {
            const ozz::math::Float4x4 affine{
                { ozz::math::simd_float4::LoadPtrU( ( *matrix )[0].data() ),
                    ozz::math::simd_float4::LoadPtrU( ( *matrix )[1].data() ),
                    ozz::math::simd_float4::LoadPtrU( ( *matrix )[2].data() ),
                    ozz::math::simd_float4::LoadPtrU( ( *matrix )[3].data() ) }
            };

            if ( !ToAffine( affine, _transform ) ) {
                ozz::log::Err() << "Failed to extract transformation from node \""
                                << _node.name << "\"." << std::endl;
                return false;
            }

            return true;
        }

        const auto &trs{ std::get<fastgltf::TRS>( _node.transform ) };
        _transform->translation = { trs.translation[0], trs.translation[1], trs.translation[2] };
        _transform->rotation = { trs.rotation[0], trs.rotation[1], trs.rotation[2], trs.rotation[3] };
        _transform->scale = { trs.scale[0], trs.scale[1], trs.scale[2] };

        return true;
    }

    MKT_NODISCARD static auto CreateTranslationRestPoseKey( const fastgltf::Node &_node ) -> 
        ozz::animation::offline::RawAnimation::TranslationKey  
    {
        MKT_BEGIN_PROFILER();
        ozz::animation::offline::RawAnimation::TranslationKey key;
        key.time = 0.0f;

        ozz::math::Transform rest;

        if ( !CreateNodeTransform( _node, &rest ) ) {
            MKT_THROW_RUNTIME_ERROR( "Invalid animation rest transform" );
        }

        key.value = rest.translation;

        return key;
    }

    MKT_NODISCARD static auto CreateRotationRestPoseKey(
        const fastgltf::Node& _node ) -> ozz::animation::offline::RawAnimation::RotationKey {
        MKT_BEGIN_PROFILER();
        ozz::animation::offline::RawAnimation::RotationKey key{};
        key.time = 0.0f;

        ozz::math::Transform rest{};

        if ( !CreateNodeTransform( _node, &rest ) ) {
            MKT_THROW_RUNTIME_ERROR( "Invalid animation rest transform" );
        }

        key.value = rest.rotation;
        return key;
    }

    MKT_NODISCARD static auto CreateScaleRestPoseKey(
        const fastgltf::Node& _node ) -> ozz::animation::offline::RawAnimation::ScaleKey {
        MKT_BEGIN_PROFILER();
        ozz::animation::offline::RawAnimation::ScaleKey key{};
        key.time = 0.0f;

        ozz::math::Transform rest{};

        if ( !CreateNodeTransform( _node, &rest ) ) {
            MKT_THROW_RUNTIME_ERROR( "Invalid animation rest transform" );
        }

        key.value = rest.scale;
        return key;
    }

    // The parsed document outlives every rig importer. Build clip indices lazily
    // so a warm Ozz cache does not pay for channel traversal or duration scans.
    class GltfAnimationIndex {
    public:
        struct Clip {
            const fastgltf::Animation* mAnimation{};
            core::f32 mDuration{ 0.001f };
            eastl::vector<eastl::fixed_vector<const fastgltf::AnimationChannel*, 3>> mChannels{};
        };

        explicit GltfAnimationIndex( const fastgltf::Asset& model )
            : mModel{ model } {
        }

        MKT_NODISCARD auto FindNode( const char* name ) -> core::usize {
            Initialize();
            const auto found{ mNodes.find( eastl::string_view{ name } ) };

            if ( found == mNodes.end() ) {
                MKT_THROW_RUNTIME_ERROR( "Animation joint has no source node." );
            }

            return found->second;
        }

        MKT_NODISCARD auto GetClip( const char *name ) -> const Clip & {
            Initialize();
            const auto found{ mAnimations.find( eastl::string_view{ name } ) };

            if ( found == mAnimations.end() ) {
                MKT_THROW_RUNTIME_ERROR( "Animation clip has no source animation." );
            }

            auto& clip{ mClips[found->second] };

            if ( !clip.mAnimation ) {
                const auto& animation{ mModel.animations[found->second] };
                clip.mChannels.resize( mModel.nodes.size() );
                // All rigs retain the complete duration, including rigs without
                // channels in this clip, so their shared clock stays aligned.
                for ( const auto& sampler: animation.samplers ) {
                    const auto& input{ mModel.accessors.at( sampler.inputAccessor ) };

                    clip.mDuration = eastl::max( clip.mDuration, GetAnimationDuration( input ) );
                }

                for ( const auto& channel: animation.channels ) {
                    if ( channel.nodeIndex && ( channel.path == fastgltf::AnimationPath::Translation ||
                                               channel.path == fastgltf::AnimationPath::Rotation ||
                                               channel.path == fastgltf::AnimationPath::Scale ) ) {
                        clip.mChannels.at( *channel.nodeIndex ).push_back( &channel );
                    }
                }

                clip.mAnimation = &animation;
            }

            return clip;
        }

    private:
        auto Initialize() -> void {
            if ( mInitialized ) {
                return;
            }

            mNodes.reserve( mModel.nodes.size() );

            for ( core::usize node{}; node < mModel.nodes.size(); ++node ) {
                mNodes.emplace( mModel.nodes[node].name.c_str(), node );
            }

            for ( core::usize clip{}; clip < mModel.animations.size(); ++clip ) {
                mAnimations.emplace( mModel.animations[clip].name.c_str(), clip );
            }

            mClips.resize( mModel.animations.size() );
            mInitialized = true;
        }

        const fastgltf::Asset& mModel;

        // Names are views into the immutable document, never temporary lookup strings.
        ankerl::unordered_dense::map<eastl::string_view, core::usize> mNodes{};
        ankerl::unordered_dense::map<eastl::string_view, core::usize> mAnimations{};
        eastl::vector<Clip> mClips{};
        bool mInitialized{};
    };

    class GltfAnimImporter : public ozz::animation::offline::OzzImporter {
    public:
        explicit GltfAnimImporter( const fastgltf::Asset& model, const eastl::vector<core::ubyte>& nodes, GltfAnimationIndex& index )
            : m_model{ model }, mIncludedNodes{ nodes }, mIndex{ index } {
        }

    private:
        bool Load( const char* ) override {
            // Ozz's file-oriented API calls Load after the glTF document has already been parsed.
            return true;
        }

        bool Import( ozz::animation::offline::RawSkeleton* _skeleton,
            const NodeType& _types ) override {
            MKT_BEGIN_PROFILER();
            ( void )_types;

            if ( m_model.scenes.empty() ) {
                ozz::log::Err() << "No scenes found." << std::endl;
                return false;
            }

            // If no default scene has been set then take the first one spec does not
            // disallow gltfs without a default scene but it makes more sense to keep
            // going instead of throwing an error here
            const auto defaultScene{ m_model.defaultScene.value_or( 0 ) };

            const fastgltf::Scene& scene = m_model.scenes[defaultScene];
            ozz::log::LogV() << "Importing from default scene #" << defaultScene
                             << " with name \"" << scene.name << "\"." << std::endl;

            if ( scene.nodeIndices.empty() ) {
                ozz::log::Err() << "Scene has no node." << std::endl;
                return false;
            }

            // Ancestors carry unit conversions and can themselves be animated.
            // Retain this rig's ancestors and rigid attachments, without pulling
            // unrelated armatures into Ozz's per-skeleton joint limit.
            eastl::vector<core::usize> roots{};

            for ( const auto root: scene.nodeIndices ) {
                if ( mIncludedNodes.at( root ) ) {
                    roots.push_back( root );
                }
            }

            // Remove nodes listed multiple times.
            eastl::sort( roots.begin(), roots.end() );
            roots.erase( eastl::unique( roots.begin(), roots.end() ), roots.end() );

            // Traverses the scene graph and record all joints starting from the roots.
            _skeleton->roots.resize( roots.size() );

            for ( core::usize i = 0; i < roots.size(); ++i ) {
                const fastgltf::Node& root_node = m_model.nodes[roots[i]];
                ozz::animation::offline::RawSkeleton::Joint& root_joint =
                    _skeleton->roots[i];

                if ( !ImportNode( root_node, &root_joint ) ) {
                    return false;
                }
            }

            if ( !_skeleton->Validate() ) {
                ozz::log::Err() << "Output skeleton failed validation. This is likely an "
                                   "implementation issue."
                                << std::endl;
                return false;
            }

            return true;
        }

        // Recursively import a node's children
        bool ImportNode( const fastgltf::Node& _node,
            ozz::animation::offline::RawSkeleton::Joint* _joint ) {
            MKT_BEGIN_PROFILER();
            // Names joint.
            _joint->name = _node.name.c_str();

            // Fills transform.
            if ( !CreateNodeTransform( _node, &_joint->transform ) ) {
                return false;
            }

            // Keep the complete ancestor chain, but omit other independent rigs.
            // Shared ancestors are evaluated independently with identical animation.
            for ( const auto child: _node.children ) {
                if ( !mIncludedNodes.at( child ) ) {
                    continue;
                }

                _joint->children.emplace_back();

                if ( !ImportNode( m_model.nodes.at( child ), &_joint->children.back() ) ) {
                    return false;
                }
            }

            return true;
        }

        // Returns all animations in the gltf document.
        AnimationNames GetAnimationNames() override {
            MKT_BEGIN_PROFILER();
            AnimationNames animNames;

            for ( core::usize i = 0; i < m_model.animations.size(); ++i ) {
                const fastgltf::Animation& animation = m_model.animations[i];
                assert( animation.name.length() != 0 );
                animNames.push_back( animation.name.c_str() );
            }

            return animNames;
        }

        bool Import( const char* _animation_name,
            const ozz::animation::Skeleton& skeleton, float _sampling_rate,
            ozz::animation::offline::RawAnimation* _animation ) override {
            MKT_BEGIN_PROFILER();

            if ( _sampling_rate == 0.0f ) {
                _sampling_rate = 30.0f;

                static bool samplingRateWarn = false;

                if ( !samplingRateWarn ) {
                    ozz::log::LogV() << "The animation sampling rate is set to 0 "
                                        "(automatic) but glTF does not carry scene frame "
                                        "rate information. Assuming a sampling rate of "
                                     << _sampling_rate << "hz." << std::endl;

                    samplingRateWarn = true;
                }
            }

            const auto& indexed{ mIndex.GetClip( _animation_name ) };
            const auto* gltf_animation{ indexed.mAnimation };
            _animation->name = gltf_animation->name.c_str();
            _animation->duration = indexed.mDuration;
            const int num_joints = skeleton.num_joints();
            _animation->tracks.resize( num_joints );

            // For each joint get all its associated channels, sample them and record
            // the samples in the joint track
            const auto& joint_names = skeleton.joint_names();

            if ( mJointNodes.empty() ) {
                for ( const auto* name: joint_names ) {
                    mJointNodes.push_back( mIndex.FindNode( name ) );
                }
            }

            for ( int i = 0; i < num_joints; i++ ) {
                const auto nodeIndex{ mJointNodes[i] };
                const auto& channels{ indexed.mChannels[nodeIndex] };
                auto& track = _animation->tracks[i];

                for ( auto& channel: channels ) {
                    auto& sampler = gltf_animation->samplers[channel->samplerIndex];

                    if ( !SampleAnimationChannel( m_model, sampler, channel->path,
                             _sampling_rate, &_animation->duration,
                             &track ) ) {
                        return false;
                    }
                }

                const fastgltf::Node* node = &m_model.nodes[nodeIndex];
                assert( node != nullptr );

                // Pads the rest pose transform for any joints which do not have an
                // associated channel for this animation
                if ( track.translations.empty() ) {
                    track.translations.push_back( CreateTranslationRestPoseKey( *node ) );
                }

                if ( track.rotations.empty() ) {
                    track.rotations.push_back( CreateRotationRestPoseKey( *node ) );
                }

                if ( track.scales.empty() ) {
                    track.scales.push_back( CreateScaleRestPoseKey( *node ) );
                }
            }

            ozz::log::LogV() << "Processed animation '" << _animation->name
                             << "' (tracks: " << _animation->tracks.size()
                             << ", duration: " << _animation->duration << "s)."
                             << std::endl;

            if ( !_animation->Validate() ) {
                ozz::log::Err() << "Animation '" << _animation->name
                                << "' failed validation." << std::endl;
                return false;
            }

            return true;
        }

        bool SampleAnimationChannel(
            const fastgltf::Asset& _model, const fastgltf::AnimationSampler& _sampler,
            fastgltf::AnimationPath _target_path, float _sampling_rate, float* _duration,
            ozz::animation::offline::RawAnimation::JointTrack* _track ) {
            MKT_BEGIN_PROFILER();
            const auto& input{ _model.accessors.at( _sampler.inputAccessor ) };
            const auto& output{ _model.accessors.at( _sampler.outputAccessor ) };
            const auto duration{ GetAnimationDuration( input ) };
            *_duration = eastl::max( *_duration, duration );

            const auto values{ ReadAnimationValues<core::f32>( _model, input ) };
            const ozz::span<const core::f32> timestamps{ values.data(), values.size() };

            if ( timestamps.empty() ) {
                return true;
            }

            // Builds keyframes.
            bool valid = false;

            if ( _target_path == fastgltf::AnimationPath::Translation ) {
                valid =
                    SampleChannel( _model, _sampler.interpolation, output, timestamps,
                        _sampling_rate, duration, &_track->translations );
            }
            else if ( _target_path == fastgltf::AnimationPath::Rotation ) {
                valid =
                    SampleChannel( _model, _sampler.interpolation, output, timestamps,
                        _sampling_rate, duration, &_track->rotations );

                if ( valid ) {
                    // Normalize quaternions.
                    for ( auto& key: _track->rotations ) {
                        key.value = ozz::math::Normalize( key.value );
                    }
                }
            }
            else if ( _target_path == fastgltf::AnimationPath::Scale ) {
                valid =
                    SampleChannel( _model, _sampler.interpolation, output, timestamps,
                        _sampling_rate, duration, &_track->scales );
            }
            else {
                assert( false && "Invalid target path" );
            }

            return valid;
        }

        // no support for user-defined tracks
        NodeProperties GetNodeProperties( const char* ) override {
            MKT_BEGIN_PROFILER();
            return NodeProperties();
        }

        bool Import( const char*, const char*, const char*, NodeProperty::Type, float,
            ozz::animation::offline::RawFloatTrack* ) override {
            MKT_BEGIN_PROFILER();
            return false;
        }

        bool Import( const char*, const char*, const char*, NodeProperty::Type, float,
            ozz::animation::offline::RawFloat2Track* ) override {
            MKT_BEGIN_PROFILER();
            return false;
        }

        bool Import( const char*, const char*, const char*, NodeProperty::Type, float,
            ozz::animation::offline::RawFloat3Track* ) override {
            MKT_BEGIN_PROFILER();
            return false;
        }

        bool Import( const char*, const char*, const char*, NodeProperty::Type, float,
            ozz::animation::offline::RawFloat4Track* ) override {
            MKT_BEGIN_PROFILER();
            return false;
        }

        const fastgltf::Asset& m_model;
        const eastl::vector<core::ubyte>& mIncludedNodes;
        GltfAnimationIndex& mIndex;
        eastl::vector<core::usize> mJointNodes{};
    };

    template<typename TVec>
    static auto LoadVertexAttribute( const fastgltf::Asset& model, const fastgltf::Accessor& accessor,
        eastl::vector<VertexDescription_Std430Alignment>& vertices,
        TVec VertexDescription_Std430Alignment::* member ) -> void {
        if ( accessor.count != vertices.size() ) {
            MKT_THROW_RUNTIME_ERROR( "Mismatched glTF vertex attribute size." );
        }

        if constexpr ( std::is_same_v<TVec, core::float4> ) {
            if ( member == &VertexDescription_Std430Alignment::mColors && accessor.type == fastgltf::AccessorType::Vec3 ) {
                ValidateAccessor( model, accessor, fastgltf::AccessorType::Vec3 );
                fastgltf::iterateAccessorWithIndex<core::float3>( model, accessor, [&]( const core::float3& color, core::usize index ) {
                    vertices[index].*member = core::float4{ color, 1.0f };
                } );
                return;
            }
        }

        ValidateAccessor( model, accessor, fastgltf::ElementTraits<TVec>::type );
        fastgltf::iterateAccessorWithIndex<TVec>( model, accessor, [&]( const TVec& value, core::usize index ) {
            vertices[index].*member = value;
        } );
    }

    static auto LoadPrimitives( const fastgltf::Asset& model, ModelDataDescription& modelData ) -> void {
        MKT_BEGIN_PROFILER();
        eastl::vector<core::ubyte> reachable( model.nodes.size(), 0 );
        const auto visit = [&]( auto&& self, core::usize index ) -> void {
            if ( reachable.at( index ) ) {
                return;
            }

            reachable[index] = true;

            for ( const auto child: model.nodes.at( index ).children ) {
                self( self, child );
            }
        };

        if ( model.scenes.empty() ) {
            return;
        }

        for ( const auto root: model.scenes.at( model.defaultScene.value_or( 0 ) ).nodeIndices ) {
            visit( visit, root );
        }

        struct PrimitiveJob {
            const fastgltf::Primitive* mPrimitive{};
            core::usize mOutput{};
        };
        eastl::vector<PrimitiveJob> jobs;
        eastl::vector<eastl::vector<eastl::pair<fastgltf::optional<core::usize>, core::usize>>> canonical( model.meshes.size() );
        // Mesh+primitive+skin determines vertex data. Node transforms and joint palettes remain per instance.
        for ( core::usize nodeIndex{}; nodeIndex < model.nodes.size(); ++nodeIndex ) {
            const auto& sourceNode{ model.nodes[nodeIndex] };

            if ( !reachable[nodeIndex] || !sourceNode.meshIndex ) {
                continue;
            }

            const auto& mesh{ model.meshes.at( *sourceNode.meshIndex ) };
            auto& variants{ canonical[*sourceNode.meshIndex] };
            auto found{ eastl::find_if( variants.begin(), variants.end(), [&]( const auto& entry ) {
                return entry.first == sourceNode.skinIndex;
            } ) };
            const bool first{ found == variants.end() };
            const core::usize base{ first ? modelData.mMeshNodes.size() : found->second };

            if ( first ) {
                variants.emplace_back( sourceNode.skinIndex, base );
            }

            for ( core::usize primitiveIndex{}; primitiveIndex < mesh.primitives.size(); ++primitiveIndex ) {
                const auto& primitive{ mesh.primitives[primitiveIndex] };
                const core::usize output{ modelData.mMeshNodes.size() };
                auto& node{ modelData.mMeshNodes.emplace_back() };
                node.mName = mesh.name.c_str();
                node.mSourceNodeIndex = core::as<core::i32>( nodeIndex );
                node.mGeometrySourceIndex = core::as<core::i32>( base + primitiveIndex );
                node.MaterialIndex = core::as<core::u32>( primitive.materialIndex.value_or( model.materials.size() ) );

                if ( first ) {
                    jobs.push_back( { &primitive, output } );
                }
            }
        }

        RunImportJobs( jobs.size(), [&]( core::usize jobIndex ) {
            const auto& primitive{ *jobs[jobIndex].mPrimitive };
            auto& node{ modelData.mMeshNodes[jobs[jobIndex].mOutput] };
            const auto position{ primitive.findAttribute( "POSITION" ) };

            if ( position == primitive.attributes.end() ) {
                MKT_THROW_RUNTIME_ERROR( "glTF primitive is missing POSITION." );
            }

            const auto& posAccessor{ model.accessors.at( position->accessorIndex ) };
            const core::usize vertexCount{ posAccessor.count };
            node.mVertices.resize( vertexCount );
            LoadVertexAttribute( model, posAccessor, node.mVertices, &VertexDescription_Std430Alignment::mPosition );

            const auto load{ [&]( std::string_view name, auto member ) {
                const auto attribute{ primitive.findAttribute( name ) };

                if ( attribute != primitive.attributes.end() ) {
                    LoadVertexAttribute( model, model.accessors.at( attribute->accessorIndex ), node.mVertices, member );
                }
            } };

            load( "NORMAL", &VertexDescription_Std430Alignment::mNormals );
            load( "COLOR_0", &VertexDescription_Std430Alignment::mColors );
            load( "TEXCOORD_0", &VertexDescription_Std430Alignment::mUv0 );
            load( "TEXCOORD_1", &VertexDescription_Std430Alignment::mUv1 );
            load( "JOINTS_0", &VertexDescription_Std430Alignment::mJoints0 );
            load( "JOINTS_1", &VertexDescription_Std430Alignment::mJoints1 );
            load( "WEIGHTS_0", &VertexDescription_Std430Alignment::mWeights0 );
            load( "WEIGHTS_1", &VertexDescription_Std430Alignment::mWeights1 );

            if ( primitive.indicesAccessor ) {
                const auto& accessor{ model.accessors.at( *primitive.indicesAccessor ) };
                ValidateAccessor( model, accessor, fastgltf::AccessorType::Scalar );

                if ( !IsIndexComponentType( accessor.componentType ) ) {
                    MKT_THROW_RUNTIME_ERROR( "Invalid glTF index component type." );
                }

                node.mIndices.resize( accessor.count );
                fastgltf::copyFromAccessor<core::u32>( model, accessor, node.mIndices.data() );
            }
            else {
                node.mIndices.resize( vertexCount );

                for ( core::usize i = 0; i < vertexCount; ++i ) {
                    node.mIndices[i] = core::as<core::u32>( i );
                }
            }
        } );
    }

    static auto ResolveImagePaths( const fastgltf::Asset& model, bool extractTextures ) -> eastl::vector<filesystem::Path> {
        eastl::vector<filesystem::Path> paths( model.images.size() );

        for ( core::usize i{}; i < model.images.size(); ++i ) {
            const auto& image{ model.images[i] };

            if ( const auto* uri{ std::get_if<fastgltf::sources::URI>( &image.data ) } ) {
                const auto path{ uri->uri->path() };
                paths[i] = filesystem::Path{ eastl::string_view{ path.data(), path.size() } };
                continue;
            }

            if ( !extractTextures ) {
                continue;
            }

            auto bytes{ GetLoadedBytes( image.data ) };

            if ( const auto* view{ std::get_if<fastgltf::sources::BufferView>( &image.data ) } ) {
                bytes = GetBufferViewBytes( model, view->bufferViewIndex );
            }

            if ( bytes.empty() ) {
                continue;
            }

            // Reuse the image loader and texture cache for embedded images.
            const auto hash{ animation::HashAnimationData( 0, bytes.data(), bytes.size() ) };
            const filesystem::Path extracted{ filesystem::PathBuilder{}
                .SetPath( AssetRegistryService::Get()->GetAssetCacheBasePath() )
                .SetPath( string::Format( "embedded/{:016x}.image", hash ) )
                .Build() };

            if ( !extracted.IsFile() ) {
                auto file{ filesystem::FileService::Get()->CreateNewFile( extracted ) };

                if ( file.IsEmpty() ) {
                    MKT_THROW_RUNTIME_ERROR( "Cannot open embedded texture file for writing." );
                }

                file->Write( bytes.data(), bytes.size() );
            }

            paths[i] = extracted.GetAbsolute();
        }

        return paths;
    }

    static auto LoadMaterials( const fastgltf::Asset& model, const eastl::vector<filesystem::Path>& imagePaths,
        ModelDataDescription& modelData ) -> void {
        MKT_BEGIN_PROFILER();
        modelData.mMaterials.reserve( model.materials.size() );

        for ( const auto& gltfMaterial: model.materials ) {
            material::SurfaceMaterialDescription props{
                .mName = gltfMaterial.name.c_str(),
                .mIsDoubleSided = gltfMaterial.doubleSided,
            };

            const auto bindTexture{ [&]( const fastgltf::TextureInfo& info, material::MapType type ) {
                const auto& texture{ model.textures.at( info.textureIndex ) };

                if ( texture.imageIndex && !imagePaths.at( *texture.imageIndex ).IsEmpty() ) {
                    props.mTextureBindings.emplace_back( imagePaths[*texture.imageIndex], PBRMap{ .mMapType = type } );
                }
            } };

            const auto& pbr{ gltfMaterial.pbrData };
            props.BaseColorFactor = { pbr.baseColorFactor[0], pbr.baseColorFactor[1], pbr.baseColorFactor[2], pbr.baseColorFactor[3] };
            props.MetallicFactor = pbr.metallicFactor;
            props.RoughnessFactor = pbr.roughnessFactor;

            if ( const auto& info{ pbr.baseColorTexture } ) {
                props.BaseColorTextureSet = core::as<core::i32>( info->texCoordIndex );
                bindTexture( *info, material::MapType::eBaseColor );
            }

            if ( const auto& info{ pbr.metallicRoughnessTexture } ) {
                props.MetallicRoughnessTextureSet = core::as<core::i32>( info->texCoordIndex );
                bindTexture( *info, material::MapType::eMetallicRoughness );
            }

            if ( const auto& info{ gltfMaterial.normalTexture } ) {
                props.NormalTextureSet = core::as<core::i32>( info->texCoordIndex );
                props.NormalScale = info->scale;
                bindTexture( *info, material::MapType::eNormal );
            }

            if ( const auto& coat{ gltfMaterial.clearcoat } ) {
                props.mShadingModel = material::ShadingModel::eClearCoat;
                props.mSurfaceShading.mCoatWeight = coat->clearcoatFactor;
                props.mSurfaceShading.mCoatRoughness = coat->clearcoatRoughnessFactor;

                if ( const auto& info{ coat->clearcoatNormalTexture } ) {
                    props.ClearCoatNormalTextureSet = core::as<core::i32>( info->texCoordIndex );
                    props.ClearCoatNormalScale = info->scale;
                    bindTexture( *info, material::MapType::eClearCoatNormal );
                }
            }

            if ( const auto& info{ gltfMaterial.occlusionTexture } ) {
                props.OcclusionTextureSet = core::as<core::i32>( info->texCoordIndex );
                props.OcclusionStrength = info->strength;
                bindTexture( *info, material::MapType::eAmbientOcclusion );
            }

            if ( const auto& info{ gltfMaterial.emissiveTexture } ) {
                props.EmissiveTextureSet = core::as<core::i32>( info->texCoordIndex );
                bindTexture( *info, material::MapType::eEmissive );
            }

            props.EmissiveFactor = { gltfMaterial.emissiveFactor[0], gltfMaterial.emissiveFactor[1], gltfMaterial.emissiveFactor[2] };
            props.EmissiveStrength = gltfMaterial.emissiveStrength;
            props.Unlit = gltfMaterial.unlit;

            if ( gltfMaterial.alphaMode == fastgltf::AlphaMode::Blend ) {
                props.mAlphaMask = material::AlphaMode::eBlend;
            }
            else if ( gltfMaterial.alphaMode == fastgltf::AlphaMode::Mask ) {
                props.mAlphaMask = material::AlphaMode::eMask;
            }

            props.AlphaMaskCutoff = gltfMaterial.alphaCutoff;

            if ( const auto& specular{ gltfMaterial.specularGlossiness } ) {
                if ( const auto& info{ specular->specularGlossinessTexture } ) {
                    props.SpecularGlossinessSet = core::as<core::i32>( info->texCoordIndex );
                    props.mWorkflow = material::Workflow::eSpecularGlossiness;
                    bindTexture( *info, material::MapType::eSpecularGlossiness );
                }

                if ( const auto& info{ specular->diffuseTexture } ) {
                    bindTexture( *info, material::MapType::eDiffuse );
                }
            }

            modelData.mMaterials.emplace_back( eastl::move( props ) );
        }
    }

    static auto LoadInverseBindMatrices( const fastgltf::Asset& model, const fastgltf::Skin& skin ) -> eastl::vector<core::float4x4> {
        MKT_BEGIN_PROFILER();
        eastl::vector<core::float4x4> inverseBindMats( skin.joints.size(), core::float4x4{ 1.0f } );

        if ( !skin.inverseBindMatrices ) {
            return inverseBindMats;
        }

        const auto& accessor{ model.accessors.at( *skin.inverseBindMatrices ) };
        ValidateAccessor( model, accessor, fastgltf::AccessorType::Mat4 );

        if ( accessor.componentType != fastgltf::ComponentType::Float || accessor.count < skin.joints.size() ) {
            MKT_THROW_RUNTIME_ERROR( "Invalid glTF inverse bind matrix accessor." );
        }

        fastgltf::iterateAccessorWithIndex<core::float4x4>( model, accessor, [&]( const core::float4x4& matrix, core::usize index ) {
            if ( index < inverseBindMats.size() ) {
                inverseBindMats[index] = matrix;
            }
        } );

        return inverseBindMats;
    }

    struct GltfRigPartition {
        eastl::vector<core::u32> mRoots{};
        eastl::vector<eastl::vector<core::ubyte>> mIncludedNodes{};
    };

    static auto PartitionGltfRigs( const fastgltf::Asset& model, ModelDataDescription& out ) -> GltfRigPartition {
        GltfRigPartition result{};
        // Join skins only when they actually share bones. Merely sharing a
        // scene root is not evidence of a shared rig (Lola has four such rigs).
        eastl::vector<core::u32> skinGroups( model.skins.size() );
        eastl::vector<core::ubyte> usedSkins( model.skins.size() );

        for ( const auto& mesh: out.mMeshNodes ) {
            const auto skin{ model.nodes.at( mesh.mSourceNodeIndex ).skinIndex };

            if ( skin ) {
                usedSkins.at( *skin ) = 1;
            }
        }

        eastl::vector<core::i32> jointOwner( model.nodes.size(), -1 );
        eastl::vector<core::i32> parents( model.nodes.size(), -1 );

        for ( core::u32 i{}; i < model.nodes.size(); ++i ) {
            for ( const auto child: model.nodes[i].children ) {
                parents.at( child ) = static_cast<core::i32>( i );
            }
        }

        for ( core::u32 i{}; i < skinGroups.size(); ++i ) {
            skinGroups[i] = i;
        }

        const auto root{ [&]( core::u32 group ) {
            while ( skinGroups[group] != group ) {
                group = skinGroups[group];
            }

            return group;
        } };

        for ( core::u32 i{}; i < model.skins.size(); ++i ) {
            if ( !usedSkins[i] ) {
                continue;
            }

            for ( const auto joint: model.skins[i].joints ) {
                auto& owner{ jointOwner.at( joint ) };

                if ( owner >= 0 ) {
                    skinGroups[root( i )] = root( static_cast<core::u32>( owner ) );
                }
                else {
                    owner = static_cast<core::i32>( i );
                }
            }
        }

        auto& roots{ result.mRoots };
        eastl::vector<core::u32> skinRigs( model.skins.size() );

        for ( core::u32 i{}; i < model.skins.size(); ++i ) {
            if ( !usedSkins[i] ) {
                continue;
            }

            const core::u32 group{ root( i ) };
            auto found{ eastl::find( roots.begin(), roots.end(), group ) };

            if ( found == roots.end() ) {
                roots.push_back( group );
                skinRigs[i] = static_cast<core::u32>( roots.size() - 1 );
            }
            else {
                skinRigs[i] = static_cast<core::u32>( found - roots.begin() );
            }
        }

        const core::u32 rigCount{ eastl::max( 1u, static_cast<core::u32>( roots.size() ) ) };
        auto& included{ result.mIncludedNodes };
        included.resize( rigCount, eastl::vector<core::ubyte>( model.nodes.size() ) );
        const auto includeAncestors{ [&]( core::u32 rig, core::i32 node ) {
            core::usize steps{};

            while ( node >= 0 ) {
                if ( ++steps > model.nodes.size() ) {
                    MKT_THROW_RUNTIME_ERROR( "glTF hierarchy contains a cycle." );
                }

                included[rig].at( node ) = 1;
                node = parents.at( node );
            }
        } };

        for ( core::u32 i{}; i < model.skins.size(); ++i ) {
            if ( !usedSkins[i] ) {
                continue;
            }

            for ( const auto joint: model.skins[i].joints ) {
                includeAncestors( skinRigs[i], core::as<core::i32>( joint ) );
            }
        }

        for ( auto& mesh: out.mMeshNodes ) {
            const auto& node{ model.nodes.at( mesh.mSourceNodeIndex ) };
            core::u32 rig{};

            if ( node.skinIndex ) {
                rig = skinRigs.at( *node.skinIndex );
            }
            else {
                // Rigid attachments belong to the nearest ancestor bone's rig.
                core::usize steps{};

                for ( core::i32 ancestor{ mesh.mSourceNodeIndex }; ancestor >= 0; ancestor = parents.at( ancestor ) ) {
                    if ( ++steps > model.nodes.size() ) {
                        MKT_THROW_RUNTIME_ERROR( "glTF hierarchy contains a cycle." );
                    }

                    if ( jointOwner.at( ancestor ) >= 0 ) {
                        rig = skinRigs[jointOwner[ancestor]];
                        break;
                    }
                }
            }

            mesh.mSkinBinding.mRigIndex = rig;
            includeAncestors( rig, mesh.mSourceNodeIndex );
        }

        if ( model.skins.empty() ) {
            // Preserve rigid-only animation and existing single-scene behavior.
            eastl::fill( included[0].begin(), included[0].end(), 1 );
        }

        return result;
    }

    static auto LoadSkeletonAndAnimations( const fastgltf::Asset& model, const ModelLoadDescription& description, ModelDataDescription& out ) -> void {
        MKT_BEGIN_PROFILER();
        // A skin can be posed without any authored animation clips. Import its
        // skeleton so rest-pose matrices, inverse binds and IK remain available.
        if ( model.animations.empty() && model.skins.empty() ) {
            return;
        }

        // Buffers are already loaded for geometry. Hash their contents so edits
        // to external .bin files invalidate animation caches without reparsing.
        // Version the cache because accessor decoding now uses fastgltf directly.
        core::u64 animationFingerprint{ 2 };

        for ( const auto& buffer: model.buffers ) {
            const auto bytes{ GetLoadedBytes( buffer.data ) };
            animationFingerprint = animation::HashAnimationData( animationFingerprint, bytes.data(), bytes.size() );
        }

        const auto source{ animation::SkinningBuilder::ReadSource( description.mFile->GetPath(), animationFingerprint ) };
        GltfAnimationIndex animationIndex{ model };
        auto partition{ PartitionGltfRigs( model, out ) };
        auto& included{ partition.mIncludedNodes };
        const auto& roots{ partition.mRoots };
        const core::u32 rigCount{ static_cast<core::u32>( included.size() ) };

        for ( core::u32 rig{}; rig < rigCount; ++rig ) {
            GltfAnimImporter importer{ model, included[rig], animationIndex };
            // Membership is part of the cache identity: distinct rigs must never
            // reuse an archive just because they originate in the same file.
            core::u64 fingerprint{ animation::HashAnimationData( 0, included[rig].data(), included[rig].size() ) };
            auto skinningData{ animation::SkinningBuilder{}
                .SetImporter( importer )
                .SetImportAnimations( !model.animations.empty() )
                .SetSourceFingerprint( fingerprint )
                .SetSource( source )
                .SetPath( description.mFile->GetPath() )
                .Build() };

            if ( !skinningData->mSkeleton || !skinningData->mSkeleton->IsArmaturePresent() ) {
                MKT_THROW_RUNTIME_ERROR( string::Format( "glTF skeleton import failed for rig {}", rig ) );
            }

            eastl::string name{ string::Format( "Rig {}", rig + 1 ) };

            if ( !roots.empty() ) {
                const auto& skin{ model.skins[roots[rig]] };
                const auto rootNode{ skin.skeleton ? skin.skeleton : ( skin.joints.empty() ? fastgltf::optional<core::usize>{} : fastgltf::optional<core::usize>{ skin.joints.front() } ) };

                if ( !skin.name.empty() ) {
                    name = skin.name.c_str();
                }
                else if ( rootNode ) {
                    name = model.nodes.at( *rootNode ).name.c_str();
                }
            }

            out.mRigs.push_back( { eastl::move( name ), eastl::move( skinningData->mSkeleton ), eastl::move( skinningData->mAnimations ) } );
        }
    }

    static auto LoadHierarchyTransform( const fastgltf::Asset& model, ModelDataDescription& out ) -> void {
        eastl::vector<core::float4x4> transforms( model.nodes.size(), core::float4x4{ 1.0f } );
        eastl::vector<core::ubyte> visited( model.nodes.size() );
        const auto visit{ [&]( auto&& self, core::usize index, const core::float4x4& parent ) -> void {
            if ( visited.at( index ) ) {
                MKT_THROW_RUNTIME_ERROR( "glTF hierarchy contains a cycle or multiple parents." );
            }

            visited[index] = 1;
            const auto& node{ model.nodes.at( index ) };
            transforms[index] = parent * GetLocalTransform( node );

            for ( const auto child: node.children ) {
                self( self, child, transforms[index] );
            }
        } };

        for ( const auto root: model.scenes.at( model.defaultScene.value_or( 0 ) ).nodeIndices ) {
            visit( visit, root, core::float4x4{ 1.0f } );
        }

        for ( auto& mesh: out.mMeshNodes ) {
            mesh.mTransform = transforms.at( mesh.mSourceNodeIndex );
        }
    }

    static auto LoadMeshSkinBindings( const fastgltf::Asset& model, ModelDataDescription& out ) -> void {
        MKT_BEGIN_PROFILER();

        if ( out.mRigs.empty() ) {
            return;
        }

        eastl::vector<ankerl::unordered_dense::map<std::string_view, core::u32>> jointIndices( out.mRigs.size() );

        for ( core::usize rig{}; rig < out.mRigs.size(); ++rig ) {
            const auto* skeleton{ out.mRigs[rig].mSkeleton->GetOzzSkeleton() };

            for ( core::i32 joint{}; joint < skeleton->num_joints(); ++joint ) {
                jointIndices[rig].emplace( skeleton->joint_names()[joint], static_cast<core::u32>( joint ) );
            }
        }

        for ( auto& mesh: out.mMeshNodes ) {
            const auto& node{ model.nodes.at( mesh.mSourceNodeIndex ) };
            auto& binding{ mesh.mSkinBinding };
            const auto findJoint{ [&]( core::usize nodeIndex ) {
                const auto& name{ model.nodes.at( nodeIndex ).name };
                const auto& indices{ jointIndices.at( binding.mRigIndex ) };
                const auto found{ indices.find( name ) };

                if ( found == indices.end() ) {
                    MKT_THROW_RUNTIME_ERROR( "Skin joint missing from imported rig: " + name );
                }

                return found->second;
            } };
            InitializeSkinningSpace( mesh.mTransform, binding );

            if ( node.skinIndex ) {
                const auto& skin{ model.skins.at( *node.skinIndex ) };

                if ( skin.joints.size() > kMaxBonesPerMesh ) {
                    MKT_THROW_RUNTIME_ERROR( string::Format( "Mesh skin exceeds the supported {} joints", asset::kMaxBonesPerMesh ) );
                }

                const auto inverseBinds{ LoadInverseBindMatrices( model, skin ) };

                for ( core::usize i = 0; i < skin.joints.size(); ++i ) {
                    binding.mJoints.push_back( { findJoint( skin.joints[i] ), inverseBinds[i] } );
                }

                for ( auto& vertex: mesh.mVertices ) {
                    NormalizeSkinWeights( vertex.mJoints0, vertex.mWeights0, vertex.mJoints1, vertex.mWeights1, skin.joints.size() );
                }
            }
            else {
                // Rigid attachments still follow animation channels on their node or ancestors.
                binding.mJoints.push_back( { findJoint( mesh.mSourceNodeIndex ), core::float4x4{ 1.0f } } );

                for ( auto& vertex: mesh.mVertices ) {
                    vertex.mJoints0 = vertex.mJoints1 = core::float4{};
                    vertex.mWeights0 = { 1, 0, 0, 0 };
                    vertex.mWeights1 = {};
                }
            }
        }
    }


    GLTFImporter::GLTFImporter( renderer::rhi::IGpuDevice* device )
        : ModelImporter{ device } {
    }


    auto GLTFImporter::Import( const ModelLoadDescription& description, ModelDataDescription& out ) -> void {
        MKT_BEGIN_PROFILER();
        auto model{ LoadGltfDocument( description.mFile->GetPath() ) };

        const auto sceneIndex{ model.defaultScene.value_or( 0 ) };

        if ( model.scenes.empty() || sceneIndex >= model.scenes.size() ) {
            return;
        }

        FixupNames( model.scenes, "Scene", "scene_" );
        FixupNames( model.animations, "Animation", "animation_" );
        FixupNames( model.nodes, "Node", "node_" );// Must match the Ozz importer's name normalization.
        out.mName = model.scenes[sceneIndex].name.empty() ? description.mFile->GetName().data() : model.scenes[sceneIndex].name.c_str();

        LoadPrimitives( model, out );
        LoadMaterials( model, ResolveImagePaths( model, description.mExtractTextures ), out );
        out.mMaterials.emplace_back();// glTF permits primitives without an explicit material.

        LoadHierarchyTransform( model, out );
        LoadSkeletonAndAnimations( model, description, out );
        LoadMeshSkinBindings( model, out );
    }
}// namespace mikoto::asset
