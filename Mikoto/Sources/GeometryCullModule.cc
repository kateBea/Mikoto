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

    GeometryAllocator::GeometryAllocator() {
        mAllocations.resize( 10 );
    }

    auto GeometryAllocator::GetOrAllocate( const asset::MeshNode* mesh ) -> GeometryAllocation {
        const auto it{ mAllocationIndices.find( mesh ) };
        if (it != mAllocationIndices.end()) {
            return mAllocations[it->second];
        }

        mFlushRequired = true;

        const u64 assignedIndex{ mCurrentAllocationIndex };
        mAllocationIndices[mesh] = assignedIndex;

        if (assignedIndex >= mAllocations.size()) {
            mAllocations.emplace_back( GeometryAllocation{} );
        }

        auto& result{ mAllocations[assignedIndex] = GeometryAllocation{
            .mAllocationIndex = assignedIndex,
            .mVertexBuffer = mesh->GetVertexBuffer()->GetGpuDeviceAddress(),
            .mIndexBuffer = mesh->GetIndexBuffer()->GetGpuDeviceAddress(),
        }};

        // Advance index
        mCurrentAllocationIndex++;

        return result;
    }

    auto GeometryAllocator::Flush( CommandContext& ctx, Blackboard& blackboard ) -> void {
        if (!mFlushRequired) {
            return;
        }

        auto& info{ blackboard.Get<GeometryCullModuleInfo>() };
        ctx.CopyBuffer( info.mGeometryAllocBuffer, 0, mAllocations.data(), MKT_VECTOR_SIZE_BYTES( mAllocations ) );

        mFlushRequired = false;
    }

    auto GeometryBatch::Get( const asset::MeshNode *node, CommandContext& ctx, Blackboard& b ) -> GeometryAllocation& {
        const auto it{ mInstanceCounts.find( node ) };
        if (it != mInstanceCounts.end()) {
            return it->second;
        }

        const auto itInsert{ mInstanceCounts.try_emplace( node, mGeometryAllocator.GetOrAllocate( node ) ) };
        mGeometryAllocator.Flush( ctx, b );

        return itInsert.first->second;
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

        auto& info{ graph.GetOrCreate<GeometryCullModuleInfo>() };

        auto indirectCommandsDesc{ FGBufferDescription{}
            .SetName( "GeometryIndirectCommands_Buffer" )
            .SetUsage( BufferUsageFlagsBits::kStorage | BufferUsageFlagsBits::kIndirectDraw | BufferUsageFlagsBits::kCopyDst )
            .SetElementsSize( kMaxIndirectCommands, MKT_SIZEOF( DrawIndirectCommand ) )
            .SetHeapType( HeapType::eDeviceLocal ) };
        mIndirectBuffer = graph.Create( indirectCommandsDesc );
        info.mIndirectBuffer = mIndirectBuffer;

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
        auto& geometryFilterInfo{ graph.GetOrCreate<GeometryCullModuleInfo>() };

        auto vertexDesc{ FGBufferDescription{}
            .SetName( "GeometryDescription_Buffer01" )
            .SetUsage( BufferUsageFlagsBits::kStorage | BufferUsageFlagsBits::kCopyDst )
            .SetElementsSize( kMaxUniqueModels, MKT_SIZEOF( GeometryAllocation ) )
            .SetHeapType( HeapType::eDeviceLocal ) };
        geometryFilterInfo.mGeometryAllocBuffer = graph.Create( vertexDesc );

        auto materialsDesc{ FGBufferDescription{}
            .SetName( "GeometryMaterials_Buffer01" )
            .SetUsage( BufferUsageFlagsBits::kStorage | BufferUsageFlagsBits::kCopyDst )
            .SetElementsSize( kMaxRenderableEntities, MKT_SIZEOF( MeshMaterialInfo ) )
            .SetHeapType( HeapType::eDeviceLocal ) };
        geometryFilterInfo.mMaterialsBuffer = graph.Create( materialsDesc );

        auto geometryDesc{ FGBufferDescription{}
            .SetName( "GeometryRender_Buffer01" )
            .SetUsage( BufferUsageFlagsBits::kStorage | BufferUsageFlagsBits::kCopyDst )
            .SetElementsSize( kMaxRenderableEntities, MKT_SIZEOF( MeshGeometryInfo ) )
            .SetHeapType( HeapType::eDeviceLocal ) };
        geometryFilterInfo.mGeometryBuffer = graph.Create( geometryDesc );

        auto skinningDesc{ FGBufferDescription{}
            .SetName( "Geometry_SkinningBuffer01" )
            .SetUsage( BufferUsageFlagsBits::kStorage | BufferUsageFlagsBits::kCopyDst )
            .SetElementsSize( kMaxSkinnedMeshes, MKT_SIZEOF( MeshSkinningInfo ) )
            .SetHeapType( HeapType::eDeviceLocal )};
        geometryFilterInfo.mSkinningBuffer = graph.Create( skinningDesc );

        graph.RegisterPass<GeometryCullModuleInfo>(
            "GeometryFilter",
            FGPassType::eTransfer,
            []( FGNodeBuilder &b, GeometryCullModuleInfo& data ) -> void {
                // Geometry
                b.UseResource( data.mGeometryAllocBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );

                // Drawing
                b.UseResource( data.mGeometryBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                b.UseResource( data.mMaterialsBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                b.UseResource( data.mSkinningBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );

                b.UseResource( data.mIndirectBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this]( CommandContext &ctx, Blackboard& b ) -> void {
                InitGeometryData( ctx, b );
            });
    }

    auto GeometryCullModule::RegisterMeshCullingPass( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass<GeometryCullModuleInfo>(
            "MeshCulling",
            FGPassType::eTransfer,
            []( FGNodeBuilder &b, GeometryCullModuleInfo& data ) -> void {
                b.UseResource( data.mGeometryBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                b.UseResource( data.mMaterialsBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            []( CommandContext &, Blackboard & ) -> void {
                // Do culling and transfer stuff to GPU
                // TODO: Compute pass to write data to proper places? instead of copy every frame
            } );
    }

     auto GeometryCullModule::InitGeometryData( CommandContext &ctx, Blackboard& b  ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        auto &registry{ mScene->GetRegistry() };
        const auto renderables{ registry.view<TagComponent, TransformComponent, MaterialComponent, MeshComponent>() };

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
            auto& meshBatchDrawInfo{ mMeshBatchInfos[meshNode] };

            // If I have more instances than what I can hold
            if ( meshBatchDrawInfo.mInstanceCount >= meshBatchDrawInfo.mGeometryList.size()) {
                meshBatchDrawInfo.mGeometryList.emplace_back();
                meshBatchDrawInfo.mMaterialsList.emplace_back();
            }

            MeshGeometryInfo& geometry{ meshBatchDrawInfo.mGeometryList[meshBatchDrawInfo.mInstanceCount] };
            MeshMaterialInfo& material{ meshBatchDrawInfo.mMaterialsList[meshBatchDrawInfo.mInstanceCount] };

            geometry.mGeometryIndex = meshBatch.mAllocationIndex;

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
                } else {
                    geometry.mSkinningMatricesID = -1;
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

            meshBatchDrawInfo.mInstanceCount += 1;
        }

        PrepareSkinning( ctx, b );
        PrepareIndirectDraw( ctx, b );
    }

    auto GeometryCullModule::PrepareSkinning( CommandContext &context, Blackboard& b ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        GeometryCullModuleInfo& info{ b.Get<GeometryCullModuleInfo>() };

        u32 highestIndex{};
        for (const auto& index : mActiveFinalMatsIndices) {
            if ( Animator * animator{ AnimationSystem::Get()->GetAnimator( index ) } ) {
                highestIndex = eastl::max( highestIndex, index );
                auto &finalMats{ animator->GetFinalBoneMatrices() };
                std::memcpy( mSkinningInfo[index - 1].mBoneTransforms.data(), finalMats.data(), finalMats.size() * MKT_SIZEOF( float4x4 ) );
            }
        }

        if (!mActiveFinalMatsIndices.empty()) {
            context.CopyBuffer( info.mSkinningBuffer, 0, mSkinningInfo.data(), highestIndex * MKT_SIZEOF( MeshSkinningInfo ) );
            mActiveFinalMatsIndices.clear();
        }
    }

    auto GeometryCullModule::PrepareIndirectDraw( CommandContext &context, Blackboard& b  ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        auto& info{ b.Get<GeometryCullModuleInfo>() };

        usize indirectDrawCount{};
        usize previousFirstInstance{};
        for (auto& [node, instanceInfo] : mBatch.mInstanceCounts) {
            auto& meshBatchDrawInfo{ mMeshBatchInfos[node] };
            if (meshBatchDrawInfo.mInstanceCount == 0) {
                continue;
            }

            // Grow vectors if required, we need to have enough space to hold at least
            // the following number of instances
            if ((previousFirstInstance + meshBatchDrawInfo.mInstanceCount) > mGeometryList.size()) {
                const usize requiredExtraSpace{ (previousFirstInstance + meshBatchDrawInfo.mInstanceCount) - mGeometryList.size() };

                mGeometryList.resize( mGeometryList.size() + requiredExtraSpace );
                mMaterialsList.resize( mMaterialsList.size() + requiredExtraSpace );
            }

            // IMPORTANT: VertexCount must be the INDEX count when doing vertex pulling.
            // The vertex shader uses SV_VertexID to index into the index buffer:
            //     index = Indices[IndexOffset + VertexID]
            // Vertex count tells how many times the vertex shader will run.
            // So the shader must run once per index, not once per vertex.
            mIndirectCmds[indirectDrawCount].mInstanceCount = meshBatchDrawInfo.mInstanceCount;
            mIndirectCmds[indirectDrawCount].mVertexCount = node->GetIndexBuffer()->GetSizeBytes() / MKT_SIZEOF( u32 ); // For vertex pulling it is the indices count
            mIndirectCmds[indirectDrawCount].mFirstInstance = previousFirstInstance;

            void* geometryStartOffsetPtr{ previousFirstInstance + mGeometryList.data() };
            void* materialStartOffsetPtr{ previousFirstInstance + mMaterialsList.data() };
            std::memcpy( geometryStartOffsetPtr, meshBatchDrawInfo.mGeometryList.data(), MKT_VECTOR_SIZE_BYTES( meshBatchDrawInfo.mGeometryList ) );
            std::memcpy( materialStartOffsetPtr, meshBatchDrawInfo.mMaterialsList.data(), MKT_VECTOR_SIZE_BYTES( meshBatchDrawInfo.mMaterialsList ) );

            // Advance base instance
            previousFirstInstance += meshBatchDrawInfo.mInstanceCount;

            // Reset for next frame
            meshBatchDrawInfo.mInstanceCount = 0;

            ++indirectDrawCount;
        }

        mDrawCount = indirectDrawCount;

        if (mDrawCount != 0) {
            context.CopyBuffer( info.mGeometryBuffer, 0, mGeometryList.data(), previousFirstInstance * MKT_SIZEOF( MeshGeometryInfo ) );
            context.CopyBuffer( info.mMaterialsBuffer, 0, mMaterialsList.data(), previousFirstInstance * MKT_SIZEOF( MeshMaterialInfo ) );

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
            .SetBuffer( mIndirectBuffer ) };

        context.DrawIndirect( params );
    }
}