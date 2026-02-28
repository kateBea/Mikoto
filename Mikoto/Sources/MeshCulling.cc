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

#include <Math/Math.hh>

#include <Core/Timer.hh>
#include <Core/Profiler.hh>

#include <Renderer/Passes/MeshCulling.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Core/FramePassResource.hh>

#include <Scene/Scene.hh>
#include <Scene/Component.hh>

#include "Animation/Animator.hh"
#include "Animation/AnimationSystem.hh"

namespace Mikoto {

    GeometryBufferAllocator::GeometryBufferAllocator(
            UInt64 vertexBufferSize,
            UInt64 indexBufferSize ) {
        m_VertexFreeList.push_back( FreeRange{ .Offset = 0, .Size = vertexBufferSize } );
        m_IndexFreeList.push_back( FreeRange{ .Offset = 0, .Size = indexBufferSize } );
    }

    auto GeometryBufferAllocator::AllocateFrom( std::vector<FreeRange> &freeList, UInt64 size ) -> std::optional<UInt64> {
        for ( auto it{ freeList.begin() }; it != freeList.end(); ++it ) {
            if ( it->Size >= size ) {
                UInt64 allocOffset{ it->Offset };

                it->Offset += size;
                it->Size -= size;

                if ( it->Size == 0 ) {
                    freeList.erase( it );
                }

                return std::make_optional( allocOffset );
            }
        }

        return {};// Out of space
    }

    auto GeometryBufferAllocator::Allocate( UInt64 vertexBytes, UInt64 indexBytes ) -> std::optional<GeometryAllocation> {
        auto v{ AllocateFrom( m_VertexFreeList, vertexBytes ) };

        if ( !v.has_value() ) { 
            return {}; 
        }

        auto i{ AllocateFrom( m_IndexFreeList, indexBytes ) };
        if ( !i.has_value() ) {
            //  Since vertex allocation did not fail but index 
            // one did we roll back vertex allocation
            m_VertexFreeList.push_back( FreeRange{
                    .Offset = v.value(),
                    .Size = vertexBytes } );

            return {};
        }

        return std::make_optional( GeometryAllocation{
            .VertexOffset = v.value(),
            .VertexSize = vertexBytes,
            .IndexOffset = i.value(),
            .IndexSize = indexBytes
        });
    }

    auto GeometryBufferAllocator::Free( const GeometryAllocation &alloc ) -> void {
        // Simple approach: push back free ranges.
        // Later we will handle merging consecutive blocks
        m_VertexFreeList.push_back( FreeRange{
                .Offset = alloc.VertexOffset,
                .Size = alloc.VertexSize } );

        m_IndexFreeList.push_back( FreeRange{
                .Offset = alloc.IndexOffset,
                .Size = alloc.IndexSize } );
    }

    auto GeometryManager::UploadMeshData( const MeshNode *node ) -> GeometryAllocation {
        const auto it{ m_Allocations.find( node ) };
        if (it != m_Allocations.end()) {
            return it->second;
        }

        const UInt64 vertexBytes{ node->GetVertexBuffer()->GetSizeBytes() };
        const UInt64 indexBytes{ node->GetIndexBuffer()->GetSizeBytes() };

        auto alloc{ m_Allocator.Allocate( vertexBytes, indexBytes ) };
        MKT_ASSERT( alloc.has_value(), "Allocation is empty" );

        // Upload contents to the GPU
        // 
        //auto stagingVB{ m_Device->CreateBuffer( vertexBytes ) };
        //auto stagingIB{ m_Device->CreateBuffer( indexBytes ) };

        //CommandListHandle cmd{ m_Device->CreateCommandList( QueueType::TRANSFER_QUEUE, true ) };

        //cmd->CopyBuffer( stagingVB.GetRaw(), node->GetVertexBuffer().GetRaw() );
        //cmd->CopyBuffer( stagingIB.GetRaw(), node->GetIndexBuffer().GetRaw() );

        //// Issue copy commands (subrange copies)
        //m_Device->CopyBuffer( stagingVB, m_VertexBuffers, alloc->VertexOffset );
        //m_Device->CopyBuffer( stagingIB, m_IndexBuffers, alloc->IndexOffset );

        //// Save allocation for FreeMeshData()
        //m_RegisteredMeshes[node] = *alloc;

        //cmd->End();
        //m_Device->SubmitCommands( cmd );

        const auto [itInser, success]{ m_Allocations.try_emplace( node, *alloc ) };

        return itInser->second;
    }

    auto GeometryManager::Initialize( GpuDevice *device ) -> void {
        m_Device = device;

        // Allocate big index buffer and vertex buffer
    }

    auto GeometryManager::FreeMeshData( const MeshNode *node ) -> void {
        const auto it{ m_Allocations.find( node ) };
        if ( it == m_Allocations.end() ) {
            return;
        }

        m_Allocator.Free( it->second );
        m_Allocations.erase( it );
    }

    auto MeshCulling::SetScene( Scene *scene ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_Scene = scene;
    }

    auto MeshCulling::SetCamera( const Camera *camera ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_Camera = camera;
    }

