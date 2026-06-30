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

#include <EASTL/array.h>
#include <EASTL/memory.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/utility.h>
#include <EASTL/vector.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/Profiler.hh>

#include <Assets/Model.hh>

#include <Assets/MeshFactory.hh>
#include <Assets/GLTFImporter.hh>
#include <Assets/MainImporter.hh>
#include <Assets/MeshOptimizer.hh>
#include <Assets/AssetsService.hh>
#include <Assets/ImageProcessor.hh>

#include <Memory/Allocator.hh>

#include <Filesystem/FileService.hh>

#include <Renderer/Core/Rhi.hh>
#include <Renderer/Core/RenderSystem.hh>

namespace mikoto::asset {

    MeshFactory::MeshFactory( const MeshFactoryCreateInfo &createInfo )
        : mDevice{ createInfo.mDevice } {
    }

    auto MeshFactory::ImportModel( const ModelLoadDescription &loadInfo ) -> ModelHandle {
        ModelDataDescription resultMain{};
        if ( loadInfo.mFile->GetPath().EndsWith( "gltf" ) || loadInfo.mFile->GetPath().EndsWith( "glb" ) ) {
            MKT_CORE_LOGGER_DEBUG( "Using GLTF Importer for {}", loadInfo.mFile->GetPath().GetC_Str() );
            mGltfImporter->Import( loadInfo, resultMain );
        } else {
            MKT_CORE_LOGGER_DEBUG( "Using Assimp Importer for {}", loadInfo.mFile->GetPath().GetC_Str() );
            mMainImporter->Import( loadInfo, resultMain );
        }

        return ConstructModel( resultMain, loadInfo );
    }

    auto MeshFactory::ConstructModel( ModelDataDescription &data, const ModelLoadDescription &loadInfo  ) -> ModelHandle {
        ModelCreateDescription modelDescription{};
        modelDescription
            .SetPath( loadInfo.mFile->GetPath() )
            .SetName( data.mName )
            .SetAnimations( std::move( data.mAnimations ) )
            .SetSkeleton( std::move( data.mSkeleton ) );

        // Here I load all images from disk and crate the GPU resource.
        // Importer already extracted the correct path for all texture images.
        // They are assumed to be relative paths to the model root directory.
        if (loadInfo.mExtractTextures) {
            CommandListHandle cmd{ mDevice->CreateCommandList( QueueType::eTransfer ) };
            cmd->Begin( {} );

            for (auto& material : data.mMaterials) {
                for (auto& [relativePath, pbrMapInfo] : material.mTexturesByUri) {
                    auto pathToTexture{ PathBuilder{}
                        .SetPath( loadInfo.mFile->GetPath().GetDirectory() )
                        .SetPath( relativePath )
                        .Build() };

                    asset::ImageHandle image{ asset::ProcessImage2D( pathToTexture ) };
                    auto textureDescription{ TextureCreateDescription{}
                        .SetWidth( as<i32>( image->mWidth ) )
                        .SetHeight( as<i32>( image->mHeight ) )
                        .SetDimensions( TextureDimension::eTexture2D )
                        .SetMultisampling( Multisampling::eMsaaX1 )
                        .SetUsage( TextureUsageFlagsBits::kShaderResource | TextureUsageFlagsBits::kCopyDst )
                        .SetFormat( Format::eRGBA8_UNORM ) };

                    pbrMapInfo.mTexture = mDevice->CreateTexture( textureDescription );
                    pbrMapInfo.mTexture->SetDebugName( string::Format( "Texture: {}", pathToTexture.GetC_Str() ) );

                    // Always written to mip 0 for simplicity.
                    // These textures are primarily shaders read resources
                    cmd->Write( pbrMapInfo.mTexture.GetRaw(), 0, image->mBufferSpan->GetData(), image->mBufferSpan->GetSize() );
                    cmd->SetResourceState( pbrMapInfo.mTexture.GetRaw(), ResourceStates::eShaderResource );
                }
            }

            cmd->End();
            mDevice->ExecuteCommands( cmd );
        }

        u32 meshIndex{ 0 };
        for (const auto& meshNode : data.mMeshNodes) {
            // Optimize vertices and indices

            // Create vertices buffer (WindingOrder counter-clockwise)
            auto verticesDesc{ BufferCreateDescription{}
                .SetBufferUsage( BufferUsageFlagsBits::kVertex | BufferUsageFlagsBits::kCopyDst | BufferUsageFlagsBits::kCopySrc )
                .SetHeapType( HeapType::eDeviceLocal )
                .SetCpuAccessType( CpuAccessType::eRead )
                .SetInitialData( BufferSpanHandle::Spawn(
                    meshNode.mVertices.data(), MKT_VECTOR_SIZE_BYTES(meshNode.mVertices) ) )
            };
            BufferHandle vertices{ mDevice->CreateBuffer( verticesDesc ) };

            // Create indices buffer
            auto indicesDesc{ BufferCreateDescription{}
                .SetBufferUsage( BufferUsageFlagsBits::kIndex | BufferUsageFlagsBits::kCopyDst | BufferUsageFlagsBits::kCopySrc )
                .SetHeapType( HeapType::eDeviceLocal )
                .SetCpuAccessType( CpuAccessType::eRead )
                .SetFormat( Format::eR32_UINT )
                .SetInitialData( BufferSpanHandle::Spawn( meshNode.mIndices.data(), MKT_VECTOR_SIZE_BYTES(meshNode.mIndices) ) )
            };
            BufferHandle indices{ mDevice->CreateBuffer( indicesDesc ) };

            auto meshCreateInfo{ MeshCreateDescription{}
                .SetName( meshNode.mName )
                .SetVertices( vertices )
                .SetIndices( indices )
                .SetTransform( meshNode.mTransform )
                .SetMaterial( data.mMaterials[meshNode.MaterialIndex] )
            };

            modelDescription.AddMesh( meshIndex++, meshCreateInfo );
        }

        return ModelHandle::Spawn( std::move( modelDescription ) );
    }

    auto MeshFactory::Initialize() -> void {
        mGltfImporter = eastl::make_unique<GLTFImporter>( mDevice );
        //mMainImporter = eastl::make_unique<MainImporter>( mDevice );

        mIsInitialized = true;
    }

    auto MeshFactory::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (!mIsInitialized) { return; }

        mGltfImporter = nullptr;
        mMainImporter = nullptr;

        MKT_CORE_LOGGER_INFO( "Shutting down AssetsService..." );
    }
}
