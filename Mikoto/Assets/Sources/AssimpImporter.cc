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

#include <EASTL/array.h>
#include <EASTL/algorithm.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/numeric_limits.h>

#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/GltfMaterial.h>

#include <glm/glm.hpp>
#include <glm/gtx/common.hpp>
#include <glm/gtx/compatibility.hpp>

#include <ozz/base/maths/transform.h>
#include <ozz/animation/offline/raw_skeleton.h>
#include <ozz/animation/offline/raw_animation.h>
#include <ozz/animation/offline/tools/import2ozz.h>

#include <stb_image.h>
#include <stb_image_write.h>

#include <Core/Core.hh>
#include <Core/Exception.hh>

#include <Logging/Logger.hh>

#include <Filesystem/File.hh>
#include <Filesystem/FileService.hh>

#include <Assets/AssimpImporter.hh>
#include <Assets/AssetRegistryService.hh>

#include <Animation/Utility.hh>
#include <Animation/SkinningBuilder.hh>
#include <Animation/SkinningPalette.hh>

namespace mikoto::asset {

    using namespace core;
    using namespace filesystem;
    using namespace material;
    using namespace animation;

    // Adapts an already loaded Assimp scene to the existing Ozz cache pipeline.
    class AssimpAnimationImporter final : public ozz::animation::offline::OzzImporter {
    public:
        explicit AssimpAnimationImporter( const aiScene& scene );
        auto Load( core::cstr filename ) -> bool override;
        auto Import( ozz::animation::offline::RawSkeleton* skeleton, const NodeType& types ) -> bool override;
        auto GetAnimationNames() -> AnimationNames override;
        auto Import( core::cstr name, const ozz::animation::Skeleton& skeleton, core::f32 rate,
            ozz::animation::offline::RawAnimation* animation ) -> bool override;
        auto GetNodeProperties( core::cstr name ) -> NodeProperties override;
        auto Import( core::cstr, core::cstr, core::cstr, NodeProperty::Type, core::f32, ozz::animation::offline::RawFloatTrack* ) -> bool override;
        auto Import( core::cstr, core::cstr, core::cstr, NodeProperty::Type, core::f32, ozz::animation::offline::RawFloat2Track* ) -> bool override;
        auto Import( core::cstr, core::cstr, core::cstr, NodeProperty::Type, core::f32, ozz::animation::offline::RawFloat3Track* ) -> bool override;
        auto Import( core::cstr, core::cstr, core::cstr, NodeProperty::Type, core::f32, ozz::animation::offline::RawFloat4Track* ) -> bool override;

        auto GetNodeName( const aiNode* node ) const -> const eastl::string&;
        auto FindNode( core::cstr name ) const -> const aiNode*;

    private:
        auto RegisterNode( const aiNode* node ) -> void;
        const aiScene& mScene;
        ankerl::unordered_dense::map<const aiNode*, eastl::string> mNames{};
        ankerl::unordered_dense::map<eastl::string, const aiNode*> mSourceNodes{};
        ankerl::unordered_dense::map<eastl::string, const aiNode*> mRuntimeNodes{};
        AnimationNames mClipNames{};
    };

    static auto ValidateRotation( const aiQuaternion& rotation ) -> void {
        const core::f64 lengthSquared{ core::f64{ rotation.x } * rotation.x + core::f64{ rotation.y } * rotation.y +
                                       core::f64{ rotation.z } * rotation.z + core::f64{ rotation.w } * rotation.w };

        if ( lengthSquared <= 0 ) {
            MKT_THROW_RUNTIME_ERROR( "Assimp animation contains a zero-length quaternion." );
        }
    }

    static auto Transform( const aiMatrix4x4& matrix ) -> ozz::math::Transform {
        aiVector3D scale{}, translation{};
        aiQuaternion rotation{};
        matrix.Decompose( scale, rotation, translation );
        ValidateRotation( rotation );
        rotation.Normalize();
        return { { translation.x, translation.y, translation.z },
            { rotation.x, rotation.y, rotation.z, rotation.w },
            { scale.x, scale.y, scale.z } };
    }

