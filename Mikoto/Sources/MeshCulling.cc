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
        m_MaterialInfo.resize( MAX_RENDERABLE_ENTITIES );

        RegisterMeshCullingPass( graph );
        RegisterGeometryFilterPass( graph );
    }

    auto MeshCulling::RegisterGeometryFilterPass(FrameGraph& graph) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass(
            "GeometryFilter",
            []( FramePassBuilder &b ) -> void {
                MKT_BEGIN_PROFILER_NAMED();

                // Create Buffers
                b.CreateBuffer( "MeshCulling_GeometryInfo", BufferUsage::SHADER_STORAGE,
                                MKT_SIZEOF( ShaderMesh ), MAX_RENDERABLE_ENTITIES, ResourceUsageType::RESOURCE_USAGE_DYNAMIC );

                b.CreateBuffer( "MeshCulling_MaterialsInfo", BufferUsage::SHADER_STORAGE,
                                MKT_SIZEOF( ShaderMaterial ), MAX_RENDERABLE_ENTITIES, ResourceUsageType::RESOURCE_USAGE_DYNAMIC );

                b.CreateBuffer( "MeshCulling_SkinningInfo", BufferUsage::SHADER_STORAGE,
                                MKT_SIZEOF( SkinningInfo ), MAX_SKINNED_MESHES, ResourceUsageType::RESOURCE_USAGE_DYNAMIC );

                b.Write( "MeshCulling_GeometryInfo", FrameResourceState::TransferDst );
                b.Write( "MeshCulling_MaterialsInfo", FrameResourceState::TransferDst );
                b.Write( "MeshCulling_SkinningInfo", FrameResourceState::TransferDst );
            },
            [this]( CommandContext &ctx, FrameGraphBlackboard & ) -> void {
                MKT_BEGIN_PROFILER_NAMED();
                SetupInstanceData( ctx );
            },
            FramePassNodeType::TRANSFER );
    }

    auto MeshCulling::RegisterMeshCullingPass( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass(
            "MeshCulling",
            []( FramePassBuilder &b ) -> void {
                MKT_BEGIN_PROFILER_NAMED();

                b.Read( "MeshCulling_GeometryInfo", FrameResourceState::UnorderedAccessView );
                b.Read( "MeshCulling_MaterialsInfo", FrameResourceState::UnorderedAccessView );
            },
            [this]( CommandContext &ctx, FrameGraphBlackboard & ) -> void {
                MKT_BEGIN_PROFILER_NAMED();
            }, FramePassNodeType::COMPUTE );
    }

     auto MeshCulling::SetupInstanceData( CommandContext &context ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        auto &registry{ m_Scene->GetRegistry() };
        auto renderables{ registry.view<TagComponent, TransformComponent, MaterialComponent, MeshComponent>() };

        for ( auto [entity, tag, transform, materialComp, meshComponent]: renderables.each() ) {
            if ( !tag.IsActive() ) {
                continue;
            }

            if ( meshComponent.HasMesh() && materialComp.HasMaterial() ) {
                MeshNode *meshNode{ meshComponent.GetMesh() };
                PBRMaterial *pbrMat{ materialComp.GetMaterial().Dynamic<PBRMaterial>() };

                auto& [geometry, material]{ m_IndexedGeometryManager.RegisterInstance( meshNode ) };

                // Geometry
                geometry.Transform = transform.GetWorldTransform();
                geometry.InverseModelView = glm::inverse( glm::mat3( m_Camera->GetViewMatrix() * geometry.Transform ) );
                if (meshComponent.IsSkinned()) {

                    if (registry.any_of<SkinnedMeshRenderer>( entity )) {
                        auto& sm{ registry.get<SkinnedMeshRenderer>( entity ) };
                        auto animator{ AnimationSystem::Get()->GetAnimator( sm.GetAnimatorID() ) };
                        if ( animator != nullptr && animator->IsPlaying() ) {
                            geometry.AnimatorID = sm.GetAnimatorID();
                        }
                    }
                }
                
                // Materials

                material.BaseColorFactor = pbrMat->GetBaseColorFactor();
                material.EmissiveFactor = Vec4F{ pbrMat->GetEmissiveFactor(), 1.0f };
                material.DiffuseFactor = pbrMat->GetDiffuseFactor();
                material.SpecularFactor = pbrMat->GetSpecularFactor();

                material.Workflow = static_cast<float>( pbrMat->GetWorkflow() );

                material.MetallicFactor = pbrMat->GetMetallicFactor();
                material.RoughnessFactor = pbrMat->GetRoughnessFactor();
                material.EmissiveStrength = pbrMat->GetEmissiveStrength();
                material.AlphaMask = static_cast<float>( pbrMat->GetAlphaMask() );
                material.AlphaMaskCutoff = pbrMat->GetAlphaMaskCutoff();

                // UV Sets
                material.BaseColorTextureSet = pbrMat->GetBaseColorTextureSet();
                material.MetallicRoughnessTextureSet = pbrMat->GetMetallicRoughnessTextureSet();
                material.SpecilarGlossinessSet = pbrMat->GetSpecularGlossinessSet();
                material.NormalTextureSet = pbrMat->GetNormalTextureSet();
                material.OcclusionTextureSet = pbrMat->GetOcclusionTextureSet();
                material.EmissiveTextureSet = pbrMat->GetEmissiveTextureSet();
                
                // Bindless texture indices
                material.AlbedoIndex = context.PushImageSampler( ResourceGroup::UnboundedImageViews, "Texture2D_List", pbrMat->GetTexture( MapType::BASE_COLOR_TEXTURE ) );
                material.DiffuseIndex = context.PushImageSampler( ResourceGroup::UnboundedImageViews, "Texture2D_List", pbrMat->GetTexture( MapType::DIFFUSE_TEXTURE ) );
                material.NormalIndex = context.PushImageSampler( ResourceGroup::UnboundedImageViews, "Texture2D_List", pbrMat->GetTexture( MapType::NORMAL_TEXTURE ) );
                material.EmissiveIndex = context.PushImageSampler( ResourceGroup::UnboundedImageViews, "Texture2D_List", pbrMat->GetTexture( MapType::EMISSIVE_TEXTURE ) );
                material.AoIndex = context.PushImageSampler( ResourceGroup::UnboundedImageViews, "Texture2D_List", pbrMat->GetTexture( MapType::AMBIENT_OCCLUSION_TEXTURE ) );

                material.MetallicIndex = context.PushImageSampler( ResourceGroup::UnboundedImageViews, "Texture2D_List", pbrMat->GetTexture( MapType::METALLIC_TEXTURE ) );
                material.RoughnessIndex = context.PushImageSampler( ResourceGroup::UnboundedImageViews, "Texture2D_List", pbrMat->GetTexture( MapType::ROUGHNESS_TEXTURE ) );

                material.SpecilarGlossinessIndex = context.PushImageSampler( ResourceGroup::UnboundedImageViews, "Texture2D_List", pbrMat->GetTexture( MapType::SPECULAR_GLOSSINESS ) );
                material.MetallicRoughnessIndex = context.PushImageSampler( ResourceGroup::UnboundedImageViews, "Texture2D_List", pbrMat->GetTexture( MapType::METALLIC_ROUGHNESS_TEXTURE ) );
            }
        }
        
        // Flaten
        Size activeMeshCount{};
        for (auto& [node, data] : m_IndexedGeometryManager.GetData()) {
            DrawIndexedState &drawState{ m_DrawIndexedState[node] };

            drawState.IndexBuffer = node->GetIndexBuffer();
            drawState.IndicesCount = node->GetIndexBuffer()->GetCount();

            if ( drawState.VertexBuffers.empty() ) {
                drawState.VertexBuffers.emplace_back( node->GetVertexBuffer(), 0 );
            }

            drawState.InstancesCount = data.size();
            drawState.FirstInstance = activeMeshCount;

            Size meshIndex{};
            for (auto& item : data) {
                m_MeshInfo[activeMeshCount + meshIndex] = item.first;
                m_MaterialInfo[activeMeshCount + meshIndex] = item.second;

                ++meshIndex;
            }

            activeMeshCount += data.size();
        }

        context.CopyBuffer( "MeshCulling_GeometryInfo", m_MeshInfo.data(), activeMeshCount * MKT_SIZEOF( ShaderMesh ) );
        context.CopyBuffer( "MeshCulling_MaterialsInfo", m_MaterialInfo.data(), activeMeshCount * MKT_SIZEOF( ShaderMaterial ) );

        // Clear after everything has been uploaded
        m_IndexedGeometryManager.Clear();
    }

    auto IndexedGeometryManager::Clear() -> void {
        for (auto& instances : m_InstanceMeshInfo | std::ranges::views::values) {
            instances.clear();
        }
    }

    auto IndexedGeometryManager::RegisterInstance(MeshNode* mesh) -> std::pair<ShaderMesh, ShaderMaterial>& {
        MKT_ASSERT( mesh, "Mesh node cannot be empty" );
        return m_InstanceMeshInfo[mesh].emplace_back();
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
                    
            },
            [this]( CommandContext &ctx, FrameGraphBlackboard& ) -> void {
                MKT_BEGIN_PROFILER_NAMED();

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
}