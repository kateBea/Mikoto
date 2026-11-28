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


#include <EASTL/utility.h>
#include <EASTL/array.h>
#include <EASTL/memory.h>
#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/string_view.h>
#include <EASTL/unordered_map.h>
#include <EASTL/numeric_limits.h>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <ozz/base/maths/simd_math.h>
#include <ozz/base/containers/vector.h>
#include <ozz/animation/runtime/local_to_model_job.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Logging/Logger.hh>

#include <Memory/Allocator.hh>

#include <Filesystem/FileService.hh>

#include <Assets/Utility.hh>
#include <Assets/AssetModel.hh>
#include <Assets/MeshFactory.hh>
#include <Assets/GLTFImporter.hh>
#include <Assets/MeshOptimizer.hh>
#include <Assets/AssimpImporter.hh>
#include <Assets/ImageProcessing.hh>
#include <Assets/ModelUploadBatch.hh>
#include <Assets/AssetRegistryService.hh>

#include <Animation/Utility.hh>
#include <Animation/SkinningPalette.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Core/RenderSystem.hh>
#include <Renderer/Core/MeshRenderData.hh>


namespace mikoto::asset {

    using namespace mikoto::core;
    using namespace mikoto::renderer;
    using namespace mikoto::material;
    using namespace mikoto::animation;
    using namespace mikoto::filesystem;
    using namespace mikoto::renderer::rhi;

    // Both importer backends apply the same retained-content policy before GPU allocation.
    static auto ApplyImportConfiguration( ModelDataDescription& data, const ImportConfiguration& options ) -> void {
        if ( !options.mImportAnimations ) {
            data.mAnimations.clear();

            for ( auto& rig: data.mRigs ) {
                rig.mAnimations.clear();
            }
        }

        if ( !options.mImportSkeletons ) {
            data.mSkeleton.reset();
            data.mRigs.clear();
            data.mAnimations.clear();

            for ( auto& mesh: data.mMeshNodes ) {
                mesh.mTransform *= mesh.mSkinBinding.mBindPoseTransform;
                mesh.mSkinBinding = {};

                for ( auto& vertex: mesh.mVertices ) {
                    vertex.mJoints0 = vertex.mJoints1 = core::float4{ -1.0f };
                    vertex.mWeights0 = vertex.mWeights1 = core::float4{};
                }
            }
        }

        if ( !options.mImportMesh ) {
            data.mMeshNodes.clear();
        }

        if ( !options.mImportMaterials ) {
            data.mMaterials.clear();
            data.mMaterials.emplace_back();

            for ( auto& mesh: data.mMeshNodes ) {
                mesh.MaterialIndex = 0;
            }
        }

        if ( !options.mImportTextures || !options.mImportMaterials ) {
            data.mTextures.clear();

            for ( auto& material: data.mMaterials ) {
                material.mTextureBindings.clear();
            }
        }

        if ( !options.mImportTranslation || !options.mImportRotation || !options.mImportScale ) {
            for ( auto& mesh: data.mMeshNodes ) {
                core::float3 scale{}, translation{}, skew{};
                core::float4 perspective{};
                core::quat rotation{};

                if ( !glm::decompose( mesh.mTransform, scale, rotation, translation, skew, perspective ) || glm::length( skew ) > 1e-5f ) {
                    MKT_THROW_RUNTIME_ERROR( "Cannot strip transform channels from a singular or sheared mesh." );
                }

                mesh.mTransform = glm::translate( core::float4x4{ 1 }, options.mImportTranslation ? translation : core::float3{} ) *
                                  glm::mat4_cast( options.mImportRotation ? rotation : core::quat{ 1, 0, 0, 0 } ) *
                                  glm::scale( core::float4x4{ 1 }, options.mImportScale ? scale : core::float3{ 1 } );
            }
        }
    }


    MeshFactory::MeshFactory( const MeshFactoryCreateInfo& createInfo )
        : mDevice{ createInfo.mDevice } {
    }