    // Assimp keys are in ticks. Preserve default local TRS when a channel
    // omits a component; identity defaults would erase FBX unit scales.
    template<typename Key, typename Value, typename Interpolate>
    static auto Sample( const Key* keys, core::u32 count, core::f64 time, const Value& fallback,
        aiAnimBehaviour before, aiAnimBehaviour after, Interpolate interpolate ) -> Value {
        if ( !count ) {
            return fallback;
        }

        const bool outside{ time < keys[0].mTime || time > keys[count - 1].mTime };
        const auto behavior{ time < keys[0].mTime ? before : after };

        if ( outside && behavior == aiAnimBehaviour_DEFAULT ) {
            return fallback;
        }

        if ( count == 1 ) {
            return keys[0].mValue;
        }

        const core::f64 span{ keys[count - 1].mTime - keys[0].mTime };

        if ( outside && behavior == aiAnimBehaviour_REPEAT && span > 0 ) {
            time = keys[0].mTime + glm::fmod( glm::fmod( time - keys[0].mTime, span ) + span, span );
        }

        if ( behavior != aiAnimBehaviour_LINEAR ) {
            if ( time <= keys[0].mTime ) {
                return keys[0].mValue;
            }

            if ( time >= keys[count - 1].mTime ) {
                return keys[count - 1].mValue;
            }
        }

        core::u32 lower{};
        core::u32 upper{ count - 1 };

        while ( upper - lower > 1 ) {
            const core::u32 middle{ lower + ( upper - lower ) / 2 };

            if ( keys[middle].mTime <= time ) {
                lower = middle;
            }
            else {
                upper = middle;
            }
        }

        const core::f64 duration{ keys[upper].mTime - keys[lower].mTime };
        const core::f32 alpha{ duration > 0 ? static_cast<core::f32>( ( time - keys[lower].mTime ) / duration ) : 0.0f };
        return interpolate( keys[lower].mValue, keys[upper].mValue, alpha );
    }

    static auto Matrix( const aiMatrix4x4& m ) -> core::float4x4 {
        return { { m.a1, m.b1, m.c1, m.d1 }, { m.a2, m.b2, m.c2, m.d2 }, { m.a3, m.b3, m.c3, m.d3 }, { m.a4, m.b4, m.c4, m.d4 } };
    }

    static auto EmbeddedTexture( const aiTexture& texture, const filesystem::Path& modelPath, core::u32 index ) -> filesystem::Path {
        const auto directory{ filesystem::PathBuilder{}
            .SetPath( AssetRegistryService::Get()->GetAssetCacheBasePath() )
            .SetPath( string::Format( "{}/Assimp", GetHashedAssetID( modelPath ) ) )
            .Build() };
        const auto path{ filesystem::PathBuilder{}
            .SetPath( directory )
            .SetPath( string::Format( "embedded_{}.png", index ) )
            .Build() };

        if ( !texture.pcData || !texture.mWidth || texture.mWidth > static_cast<core::u32>( eastl::numeric_limits<core::i32>::max() ) ) {
            MKT_THROW_RUNTIME_ERROR( "Assimp embedded texture has invalid data or dimensions." );
        }

        core::i32 width{};
        core::i32 height{};
        core::i32 channels{};
        eastl::vector<core::ubyte> pixels{};

        if ( texture.mHeight == 0 ) {
            // Copy decoded pixels, not the compressed source bytes, into
            // owned storage before releasing the decoder allocation.
            eastl::unique_ptr<stbi_uc, void ( * )( void* )> decoded{
                stbi_load_from_memory( reinterpret_cast<const stbi_uc*>( texture.pcData ), static_cast<core::i32>( texture.mWidth ),
                    &width, &height, &channels, STBI_rgb_alpha ),
                stbi_image_free
            };

            if ( !decoded || width <= 0 || height <= 0 || width > eastl::numeric_limits<core::i32>::max() / 4 ) {
                MKT_THROW_RUNTIME_ERROR( "Assimp embedded texture could not be decoded." );
            }

            pixels.assign( decoded.get(), decoded.get() + core::usize{ static_cast<core::u32>( width ) } * static_cast<core::u32>( height ) * 4 );
        }
        else {
            if ( texture.mHeight > static_cast<core::u32>( eastl::numeric_limits<core::i32>::max() ) || texture.mWidth > static_cast<core::u32>( eastl::numeric_limits<core::i32>::max() / 4 ) ) {
                MKT_THROW_RUNTIME_ERROR( "Assimp embedded texture exceeds encoder dimensions." );
            }

            width = static_cast<core::i32>( texture.mWidth );
            height = static_cast<core::i32>( texture.mHeight );

            pixels.resize( core::usize{ texture.mWidth } * texture.mHeight * 4 );
            // aiTexel is BGRA in memory; explicitly export RGBA for the image loader.
            for ( core::usize pixel{}; pixel < core::usize{ texture.mWidth } * texture.mHeight; ++pixel ) {
                const auto& value{ texture.pcData[pixel] };
                pixels[pixel * 4] = value.r;
                pixels[pixel * 4 + 1] = value.g;
                pixels[pixel * 4 + 2] = value.b;
                pixels[pixel * 4 + 3] = value.a;
            }
        }

        eastl::vector<core::ubyte> png{};
        const auto append{ []( void* context, void* data, core::i32 size ) {
            auto& bytes{ *static_cast<eastl::vector<core::ubyte>*>( context ) };
            const auto* first{ static_cast<const core::ubyte*>( data ) };
            bytes.insert( bytes.end(), first, first + size );
        } };

        if ( !stbi_write_png_to_func( append, &png, width, height, 4, pixels.data(), width * 4 ) ) {
            MKT_THROW_RUNTIME_ERROR( "Assimp embedded texture PNG encoding failed." );
        }

        filesystem::FileHandle file{ filesystem::FileService::Get()->CreateNewFile( path ) };

        if ( file.IsEmpty() ) {
            MKT_THROW_RUNTIME_ERROR( "Cannot open embedded texture file for writing." );
        }

        file->Write( png.data(), png.size() );
        return filesystem::Path{ path.GetAbsolute() };
    }

