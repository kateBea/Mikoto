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

#include <Math/Math.hh>

#include <Core/Profiler.hh>

#include <Scene/Scene.hh>
#include <Scene/Component.hh>

#include <Renderer/Passes/MeshCulling.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Core/FramePassResource.hh>

namespace Mikoto {

    auto MeshCulling::SetScene( Scene *scene ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_Scene = scene;
    }

    auto MeshCulling::RegisterPasses( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        RegisterMeshCullingPass( graph );
    }

    auto MeshCulling::RegisterMeshCullingPass(FrameGraph &graph) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_Meshes.resize( MAX_RENDERABLE_ENTITIES );

        m_MeshInfo.resize( MAX_RENDERABLE_ENTITIES );
        m_Materials.resize( MAX_RENDERABLE_ENTITIES );

        graph.RegisterPass(
                "MeshCulling",
                []( FramePassBuilder &b ) -> void {
                    MKT_BEGIN_PROFILER_NAMED();
                    auto meshInfoBuilder{ BufferBuilder{} };
                    meshInfoBuilder
                        .ForElement( sizeof( MeshParameters ), MAX_RENDERABLE_ENTITIES )
                        .WithUsage( BufferUsage::SHADER_STORAGE )
                        .IsDynamic( false )
                        .Build( "MeshCulling_MeshInfo" );

                    auto materialsInfoBuilder{ BufferBuilder{} };
                    materialsInfoBuilder
                        .ForElement( sizeof( MaterialParameters ), MAX_RENDERABLE_ENTITIES )
                        .WithUsage( BufferUsage::SHADER_STORAGE )
                        .IsDynamic( false )
                        .Build( "MeshCulling_MaterialInfo" );

                    //b.CreateBuffer( meshInfoBuilder );
                    //b.CreateBuffer( materialsInfoBuilder );

                    b.Create<Buffer>( "FinalCompositionPass_MeshInfo", BufferUsage::SHADER_STORAGE, sizeof( ShaderMaterialParams ), MAX_RENDERABLE_ENTITIES );
                    b.Write( "FinalCompositionPass_MeshInfo", FrameResourceState::UnorderedAccess );

                    //b.Write( "MeshCulling_MeshInfo", FrameResourceState::UnorderedAccess );
                    //b.Write( "MeshCulling_MaterialInfo", FrameResourceState::UnorderedAccess );
                },
                [this]( CommandContext &ctx, FrameGraphBlackboard & ) -> void {
                    MKT_BEGIN_PROFILER_NAMED();

                    // We begin the pass by assuming all meshes are not visible and update accordingly
                    for ( auto &instanceInfo: m_MeshDrawState | std::views::values ) {
                        for ( const auto &entityID: instanceInfo.InstanceInfos | std::views::keys ) {
                            instanceInfo.Disable( entityID );
                        }
                    }

                    SetupInstanceData( ctx );
                    UploadInstanceData( ctx );
                } );
    }

    auto MeshCulling::DrawInstances( CommandContext &context ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        for ( auto &instanceInfo: m_MeshDrawState | std::views::values ) {
            DrawIndexedState &drawState{ instanceInfo.InstanceDrawState };
            if (drawState.InstancesCount == 0) {
                continue;
            }

            context.DrawIndexed( drawState );
        }
    }

    auto MeshCulling::UploadInstanceData( CommandContext &context ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        Size meshIndex{};
        Size activeMeshCount{};

        for (auto &[meshNode, instanceInfo]: m_MeshDrawState) {
            DrawIndexedState &drawState{ instanceInfo.InstanceDrawState };

            drawState.IndexBuffer = meshNode->GetIndexBuffer();

            if (drawState.VertexBuffers.empty()) {
                drawState.VertexBuffers.emplace_back( meshNode->GetVertexBuffer(), 0 );
            }

            Size drawCount{};
            for (const auto &[entityID, meshInstanceInfo]: instanceInfo.InstanceInfos) {
                if (instanceInfo.IsActive( entityID )) {
                    m_Meshes[meshIndex++] = meshInstanceInfo;
                    ++drawCount;
                }
            }

            drawState.IndicesCount = meshNode->GetIndexBuffer()->GetCount();
            drawState.FirstInstance = activeMeshCount;
            drawState.InstancesCount = drawCount;

            activeMeshCount += drawCount;
        }

        MKT_ASSERT( activeMeshCount <= MAX_RENDERABLE_ENTITIES, "Exceeded limit of renderable entities" );
        context.UploadBufferData( "FinalCompositionPass_MeshInfo", m_Meshes.data(), sizeof( ShaderMaterialParams ), activeMeshCount );
    }

    auto MeshCulling::SetupInstanceData( CommandContext &context ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        auto &registry{ m_Scene->GetRegistry() };
        auto renderables{ registry.view<TagComponent, TransformComponent, MaterialComponent, MeshComponent>() };

        for (auto &entity: renderables) {
            auto &tag{ registry.get<TagComponent>( entity ) };
            auto &transform{ registry.get<TransformComponent>( entity ) };
            auto &meshComponent{ registry.get<MeshComponent>( entity ) };
            auto &materialComp{ registry.get<MaterialComponent>( entity ) };

            if (meshComponent.HasMesh() && materialComp.HasMaterial()) {
                MeshNode *meshNode{ meshComponent.GetMesh() };
                PBRMaterial *pbrMat{ materialComp.GetMaterial().Dynamic<PBRMaterial>() };

                auto &[DrawIndexedState, ActiveEntities, Instances]{
                    m_MeshDrawState[meshNode]
                };

                ActiveEntities[tag.GetGUID()] = tag.IsActive();

                ShaderMaterialParams &ubo{ Instances[tag.GetGUID()] };

                ubo.Transform = transform.GetTransform();
                ubo.Albedo = pbrMat->GetColor();
                ubo.Factors.x = pbrMat->GetMetallicFactor();
                ubo.Factors.y = pbrMat->GetRoughnessFactor();
                ubo.Factors.z = pbrMat->GetAoFactor();

                ubo.EmissiveFactors = pbrMat->GetEmissiveFactors();
                ubo.EmissiveIntensity = pbrMat->GetEmissiveIntensity();

                ubo.Alpha = pbrMat->GetAlpha();

                ubo.AlbedoIndex = context.PushTexture( pbrMat->GetTextureType( MapType::ALBEDO_TEXTURE ) );
                ubo.NormalIndex = context.PushTexture( pbrMat->GetTextureType( MapType::NORMAL_TEXTURE ) );
                ubo.MetallicIndex = context.PushTexture( pbrMat->GetTextureType( MapType::METALLIC_TEXTURE ) );
                ubo.RoughnessIndex = context.PushTexture( pbrMat->GetTextureType( MapType::ROUGHNESS_TEXTURE ) );
                ubo.AoIndex = context.PushTexture( pbrMat->GetTextureType( MapType::AMBIENT_OCCLUSION_TEXTURE ) );
                ubo.EmissiveIndex = context.PushTexture( pbrMat->GetTextureType( MapType::EMISSIVE_TEXTURE ) );
            }
        }
    }
}