    auto MeshFactory::ImportModel( const ModelLoadDescription& description ) -> ModelHandle {
        MKT_BEGIN_PROFILER();

        auto loadInfo{ description };
        const Path settingsPath{ string::Format( "{}.mktimport", loadInfo.mFile->GetPath().GetC_Str() ) };

        if ( settingsPath.Exists() ) {
            loadInfo.mImportConfig = ImportConfiguration::Load( loadInfo.mFile->GetPath() );
        }

        const auto& options{ loadInfo.mImportConfig };
        loadInfo.mExtractTextures &= options.mImportTextures && options.mImportMaterials;

        if ( options.mImportSkeletons && ( !options.mImportTranslation || !options.mImportRotation || !options.mImportScale ) ) {
            MKT_THROW_RUNTIME_ERROR( "Skeleton import requires authored transforms. Disable skeletons before stripping transforms." );
        }

        ModelDataDescription resultMain{};

        if ( loadInfo.mFile->IsExtension( "gltf" ) || loadInfo.mFile->IsExtension( "glb" ) ) {
            MKT_CORE_LOGGER_DEBUG( "Using GLTF Importer for {}", loadInfo.mFile->GetPath().GetC_Str() );
            mGltfImporter->Import( loadInfo, resultMain );
        }
        else {
            MKT_CORE_LOGGER_DEBUG( "Using Assimp Importer for {}", loadInfo.mFile->GetPath().GetC_Str() );
            mAssimpImporter->Import( loadInfo, resultMain );
        }

        ApplyImportConfiguration( resultMain, options );
        return ConstructModel( resultMain, loadInfo );
    }