    static auto ImportMaterial( const aiMaterial& source ) -> material::SurfaceMaterialDescription {
        material::SurfaceMaterialDescription result{};
        // Legacy materials are usually dielectric and non-emissive.
        result.MetallicFactor = 0.0f;
        result.EmissiveFactor = core::float3{};
        aiString name{};
        source.Get( AI_MATKEY_NAME, name );
        result.mName = name.C_Str();
        aiColor4D color{ 1, 1, 1, 1 };

        if ( source.Get( AI_MATKEY_BASE_COLOR, color ) != AI_SUCCESS ) {
            source.Get( AI_MATKEY_COLOR_DIFFUSE, color );
        }

        result.BaseColorFactor = { color.r, color.g, color.b, color.a };
        result.DiffuseFactor = result.BaseColorFactor;
        source.Get( AI_MATKEY_METALLIC_FACTOR, result.MetallicFactor );

        if ( source.GetTextureCount( aiTextureType_METALNESS ) && result.MetallicFactor == 0.0f ) {
            core::f32 authored{};

            if ( source.Get( AI_MATKEY_METALLIC_FACTOR, authored ) != AI_SUCCESS ) {
                result.MetallicFactor = 1.0f;
            }
        }

        if ( source.Get( AI_MATKEY_ROUGHNESS_FACTOR, result.RoughnessFactor ) != AI_SUCCESS ) {
            core::f32 shininess{};

            if ( source.Get( AI_MATKEY_SHININESS, shininess ) == AI_SUCCESS ) {
                result.RoughnessFactor = glm::sqrt( 2.0f / ( eastl::max( shininess, 0.0f ) + 2.0f ) );
            }
        }

        aiColor3D emission{};

        if ( source.Get( AI_MATKEY_COLOR_EMISSIVE, emission ) != AI_SUCCESS &&
             ( source.GetTextureCount( aiTextureType_EMISSIVE ) || source.GetTextureCount( aiTextureType_EMISSION_COLOR ) ) ) {
            emission = aiColor3D{ 1, 1, 1 };
        }

        result.EmissiveFactor = { emission.r, emission.g, emission.b };
        source.Get( AI_MATKEY_EMISSIVE_INTENSITY, result.EmissiveStrength );
        core::i32 twoSided{};
        source.Get( AI_MATKEY_TWOSIDED, twoSided );
        result.mIsDoubleSided = twoSided != 0;
        core::f32 opacity{ color.a };
        source.Get( AI_MATKEY_OPACITY, opacity );
        result.BaseColorFactor.a = glm::clamp( opacity, 0.0f, 1.0f );
        aiString mode{};

        if ( source.Get( AI_MATKEY_GLTF_ALPHAMODE, mode ) == AI_SUCCESS ) {
            const eastl::string_view alpha{ mode.C_Str() };
            result.mAlphaMask = alpha == "MASK" ? material::AlphaMode::eMask : alpha == "BLEND" ? material::AlphaMode::eBlend
                                                                                                : material::AlphaMode::eOpaque;
        }
        else if ( opacity < 1.0f ) {
            result.mAlphaMask = material::AlphaMode::eBlend;
        }

        result.AlphaMaskCutoff = 0.5f;
        source.Get( AI_MATKEY_GLTF_ALPHACUTOFF, result.AlphaMaskCutoff );
        return result;
    }

    struct TextureSlot {
        aiTextureType mType{};
        material::MapType mMap{};
        core::i32 material::SurfaceMaterialDescription::* mUv{};
    };

