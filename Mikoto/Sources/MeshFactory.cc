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

#include <array>
#include <memory>
#include <vector>
#include <cstdlib>

#include <Common/String.hh>

#include <Renderer/Core/Pipeline.hh>
#include <Renderer/Core/RenderService.hh>

#include <Assets/Model.hh>
#include <Assets/AssetsService.hh>
#include <Assets/MeshFactory.hh>
#include <Filesystem/FileService.hh>
#include <Library/Utility/Types.hh>
#include <Renderer/Core/RenderUtility.hh>

#include <Assets/GltfImporter.hh>
#include <Assets/MainImporter.hh>
#include <Assets/MeshOptimizer.hh>

namespace Mikoto {

    MeshFactory::MeshFactory( const MeshFactoryCreateInfo &createInfo )
        : m_Device{ createInfo.Device } {
    }

    auto MeshFactory::ImportModel( const ModelLoadDescription &loadInfo ) -> ModelHandle {
        ModelData resultMain{};

        if ( loadInfo.ModelFile->GetPath().ends_with( "gltf" ) || loadInfo.ModelFile->GetPath().ends_with( "glb" ) ) {
            MKT_CORE_LOGGER_DEBUG( "Using GLTF Importer for {}", loadInfo.ModelFile->GetPath() );
            m_GLTFImporter->Import( loadInfo, resultMain );
        } else {
            MKT_CORE_LOGGER_DEBUG( "Using Assimp Importer for {}", loadInfo.ModelFile->GetPath() );
            m_MainImporter->Import( loadInfo, resultMain );
        }


        Model *result{ ConstructModel( resultMain, loadInfo ) };

        return ModelHandle::Create( result );
    }

    auto MeshFactory::ConstructModel( ModelData &data, const ModelLoadDescription &loadInfo  ) -> Model * {
        Model *result{ new Model( data.Name, loadInfo.ModelFile->GetPath(), std::move( data.SceneSkeleton ) ) };

        result->SetAnimations( std::move(data.Animations) );

        UInt32 meshIndex{ 0 };
        for (const auto& meshNode : data.MeshNodes) {

            // Change this to be a raw stream of bytes so im not forced to attributes being floats only
            std::vector<float> vertices{};
            std::vector<UInt32> indices{ meshNode.Indices };

            vertices.reserve( DEFAULT_VERTEX_BUFFER_LAYOUT.GetStride() / sizeof( float ) * meshNode.Vertices.size() );

            // TODO: automate with the provided vertex buffer layout
            for (const auto& vertex : meshNode.Vertices) {
                // Positions
                vertices.emplace_back( vertex.Position.x );
                vertices.emplace_back( vertex.Position.y );
                vertices.emplace_back( vertex.Position.z );

                // Normals
                vertices.emplace_back( vertex.Normals.x );
                vertices.emplace_back( vertex.Normals.y );
                vertices.emplace_back( vertex.Normals.z );

                // Color
                vertices.emplace_back( vertex.Colors.r );
                vertices.emplace_back( vertex.Colors.g );
                vertices.emplace_back( vertex.Colors.b );

                // Uv0
                vertices.emplace_back( vertex.UV_0.x );
                vertices.emplace_back( vertex.UV_0.y );
                
                // Uv1
                vertices.emplace_back( vertex.UV_1.x );
                vertices.emplace_back( vertex.UV_1.y );

                // Joint
                vertices.emplace_back( vertex.Joints.x );
                vertices.emplace_back( vertex.Joints.y );
                vertices.emplace_back( vertex.Joints.z );
                vertices.emplace_back( vertex.Joints.w );

                // Weight
                vertices.emplace_back( vertex.Weights.x );
                vertices.emplace_back( vertex.Weights.y );
                vertices.emplace_back( vertex.Weights.z );
                vertices.emplace_back( vertex.Weights.w );
            }

            BufferDescription vertexDesc{};
            vertexDesc.WithData( reinterpret_cast<Byte *>( vertices.data() ) )
                      .WithUsage( BufferUsage::VERTEX )
                      .WithBufferDataType( BufferDataType::BUFFER_DATA_FLOAT32 )
                      .WithSizeBytes( InferSize<float>( vertices.size() ) )
                      .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_STATIC );

            BufferDescription indexDesc{};
            indexDesc.WithData( reinterpret_cast<Byte *>( indices.data() ) )
                     .WithUsage( BufferUsage::INDEX )
                     .WithSizeBytes( InferSize<UInt32>(  meshNode.Indices.size() ) )
                     .WithBufferDataType( BufferDataType::BUFFER_DATA_UINT32 )
                     .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_STATIC );

            BufferHandle vertexBuffer{ m_Device->CreateBuffer( vertexDesc ) };
            BufferHandle indexBuffer{ m_Device->CreateBuffer( indexDesc ) };

            vertexBuffer->SetDebugName( StringUtil::Format("Vertices for: {}", meshNode.Name) );
            indexBuffer->SetDebugName( StringUtil::Format( "Indices for: {}", meshNode.Name ) );

            // If it does not have any material construct one by default
            MaterialProperties material{ data.Materials[meshNode.MaterialIndex]  };
            MeshNode node{ meshIndex,vertexBuffer, indexBuffer, meshNode.Name, std::move( material) };

            result->PushMeshNode( meshIndex,std::move( node) );

            ++meshIndex;
        }

        return result;
    }

    auto MeshFactory::Init() -> void {
        m_GLTFImporter = CreateScope<GLTFImporter>( m_Device );
        m_MainImporter = CreateScope<MainImporter>( m_Device );

        MeshOptimizer::OptimizerTestRun();

        m_IsInitialized = true;
    }

    auto MeshFactory::Shutdown() -> void {

        if (!m_IsInitialized) { return; }

        m_GLTFImporter = nullptr;
        m_MainImporter = nullptr;

        MKT_CORE_LOGGER_INFO( "Shutting down AssetsService..." );
    }
}