    auto MeshFactory::ConstructModel( ModelDataDescription& data, const ModelLoadDescription& loadInfo ) -> ModelHandle {
        MKT_BEGIN_PROFILER();

        if ( data.mMeshNodes.empty() && data.mRigs.empty() && !data.mSkeleton ) {
            MKT_CORE_LOGGER_ERROR( "Model import produced no meshes: {}. See the preceding importer error.",
                loadInfo.mFile->GetPath().GetC_Str() );
            return ModelHandle::CreateEmpty();
        }

        ModelCreateDescription modelDescription{};
        modelDescription
            .SetPath( loadInfo.mFile->GetPath() )
            .SetName( data.mName )
            .SetAnimations( eastl::move( data.mAnimations ) )
            .SetSkeleton( eastl::move( data.mSkeleton ) );

        modelDescription.mRigs = eastl::move( data.mRigs );

        // Collision geometry is static, but must use the same imported rest pose
        // as skinning. Raw POSITION values need not be in the mesh node's units.
        eastl::vector<eastl::vector<core::float4x4>> restModels( modelDescription.mRigs.empty() ? 
            ( modelDescription.mSkeleton ? 1 : 0 ) : modelDescription.mRigs.size() );

        for ( core::usize rig{}; rig < restModels.size(); ++rig ) {
            const auto* skeleton{ ( modelDescription.mRigs.empty() ? modelDescription.mSkeleton.get() : modelDescription.mRigs[rig].mSkeleton.get() )->GetOzzSkeleton() };
            ozz::vector<ozz::math::Float4x4> matrices( skeleton->num_joints() );
            ozz::animation::LocalToModelJob job{};
            job.skeleton = skeleton;
            job.input = skeleton->joint_rest_poses();
            job.output = ozz::make_span( matrices );

            if ( !job.Run() ) {
                MKT_THROW_RUNTIME_ERROR( "Cannot evaluate the imported skeleton's rest pose for collision geometry." );
            }

            auto& models{ restModels[rig] };
            models.resize( matrices.size() );

            for ( core::usize joint{}; joint < matrices.size(); ++joint ) {
                for ( core::usize column{}; column < 4; ++column ) {
                    ozz::math::StorePtrU( matrices[joint].cols[column], &models[joint][column][0] );
                }
            }
        }

        // Run mesh optimization in parallel for all meshes, blocks calling thread
        RunImportJobs( data.mMeshNodes.size(), [&]( core::usize index ) {
            auto& mesh{ data.mMeshNodes[index] };

            if ( mesh.mGeometrySourceIndex < 0 || mesh.mGeometrySourceIndex == index ) {
                OptimizeMesh( mesh );
            }
        } );

        // The upload owner records all commands.
        // CPU jobs only fill private image slots.
        ModelUploadBatch uploads{ *mDevice };

        // Logical paths must survive without a resident image, including save
        // and later reload from a different scene or preview consumer.
        for ( auto& material: data.mMaterials ) {
            for ( auto& [path, map]: material.mTextureBindings ) {
                path = filesystem::Path{ ( loadInfo.mFile->GetPath().GetDirectory().GetPathTyped<std::filesystem::path>() /
                                             path.GetPathTyped<std::filesystem::path>() )
                    .lexically_normal() }
                    .GetAbsolute();
            }
        }

        if ( loadInfo.mExtractTextures && !loadInfo.mDeferTextures ) {
            struct TextureWork {
                filesystem::Path mPath{};
                ImageHandle mImage{};
                renderer::rhi::TextureHandle mTexture{};
            };

            eastl::vector<TextureWork> textures{};
            eastl::unordered_map<eastl::string, core::usize> unique{};
            eastl::vector<eastl::pair<PBRMap*, core::usize>> bindings{};

            for ( auto& material: data.mMaterials ) {
                for ( auto& [relativePath, map]: material.mTextureBindings ) {
                    const filesystem::Path path{ ( loadInfo.mFile->GetPath().GetDirectory().GetPathTyped<std::filesystem::path>() /
                                                   relativePath.GetPathTyped<std::filesystem::path>() )
                        .lexically_normal() };

                    eastl::string key{ path.GetAbsolute() };
                    const auto [entry, added]{ unique.try_emplace( key, textures.size() ) };

                    if ( added ) {
                        textures.push_back( { path } );
                    }

                    bindings.emplace_back( &map, entry->second );
                }
            }

            const auto cache{ AssetRegistryService::Get()->GetAssetCacheBasePath() };
            const bool compress{ AssetRegistryService::Get()->IsTextureCompressionEnabled() };

            // Bound decoded-image residency as well as worker count; a whole large scene's
            // uncompressed textures must not accumulate before uploads start.
            constexpr core::usize window{ 4 };

            for ( core::usize begin{}; begin < textures.size(); begin += window ) {
                const core::usize count{ eastl::min( window, textures.size() - begin ) };
                RunImportJobs( count, [&]( core::usize offset ) {
                    auto& texture{ textures[begin + offset] };
                    texture.mImage = LoadTextureImage( texture.mPath, cache, compress );
                } );

                for ( core::usize offset{}; offset < count; ++offset ) {
                    auto& texture{ textures[begin + offset] };
                    const auto image{ PrepareImageForUpload( texture.mImage, *mDevice ) };
                    auto desc{ renderer::rhi::TextureCreateDescription{}
                        .SetMipCount( image->mMipCount )
                        .SetWidth( core::as<core::i32>( image->mWidth ) )
                        .SetHeight( core::as<core::i32>( image->mHeight ) )
                        .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
                        .SetMultisampling( renderer::rhi::Multisampling::eMsaaX1 )
                        .SetUsage( renderer::rhi::TextureUsageFlagsBits::ShaderResource |
                                       renderer::rhi::TextureUsageFlagsBits::CopyDest )
                        .SetFormat( GetTextureFormat( image->mFormat ) ) };

                    texture.mTexture = mDevice->CreateTexture( desc );
                    uploads.Write( texture.mTexture, image->mBufferSpan->GetData(), image->mBufferSpan->GetSize() );

                    // I no longer needs this image
                    texture.mImage.Reset();
                }
            }

            // All current material maps use the same linear image resource; sampler/material
            // properties remain per binding. Future sRGB views must extend this compatibility key.
            for ( const auto& [map, index]: bindings ) {
                map->mTexture = textures[index].mTexture;
            }
        }

        struct GeometryResources {
            BufferHandle mVertices{};
            BufferHandle mIndices{};
            renderer::MeshRenderDataHandle mRenderData{};
        };

        core::u32 meshIndex{ 0 };
        eastl::vector<GeometryResources> geometry( data.mMeshNodes.size() );

        for ( auto& meshNode: data.mMeshNodes ) {
            const core::usize source{ meshNode.mGeometrySourceIndex < 0 ? meshIndex : core::as<core::usize>( meshNode.mGeometrySourceIndex ) };
            const auto& sourceMesh{ data.mMeshNodes[source] };
            auto& resources{ geometry[source] };

            if ( sourceMesh.mVertices.empty() || sourceMesh.mIndices.empty() ) {
                MKT_THROW_RUNTIME_ERROR( "Model contains an empty primitive." );
            }

            if ( !resources.mVertices ) {
                auto verticesDesc{ renderer::rhi::BufferCreateDescription{}
                    .SetBufferUsage( renderer::rhi::BufferUsageFlagsBits::Vertex | 
                        renderer::rhi::BufferUsageFlagsBits::CopyDest | 
                        renderer::rhi::BufferUsageFlagsBits::CopySource )
                    .SetHeapType( renderer::rhi::HeapType::eDeviceLocal )
                    .SetCpuAccessType( renderer::rhi::AccessType::eRead ) };
                verticesDesc.mElementSize = MKT_VECTOR_SIZE_BYTES( sourceMesh.mVertices );
                resources.mVertices = mDevice->CreateBuffer( verticesDesc );

                auto indicesDesc{ renderer::rhi::BufferCreateDescription{}
                    .SetBufferUsage( renderer::rhi::BufferUsageFlagsBits::Index | 
                        renderer::rhi::BufferUsageFlagsBits::CopyDest | 
                        renderer::rhi::BufferUsageFlagsBits::CopySource )
                    .SetHeapType( renderer::rhi::HeapType::eDeviceLocal )
                    .SetCpuAccessType( renderer::rhi::AccessType::eRead )
                    .SetFormat( renderer::rhi::Format::eR32_UINT ) };
                indicesDesc.mElementSize = MKT_VECTOR_SIZE_BYTES( sourceMesh.mIndices );
                resources.mIndices = mDevice->CreateBuffer( indicesDesc );

                uploads.Write( resources.mVertices, sourceMesh.mVertices.data(), verticesDesc.mElementSize, renderer::rhi::ResourceStates::eVertexBuffer );
                uploads.Write( resources.mIndices, sourceMesh.mIndices.data(), indicesDesc.mElementSize, renderer::rhi::ResourceStates::eIndexBuffer );

                resources.mRenderData = renderer::MeshRenderDataHandle::New(
                    eastl::span<const VertexDescription_Std430Alignment>{ sourceMesh.mVertices.data(), sourceMesh.mVertices.size() },
                    eastl::span<const core::u32>{ sourceMesh.mIndices.data(), sourceMesh.mIndices.size() }, !meshNode.mSkinBinding.mJoints.empty() );
            }

            auto meshCreateInfo{ MeshCreateDescription{}
                .SetName( meshNode.mName )
                .SetVertices( resources.mVertices )
                .SetIndices( resources.mIndices )
                .SetTransform( meshNode.mTransform )
                .SetMaterial( data.mMaterials[meshNode.MaterialIndex] ) };
            meshCreateInfo.mMaterialSourceIndex = meshNode.MaterialIndex;

            meshCreateInfo.mCollisionVertices.reserve( sourceMesh.mVertices.size() );
            meshCreateInfo.mRenderData = resources.mRenderData;

            eastl::vector<core::float4x4> collisionPalette( meshNode.mSkinBinding.mJoints.size() );

            if ( !collisionPalette.empty() && ( meshNode.mSkinBinding.mRigIndex >= restModels.size() ||
                                                  !BuildSkinningPalette( meshNode.mSkinBinding, restModels[meshNode.mSkinBinding.mRigIndex], collisionPalette ) ) ) {
                MKT_THROW_RUNTIME_ERROR( "Cannot build the mesh's rest-pose collision palette." );
            }

            for ( const auto& vertex: sourceMesh.mVertices ) {
                const auto position{ collisionPalette.empty()
                                         ? core::float3{ meshNode.mSkinBinding.mBindPoseTransform * core::float4{ vertex.mPosition, 1.0f } }
                                         : SkinVertexPosition( { vertex.mPosition, vertex.mJoints0, vertex.mWeights0, vertex.mJoints1, vertex.mWeights1 }, collisionPalette ) };
                meshCreateInfo.mCollisionVertices.push_back( position );
            }

            meshCreateInfo.mCollisionIndices.assign( sourceMesh.mIndices.begin(), sourceMesh.mIndices.end() );

            if ( !meshNode.mSkinBinding.mJoints.empty() ) {
                meshCreateInfo.mSkinningVertices.reserve( sourceMesh.mVertices.size() );

                for ( const auto& vertex: sourceMesh.mVertices ) {
                    meshCreateInfo.mSkinningVertices.push_back( { vertex.mPosition, vertex.mJoints0, vertex.mWeights0, vertex.mJoints1, vertex.mWeights1 } );
                }
            }

            meshCreateInfo.mSkinBinding = eastl::move( meshNode.mSkinBinding );
            modelDescription.AddMesh( meshIndex++, meshCreateInfo );
        }

        uploads.Finish();
        return ModelHandle::New( eastl::move( modelDescription ) );
    }

    auto MeshFactory::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        mGltfImporter = eastl::make_unique<GLTFImporter>( mDevice );
        mAssimpImporter = eastl::make_unique<AssimpImporter>( mDevice );

        mIsInitialized = true;
    }

    auto MeshFactory::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( !mIsInitialized ) {
            return;
        }

        mGltfImporter = nullptr;
        mAssimpImporter = nullptr;

        MKT_CORE_LOGGER_INFO( "Shutting down AssetRegistryService..." );
    }
}// namespace mikoto::asset