    static auto Textures( const aiScene& scene, const aiMaterial& source, const filesystem::Path& path,
        material::SurfaceMaterialDescription& target, ankerl::unordered_dense::map<const aiTexture*, filesystem::Path>& embedded ) -> void {
        // Prefer PBR names, then legacy aliases. UV index is returned by
        // GetTexture; texture-stack index and UV-channel index are unrelated.
        const eastl::array slots{
            TextureSlot{ aiTextureType_BASE_COLOR, material::MapType::eBaseColor, &material::SurfaceMaterialDescription::BaseColorTextureSet },
            TextureSlot{ aiTextureType_DIFFUSE, material::MapType::eBaseColor, &material::SurfaceMaterialDescription::BaseColorTextureSet },
            TextureSlot{ aiTextureType_NORMALS, material::MapType::eNormal, &material::SurfaceMaterialDescription::NormalTextureSet },
            TextureSlot{ aiTextureType_NORMAL_CAMERA, material::MapType::eNormal, &material::SurfaceMaterialDescription::NormalTextureSet },
            TextureSlot{ aiTextureType_EMISSION_COLOR, material::MapType::eEmissive, &material::SurfaceMaterialDescription::EmissiveTextureSet },
            TextureSlot{ aiTextureType_EMISSIVE, material::MapType::eEmissive, &material::SurfaceMaterialDescription::EmissiveTextureSet },
            TextureSlot{ aiTextureType_AMBIENT_OCCLUSION, material::MapType::eAmbientOcclusion, &material::SurfaceMaterialDescription::OcclusionTextureSet },
            TextureSlot{ aiTextureType_LIGHTMAP, material::MapType::eAmbientOcclusion, &material::SurfaceMaterialDescription::OcclusionTextureSet },
            TextureSlot{ aiTextureType_METALNESS, material::MapType::eMetallic, &material::SurfaceMaterialDescription::MetallicRoughnessTextureSet },
            TextureSlot{ aiTextureType_DIFFUSE_ROUGHNESS, material::MapType::eRoughness, &material::SurfaceMaterialDescription::MetallicRoughnessTextureSet }
        };
        ankerl::unordered_dense::set<material::MapType> loaded{};

        for ( const auto& slot: slots ) {
            if ( loaded.contains( slot.mMap ) ) {
                continue;
            }

            aiString uri{};
            core::u32 uv{};

            if ( source.GetTexture( slot.mType, 0, &uri, nullptr, &uv ) != AI_SUCCESS ) {
                continue;
            }

            if ( uv > 1 ) {
                MKT_CORE_LOGGER_WARN( "Assimp material '{}' uses unsupported UV channel {}.", target.mName, uv );
                continue;
            }

            filesystem::Path texturePath{};

            if ( const auto* texture{ scene.GetEmbeddedTexture( uri.C_Str() ) } ) {
                auto found{ embedded.find( texture ) };

                if ( found == embedded.end() ) {
                    found = embedded.emplace( texture, EmbeddedTexture( *texture, path, static_cast<core::u32>( embedded.size() ) ) ).first;
                }

                texturePath = found->second;
            }
            else {
                texturePath = filesystem::PathBuilder{}
                    .SetPath( path.GetDirectory() )
                    .SetPath( uri.C_Str() )
                    .Build();
                texturePath = filesystem::Path{ texturePath.GetAbsolute() };

                if ( !texturePath.IsFile() ) {
                    // Exporters may retain an absolute path from the author's
                    // machine. A texture beside the model is a useful fallback.
                    texturePath = filesystem::PathBuilder{}
                        .SetPath( path.GetDirectory() )
                        .SetPath( filesystem::Path{ uri.C_Str() }.GetFilename() )
                        .Build();
                    texturePath = filesystem::Path{ texturePath.GetAbsolute() };
                }

                if ( !texturePath.IsFile() ) {
                    MKT_CORE_LOGGER_WARN( "Assimp texture is missing: {}", texturePath.GetC_Str() );
                    continue;
                }
            }

            target.*( slot.mUv ) = static_cast<core::i32>( uv );
            // MeshFactory owns the single texture upload. Embedded images
            // are exported first so it can consume ordinary file paths.
            const auto existing{ eastl::find_if( target.mTextureBindings.begin(), target.mTextureBindings.end(), [&]( const auto& entry ) {
                return entry.first == texturePath &&
                       ( ( entry.second.mMapType == material::MapType::eMetallic && slot.mMap == material::MapType::eRoughness ) ||
                           ( entry.second.mMapType == material::MapType::eRoughness && slot.mMap == material::MapType::eMetallic ) );
            } ) };

            if ( existing != target.mTextureBindings.end() ) {
                existing->second.mMapType = material::MapType::eMetallicRoughness;
            }
            else {
                target.mTextureBindings.emplace_back( texturePath, PBRMap{ .mMapType = slot.mMap } );
            }

            loaded.insert( slot.mMap );
        }
    }

