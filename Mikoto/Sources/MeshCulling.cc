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
        RegisterScatteredWrites( graph );
    }

    auto MeshCulling::RegisterScatteredWrites( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_MeshInfo.resize( MAX_RENDERABLE_ENTITIES );
        m_MeshInfoIndices.resize( MAX_RENDERABLE_ENTITIES );

        struct ScatterPushConstant {
            UInt32 UpdateCount{};
        };

        graph.RegisterPass(
                "ScatteredWritesMeshPass",
                []( FramePassBuilder &b) {
                    b.Create<Buffer>( "ScatteredWrites_MeshData", BufferUsage::SHADER_STORAGE, sizeof( MeshParameters ), MAX_RENDERABLE_ENTITIES );
                    b.Create<Buffer>( "ScatteredWrites_MeshDataIndices", BufferUsage::SHADER_STORAGE, sizeof( UInt32 ), MAX_RENDERABLE_ENTITIES );

                    auto finalBuffer{ BufferBuilder{} };
                    finalBuffer.WithUsage( BufferUsage::SHADER_STORAGE )
                        .ForElement( sizeof(MeshParameters), MAX_RENDERABLE_ENTITIES )
                        .IsDynamic( false )
                        .Build( "FinalBuffer_ObjectInfo" );

                    b.CreateBuffer( finalBuffer );

                    b.UseShader( "Resources/Shaders/vulkan-spirv/ScatteredWritesMesh_Comp.sprv", ShaderStage::COMPUTE );
                    b.Create<Pipeline>( "ScatteredWritesMeshPass_Pipeline", ComputePipelineDescription{} );

                    b.Write( "ScatteredWrites_MeshData", FrameResourceState::UnorderedAccess );
                    b.Write( "ScatteredWrites_MeshDataIndices", FrameResourceState::UnorderedAccess );
                    b.Write( "FinalBuffer_ObjectInfo", FrameResourceState::UnorderedAccess );

                    // This pass goes after mesh culling
                    b.Read( "FinalCompositionPass_MeshInfo", FrameResourceState::UnorderedAccess );

                    b.Use( SRGType::SRG_PerPass, "FinalBuffer_ObjectInfo", 0 );
                    b.Use( SRGType::SRG_PerPass, "ScatteredWrites_MeshData", 1 );
                    b.Use( SRGType::SRG_PerPass, "ScatteredWrites_MeshDataIndices", 2 );
                },
                [this]( CommandContext &ctx, FrameGraphBlackboard& ) -> void {
                    // Need at least m_ObjectUpdateCount threads.
                    // But dispatch works in groups, not individual threads.
                    Size dispatchCount{ (m_ObjectUpdateCount + 64 - 1) };
                    if (dispatchCount == 0) {
                        return;
                    }

                    ScatterPushConstant pushConstants{};
                    pushConstants.UpdateCount = m_ObjectUpdateCount;

                    ctx.BindPipeline( "ScatteredWritesMeshPass_Pipeline" );

                    ctx.PushConstants( std::addressof( pushConstants ), sizeof( ScatterPushConstant ) );
                    ctx.UploadBufferData( "ScatteredWrites_MeshData", m_MeshInfo.data(), sizeof( MeshParameters ), m_ObjectUpdateCount );
                    ctx.UploadBufferData( "ScatteredWrites_MeshDataIndices", m_MeshInfoIndices.data(), sizeof( UInt32 ), m_ObjectUpdateCount );

                    ctx.Dispatch( (m_ObjectUpdateCount + 64 - 1) / 64, 1, 1 );
                } );
    }

    auto MeshCulling::RegisterMeshCullingPass(FrameGraph &graph) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass(
                "MeshCulling",
                []( FramePassBuilder &b ) -> void {
                    MKT_BEGIN_PROFILER_NAMED();
                    b.Create<Buffer>( "FinalCompositionPass_MeshInfo", BufferUsage::SHADER_STORAGE, sizeof( ShaderMaterialParams ), MAX_RENDERABLE_ENTITIES );
                    b.Write( "FinalCompositionPass_MeshInfo", FrameResourceState::UnorderedAccess );
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

            Size instanceCount{};
            for (const auto &[entityID, meshInstanceInfo]: instanceInfo.InstanceInfos) {
                if (instanceInfo.IsActive( entityID )) {
                    // We have already allocated enough space to hold this many active entities
                    m_Meshes[meshIndex] = meshInstanceInfo;
                    m_MeshInfo[meshIndex] = MeshParameters{
                        .Transform{ meshInstanceInfo.Transform },
                        .MeshIndex{ static_cast<UInt32>(meshIndex) },
                        .MaterialIndex{ static_cast<UInt32>(meshIndex) }
                    };

                    m_MeshInfoIndices[meshIndex] = meshIndex;

                    ++meshIndex;
                    ++instanceCount;
                }
            }

            drawState.IndicesCount = meshNode->GetIndexBuffer()->GetCount();
            drawState.FirstInstance = activeMeshCount;
            drawState.InstancesCount = instanceCount;

            activeMeshCount += instanceCount;
        }

        MKT_ASSERT( activeMeshCount <= MAX_RENDERABLE_ENTITIES, "Exceeded limit of renderable entities" );

        m_ObjectUpdateCount = activeMeshCount;

        context.UploadBufferData( "FinalCompositionPass_MeshInfo", m_Meshes.data(), sizeof( ShaderMaterialParams ), activeMeshCount );
    }

    auto MeshCulling::SetupInstanceData( CommandContext &context ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Potential count
        Size count{};

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

                if (tag.IsActive()) {
                    ++count;
                }
            }
        }

        if (m_Meshes.size() < count) {
            m_Meshes.resize( count );
        }
    }
}// namespace Mikoto