    auto MeshCulling::RegisterPasses( FrameGraph &graph, GpuDevice* device ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_GeometryManager.Initialize( device );

        m_SkinningInfo.resize( MAX_SKINNED_MESHES );
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
                    
                b.Create<Buffer>( "ScatteredWrites_MeshSkinnedMatrices", BufferUsage::SHADER_STORAGE, sizeof( decltype( m_SkinningInfo )::value_type ), MAX_SKINNED_MESHES );
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

                b.Use( ResourceGroup::Dynamic, "FinalBuffer_ObjectInfo", 0 );
                b.Use( ResourceGroup::Dynamic, "ScatteredWrites_MeshData", 1 );
                b.Use( ResourceGroup::Dynamic, "ScatteredWrites_MeshDataIndices", 2 );
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

                // Create the buffers containing the materials and the information about the meshes
                auto finalBufferMaterials{ BufferBuilder{} };
                finalBufferMaterials.WithUsage( BufferUsage::SHADER_STORAGE )
                        .ForElement( sizeof( ShaderMaterial ), MAX_RENDERABLE_ENTITIES )
                        .IsDynamic( false )
                        .Build( "MaterialhInfo_Buffer" );
                b.CreateBuffer( finalBufferMaterials );

                auto finalBufferMeshInfo{ BufferBuilder{} };
                finalBufferMeshInfo.WithUsage( BufferUsage::SHADER_STORAGE )
                        .ForElement( sizeof( ShaderMesh ), MAX_RENDERABLE_ENTITIES )
                        .IsDynamic( false )
                        .Build( "MeshInfo_Buffer" );
                b.CreateBuffer( finalBufferMeshInfo );

                // To force this pass to go before ScatteredWritesMeshPass
                b.Write( "MeshCulling_MeshCullingNode", FrameResourceState::UnorderedAccess );

                b.Write( "MeshInfo_Buffer", FrameResourceState::UnorderedAccess );
                b.Write( "MaterialhInfo_Buffer", FrameResourceState::UnorderedAccess );
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

    auto MeshCulling::DrawInstancesIndirect( CommandContext &context ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // TODO: Implement indirect draw
        // Prepare indirect commands first, then draw
        
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

                ubo.Transform = transform.GetWorldTransform();
                ubo.Albedo = pbrMat->GetColor();
                ubo.Factors.x = pbrMat->GetMetallicFactor();
                ubo.Factors.y = pbrMat->GetRoughnessFactor();
                ubo.Factors.z = pbrMat->GetAoFactor();

                if (meshComponent.IsSkinned()) {

                    if (registry.any_of<SkinnedMeshRenderer>( entity )) {
                        auto& sm{ registry.get<SkinnedMeshRenderer>( entity ) };
                        auto animator{ AnimationSystem::Get()->GetAnimator( sm.GetAnimatorID() ) };
                        if ( animator != nullptr && animator->IsPlaying() ) {
                            ubo.AnimatorID = sm.GetAnimatorID();
                        }
                    }
                }

                ubo.EmissiveFactors = pbrMat->GetEmissiveFactor();
                ubo.EmissiveIntensity = pbrMat->GetEmissiveStrength();

                ubo.Alpha = pbrMat->GetAlphaMaskCutoff();

                ubo.AlbedoIndex = context.PushTexture( pbrMat->GetTexture( MapType::BASE_COLOR_TEXTURE ) );
                ubo.NormalIndex = context.PushTexture( pbrMat->GetTexture( MapType::NORMAL_TEXTURE ) );
                ubo.MetallicIndex = context.PushTexture( pbrMat->GetTexture( MapType::METALLIC_TEXTURE ) );
                ubo.RoughnessIndex = context.PushTexture( pbrMat->GetTexture( MapType::ROUGHNESS_TEXTURE ) );
                ubo.AoIndex = context.PushTexture( pbrMat->GetTexture( MapType::AMBIENT_OCCLUSION_TEXTURE ) );
                ubo.EmissiveIndex = context.PushTexture( pbrMat->GetTexture( MapType::EMISSIVE_TEXTURE ) );

                // Set buffers
                drawState.IndexBuffer = meshNode->GetIndexBuffer();
                drawState.IndicesCount = meshNode->GetIndexBuffer()->GetCount();

                if ( drawState.VertexBuffers.empty() ) {
                    drawState.VertexBuffers.emplace_back( meshNode->GetVertexBuffer(), 0 );
                }

                instanceCount += 1;
            }
        }

        // Upload indirect draw contents
        for (const auto& [node, meshCount] : m_MeshDrawInstanceCount) {
            m_IndirectDrawInfo[node] = m_GeometryManager.UploadMeshData( node );
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
                info.InverseModelView = glm::inverse(glm::mat3(m_Camera->GetViewMatrix() * instance.Transform));
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
                if (instance.AnimatorID != 0) {
                    // AnimatorID starts from 1, this is an index to the list of 
                    // final matrices of a given animation into the global list of final matrices
                    info.BonesID = instance.AnimatorID - 1;

                    auto &matrices{ m_SkinningInfo[info.BonesID].BoneTransforms };
                    Animator* animator{ AnimationSystem::Get()->GetAnimator( instance.AnimatorID ) };
                    MKT_ASSERT( animator != nullptr, "Skinned mesh animator is null" );

                    auto& joinMatrices{ animator->GetFinalBoneMatrices() };

                    MKT_ASSERT( joinMatrices.size() == matrices.size() && joinMatrices.size() == MAX_BONES_PER_MESH, "Matrices must be same sized" );
                    for (Size matrixIndex{}; matrixIndex < matrices.size(); ++matrixIndex) {
                        matrices[matrixIndex] = joinMatrices[matrixIndex];
                    }
                }

                m_MeshInfoIndices[meshIndex] = meshIndex;

                ++meshIndex;
            }

            meshDrawState.FirstInstance = activeMeshCount;

            activeMeshCount += meshDrawState.InstancesCount;
        }

        context.UploadBufferData( "ScatteredWrites_MeshSkinnedMatrices", m_SkinningInfo.data(), sizeof( decltype( m_SkinningInfo )::value_type ), m_SkinningInfo.size() );

        MKT_ASSERT( activeMeshCount <= MAX_RENDERABLE_ENTITIES, "Exceeded limit of renderable entities" );

        m_ObjectUpdateCount = activeMeshCount;
    }
}