    static auto AddWeight( VertexDescription_Std430Alignment& vertex, core::u32 joint, core::f32 weight ) -> void {
        if ( weight <= 0 ) {
            return;
        }
        // Keep the strongest eight influences, then normalize once. Input
        // bone order must not decide which influences survive truncation.
        for ( core::u32 index{}; index < 8; ++index ) {
            auto& weights{ index < 4 ? vertex.mWeights0 : vertex.mWeights1 };
            auto& joints{ index < 4 ? vertex.mJoints0 : vertex.mJoints1 };
            const core::u32 component{ index % 4 };

            if ( weight > weights[component] ) {
                const core::f32 previousWeight{ weights[component] };
                const core::u32 previousJoint{ weights[component] > 0 ? static_cast<core::u32>( joints[component] ) : 0 };
                weights[component] = weight;
                joints[component] = static_cast<core::f32>( joint );
                weight = previousWeight;
                joint = previousJoint;
            }
        }
    }


    AssimpAnimationImporter::AssimpAnimationImporter( const aiScene& scene )
        : mScene{ scene } {
        RegisterNode( scene.mRootNode );

        for ( core::u32 index{}; index < scene.mNumAnimations; ++index ) {
            // The index keeps names unique even after filesystem sanitization.
            auto name{ string::Format( "clip_{}_{}", index, scene.mAnimations[index]->mName.C_Str() ) };

            for ( auto& character: name ) {
                if ( !( ( character >= 'a' && character <= 'z' ) || ( character >= 'A' && character <= 'Z' ) ||
                         ( character >= '0' && character <= '9' ) || character == '_' || character == '-' ) ) {
                    character = '_';
                }
            }

            mClipNames.emplace_back( name.c_str() );
        }
    }

    auto AssimpAnimationImporter::RegisterNode( const aiNode* node ) -> void {
        if ( !node ) {
            return;
        }

        const auto name{ string::Format( "{}__node_{}", node->mName.C_Str(), mNames.size() ) };
        mNames.emplace( node, name );
        mRuntimeNodes.emplace( name, node );
        const auto [entry, inserted]{ mSourceNodes.emplace( node->mName.C_Str(), node ) };

        if ( !inserted ) {
            entry->second = nullptr;// Ambiguous source names must never silently bind to another bone.
        }

        for ( core::u32 child{}; child < node->mNumChildren; ++child ) {
            RegisterNode(
                node->mChildren[child] );
        }
    }

    auto AssimpAnimationImporter::GetNodeName( const aiNode* node ) const -> const eastl::string& {
        const auto found{ mNames.find( node ) };

        if ( found == mNames.end() ) {
            MKT_THROW_RUNTIME_ERROR( "Assimp bone references a node outside the imported hierarchy." );
        }

        return found->second;
    }

    auto AssimpAnimationImporter::FindNode( core::cstr name ) const -> const aiNode* {
        const auto found{ mSourceNodes.find( name ) };

        if ( found == mSourceNodes.end() || !found->second ) {
            MKT_THROW_RUNTIME_ERROR( string::Format( "Missing or ambiguous Assimp animation node '{}'", name ).c_str() );
        }

        return found->second;
    }

    auto AssimpAnimationImporter::Load( core::cstr ) -> bool {
        return mScene.mRootNode != nullptr;
    }

    auto AssimpAnimationImporter::Import( ozz::animation::offline::RawSkeleton* skeleton, const NodeType& ) -> bool {
        // Include ancestors and rigid attachments, not just aiBones. Their local
        // transforms carry unit conversion and can have animation channels too.
        skeleton->roots.resize( 1 );
        const auto visit{ [&]( auto&& self, const aiNode* node, auto& joint ) -> void {
            joint.name = GetNodeName( node ).c_str();
            joint.transform = Transform( node->mTransformation );
            joint.children.resize( node->mNumChildren );

            for ( core::u32 index{}; index < node->mNumChildren; ++index ) {
                self( self, node->mChildren[index], joint.children[index] );
            }
        } };
        visit( visit, mScene.mRootNode, skeleton->roots[0] );
        return skeleton->Validate();
    }

    auto AssimpAnimationImporter::GetAnimationNames() -> AnimationNames {
        return mClipNames;
    }

