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

#include <Core/Profiler.hh>
#include <Core/Timer.hh>
#include <Math/Math.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Core/FramePassResource.hh>
#include <Renderer/Passes/MeshCulling.hh>
#include <Scene/Component.hh>
#include <Scene/Scene.hh>

#include "Animation/AnimationSystem.hh"
#include "Animation/Animator.hh"

namespace Mikoto {

    auto MeshCulling::SetScene( Scene *scene ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_Scene = scene;
    }

    auto MeshCulling::RegisterPasses( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_SkinnedMeshes.resize( MAX_SKINNED_MESHES );
        m_MeshInfo.resize( MAX_RENDERABLE_ENTITIES );
        m_MeshInfoIndices.resize( MAX_RENDERABLE_ENTITIES );

        RegisterMeshCullingPass( graph );
        RegisterScatteredWrites( graph );
    }

    auto MeshCulling::RegisterScatteredWrites( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        struct ScatterPushConstant {
            UInt32 UpdateCount{};
        };

        graph.RegisterPass(
                "ScatteredWritesMeshPass",
                []( FramePassBuilder &b) {
                    MKT_BEGIN_PROFILER_NAMED();

                    b.Create<Buffer>( "ScatteredWrites_MeshSkinnedMatrices", BufferUsage::SHADER_STORAGE, sizeof(decltype(m_SkinnedMeshes)::value_type), MAX_SKINNED_MESHES );
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
                    b.Write( "ScatteredWrites_MeshSkinnedMatrices", FrameResourceState::UnorderedAccess );

                    // This pass goes after mesh culling
                    b.Read( "MeshCulling_MeshCullingNode", FrameResourceState::UnorderedAccess );

                    b.Use( SRGType::SRG_PerPass, "FinalBuffer_ObjectInfo", 0 );
                    b.Use( SRGType::SRG_PerPass, "ScatteredWrites_MeshData", 1 );
                    b.Use( SRGType::SRG_PerPass, "ScatteredWrites_MeshDataIndices", 2 );
                },
                [this]( CommandContext &ctx, FrameGraphBlackboard& ) -> void {
                    MKT_BEGIN_PROFILER_NAMED();

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

                    ctx.Dispatch( dispatchCount / 64, 1, 1 );
                } );
    }

