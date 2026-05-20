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

#include <EASTL/unique_ptr.h>

#include <Core/Core.hh>
#include <Core/Timer.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/Profiler.hh>
#include <Core/Blackboard.hh>

#include <Math/Math.hh>

#include <Animation/AnimationSystem.hh>

#include <Scene/Scene.hh>
#include <Scene/Component.hh>

#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Passes/GeometryCullModule.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::asset;
    using namespace mikoto::scene;
    using namespace mikoto::animation;

    auto GeometryAllocator::GetOrAllocate( const MeshNode* mesh, GeometryAllocation& outAlloc ) -> bool {
        if (auto it = mAllocations.find(mesh); it != mAllocations.end()) {
            outAlloc = it->second;
            return false; // already exists
        }

        const u64 vertexBytes = mesh->GetVertexBuffer()->GetSizeBytes();
        const u64 indexBytes  = mesh->GetIndexBuffer()->GetSizeBytes();

        auto alloc = mAllocator.Allocate(vertexBytes, indexBytes);
        MKT_ASSERT(alloc.has_value(), "Out of geometry space");

        outAlloc = {
            .mVertexOffset = static_cast<u32>(alloc->mVertexOffset),
            .mIndexOffset  = static_cast<u32>(alloc->mIndexOffset),
            .mOffsetID     = static_cast<u32>(mAllocations.size())
        };

        mAllocations.emplace(mesh, outAlloc);
        return true; // newly allocated
    }

    auto GeometryAllocator::GetOrAllocate(const MeshNode* mesh) -> GeometryAllocation {
        // Already registered
        if (auto it = mAllocations.find(mesh); it != mAllocations.end())
            return it->second;

        const u64 vertexBytes = mesh->GetVertexBuffer()->GetSizeBytes();
        const u64 indexBytes  = mesh->GetIndexBuffer()->GetSizeBytes();

        auto alloc = mAllocator.Allocate(vertexBytes, indexBytes);
        MKT_ASSERT(alloc.has_value(), "Out of geometry space");

        GeometryAllocation result{
            .mVertexOffset = static_cast<u32>(alloc->mVertexOffset),
            .mVertexSize = (u32)vertexBytes,
            .mIndexOffset  = static_cast<u32>(alloc->mIndexOffset),
            .mIndexSize = (u32)alloc->mIndexSize,
            .mOffsetID     = static_cast<u32>(mAllocations.size()) // simple ID
        };

        mAllocations.emplace(mesh, result);
        return result;
    }

    GeometryBufferAllocator::GeometryBufferAllocator(
            u64 vertexBufferSize,
            u64 indexBufferSize ) {
        mVertexFreeList.push_back( FreeRange{
                .mOffset = 0,
                .mSize = vertexBufferSize } );

        mIndexFreeList.push_back( FreeRange{
                .mOffset = 0,
                .mSize = indexBufferSize } );
    }

    auto GeometryBufferAllocator::AllocateFrom(
            eastl::vector<FreeRange> &freeList,
            u64 size ) -> eastl::optional<u64> {
        for ( auto it = freeList.begin(); it != freeList.end(); ++it ) {
            if ( it->mSize >= size ) {
                u64 allocOffset = it->mOffset;

                it->mOffset += size;
                it->mSize -= size;

                if ( it->mSize == 0 ) {
                    freeList.erase( it );
                }

                return allocOffset;
            }
        }

        return {};// out of space
    }

    auto GeometryBufferAllocator::Allocate(
            u64 vertexBytes,
            u64 indexBytes ) -> eastl::optional<GeometryAllocation> {
        auto vertexAlloc = AllocateFrom( mVertexFreeList, vertexBytes );
        if ( !vertexAlloc.has_value() ) {
            return {};
        }

        auto indexAlloc = AllocateFrom( mIndexFreeList, indexBytes );
        if ( !indexAlloc.has_value() ) {
            // rollback vertex allocation
            mVertexFreeList.push_back( FreeRange{
                    .mOffset = vertexAlloc.value(),
                    .mSize = vertexBytes } );

            return {};
        }

        return GeometryAllocation{
            .mVertexOffset = (u32)vertexAlloc.value(),
            .mVertexSize = (u32)vertexBytes,
            .mIndexOffset = (u32)indexAlloc.value(),
            .mIndexSize = (u32)indexBytes
        };
    }

    auto GeometryBufferAllocator::Free( const GeometryAllocation &alloc ) -> void {
        // NOTE: no merging (simple first-fit allocator)
        mVertexFreeList.push_back( FreeRange{
                .mOffset = alloc.mVertexOffset,
                .mSize = alloc.mVertexSize } );

        mIndexFreeList.push_back( FreeRange{
                .mOffset = alloc.mIndexOffset,
                .mSize = alloc.mIndexSize } );
    }

    auto GeometryBatch::Get( const asset::MeshNode *node, CommandContext& ctx, Blackboard& b ) -> MeshNodeInstancesInfo & {
        const auto it{ mInstanceCounts.find( node ) };
        if (it != mInstanceCounts.end()) {
            return it->second;
        }

        u32 newIndex{};
        if (!mMeshFreeNodeAllocationIndices.empty()) {
            newIndex = mMeshFreeNodeAllocationIndices.back();
            mMeshFreeNodeAllocationIndices.pop_back();
        } else {
            newIndex = mNodeAllocationIndex++;
        }

        const auto result{ mInstanceCounts.try_emplace( node, MeshNodeInstancesInfo{
            .mAllocationIndex = newIndex,
            .mGeometryInfo = mGeometryAllocator.GetOrAllocate( node ),
        }) };

        auto& info{ b.Get<GeometryManagementModuleInfo>() };

        ctx.CopyBuffer( info.mVerticesBuffer, node->GetVertexBuffer().GetRaw(), result.first->second.mGeometryInfo.mVertexOffset );
        ctx.CopyBuffer( info.mIndicesBuffer, node->GetIndexBuffer().GetRaw(), result.first->second.mGeometryInfo.mIndexOffset );

        return result.first->second;
    }

    auto GeometryCullModule::SetScene( const Scene *scene ) -> void {
        MKT_BEGIN_PROFILER_NAMED();
        mScene = scene;
    }

    auto GeometryCullModule::SetCamera( const Camera *camera ) -> void {
        MKT_BEGIN_PROFILER_NAMED();
        mCamera = camera;
    }

    auto GeometryCullModule::RegisterPasses( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        GeometryManagementModuleInfo& info{ graph.GetOrCreate<GeometryManagementModuleInfo>() };

        auto indirectCommandsDesc{ FGBufferDescription{}
            .SetName( "GeometryIndirectCommands_Buffer" )
            .SetUsage( BufferUsageFlagsBits::kStorage | BufferUsageFlagsBits::kIndirectDraw | BufferUsageFlagsBits::kCopyDst )
            .SetElementsSize( kMaxIndirectCommands, MKT_SIZEOF( DrawIndirectCommand ) )
            .SetHeapType( HeapType::eDeviceLocal )};

        info.mIndirectBuffer = graph.Create( indirectCommandsDesc );
        mIndirectBuffer = info.mIndirectBuffer;

        auto samplerDes{ FGSamplerDescription{}
            .SetName( "BasicSampler_Sampler01" )
            .SetFilter( rhi::SamplerFilter::eNearest )
            .SetWrap( SamplerWrapMode::eRepeat )
            .SetBorderColor( kColorWhite ) };

        info.mBasicSampler = graph.Create( samplerDes );

        mIndirectCmds.resize( kMaxIndirectCommands );
        mSkinningInfo.resize( kMaxSkinnedMeshes );

        RegisterGeometryFilterPass( graph );
        RegisterMeshCullingPass( graph );
    }

    auto GeometryCullModule::RegisterGeometryFilterPass(FrameGraph& graph) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Create the resources
        GeometryManagementModuleInfo& geometryFilterInfo{ graph.GetOrCreate<GeometryManagementModuleInfo>() };

        auto vertexDesc{ FGBufferDescription{}
            .SetName( "Geometry_VerticesBuffer01" )
            .SetUsage( BufferUsageFlagsBits::kStorage | BufferUsageFlagsBits::kCopyDst )
            .SetSizeBytes( MKT_MEGABYTES( kVertexBufferSizeMB ) )
            .SetHeapType( HeapType::eDeviceLocal )};

        geometryFilterInfo.mVerticesBuffer = graph.Create( vertexDesc );

        auto indexDesc{ FGBufferDescription{}
            .SetName( "Geometry_IndicesBuffer01" )
            .SetUsage( BufferUsageFlagsBits::kStorage | BufferUsageFlagsBits::kCopyDst )
            .SetSizeBytes( MKT_MEGABYTES( kIndexBufferSizeMB ) )
            .SetHeapType( HeapType::eDeviceLocal ) };

        geometryFilterInfo.mIndicesBuffer = graph.Create( indexDesc );

        auto materialsDesc{ FGBufferDescription{}
            .SetName( "Geometry_MaterialsBuffer01" )
            .SetUsage( BufferUsageFlagsBits::kStorage | BufferUsageFlagsBits::kCopyDst )
            .SetElementsSize( kMaxRenderableEntities, MKT_SIZEOF( MeshMaterialInfo ) )
            .SetHeapType( HeapType::eDeviceLocal )};

        geometryFilterInfo.mMaterialsBuffer = graph.Create( materialsDesc );

        auto geometryDesc{ FGBufferDescription{}
            .SetName( "Geometry_GeometryBuffer01" )
           .SetUsage( BufferUsageFlagsBits::kStorage | BufferUsageFlagsBits::kCopyDst )
           .SetElementsSize( kMaxRenderableEntities, MKT_SIZEOF( MeshGeometryInfo ) )
           .SetHeapType( HeapType::eDeviceLocal )};

        geometryFilterInfo.mGeometryBuffer = graph.Create( geometryDesc );

        auto skinningDesc{ FGBufferDescription{}
            .SetName( "Geometry_SkinningBuffer01" )
           .SetUsage( BufferUsageFlagsBits::kStorage | BufferUsageFlagsBits::kCopyDst )
           .SetElementsSize( kMaxSkinnedMeshes, MKT_SIZEOF( MeshSkinningInfo ) )
           .SetHeapType( HeapType::eDeviceLocal )};

        geometryFilterInfo.mSkinningBuffer = graph.Create( skinningDesc );

        graph.RegisterPass<GeometryManagementModuleInfo>(
            "GeometryFilter",
            FGPassType::eTransfer,
            []( FGNodeBuilder &b, GeometryManagementModuleInfo& data ) -> void {
                // Geometry
                b.Write( data.mVerticesBuffer, FGResourceState::eCopyDest );
                b.Write( data.mIndicesBuffer, FGResourceState::eCopyDest );

                // Drawing
                b.Write( data.mGeometryBuffer, FGResourceState::eCopyDest );
                b.Write( data.mMaterialsBuffer, FGResourceState::eCopyDest );
                b.Write( data.mSkinningBuffer, FGResourceState::eCopyDest );
            },
            [this]( CommandContext &ctx, Blackboard& b ) -> void {
                InitGeometryData( ctx, b );
            });
    }

    auto GeometryCullModule::RegisterMeshCullingPass( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass<GeometryManagementModuleInfo>(
            "MeshCulling",
            FGPassType::eTransfer,
            []( FGNodeBuilder &b, GeometryManagementModuleInfo& data ) -> void {
                b.Write( data.mGeometryBuffer, FGResourceState::eCopyDest );
                b.Write( data.mMaterialsBuffer, FGResourceState::eCopyDest );
            },
            []( CommandContext &, Blackboard & ) -> void {
                // Do culling and transfer stuff to GPU
            } );

        // TODO: Compute pass to write data to proper places? instead of copy every frame
    }

     auto GeometryCullModule::InitGeometryData( CommandContext &ctx, Blackboard& b  ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        auto &registry{ mScene->GetRegistry() };
        const auto renderables{
            registry.view<TagComponent, TransformComponent, MaterialComponent, MeshComponent>() };

        for ( const auto& [entity,
            tagComponent,
            transformComponent,
            materialComponent,
            meshComponent]: renderables.each() ) {

            if ( !tagComponent.IsActive() ) {
                continue;
            }

            if ( !meshComponent.HasMesh() || !materialComponent.HasMaterial() ) {
                continue;
            }

            const MeshNode* meshNode{ meshComponent.GetMesh() };
            const PhysicalMaterial *meshMaterial{ checked_cast<const PhysicalMaterial*>( materialComponent.GetMaterial().GetRaw() ) };

            auto& meshBatch{ mBatch.Get( meshNode, ctx, b ) };

            // Pack materials
            // If I have more instances than what I can hold
            if ( meshBatch.mInstanceCount >= meshBatch.mGeometryList.size()) {
                meshBatch.mGeometryList.emplace_back();
                meshBatch.mMaterialsList.emplace_back();
            }

            MeshGeometryInfo& geometry{ meshBatch.mGeometryList[meshBatch.mInstanceCount] };
            MeshMaterialInfo& material{ meshBatch.mMaterialsList[meshBatch.mInstanceCount] };

            geometry.mIndexOffset = meshBatch.mGeometryInfo.mIndexOffset;
            geometry.mVertexOffset = meshBatch.mGeometryInfo.mVertexOffset;

            geometry.mTransform = transformComponent.GetWorldTransform();
            geometry.mInverseModelView = glm::inverse( glm::mat3( mCamera->GetViewMatrix() * geometry.mTransform ) );

            geometry.mObjectID = tagComponent.GetGUID();

            if (meshComponent.IsSkinned()) {
                // Instead of uploading the animator final matrices we want to upload matrices that deform the object if any
                // some models are skinned but have no animations, those will require these matrices uploaded
                auto& sm{ registry.get<SkinnedMeshRenderer>( entity ) };
                auto animator{ AnimationSystem::Get()->GetAnimator( sm.GetAnimatorID() ) };
                if ( animator != nullptr && animator->IsPlaying() ) {
                    geometry.mSkinningMatricesID = as<i32>( sm.GetAnimatorID() );
                    mActiveFinalMatsIndices.emplace( geometry.mSkinningMatricesID );
                }
            }

            // Materials
            material.mBaseColorFactor = meshMaterial->GetBaseColorFactor();
            material.mEmissiveFactor = float4{ meshMaterial->GetEmissiveFactor(), 1.0f };
            material.mDiffuseFactor = meshMaterial->GetDiffuseFactor();
            material.mSpecularFactor = meshMaterial->GetSpecularFactor();

            material.mWorkflow = as<i32>( meshMaterial->GetWorkflow() );

            material.mMetallicFactor = meshMaterial->GetMetallicFactor();
            material.mRoughnessFactor = meshMaterial->GetRoughnessFactor();
            material.mEmissiveStrength = meshMaterial->GetEmissiveStrength();
            material.mAlphaMask = as<f32>( meshMaterial->GetAlphaMask() );
            material.mAlphaMaskCutoff = meshMaterial->GetAlphaMaskCutoff();

            // UV Sets
            material.mBaseColorTextureSet = meshMaterial->GetBaseColorTextureSet();
            material.mMetallicRoughnessTextureSet = meshMaterial->GetMetallicRoughnessTextureSet();
            material.mSpecularGlossinessSet = meshMaterial->GetSpecularGlossinessSet();
            material.mNormalTextureSet = meshMaterial->GetNormalTextureSet();
            material.mOcclusionTextureSet = meshMaterial->GetOcclusionTextureSet();
            material.mEmissiveTextureSet = meshMaterial->GetEmissiveTextureSet();

            // Texture
            material.mAlbedoIndex = PushTextureID( ctx, meshMaterial->GetTexture( MapType::eBaseColor ) );
            material.mDiffuseIndex  = PushTextureID( ctx, meshMaterial->GetTexture( MapType::eDiffuse ) );
            material.mNormalIndex  = PushTextureID( ctx, meshMaterial->GetTexture( MapType::eNormal ) );
            material.mEmissiveIndex  = PushTextureID( ctx, meshMaterial->GetTexture( MapType::eEmissive ) );
            material.mAoIndex  = PushTextureID( ctx, meshMaterial->GetTexture( MapType::eAmbientOcclusion ) );
            material.mMetallicIndex  = PushTextureID( ctx, meshMaterial->GetTexture( MapType::eMetallic ) );
            material.mRoughnessIndex  = PushTextureID( ctx, meshMaterial->GetTexture( MapType::eRoughness ) );
            material.mMetallicRoughnessIndex   = PushTextureID( ctx, meshMaterial->GetTexture( MapType::eMetallicRoughness ) );
            material.mSpecularGlossinessIndex   = PushTextureID( ctx, meshMaterial->GetTexture( MapType::eSpecularGlossiness ) );

            meshBatch.mInstanceCount += 1;
        }

        PrepareSkinning( ctx, b );
        PrepareIndirectDraw( ctx, b );
    }

    auto GeometryCullModule::PrepareSkinning( CommandContext &context, Blackboard& b ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        GeometryManagementModuleInfo& info{ b.Get<GeometryManagementModuleInfo>() };

        // Every ID takes up an index, just copy up to maximum index
        u32 maxIndex{};

        for (const auto& index : mActiveFinalMatsIndices) {
            if ( Animator * animator{ AnimationSystem::Get()->GetAnimator( index ) } ) {
                auto &finalMats{ animator->GetFinalBoneMatrices() };
                std::memcpy( mSkinningInfo[index - 1].mBoneTransforms.data(), finalMats.data(), finalMats.size() * MKT_SIZEOF( float4x4 ) );

                maxIndex = math::Max( maxIndex, index );
            }
        }

        if (!mActiveFinalMatsIndices.empty()) {
            context.CopyBuffer( info.mSkinningBuffer, 0, mSkinningInfo.data(), maxIndex * MKT_SIZEOF( MeshSkinningInfo ) );
            mActiveFinalMatsIndices.clear();
        }
    }

    auto GeometryCullModule::PrepareIndirectDraw( CommandContext &context, Blackboard& b  ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        GeometryManagementModuleInfo& info{ b.Get<GeometryManagementModuleInfo>() };

        size_t previousFirstInstance{};
        size_t indirectDrawCount{};
        for (auto& [node, instanceInfo] : mBatch.mInstanceCounts) {
            if (instanceInfo.mInstanceCount == 0) {
                continue;
            }

            // IMPORTANT: VertexCount must be the INDEX count when doing vertex pulling.
            // The vertex shader uses SV_VertexID to index into the index buffer:
            //     index = Indices[IndexOffset + VertexID]
            // Vertex count tells how many times the vertex shader will run.
            // So the shader must run once per index, not once per vertex.
            mIndirectCmds[indirectDrawCount].mInstanceCount = instanceInfo.mInstanceCount;
            mIndirectCmds[indirectDrawCount].mVertexCount = node->GetIndexBuffer()->GetSizeBytes() / MKT_SIZEOF( u32 ); // For vertex pulling it is the indices count
            mIndirectCmds[indirectDrawCount].mFirstInstance = previousFirstInstance;

            context.CopyBuffer( info.mGeometryBuffer, previousFirstInstance * MKT_SIZEOF( MeshGeometryInfo ), instanceInfo.mGeometryList.data(), instanceInfo.mInstanceCount * MKT_SIZEOF( MeshGeometryInfo ) );
            context.CopyBuffer( info.mMaterialsBuffer, previousFirstInstance * MKT_SIZEOF( MeshMaterialInfo ), instanceInfo.mMaterialsList.data(), instanceInfo.mInstanceCount * MKT_SIZEOF( MeshMaterialInfo ) );

            // Advance base instance
            previousFirstInstance += instanceInfo.mInstanceCount;

            // Reset for next frame
            instanceInfo.mInstanceCount = 0;

            ++indirectDrawCount;
        }

        mDrawCount = indirectDrawCount;

        if (mDrawCount != 0) {
            context.CopyBuffer( mIndirectBuffer, 0, mIndirectCmds.data(), mDrawCount * MKT_SIZEOF(DrawIndirectCommand) );
        }
    }

    auto GeometryCullModule::PushTextureID( CommandContext& ctx, TextureHandle handle ) -> i32 {
        if (handle.IsEmpty()) {
            return kInvalidTextureID;
        }

        return ctx.PushTexture_SRV( ctx.ImportTexture( handle ) );
    }

    auto GeometryCullModule::DrawInstancesIndirect( CommandContext &context ) -> void {
        MKT_BEGIN_PROFILER_NAMED();
        const auto params{ DrawIndirectState{}
            .SetDrawCount( mDrawCount )
            .SetBuffer( mIndirectBuffer )
        };

        context.DrawIndirect( params );
    }
}