    auto AssimpAnimationImporter::Import( core::cstr name, const ozz::animation::Skeleton& skeleton, core::f32 rate,
        ozz::animation::offline::RawAnimation* output ) -> bool {
        const aiAnimation* clip{};

        for ( core::usize index{}; index < mClipNames.size(); ++index ) {
            if ( mClipNames[index] == name ) {
                clip = mScene.mAnimations[index];
                break;
            }
        }

        if ( !clip ) {
            return false;
        }

        const core::f64 ticksPerSecond{ clip->mTicksPerSecond > 0 ? clip->mTicksPerSecond : 25.0 };
        core::f64 durationTicks{ eastl::max( 0.0, clip->mDuration ) };
        ankerl::unordered_dense::map<const aiNode*, const aiNodeAnim*> channels{};

        for ( core::u32 index{}; index < clip->mNumChannels; ++index ) {
            const auto* channel{ clip->mChannels[index] };
            channels.emplace( FindNode( channel->mNodeName.C_Str() ), channel );

            if ( channel->mNumPositionKeys ) {
                durationTicks = eastl::max( durationTicks, channel->mPositionKeys[channel->mNumPositionKeys - 1].mTime );
            }

            if ( channel->mNumRotationKeys ) {
                durationTicks = eastl::max( durationTicks, channel->mRotationKeys[channel->mNumRotationKeys - 1].mTime );
            }

            if ( channel->mNumScalingKeys ) {
                durationTicks = eastl::max( durationTicks, channel->mScalingKeys[channel->mNumScalingKeys - 1].mTime );
            }
        }

        output->name = name;
        output->duration = static_cast<core::f32>( eastl::max( durationTicks / ticksPerSecond, 1.0 / 30.0 ) );
        const core::f32 sampleRate{ rate > 0 ? rate : 30.0f };
        const core::f64 intervalCount{ glm::ceil( core::f64{ output->duration } * sampleRate ) };

        if ( intervalCount > 1000000.0 ) {
            MKT_THROW_RUNTIME_ERROR( "Assimp animation sampling exceeds one million intervals." );
        }

        const core::u32 intervals{ eastl::max( 1u, static_cast<core::u32>( intervalCount ) ) };
        output->tracks.resize( skeleton.num_joints() );

        for ( core::i32 joint{}; joint < skeleton.num_joints(); ++joint ) {
            const auto* node{ mRuntimeNodes.at( skeleton.joint_names()[joint] ) };
            aiVector3D scale{}, translation{};
            aiQuaternion rotation{};
            node->mTransformation.Decompose( scale, rotation, translation );
            auto& track{ output->tracks[joint] };
            const auto found{ channels.find( node ) };
            const auto* channel{ found != channels.end() ? found->second : nullptr };
            const core::u32 samples{ channel ? intervals + 1 : 1 };

            for ( core::u32 sample{}; sample < samples; ++sample ) {
                const core::f32 time{ sample == intervals ? output->duration : output->duration * ( static_cast<core::f32>( sample ) / intervals ) };
                const core::f64 ticks{ time * ticksPerSecond };
                auto position{ translation };
                auto orientation{ rotation };
                auto scaling{ scale };

                if ( channel ) {
                    const auto lerp{ []( const aiVector3D& a, const aiVector3D& b, core::f32 t ) {
                        return a + ( b - a ) * t;
                    } };
                    const auto slerp{ []( const aiQuaternion& a, const aiQuaternion& b, core::f32 t ) {
                        aiQuaternion value{};
                        aiQuaternion::Interpolate( value, a, b, t );
                        return value.Normalize();
                    } };
                    position = Sample( channel->mPositionKeys, channel->mNumPositionKeys, ticks, translation, channel->mPreState, channel->mPostState, lerp );
                    scaling = Sample( channel->mScalingKeys, channel->mNumScalingKeys, ticks, scale, channel->mPreState, channel->mPostState, lerp );
                    orientation = Sample( channel->mRotationKeys, channel->mNumRotationKeys, ticks, rotation, channel->mPreState, channel->mPostState, slerp );
                }

                ValidateRotation( orientation );
                orientation.Normalize();
                track.translations.push_back( { time, { position.x, position.y, position.z } } );
                track.rotations.push_back( { time, { orientation.x, orientation.y, orientation.z, orientation.w } } );
                track.scales.push_back( { time, { scaling.x, scaling.y, scaling.z } } );
            }
        }

        return output->Validate();
    }

    auto AssimpAnimationImporter::GetNodeProperties( core::cstr ) -> NodeProperties {
        return {};
    }

    auto AssimpAnimationImporter::Import( core::cstr, core::cstr, core::cstr, NodeProperty::Type, core::f32, ozz::animation::offline::RawFloatTrack* ) -> bool {
        return false;
    }

    auto AssimpAnimationImporter::Import( core::cstr, core::cstr, core::cstr, NodeProperty::Type, core::f32, ozz::animation::offline::RawFloat2Track* ) -> bool {
        return false;
    }