    auto MeshCulling::RegisterMeshCullingPass(FrameGraph &graph) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass(
                "MeshCulling",
                []( FramePassBuilder &b ) -> void {
                    MKT_BEGIN_PROFILER_NAMED();
                    // To force this pass to go before ScatteredWritesMeshPass
                    b.Write( "MeshCulling_MeshCullingNode", FrameResourceState::UnorderedAccess );
                },
                [this]( CommandContext &ctx, FrameGraphBlackboard & ) -> void {
                    MKT_BEGIN_PROFILER_NAMED();
                    SetupInstanceData( ctx );
                } );
    }

    auto MeshCulling::DrawInstances( CommandContext &context ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        for ( auto &instanceInfo: m_DrawIndexedState | std::views::values ) {
            if (instanceInfo.InstancesCount == 0) {
                continue;
            }

            context.DrawIndexed( instanceInfo );
        }
    }

    auto MeshCulling::SetupInstanceData( CommandContext &context ) -> void {
        MKT_BEGIN_PROFILER_NAMED();
        // Prepare for draw
        for ( auto &[meshNode, meshNodeCount]: m_MeshDrawInstanceCount ) {
            // assume no instances and populate accordingly later
            meshNodeCount = 0;

            if ( m_InstanceInfos[meshNode].empty() ) {
                constexpr UInt32 mindObjectInfo{ 5 };
                m_InstanceInfos[meshNode].resize( mindObjectInfo );
            }
        }

        auto &registry{ m_Scene->GetRegistry() };

        // already created owned group using one of the same components.
        // Cannot create another group that owns the same components
        auto renderables{ registry.view<TagComponent, TransformComponent, MaterialComponent, MeshComponent>() };

        for ( auto [entity, tag, transform, materialComp, meshComponent]: renderables.each() ) {
            if ( !tag.IsActive() ) {
                continue;
            }

            if ( meshComponent.HasMesh() && materialComp.HasMaterial() ) {
                MeshNode *meshNode{ meshComponent.GetMesh() };
                PBRMaterial *pbrMat{ materialComp.GetMaterial().Dynamic<PBRMaterial>() };

                // Resize if we need more space for more objects of this meshNode
                Size &instanceCount{ m_MeshDrawInstanceCount[meshNode] };
                if ( m_InstanceInfos[meshNode].size() <= instanceCount ) {
                    m_InstanceInfos[meshNode].emplace_back();
                }

                ShaderMaterialParams &ubo{ m_InstanceInfos[meshNode][instanceCount] };
                DrawIndexedState &drawState{ m_DrawIndexedState[meshNode] };

                ubo.Transform = transform.GetTransform();
                ubo.Albedo = pbrMat->GetColor();
                ubo.Factors.x = pbrMat->GetMetallicFactor();
                ubo.Factors.y = pbrMat->GetRoughnessFactor();
                ubo.Factors.z = pbrMat->GetAoFactor();

                if (meshComponent.IsSkinned()) {

                    if (registry.any_of<SkinnedMeshRenderer>( entity )) {
                        auto& sm{ registry.get<SkinnedMeshRenderer>( entity ) };
                        ubo.AnimatorID = sm.GetAnimatorID();
                    }
                }

                ubo.EmissiveFactors = pbrMat->GetEmissiveFactors();
                ubo.EmissiveIntensity = pbrMat->GetEmissiveIntensity();

                ubo.Alpha = pbrMat->GetAlpha();

                ubo.AlbedoIndex = context.PushTexture( pbrMat->GetTextureType( MapType::ALBEDO_TEXTURE ) );
                ubo.NormalIndex = context.PushTexture( pbrMat->GetTextureType( MapType::NORMAL_TEXTURE ) );
                ubo.MetallicIndex = context.PushTexture( pbrMat->GetTextureType( MapType::METALLIC_TEXTURE ) );
                ubo.RoughnessIndex = context.PushTexture( pbrMat->GetTextureType( MapType::ROUGHNESS_TEXTURE ) );
                ubo.AoIndex = context.PushTexture( pbrMat->GetTextureType( MapType::AMBIENT_OCCLUSION_TEXTURE ) );
                ubo.EmissiveIndex = context.PushTexture( pbrMat->GetTextureType( MapType::EMISSIVE_TEXTURE ) );

                // Set buffers
                drawState.IndexBuffer = meshNode->GetIndexBuffer();
                drawState.IndicesCount = meshNode->GetIndexBuffer()->GetCount();

                if ( drawState.VertexBuffers.empty() ) {
                    drawState.VertexBuffers.emplace_back( meshNode->GetVertexBuffer(), 0 );
                }

                instanceCount += 1;
            }
        }

        // Prepare render data
        Size meshIndex{};
        Size activeMeshCount{};

        for ( auto &[meshNode, meshDrawState]: m_DrawIndexedState ) {
            meshDrawState.InstancesCount = m_MeshDrawInstanceCount[meshNode];

            for (Size index{}; index < meshDrawState.InstancesCount; ++index) {
                auto& instance{ m_InstanceInfos[meshNode][index] };
                auto& info{ m_MeshInfo[meshIndex] };

                info.Transform = instance.Transform;
                info.MaterialIndex = meshIndex;

                info.Albedo = instance.Albedo;
                info.AlbedoIndex = instance.AlbedoIndex;

                info.AlphaCutoff = instance.Alpha;
                info.MetallicFactor = instance.Factors.x;
                info.RoughnessFactor = instance.Factors.y;
                info.OcclusionStrength = instance.Factors.z;
                info.EmissiveFactors = instance.EmissiveFactors;
                info.EmissiveIntensity = instance.EmissiveIntensity;

                info.NormalIndex = instance.NormalIndex;
                info.MetallicIndex = instance.MetallicIndex;
                info.RoughnessIndex = instance.RoughnessIndex;
                info.AoIndex = instance.AoIndex;
                info.EmissiveIndex = instance.EmissiveIndex;

               // Copy matrices if skinned
#if true       // Disabled for now
                if (instance.AnimatorID != 0) {
                    info.BonesID = instance.AnimatorID - 1;// AnimatorID starts from 1, so we need to subtract 1 to use as index multiple meshes can share the same animator, so we can use AnimatorID as index for skinned meshes matrices

                    auto& matrices{ m_SkinnedMeshes[info.BonesID] };
                    Animator* animator{ AnimationSystem::Get()->GetAnimator( instance.AnimatorID ) };
                    MKT_ASSERT( animator != nullptr, "Skinned mesh animator is null" );

                    auto& joinMatrices{ animator->GetFinalBoneMatrices() };

                    MKT_ASSERT( joinMatrices.size() == matrices.size() && joinMatrices.size() == MAX_BONES_PER_MESH, "Matrices must be same sized" );
                    for (Size matrixIndex{}; matrixIndex < matrices.size(); ++matrixIndex) {
                        matrices[matrixIndex] = joinMatrices[matrixIndex];
                    }
                }
#endif

                m_MeshInfoIndices[meshIndex] = meshIndex;

                ++meshIndex;
            }

            meshDrawState.FirstInstance = activeMeshCount;

            activeMeshCount += meshDrawState.InstancesCount;
        }

        //context.UploadBufferData( "ScatteredWrites_MeshSkinnedMatrices", m_SkinnedMeshes.data(), sizeof( decltype( m_SkinnedMeshes )::value_type ), m_SkinnedMeshes.size() );

        MKT_ASSERT( activeMeshCount <= MAX_RENDERABLE_ENTITIES, "Exceeded limit of renderable entities" );

        m_ObjectUpdateCount = activeMeshCount;
    }
}