    auto AssimpAnimationImporter::Import( core::cstr, core::cstr, core::cstr, NodeProperty::Type, core::f32, ozz::animation::offline::RawFloat3Track* ) -> bool {
        return false;
    }

    auto AssimpAnimationImporter::Import( core::cstr, core::cstr, core::cstr, NodeProperty::Type, core::f32, ozz::animation::offline::RawFloat4Track* ) -> bool {
        return false;
    }


    AssimpImporter::AssimpImporter( renderer::rhi::IGpuDevice* device )
        : ModelImporter{ device } {
    }

    auto AssimpImporter::Import( const ModelLoadDescription& description, ModelDataDescription& out ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !description.mFile ) {
            MKT_THROW_RUNTIME_ERROR( "Assimp import requires a source file." );
        }

        const filesystem::Path path{ description.mFile->GetPath() };
        // Parser lifetime is scoped to this call; exceptions cannot strand a
        // busy pool slot and parallel imports never share mutable Assimp state.
        Assimp::Importer parser{};
        const auto* scene{ parser.ReadFile( path.GetAbsolute().c_str(),
            aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_PopulateArmatureData ) };

        if ( !scene || !scene->mRootNode || ( scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ) ) {
            MKT_THROW_RUNTIME_ERROR( string::Format( "Assimp '{}' failed: {}", path.GetC_Str(), parser.GetErrorString() ).c_str() );
        }

        ModelDataDescription result{};
        result.mName = scene->mName.length ? scene->mName.C_Str() : description.mFile->GetName().data();
        AssimpAnimationImporter animationImporter{ *scene };
        bool needsSkeleton{ scene->HasAnimations() };

        for ( core::u32 index{}; index < scene->mNumMeshes; ++index ) {
            needsSkeleton |= scene->mMeshes[index]->HasBones();
        }

        if ( needsSkeleton ) {
            auto skinning{ animation::SkinningBuilder{}
                .SetPath( path )
                .SetImporter( animationImporter )
                .SetSourceFingerprint( 0x415353494d503031ull )
                .SetImportAnimations( scene->HasAnimations() )
                .Build() };

            if ( !skinning || !skinning->mSkeleton ) {
                MKT_THROW_RUNTIME_ERROR( "Assimp skeleton import failed." );
            }

            result.mSkeleton = eastl::move( skinning->mSkeleton );
            result.mAnimations = eastl::move( skinning->mAnimations );
        }

        ankerl::unordered_dense::map<const aiTexture*, filesystem::Path> embedded{};

        for ( core::u32 index{}; index < scene->mNumMaterials; ++index ) {
            auto material{ ImportMaterial( *scene->mMaterials[index] ) };

            if ( description.mExtractTextures ) {
                Textures( *scene, *scene->mMaterials[index], path, material, embedded );
            }

            result.mMaterials.push_back( eastl::move( material ) );
        }

        const core::u32 defaultMaterial{ static_cast<core::u32>( result.mMaterials.size() ) };
        result.mMaterials.push_back( material::SurfaceMaterialDescription{} );
        result.mMaterials.back().MetallicFactor = 0;
        result.mMaterials.back().EmissiveFactor = core::float3{};
        const auto jointIndex{ [&]( const aiNode* node ) -> core::u32 {
            const auto* joint{ result.mSkeleton->FindJoint( animationImporter.GetNodeName( node ) ) };

            if ( !joint ) {
                MKT_THROW_RUNTIME_ERROR( "Assimp mesh references a joint absent from the runtime skeleton." );
            }

            return static_cast<core::u32>( joint->GetID() );
        } };
        core::i32 nodeIndex{};
        const auto visit{ [&]( auto&& self, const aiNode* node, const core::float4x4& parent ) -> void {
            const core::float4x4 global{ parent * Matrix( node->mTransformation ) };
            const core::i32 sourceIndex{ nodeIndex++ };

            for ( core::u32 instance{}; instance < node->mNumMeshes; ++instance ) {
                if ( node->mMeshes[instance] >= scene->mNumMeshes ) {
                    MKT_THROW_RUNTIME_ERROR( "Assimp mesh index is out of range." );
                }

                const auto& source{ *scene->mMeshes[node->mMeshes[instance]] };

                if ( !source.HasPositions() ) {
                    continue;
                }

                MeshNodeDescription mesh{};
                mesh.mName = source.mName.length ? source.mName.C_Str() : node->mName.C_Str();
                mesh.mSourceNodeIndex = sourceIndex;
                mesh.mTransform = global;
                mesh.MaterialIndex = source.mMaterialIndex < scene->mNumMaterials ? source.mMaterialIndex : defaultMaterial;
                mesh.mVertices.resize( source.mNumVertices );

                for ( core::u32 index{}; index < source.mNumVertices; ++index ) {
                    auto& vertex{ mesh.mVertices[index] };
                    const auto& position{ source.mVertices[index] };
                    vertex.mPosition = { position.x, position.y, position.z };

                    if ( source.HasNormals() ) {
                        const auto& normal{ source.mNormals[index] };
                        vertex.mNormals = { normal.x, normal.y, normal.z };
                    }

                    if ( source.HasVertexColors( 0 ) ) {
                        const auto& color{ source.mColors[0][index] };
                        vertex.mColors = { color.r, color.g, color.b, color.a };
                    }

                    if ( source.HasTextureCoords( 0 ) ) {
                        vertex.mUv0 = { source.mTextureCoords[0][index].x, source.mTextureCoords[0][index].y };
                    }

                    if ( source.HasTextureCoords( 1 ) ) {
                        vertex.mUv1 = { source.mTextureCoords[1][index].x, source.mTextureCoords[1][index].y };
                    }
                }

                for ( core::u32 faceIndex{}; faceIndex < source.mNumFaces; ++faceIndex ) {
                    const auto& face{ source.mFaces[faceIndex] };

                    if ( face.mNumIndices != 3 ) {
                        continue;
                    }

                    for ( core::u32 corner{}; corner < 3; ++corner ) {
                        if ( face.mIndices[corner] >= source.mNumVertices ) {
                            MKT_THROW_RUNTIME_ERROR( "Assimp vertex index is out of range." );
                        }

                        mesh.mIndices.push_back( face.mIndices[corner] );
                    }
                }

                if ( mesh.mIndices.empty() ) {
                    continue;
                }

                if ( needsSkeleton ) {
                    auto& binding{ mesh.mSkinBinding };
                    InitializeSkinningSpace( mesh.mTransform, binding );

                    if ( source.HasBones() ) {
                        if ( source.mNumBones > asset::kMaxBonesPerMesh ) {
                            MKT_THROW_RUNTIME_ERROR( "Assimp mesh exceeds the supported bone palette size." );
                        }

                        for ( core::u32 boneIndex{}; boneIndex < source.mNumBones; ++boneIndex ) {
                            const auto& bone{ *source.mBones[boneIndex] };
                            const auto* boneNode{ bone.mNode ? bone.mNode : animationImporter.FindNode( bone.mName.C_Str() ) };
                            // Assimp offsets map mesh-local vertices to bone-local.
                            // Ozz evaluates model-space joints; remove the mesh
                            // transform in the palette so unit scales apply once.
                            const core::float4x4 inverseBind{ Matrix( bone.mOffsetMatrix ) };
                            binding.mJoints.push_back( { jointIndex( boneNode ), inverseBind } );

                            for ( core::u32 weightIndex{}; weightIndex < bone.mNumWeights; ++weightIndex ) {
                                const auto& weight{ bone.mWeights[weightIndex] };

                                if ( weight.mVertexId >= mesh.mVertices.size() ) {
                                    MKT_THROW_RUNTIME_ERROR( "Assimp bone vertex index is out of range." );
                                }

                                AddWeight( mesh.mVertices[weight.mVertexId], boneIndex, weight.mWeight );
                            }
                        }

                        for ( auto& vertex: mesh.mVertices ) {
                            NormalizeSkinWeights( vertex.mJoints0, vertex.mWeights0, vertex.mJoints1, vertex.mWeights1, binding.mJoints.size() );
                        }
                    }
                    else {
                        binding.mJoints.push_back( { jointIndex( node ), core::float4x4{ 1.0f } } );

                        for ( auto& vertex: mesh.mVertices ) {
                            vertex.mJoints0 = vertex.mJoints1 = core::float4{};
                            vertex.mWeights0 = { 1, 0, 0, 0 };
                            vertex.mWeights1 = core::float4{};
                        }
                    }
                }

                result.mMeshNodes.push_back( eastl::move( mesh ) );
            }

            for ( core::u32 child{}; child < node->mNumChildren; ++child ) {
                self( self, node->mChildren[child], global );
            }
        } };
        visit( visit, scene->mRootNode, core::float4x4{ 1.0f } );

        if ( result.mMeshNodes.empty() ) {
            MKT_THROW_RUNTIME_ERROR( "Assimp model contains no triangle meshes." );
        }
        // Publish only a complete import; callers never see partially loaded data.
        out = eastl::move( result );
    }
}// namespace mikoto::asset
