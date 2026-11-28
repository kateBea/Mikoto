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

#include <cmath>
#include <utility>

#include <EASTL/algorithm.h>
#include <EASTL/bit.h>
#include <EASTL/sort.h>
#include <EASTL/unique_ptr.h>

#include <glm/glm.hpp>
#include <glm/gtx/compatibility.hpp>

#include <Animation/AnimationSystem.hh>
#include <Animation/Utility.hh>

#include <Core/Blackboard.hh>
#include <Core/Core.hh>
#include <Core/Exception.hh>
#include <Core/Profiler.hh>
#include <Core/Timer.hh>
#include <Core/Types.hh>

#include <Logging/Logger.hh>

#include <Material/SurfaceMaterial.hh>

#include <Math/Math.hh>

#include <Renderer/Core/CommandContext.hh>
#include <Renderer/ForwardRenderer/Passes/GeometryCullModule.hh>
#include <Renderer/Video/Video.hh>

#include <Scene/Component.hh>
#include <Scene/Scene.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::asset;
    using namespace mikoto::scene;
    using namespace mikoto::material;

    // Independent implementation of the discrete near/far Gaussian model described at:
    // https://bentobaux.github.io/posts/separable-subsurface-scattering-in-unity-6/
    //
    // Reference shader: Assets/Shaders/Library/SSS Util/ArtistFriendlyKernel.shader
    // https://github.com/bentoBAUX/Separable-Subsurface-Scattering-in-Unity-6
    // Underlying separable model: https://iryoku.com/separable-sss/

    /** @brief Builds the reference's discrete Gaussian mixture on material edits. */
    MKT_NODISCARD static auto BuildSubsurfaceKernel( const material::SubsurfaceSettings &settings ) -> material::SubsurfaceKernel {
        SubsurfaceKernel result{};
        const core::i32 count{ eastl::clamp( settings.mSamples, 7, 65 ) | 1 };

        // Zero radius is exactly the identity filter, without numerical tails.
        if ( settings.mScatterScale <= 0 ) {
            result.mSamples[0] = core::float4{ 1, 1, 1, 0 };
            return result;
        }

        const core::f32 spread{ settings.mScatterScale * 2.0f };
        const auto nearSigma{ glm::max( settings.mNear * spread, core::float3{ 0.01f } ) };
        const auto farSigma{ glm::max( settings.mFar * spread, core::float3{ 0.01f } ) };
        const core::f32 blend{ glm::clamp( settings.mNearWeight, 0.0f, 1.0f ) };
        core::float3 total{};

        for ( core::i32 tap{}; tap < count; ++tap ) {
            // Center, -1, +1, -2, +2, ...; 65 taps match reference stepCount=32.
            const core::i32 offset{ tap == 0 ? 0 : ( tap % 2 ? -( tap + 1 ) / 2 : tap / 2 ) };
            auto &sample{ result.mSamples[tap] };
            sample.w = core::as<core::f32>( offset );

            for ( core::i32 channel{}; channel < 3; ++channel ) {
                const auto gaussian{ [&]( core::f32 sigma ) {
                    const core::f32 x{ sample.w / sigma };
                    return glm::exp( -0.5f * x * x ) / sigma;
                } };
                // The common 1/sqrt(2*pi) cancels during normalization.
                sample[channel] = blend * gaussian( nearSigma[channel] ) + ( 1.0f - blend ) * gaussian( farSigma[channel] );
                total[channel] += sample[channel];
            }
        }

        for ( core::i32 tap{}; tap < count; ++tap ) {
            for ( core::i32 channel{}; channel < 3; ++channel ) {
                result.mSamples[tap][channel] /= total[channel];
            }
        }

        return result;
    }

    /** @brief Beer-Lambert attenuation from the same near/far profile; no shadow-depth approximation. */
    MKT_NODISCARD static auto BuildSubsurfaceTransmission( const material::SubsurfaceSettings &settings ) -> core::float3 {
        // Artist-authored thickness, as in the reference's SSSSTransmission.hlsl.
        // This is independent of mesh dimensions; it is not inferred from shadow maps.
        const core::float3 distance{ glm::mix( settings.mFar, settings.mNear, settings.mNearWeight ) * settings.mScatterScale };
        core::float3 result{};

        for ( core::i32 channel{}; channel < 3; ++channel ) {
            result[channel] = glm::exp( -settings.mThickness / glm::max( distance[channel], 0.0001f ) );
        }

        return result * settings.mTranslucency * glm::clamp( settings.mScatterScale / 10.0f, 0.0f, 1.0f );
    }

    auto GeometryAllocator::Get( core::u32 index ) const -> const GeometryAllocation & {
        MKT_ASSERT( index <= mAllocations.size(), "Index out of bounds for allocations." );

        return mAllocations[index];
    }

    auto GeometryAllocator::GetOrAllocate( const MeshRenderDataHandle &mesh ) -> const GeometryAllocation & {
        MKT_BEGIN_PROFILER();
        const auto found{ mAllocationIndices.find( mesh.GetPtr() ) };

        if ( found != mAllocationIndices.end() ) {
            return mAllocations[found->second];
        }

        const auto indices{ mesh->GetIndices() };
        const auto attributes{ mesh->GetAttributes() };

        constexpr core::usize attributeCapacity{ core::usize{ kVertexBufferSizeMB } * 1024 * 1024 };
        constexpr core::usize indexCapacity{ core::usize{ kIndexBufferSizeMB } * 1024 * 1024 / sizeof( core::u32 ) };

        if ( ( mFreeSlots.empty() && mAllocations.size() >= kMaxUniqueModels ) || attributes.size() > attributeCapacity - mAttributeBytes ||
             indices.size() > indexCapacity - mIndexCount ) {
            MKT_THROW_RUNTIME_ERROR( "Scene geometry arena capacity exceeded. Increase the geometry buffer budgets." );
        }

        GeometryAllocation allocation{};
        allocation.mFirstIndex = core::as<core::u32>( mIndexCount );
        allocation.mIndexCount = core::as<core::u32>( indices.size() );
        allocation.mAllocationIndex = mFreeSlots.empty() ? core::as<core::u32>( mAllocations.size() ) : mFreeSlots.back();

        const core::usize streamCount{ mesh->IsSkinned() ? kMeshStreamCount : core::as<core::usize>( MeshStream::Joints0 ) };

        for ( core::usize stream{}; stream < streamCount; ++stream ) {
            allocation.mStreams[stream] = mAttributeBytes + mesh->GetOffsets()[stream];
        }

        if ( mFreeSlots.empty() ) {
            mSources.push_back( mesh );
            mUploadedRevisions.push_back( ~core::u64{} );
            mAllocations.push_back( allocation );
            mLastSeen.push_back( 0 );
        }
        else {
            mFreeSlots.pop_back();
            mSources[allocation.mAllocationIndex] = mesh;
            mUploadedRevisions[allocation.mAllocationIndex] = ~core::u64{};
            mAllocations[allocation.mAllocationIndex] = allocation;
            mRepack = true;
        }

        mAllocationIndices.emplace( mesh.GetPtr(), allocation.mAllocationIndex );
        mAttributeBytes += attributes.size();
        mIndexCount += indices.size();
        return mAllocations[allocation.mAllocationIndex];
    }

    auto GeometryAllocator::Clear() -> void {
        mAllocationIndices.clear();
        mAllocations.clear();
        mSources.clear();
        mUploadedRevisions.clear();
        mLastSeen.clear();
        mFreeSlots.clear();
        mRepack = true;
        mUploadedCount = 0;
        mAttributeBytes = 0;
        mIndexCount = 0;
        mUploadedAttributeBytes = 0;
    }

    auto GeometryAllocator::Touch( core::u32 index, core::u64 frame ) -> void {
        mLastSeen[index] = frame;
    }

    auto GeometryAllocator::CollectUnused( core::u64 frame ) -> void {
        // Avoid reallocating arenas as an object briefly leaves a view. This
        // sweep releases CPU sources too; cache eviction can then drop models.
        if ( frame % 120 != 0 ) {
            return;
        }

        for ( core::u32 index{}; index < mSources.size(); ++index ) {
            if ( mSources[index] && frame - mLastSeen[index] >= 120 ) {
                mAllocationIndices.erase( mSources[index].GetPtr() );

                mSources[index].Reset();
                mAllocations[index] = {};
                mFreeSlots.push_back( index );

                mRepack = true;
            }
        }
    }

    auto GeometryAllocator::Prepare( CommandContext &ctx, core::Blackboard &blackboard ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mUploadedCount == mAllocations.size() && !mRepack ) {
            return;
        }

        const auto &info{ blackboard.Get<GeometryCullModuleInfo>() };
        const auto repack{ [&] {
            mAttributeBytes = 0;
            mIndexCount = 0;

            for ( core::usize index{}; index < mSources.size(); ++index ) {
                if ( !mSources[index] ) {
                    continue;
                }

                const auto &source{ mSources[index] };
                auto &allocation{ mAllocations[index] };
                allocation.mFirstIndex = core::as<core::u32>( mIndexCount );
                const core::usize streams{ source->IsSkinned() ? kMeshStreamCount : core::as<core::usize>( MeshStream::Joints0 ) };

                for ( core::usize stream{}; stream < streams; ++stream ) {
                    allocation.mStreams[stream] = mAttributeBytes + source->GetOffsets()[stream];
                }

                mAttributeBytes += source->GetAttributes().size();
                mIndexCount += source->GetIndices().size();
            }

            mUploadedCount = 0;
            mUploadedAttributeBytes = 0;
        } };

        if ( mRepack ) {
            repack();
            ctx.ReplaceBufferStorage( info.mAttributeBuffer, mAttributeBytes );
            ctx.ReplaceBufferStorage( info.mIndexBuffer, mIndexCount * sizeof( core::u32 ) );
        }

        const bool attributesMoved{ ctx.EnsureBufferCapacity( info.mAttributeBuffer, mAttributeBytes ) };
        const bool indicesMoved{ ctx.EnsureBufferCapacity( info.mIndexBuffer, mIndexCount * sizeof( core::u32 ) ) };
        const bool allocationsMoved{ ctx.EnsureBufferCapacity( info.mGeometryAllocBuffer, mAllocations.size() * sizeof( GeometryAllocation ) ) };

        // Fresh allocations are filled from the retained mesh sources in the
        // following upload pass. No prefix copy or transition is hidden here.
        if ( attributesMoved || indicesMoved || allocationsMoved ) {
            repack();
        }

        const auto baseAddress{ ctx.GetDeviceBufferAddress( info.mAttributeBuffer ) };
        const auto indexAddress{ ctx.GetDeviceBufferAddress( info.mIndexBuffer ) };

        for ( core::usize index{ mUploadedCount }; index < mAllocations.size(); ++index ) {
            if ( !mSources[index] ) {
                continue;
            }

            const auto &source{ mSources[index] };
            auto &allocation{ mAllocations[index] };
            allocation.mIndexBuffer = indexAddress + core::usize{ allocation.mFirstIndex } * sizeof( core::u32 );
            const core::usize streams{ source->IsSkinned() ? kMeshStreamCount : core::as<core::usize>( MeshStream::Joints0 ) };

            for ( core::usize stream{}; stream < streams; ++stream ) {
                allocation.mStreams[stream] += baseAddress;
            }
        }

        mRepack = false;
    }

    auto GeometryAllocator::Flush( CommandContext &ctx, core::Blackboard &blackboard ) -> void {
        MKT_BEGIN_PROFILER();
        const auto &info{ blackboard.Get<GeometryCullModuleInfo>() };
        // Deformable instances reuse their ranges. The existing upload pass declares these transfer writes.
        const auto baseAddress{ ctx.GetDeviceBufferAddress( info.mAttributeBuffer ) };

        for ( core::usize index{}; index < mUploadedCount; ++index ) {
            const auto &source{ mSources[index] };

            if ( !source || mUploadedRevisions[index] == source->GetRevision() ) {
                continue;
            }

            const auto attributes{ source->GetAttributes() };
            const auto positionStream{ core::as<core::usize>( MeshStream::Position ) };
            const auto offset{ mAllocations[index].mStreams[positionStream] - baseAddress - source->GetOffsets()[positionStream] };
            ctx.Copy( info.mAttributeBuffer, offset, attributes.data(), attributes.size() );
            mUploadedRevisions[index] = source->GetRevision();
        }

        for ( core::usize index{ mUploadedCount }; index < mAllocations.size(); ++index ) {
            const auto &source{ mSources[index] };

            if ( !source ) {
                continue;
            }

            const auto attributes{ source->GetAttributes() };
            const auto indices{ source->GetIndices() };
            const auto &allocation{ mAllocations[index] };

            ctx.Copy( info.mAttributeBuffer, mUploadedAttributeBytes, attributes.data(), attributes.size() );
            ctx.Copy( info.mIndexBuffer, core::usize{ allocation.mFirstIndex } * sizeof( core::u32 ), indices.data(), indices.size() * sizeof( core::u32 ) );

            mUploadedAttributeBytes += attributes.size();
            mUploadedRevisions[index] = source->GetRevision();
        }

        if ( mUploadedCount < mAllocations.size() ) {
            ctx.Copy( info.mGeometryAllocBuffer, mUploadedCount * sizeof( GeometryAllocation ),
                mAllocations.data() + mUploadedCount, ( mAllocations.size() - mUploadedCount ) * sizeof( GeometryAllocation ) );
        }

        mUploadedCount = mAllocations.size();
    }

    auto SceneGeometryCache::Resolve( entt::entity entity ) -> Entry & {
        const auto index{ entt::to_entity( entity ) };

        if ( index >= mEntries.size() ) {
            mEntries.resize( core::usize{ index } + 1 );
        }

        auto &entry{ mEntries[index] };

        if ( entry.mEntity != entity ) {
            entry = {};
            entry.mEntity = entity;
        }

        return entry;
    }

    auto GeometryCullModule::ResetGeometryData() -> void {
        // Drop scene identities before reusing the backing upload and geometry storage.
        mMeshBatchInfos.clear();
        mInstanceSlots.clear();
        mMaterialSlots.clear();
        mInstanceStates.clear();
        mMaterialStates.clear();
        mFreeInstances.clear();
        mFreeMaterials.clear();
        mGeometryList.clear();
        mMaterialsList.clear();
        mDrawInstances.clear();

        mInstanceUpload.Clear();
        mMaterialUpload.Clear();
        mSubsurfaceUpload.Clear();
        mSubsurfaceKernels.clear();
        mDrawInstanceUpload.Clear();

        // Retire pose-dependent allocations so the next scene cannot reuse stale skinning data.
        mSkinningSlots.clear();
        mSkinningStates.clear();
        mSkinningInfo.clear();
        mFreeSkinningSlots.clear();
        mDirtySkinningSlots.clear();
        mSkinningJobs.clear();
        mSkinningJobIndices.clear();
        mSkinningCacheBytes = 0;
        mWarnedSkinningCacheCapacity = false;

        // Force command generation to rebuild against the new allocation and draw lists.
        mGeometryAllocator.Clear();
        mIndirectCmds.clear();
        mDrawBounds.clear();
        mDirtyDrawBounds.clear();
        mFirstChangedDraw = 0;
        mTransparentDraws.clear();
        mOpaqueDrawCount = 0;
        mCommandsDirty = true;
        mHadCullingDebugCamera = false;
    }

    auto GeometryCullModule::SetFoliageWindEnabled( bool enabled ) -> void {
        // Keep color, depth, picking and shadow geometry on the same wind setting.
        mFoliageWindEnabled = enabled;
    }

    auto GeometryCullModule::SetScene( const scene::Scene *scene ) -> void {
        MKT_BEGIN_PROFILER_NAMED();
        eastl::shared_ptr<SceneGeometryCache> cache{};

        if ( scene ) {
            auto &shared{ scene->GetGeometryCache() };

            if ( !shared ) {
                shared = eastl::make_shared<SceneGeometryCache>();
            }

            cache = shared;
        }

        if ( mScene != scene || mSharedCache != cache ) {
            // Reuse arena storage, but release old scene identities and cached
            // batches. Graph barriers order the new uploads after prior draws.
            ResetGeometryData();
        }

        mScene = scene;
        mSharedCache = eastl::move( cache );
    }

    auto GeometryCullModule::SetCamera( const scene::Camera *camera ) -> void {
        MKT_BEGIN_PROFILER_NAMED();
        mCamera = camera;
    }

    auto GeometryCullModule::SetCullingDebugCamera( const scene::Camera *camera ) -> void {
        mCullingDebugCamera = camera;
    }

    auto GeometryCullModule::SetFrustumCullingEnabled( bool enabled ) -> void {
        mFrustumCullingEnabled = enabled;
    }

    auto GeometryCullModule::GetShadowCasterRevision() const -> core::u64 {
        return mShadowCasterRevision;
    }

    auto GeometryCullModule::SetMeshesVisible( bool visible ) -> void {
        mShadowCasterRevision += mMeshesVisible != visible ? 1 : 0;
        mMeshesVisible = visible;
    }

    auto GeometryCullModule::GetInstanceUploadStatistics() const -> const SceneUploadStatistics & {
        return mInstanceUpload.GetStatistics();
    }

    auto GeometryCullModule::GetMaterialUploadStatistics() const -> const SceneUploadStatistics & {
        return mMaterialUpload.GetStatistics();
    }

    auto GeometryCullModule::GetSharedMaterialCount() const -> core::usize {
        return mMaterialSlots.size();
    }

    auto GeometryCullModule::RegisterGeometryFilterPass( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Create the resources
        auto &geometryFilterInfo{ graph.GetOrCreate<GeometryCullModuleInfo>() };

        auto indirectCommandsDesc{ FGBufferDescription{}
            .SetName( "GeometryUnculledCommands_Buffer" )
            .SetUsage( renderer::rhi::BufferUsageFlagsBits::Structured |
                           renderer::rhi::BufferUsageFlagsBits::IndirectDraw |
                           renderer::rhi::BufferUsageFlagsBits::CopySource |
                           renderer::rhi::BufferUsageFlagsBits::CopyDest )
            .SetSizeBytes( 65536 )
            .SetHeapType( renderer::rhi::HeapType::eDeviceLocal ) };
        mUnculledIndirectBuffer = graph.Create( indirectCommandsDesc );
        geometryFilterInfo.mUnculledIndirectBuffer = mUnculledIndirectBuffer;

        geometryFilterInfo.mDrawBoundsBuffer = graph.Create( FGBufferDescription{}
            .SetName( "GeometryDrawBounds" )
            .SetUsage( renderer::rhi::BufferUsageFlagsBits::Structured |
                           renderer::rhi::BufferUsageFlagsBits::CopySource |
                           renderer::rhi::BufferUsageFlagsBits::CopyDest )
            .SetSizeBytes( 65536 ) );

        auto samplerDes{ FGSamplerDescription{}
            .SetName( "BasicSampler_Sampler01" )
            .SetFilter( rhi::SamplerFilter::eNearest )
            .SetWrap( renderer::rhi::SamplerWrapMode::eRepeat )
            .SetBorderColor( renderer::rhi::kColorWhite ) };
        geometryFilterInfo.mBasicSampler = graph.Create( samplerDes );

        // All material coverage tests must sample alpha with the same filter.
        // Nearest depth with linear shading can turn a translucent edge into
        // an opaque occluder. Retain the basic sampler for debug presentation.
        geometryFilterInfo.mMaterialSampler = graph.Create( FGSamplerDescription{}
            .SetName( "Geometry_MaterialSampler" )
            .SetMipLevels( 32.0f )
            .SetMaxAnisotropy( 8.0f )
            .SetFilter( rhi::SamplerFilter::eLinear )
            .SetWrap( renderer::rhi::SamplerWrapMode::eRepeat )
            .SetBorderColor( renderer::rhi::kColorWhite ) );

        mIndirectCmds.reserve( kMaxRenderableEntities );

        // One index arena and an attribute arena containing separate contiguous
        // streams per mesh. Static meshes omit the four skinning streams.
        // Start small for empty scenes. Preparation grows the arenas as needed;
        // the following upload pass restores mesh data after a replacement.
        geometryFilterInfo.mAttributeBuffer = graph.Create( FGBufferDescription{}
            .SetName( "GeometryAttributeArena" )
            .SetUsage( renderer::rhi::BufferUsageFlagsBits::Structured |
                           renderer::rhi::BufferUsageFlagsBits::CopySource |
                           renderer::rhi::BufferUsageFlagsBits::CopyDest )
            .SetSizeBytes( 1024 * 1024 ) );
        geometryFilterInfo.mIndexBuffer = graph.Create( FGBufferDescription{}
            .SetName( "GeometryIndexArena" )
            .SetUsage( renderer::rhi::BufferUsageFlagsBits::Structured |
                           renderer::rhi::BufferUsageFlagsBits::CopySource |
                           renderer::rhi::BufferUsageFlagsBits::CopyDest )
            .SetSizeBytes( 1024 * 1024 ) );

        ResetGeometryData();

        auto vertexDesc{ FGBufferDescription{}
            .SetName( "GeometryDescription_Buffer01" )
            .SetUsage( renderer::rhi::BufferUsageFlagsBits::Structured |
                           renderer::rhi::BufferUsageFlagsBits::CopySource |
                           renderer::rhi::BufferUsageFlagsBits::CopyDest )
            .SetSizeBytes( 65536 )
            .SetHeapType( renderer::rhi::HeapType::eDeviceLocal ) };
        geometryFilterInfo.mGeometryAllocBuffer = graph.Create( vertexDesc );

        auto materialsDesc{ FGBufferDescription{}
            .SetName( "GeometryMaterials_Buffer01" )
            .SetUsage( renderer::rhi::BufferUsageFlagsBits::Structured |
                           renderer::rhi::BufferUsageFlagsBits::CopySource |
                           renderer::rhi::BufferUsageFlagsBits::CopyDest )
            .SetSizeBytes( 65536 )
            .SetHeapType( renderer::rhi::HeapType::eDeviceLocal ) };
        geometryFilterInfo.mMaterialsBuffer = graph.Create( materialsDesc );
        geometryFilterInfo.mSubsurfaceKernels = graph.Create( FGBufferDescription{}
            .SetName( "SubsurfaceKernels" )
            .SetUsage( renderer::rhi::BufferUsageFlagsBits::Structured |
                           renderer::rhi::BufferUsageFlagsBits::CopySource |
                           renderer::rhi::BufferUsageFlagsBits::CopyDest )
            .SetSizeBytes( sizeof( material::SubsurfaceKernel ) ) );

        auto geometryDesc{ FGBufferDescription{}
            .SetName( "GeometryRender_Buffer01" )
            .SetUsage( renderer::rhi::BufferUsageFlagsBits::Structured |
                           renderer::rhi::BufferUsageFlagsBits::CopySource |
                           renderer::rhi::BufferUsageFlagsBits::CopyDest )
            .SetSizeBytes( 65536 )
            .SetHeapType( renderer::rhi::HeapType::eDeviceLocal ) };
        geometryFilterInfo.mInstanceBuffer = graph.Create( geometryDesc );

        geometryFilterInfo.mGeometryBuffer = graph.Create( FGBufferDescription{}
            .SetName( "SceneGeometryView" )
            .SetUsage( renderer::rhi::BufferUsageFlagsBits::Structured |
                           renderer::rhi::BufferUsageFlagsBits::CopySource |
                           renderer::rhi::BufferUsageFlagsBits::CopyDest )
            .SetSizeBytes( 16 ) );

        geometryFilterInfo.mDrawInstanceBuffer = graph.Create( FGBufferDescription{}
            .SetName( "SceneDrawInstances" )
            .SetUsage( renderer::rhi::BufferUsageFlagsBits::Structured |
                           renderer::rhi::BufferUsageFlagsBits::CopySource |
                           renderer::rhi::BufferUsageFlagsBits::CopyDest )
            .SetSizeBytes( 65536 ) );

        const auto scratch = [&]( const char *name, core::usize size ) {
            return graph.Create( FGBufferDescription{}
                .SetName( name )
                .SetUsage( renderer::rhi::BufferUsageFlagsBits::Structured |
                               renderer::rhi::BufferUsageFlagsBits::CopySource |
                               renderer::rhi::BufferUsageFlagsBits::CopyDest )
                .SetSizeBytes( size ) );
        };

        mSubsurfaceScratch = scratch( "SubsurfaceKernelPayload", sizeof( material::SubsurfaceKernel ) + 4 );
        mInstanceScratch = scratch( "InstanceScatterPayload", SparseBufferUpload<MeshGeometryInfo>::kScratchBytes );
        mMaterialScratch = scratch( "MaterialScatterPayload", SparseBufferUpload<MeshMaterialInfo>::kScratchBytes );
        mDrawInstanceScratch = scratch( "DrawInstanceScatterPayload", SparseBufferUpload<DrawInstance>::kScratchBytes );

        auto skinningDesc{ FGBufferDescription{}
            .SetName( "Geometry_SkinningBuffer01" )
            .SetUsage( renderer::rhi::BufferUsageFlagsBits::Structured |
                           renderer::rhi::BufferUsageFlagsBits::CopySource |
                           renderer::rhi::BufferUsageFlagsBits::CopyDest )
            .SetSizeBytes( 65536 )
            .SetHeapType( renderer::rhi::HeapType::eDeviceLocal ) };
        geometryFilterInfo.mSkinningBuffer = graph.Create( skinningDesc );

        // This CPU preparation pass publishes new storage without recording GPU
        // work. The upload pass then transitions the actual replacement buffers.
        const auto declareWrites{ [this]( FGNodeBuilder &builder, GeometryCullModuleInfo &data ) -> void {
            builder.UseResource( data.mAttributeBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            builder.UseResource( data.mIndexBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            builder.UseResource( data.mGeometryAllocBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            builder.UseResource( data.mGeometryBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );

            for ( auto handle: { data.mInstanceBuffer, data.mDrawInstanceBuffer, mInstanceScratch, mMaterialScratch, mDrawInstanceScratch, mSubsurfaceScratch } ) {
                builder.UseResource( handle, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            }

            builder.UseResource( data.mSubsurfaceKernels, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            builder.UseResource( data.mMaterialsBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            builder.UseResource( data.mSkinningBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );

            builder.UseResource( data.mUnculledIndirectBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            builder.UseResource( data.mDrawBoundsBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
        } };

        graph.RegisterPass<GeometryCullModuleInfo>(
            "GeometryPrepare",
            FGPassType::eTransfer,
            declareWrites,
            [this]( CommandContext &ctx, core::Blackboard &b ) {
                InitGeometryData( ctx, b );
                const auto &info{ b.Get<GeometryCullModuleInfo>() };
                const auto grow{ [&]( FGBufferHandle handle, const auto &values, auto &upload ) {
                    if ( ctx.EnsureBufferCapacity( handle, values.size() * sizeof( values[0] ) ) ) {
                        for ( core::u32 slot{}; slot < values.size(); ++slot ) {
                            upload.MarkDirty( slot );
                        }
                    }
                } };

                grow( info.mInstanceBuffer, mGeometryList, mInstanceUpload );
                grow( info.mMaterialsBuffer, mMaterialsList, mMaterialUpload );
                grow( info.mDrawInstanceBuffer, mDrawInstances, mDrawInstanceUpload );
                grow( info.mSubsurfaceKernels, mSubsurfaceKernels, mSubsurfaceUpload );

                ctx.EnsureBufferCapacity(
                    mSubsurfaceScratch,
                    eastl::max<core::usize>( 1, eastl::min<core::usize>( mSubsurfaceKernels.size(), 65536 ) ) *
                        ( sizeof( material::SubsurfaceKernel ) + 4 ) );
            } );

        graph.RegisterPass<GeometryCullModuleInfo>(
            "GeometryFilter", FGPassType::eTransfer, declareWrites,
            [this]( CommandContext &ctx, core::Blackboard &b ) {
                const auto &info{ b.Get<GeometryCullModuleInfo>() };

                mGeometryAllocator.Flush( ctx, b );

                UploadSkinningData( ctx, b );
                UploadIndirectDraw( ctx, b );

                mInstanceUpload.Stage( ctx, info.mInstanceBuffer, mInstanceScratch, { mGeometryList.data(), mGeometryList.size() } );
                mSubsurfaceUpload.Stage( ctx, info.mSubsurfaceKernels, mSubsurfaceScratch, { mSubsurfaceKernels.data(), mSubsurfaceKernels.size() } );
                mMaterialUpload.Stage( ctx, info.mMaterialsBuffer, mMaterialScratch, { mMaterialsList.data(), mMaterialsList.size() } );
                mDrawInstanceUpload.Stage( ctx, info.mDrawInstanceBuffer, mDrawInstanceScratch, { mDrawInstances.data(), mDrawInstances.size() } );

                struct View {
                    core::u64 mInstances;
                    core::u64 mDraws;
                } view{
                    .mInstances = ctx.GetDeviceBufferAddress( info.mInstanceBuffer ),
                    .mDraws = ctx.GetDeviceBufferAddress( info.mDrawInstanceBuffer )
                };

                ctx.Copy( info.mGeometryBuffer, 0, &view, sizeof( view ) );
            } );
    }

    auto GeometryCullModule::RegisterSceneUploadPass( FrameGraph &graph ) -> void {
        const auto pipeline{ graph.Create( FGPipelineDescription{}
            .SetName( "SceneScatterUpload" )
            .SetPipelineType( renderer::rhi::PipelineType::eCompute )
            .PushShader( "SceneScatterUpload_Comp.slang", FGStageType::eCompute ) ) };

        graph.RegisterPass(
            "SceneScatterUpload",
            FGPassType::eCompute,
            [this]( FGNodeBuilder &builder, core::Blackboard &b ) {
                const auto &info{ b.Get<GeometryCullModuleInfo>() };

                for ( auto handle: { info.mInstanceBuffer, info.mMaterialsBuffer, info.mDrawInstanceBuffer, info.mSubsurfaceKernels } ) {
                    builder.UseResource( handle, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
                }

                for ( auto handle: { mInstanceScratch, mMaterialScratch, mDrawInstanceScratch, mSubsurfaceScratch } ) {
                    builder.UseResource( handle, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                }
            },
            [this, pipeline]( CommandContext &ctx, core::Blackboard &b ) {
                const auto &info{ b.Get<GeometryCullModuleInfo>() };

                mInstanceUpload.Scatter( ctx, info.mInstanceBuffer, mInstanceScratch, pipeline );
                mMaterialUpload.Scatter( ctx, info.mMaterialsBuffer, mMaterialScratch, pipeline );
                mSubsurfaceUpload.Scatter( ctx, info.mSubsurfaceKernels, mSubsurfaceScratch, pipeline );
                mDrawInstanceUpload.Scatter( ctx, info.mDrawInstanceBuffer, mDrawInstanceScratch, pipeline );
            } );
    }

    auto GeometryCullModule::RegisterSkinningPass( FrameGraph &graph ) -> void {
        auto &info{ graph.GetOrCreate<GeometryCullModuleInfo>() };
        info.mSkinningCacheBuffer = graph.Create( FGBufferDescription{}
            .SetName( "GeometrySkinningCache" )
            .SetUsage( renderer::rhi::BufferUsageFlagsBits::Structured |
                           renderer::rhi::BufferUsageFlagsBits::CopySource |
                           renderer::rhi::BufferUsageFlagsBits::CopyDest )
            .SetSizeBytes( 65536 ) );

        const auto pipeline{ graph.Create( FGPipelineDescription{}
            .SetName( "GeometrySkinning_Pipeline" )
            .SetPipelineType( renderer::rhi::PipelineType::eCompute )
            .PushShader( "GeometrySkinning_Comp.slang", FGStageType::eCompute ) ) };

        graph.RegisterPass<GeometryCullModuleInfo>(
            "GeometrySkinning",
            FGPassType::eCompute,
            []( FGNodeBuilder &builder, GeometryCullModuleInfo &data ) {
                builder.UseResource( data.mAttributeBuffer, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( data.mSkinningBuffer, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( data.mMaterialsBuffer, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( data.mSkinningCacheBuffer, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
            },
            [this, pipeline]( CommandContext &ctx, core::Blackboard &blackboard ) {
                if ( mSkinningJobs.empty() ) {
                    return;
                }

                const auto &data{ blackboard.Get<GeometryCullModuleInfo>() };
                struct Parameters {
                    core::u64 mSourcePositions{};
                    core::u64 mSourceNormals{};
                    core::u64 mJoints0{};
                    core::u64 mWeights0{};
                    core::u64 mJoints1{};
                    core::u64 mWeights1{};
                    core::u64 mPalette{};
                    core::u64 mPositions{};
                    core::u64 mNormals{};
                    core::u32 mVertexCount{};
                    core::u32 mHasPalette{};
                    core::u64 mUV0{};
                    core::u64 mUV1{};
                    core::u64 mMaterials{};
                    core::u32 mMaterialIndex{};
                    core::u32 mSampler{};
                };

                const auto palettes{ ctx.GetDeviceBufferAddress( data.mSkinningBuffer ) };

                ctx.BindPipeline( pipeline );

                for ( const auto &job: mSkinningJobs ) {
                    // Resolve mesh metadata once on the CPU, rather than
                    // chasing the allocation table for every GPU vertex.
                    const auto &streams{ mGeometryAllocator.Get( job.mGeometryIndex ).mStreams };
                    const Parameters parameters{
                        .mSourcePositions = streams[core::as<core::usize>( MeshStream::Position )],
                        .mSourceNormals = streams[core::as<core::usize>( MeshStream::Normal )],
                        .mJoints0 = streams[core::as<core::usize>( MeshStream::Joints0 )],
                        .mWeights0 = streams[core::as<core::usize>( MeshStream::Weights0 )],
                        .mJoints1 = streams[core::as<core::usize>( MeshStream::Joints1 )],
                        .mWeights1 = streams[core::as<core::usize>( MeshStream::Weights1 )],
                        .mPalette = job.mPaletteIndex != 0xffffffffu ? palettes + core::usize{ job.mPaletteIndex } * MKT_SIZEOF( SkinningPalette ) : 0,
                        .mPositions = job.mPositions,
                        .mNormals = job.mNormals,
                        .mVertexCount = job.mVertexCount,
                        .mHasPalette = job.mPaletteIndex != 0xffffffffu ? 1u : 0u,
                        .mUV0 = streams[core::as<core::usize>( MeshStream::UV0 )],
                        .mUV1 = streams[core::as<core::usize>( MeshStream::UV1 )],
                        .mMaterials = ctx.GetDeviceBufferAddress( data.mMaterialsBuffer ),
                        .mMaterialIndex = job.mMaterialIndex,
                        .mSampler = ctx.PushSampler( data.mMaterialSampler )
                    };

                    ctx.PushConstants( parameters );

                    ctx.Dispatch( ( job.mVertexCount + 127 ) / 128, 1, 1 );
                }
            } );
    }

    auto GeometryCullModule::RetireSkinningSlots() -> void {
        for ( auto it{ mSkinningSlots.begin() }; it != mSkinningSlots.end(); ) {
            const auto slot{ it->second };

            if ( mSkinningStates[slot].mLastSeen != mFrame ) {
                mSkinningStates[slot] = {};
                mFreeSkinningSlots.push_back( slot );
                it = mSkinningSlots.erase( it );
            }
            else {
                ++it;
            }
        }
    }

    auto GeometryCullModule::ResolveSkinningPalette( core::u64 animatorID, const asset::MeshNode &mesh, core::RefConst<asset::Model> model ) -> core::i32 {
        const auto *animator{ animation::AnimationSystem::Get()->GetAnimator( animatorID ) };
        const auto &binding{ mesh.GetSkinBinding() };

        if ( !animator || binding.mJoints.empty() || animator->GetJointModelMatrices( binding.mRigIndex ).empty() ) {
            return -1;
        }

        const auto key{ std::make_pair( animatorID, &mesh ) };
        // Existing palettes and new bindings share a single hash-table lookup.
        auto [found, inserted]{ mSkinningSlots.try_emplace( key, kInvalidSlot ) };

        if ( inserted ) {
            if ( mFreeSkinningSlots.empty() && mSkinningStates.size() >= kMaxRenderableEntities ) {
                mSkinningSlots.erase( found );
                MKT_THROW_RUNTIME_ERROR( "Skinning palette count exceeded the scene renderable capacity." );
            }

            core::u32 slot{};

            if ( !mFreeSkinningSlots.empty() ) {
                slot = mFreeSkinningSlots.back();
                mFreeSkinningSlots.pop_back();
            }
            else {
                slot = static_cast<core::u32>( mSkinningStates.size() );
                mSkinningStates.emplace_back();
                mSkinningInfo.emplace_back();
            }

            mSkinningStates[slot].mModel = model;
            found->second = slot;
        }

        const auto slot{ found->second };
        auto &state{ mSkinningStates[slot] };
        const auto revision{ animator->GetPoseRevision( binding.mRigIndex ) };

        if ( !state.mValid || state.mRevision != revision ) {
            ++mShadowCasterRevision;
            const auto &pose{ animator->GetJointModelMatrices( binding.mRigIndex ) };
            auto &palette{ mSkinningInfo[slot].mBoneTransforms };
            state.mValid = animation::BuildSkinningPalette( binding, { pose.data(), pose.size() }, { palette.data(), palette.size() } );
            state.mRevision = revision;

            if ( state.mValid ) {
                mDirtySkinningSlots.push_back( slot );
            }
        }

        state.mLastSeen = mFrame;
        return state.mValid ? static_cast<core::i32>( slot + 1 ) : -1;
    }

    auto GeometryCullModule::CacheSkinning( MeshGeometryInfo &geometry, const MeshRenderData &mesh ) -> void {
        geometry.mSkinnedPositions = 0;
        geometry.mSkinnedNormals = 0;

        const auto *surface{ dynamic_cast<const material::SurfaceMaterial *>( mMaterialStates[geometry.mMaterialIndex].mSource.GetPtr() ) };
        const bool displaced{ surface && surface->GetHeightMapping().mMode == material::HeightMappingMode::eDisplacement &&
                              !surface->GetTexturePath( material::MapType::eHeight ).IsEmpty() && surface->IsTextureUsed( material::MapType::eHeight ) };
        const bool skinned{ geometry.mSkinningMatricesID > 0 && mesh.IsSkinned() };
        if ( !skinned && !displaced ) {
            return;
        }
        const core::u32 palette{ skinned ? core::as<core::u32>( geometry.mSkinningMatricesID - 1 ) : 0xffffffffu };
        const auto key{ std::make_pair( geometry.mGeometryIndex, ( core::u64{ palette } << 32 ) | ( displaced ? geometry.mMaterialIndex + 1u : 0u ) ) };
        auto found{ mSkinningJobIndices.find( key ) };

        if ( found == mSkinningJobIndices.end() ) {
            const core::usize streamBytes{ core::usize{ mesh.GetVertexCount() } * sizeof( core::float4 ) };
            constexpr core::usize capacity{ core::usize{ kSkinningCacheSizeMB } * 1024 * 1024 };

            if ( streamBytes > ( capacity - mSkinningCacheBytes ) / 2 ) {
                if ( displaced ) {
                    MKT_THROW_RUNTIME_ERROR( "Displaced geometry exceeds the shared deformation cache capacity." );
                }
                if ( !mWarnedSkinningCacheCapacity ) {
                    MKT_CORE_LOGGER_WARN( "Compute skinning cache capacity reached ({} MiB). Remaining meshes use vertex skinning.", kSkinningCacheSizeMB );
                    mWarnedSkinningCacheCapacity = true;
                }

                return;
            }

            const auto jobIndex{ core::as<core::u32>( mSkinningJobs.size() ) };
            mSkinningJobs.push_back( { .mGeometryIndex = geometry.mGeometryIndex,
                .mPaletteIndex = palette,
                .mMaterialIndex = geometry.mMaterialIndex,
                .mVertexCount = mesh.GetVertexCount(),
                .mPositions = mSkinningCacheAddress + mSkinningCacheBytes,
                .mNormals = mSkinningCacheAddress + mSkinningCacheBytes + streamBytes } );
            mSkinningCacheBytes += streamBytes * 2;
            found = mSkinningJobIndices.emplace( key, jobIndex ).first;
        }

        const auto &job{ mSkinningJobs[found->second] };
        geometry.mSkinnedPositions = job.mPositions;
        geometry.mSkinnedNormals = job.mNormals;
    }

    auto GeometryCullModule::PrepareSkinningData( CommandContext &context, core::Blackboard &b ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        const auto cacheAddress{ mSkinningCacheAddress };

        RetireSkinningSlots();
        const auto &info{ b.Get<GeometryCullModuleInfo>() };

        if ( context.EnsureBufferCapacity( info.mSkinningBuffer, mSkinningInfo.size() * MKT_SIZEOF( SkinningPalette ) ) ) {
            mDirtySkinningSlots.clear();

            for ( core::u32 slot{}; slot < mSkinningInfo.size(); ++slot ) {
                mDirtySkinningSlots.push_back( slot );
            }
        }

        if ( context.EnsureBufferCapacity( info.mSkinningCacheBuffer, mSkinningCacheBytes ) ) {
            const auto address{ context.GetDeviceBufferAddress( info.mSkinningCacheBuffer ) };

            for ( auto &job: mSkinningJobs ) {
                job.mPositions = address + ( job.mPositions - mSkinningCacheAddress );
                job.mNormals = address + ( job.mNormals - mSkinningCacheAddress );
            }

            mSkinningCacheAddress = address;
        }

        const auto address{ mSkinningCacheAddress };

        if ( address != cacheAddress ) {
            for ( core::u32 slot{}; slot < mInstanceStates.size(); ++slot ) {
                auto &geometry{ mGeometryList[slot] };

                if ( mInstanceStates[slot].mLastSeen == mFrame && geometry.mSkinnedPositions ) {
                    geometry.mSkinnedPositions = address + ( geometry.mSkinnedPositions - cacheAddress );
                    geometry.mSkinnedNormals = address + ( geometry.mSkinnedNormals - cacheAddress );
                    mInstanceUpload.MarkDirty( slot );
                }
            }
        }
    }

    auto GeometryCullModule::UploadSkinningData( CommandContext &context, core::Blackboard &b ) -> void {
        MKT_BEGIN_PROFILER();

        const auto &info{ b.Get<GeometryCullModuleInfo>() };
        eastl::sort( mDirtySkinningSlots.begin(), mDirtySkinningSlots.end() );

        // Transfer only changed contiguous ranges into the graph's persistent
        // buffer. Its write/read history orders slot reuse after earlier GPU readers;
        // CPU retirement never overwrites host memory still consumed by the GPU.
        for ( core::usize first{}; first < mDirtySkinningSlots.size(); ) {
            const auto slot{ mDirtySkinningSlots[first] };
            core::usize count{ 1 };

            while ( first + count < mDirtySkinningSlots.size() && mDirtySkinningSlots[first + count] == slot + count ) {
                ++count;
            }

            context.Copy( info.mSkinningBuffer, core::usize{ slot } * sizeof( SkinningPalette ),
                mSkinningInfo.data() + slot, count * sizeof( SkinningPalette ) );
            first += count;
        }

        mDirtySkinningSlots.clear();
    }

    auto GeometryCullModule::RegisterFrustumCullPass( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER();

        auto &info{ graph.GetOrCreate<GeometryCullModuleInfo>() };
        auto indirectCommandsDesc{ FGBufferDescription{}
            .SetName( "GeometryIndirectCommands_Buffer" )
            .SetUsage( renderer::rhi::BufferUsageFlagsBits::Structured |
                           renderer::rhi::BufferUsageFlagsBits::IndirectDraw |
                           renderer::rhi::BufferUsageFlagsBits::CopySource |
                           renderer::rhi::BufferUsageFlagsBits::CopyDest )
            .SetSizeBytes( 65536 )
            .SetHeapType( renderer::rhi::HeapType::eDeviceLocal ) };
        mIndirectBuffer = graph.Create( indirectCommandsDesc );
        info.mIndirectBuffer = mIndirectBuffer;

        const auto pipeline{ graph.Create( FGPipelineDescription{}
            .SetName( "GeometryFrustumCull_Pipeline" )
            .SetPipelineType( renderer::rhi::PipelineType::eCompute )
            .PushShader( "GeometryFrustumCull_Comp.slang", FGStageType::eCompute ) ) };

        graph.RegisterPass<GeometryCullModuleInfo>(
            "GeometryFrustumCull",
            FGPassType::eCompute,
            []( FGNodeBuilder &builder, GeometryCullModuleInfo &data ) {
                builder.UseResource( data.mUnculledIndirectBuffer, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( data.mDrawBoundsBuffer, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( data.mIndirectBuffer, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
            },

            [this, pipeline]( CommandContext &ctx, core::Blackboard &blackboard ) {
                const auto drawCount{ core::as<core::u32>( mIndirectCmds.size() ) };

                if ( !mCamera || drawCount == 0 ) {
                    return;
                }

                const auto &data{ blackboard.Get<GeometryCullModuleInfo>() };
                const auto rows{ glm::transpose( mCamera->GetViewProjection() ) };
                struct Parameters {
                    eastl::array<core::float4, 6> mPlanes{};
                    core::u64 mSource{};
                    core::u64 mBounds{};
                    core::u64 mOutput{};
                    core::u32 mCount{};
                    core::u32 mEnabled{};
                } parameters{
                    .mPlanes = { rows[3] + rows[0], rows[3] - rows[0], rows[3] + rows[1],
                        rows[3] - rows[1], rows[2], rows[3] - rows[2] },
                    .mSource = ctx.GetDeviceBufferAddress( data.mUnculledIndirectBuffer ),
                    .mBounds = ctx.GetDeviceBufferAddress( data.mDrawBoundsBuffer ),
                    .mOutput = ctx.GetDeviceBufferAddress( data.mIndirectBuffer ),
                    .mCount = drawCount,
                    .mEnabled = mFrustumCullingEnabled ? 1u : 0u
                };

                ctx.BindPipeline( pipeline );
                ctx.PushConstants( parameters );
                ctx.Dispatch( ( drawCount + 63 ) / 64, 1, 1 );
            } );
    }

    auto GeometryCullModule::FindInstance( entt::entity entity ) const -> core::u32 {
        const auto index{ entt::to_entity( entity ) };

        if ( index < mInstanceSlots.size() ) {
            const auto slot{ mInstanceSlots[index] };

            if ( slot != kInvalidSlot && mInstanceStates[slot].mEntity == entity ) {
                return slot;
            }
        }

        return kInvalidSlot;
    }

    auto GeometryCullModule::ResolveInstance( entt::entity entity ) -> core::u32 {
        if ( const auto slot{ FindInstance( entity ) }; slot != kInvalidSlot ) {
            mInstanceStates[slot].mLastSeen = mFrame;
            return slot;
        }

        // entt::to_entity(entity) extracts the entity index, removing its
        // generation/version bits. Despite its name, it returns an integer.
        // For example, after entity index 42, generation 1 is destroyed, EnTT may create index 42, generation 2.
        // The tradeoff is memory as this lookup array grows to the highest encountered entity index, rather than
        // just the number of renderable entities.
        const auto index{ entt::to_entity( entity ) };

        if ( index >= mInstanceSlots.size() ) {
            mInstanceSlots.resize( core::usize{ index } + 1, kInvalidSlot );
        }

        core::u32 slot{ mInstanceSlots[index] };

        // A recycled entity index can reuse its old GPU slot immediately. Reset
        // every cached association, even if its new generation uses the same mesh.
        if ( slot == kInvalidSlot ) {
            if ( !mFreeInstances.empty() ) {
                slot = mFreeInstances.back();
                mFreeInstances.pop_back();
            }
            else {
                slot = core::as<core::u32>( mInstanceStates.size() );

                if ( slot >= kMaxRenderableEntities ) {
                    MKT_THROW_RUNTIME_ERROR( "Persistent instance table capacity exceeded." );
                }

                mInstanceStates.emplace_back();
                mGeometryList.emplace_back();
            }
        }

        mInstanceStates[slot] = { .mEntity = entity, .mLastSeen = mFrame };
        mGeometryList[slot] = {};
        mInstanceSlots[index] = slot;
        return slot;
    }

    auto GeometryCullModule::ResolveMesh( core::u32 instanceSlot, const MeshRenderDataHandle &mesh ) -> void {
        auto &instance{ mInstanceStates[instanceSlot] };

        if ( instance.mMesh == mesh.GetPtr() ) {
            mGeometryAllocator.Touch( instance.mGeometryIndex, mFrame );
            return;
        }

        const auto &allocation{ mGeometryAllocator.GetOrAllocate( mesh ) };
        const core::u32 index{ allocation.mAllocationIndex };
        mGeometryAllocator.Touch( index, mFrame );

        if ( index >= mMeshBatchInfos.size() ) {
            mMeshBatchInfos.resize( core::usize{ index } + 1 );
        }

        auto &batch{ mMeshBatchInfos[index] };
        batch.mGeometryIndex = index;
        instance.mMesh = mesh.GetPtr();
        instance.mGeometryIndex = index;
    }

    auto GeometryCullModule::ResolveMaterial( const material::MaterialHandle &source, core::u32 cachedSlot ) -> core::u32 {
        if ( cachedSlot < mMaterialStates.size() && mMaterialStates[cachedSlot].mSource == source ) {
            if ( mMaterialStates[cachedSlot].mLastSeen != mFrame ) {
                UpdateMaterialClassification( cachedSlot );
            }

            mMaterialStates[cachedSlot].mLastSeen = mFrame;
            return cachedSlot;
        }

        // Hash only a new or reassigned material. Shared materials still resolve
        // to one slot, and its owning handle prevents pointer identity reuse.
        auto found{ mMaterialSlots.find( source.GetPtr() ) };
        core::u32 slot{};

        if ( found == mMaterialSlots.end() ) {
            if ( !mFreeMaterials.empty() ) {
                slot = mFreeMaterials.back();
                mFreeMaterials.pop_back();
            }
            else {
                slot = core::as<core::u32>( mMaterialStates.size() );

                if ( slot >= kMaxRenderableEntities ) {
                    MKT_THROW_RUNTIME_ERROR( "Persistent material table capacity exceeded." );
                }

                mMaterialStates.emplace_back();
                mMaterialsList.emplace_back();
            }

            mMaterialStates[slot] = { source, 0, mFrame };
            mMaterialSlots.emplace( source.GetPtr(), slot );
        }
        else {
            slot = found->second;
        }

        if ( !mMaterialStates[slot].mClassificationValid || mMaterialStates[slot].mLastSeen != mFrame ) {
            UpdateMaterialClassification( slot );
        }

        mMaterialStates[slot].mLastSeen = mFrame;
        return slot;
    }

    auto GeometryCullModule::UpdateMaterialClassification( core::u32 slot ) -> void {
        auto &state{ mMaterialStates[slot] };
        const auto *source{ core::checked_cast<const material::SurfaceMaterial *>( state.mSource.GetPtr() ) };
        const auto revision{ source->GetRevision() };

        if ( !state.mClassificationValid || state.mClassificationRevision != revision ) {
            state.mAlphaMode = core::as<core::u32>( source->GetAlphaMask() );
            state.mFaceFlags = source->IsDoubleSided() ? 1u : 0u;
            state.mClassificationRevision = revision;
            state.mClassificationValid = true;
        }
    }

    auto GeometryCullModule::UpdateMaterials( CommandContext &ctx ) -> void {
        mHasStylizedContours = false;
        // All instances have marked their slots. Inspect each active material
        // once per view/frame, including edits that leave its identity unchanged.
        for ( core::u32 slot{}; slot < mMaterialStates.size(); ++slot ) {
            auto &state{ mMaterialStates[slot] };

            if ( !state.mSource || state.mLastSeen != mFrame ) {
                continue;
            }

            auto *pSurfaceMaterial{ core::checked_cast<material::SurfaceMaterial *>( state.mSource.GetPtr() ) };
            pSurfaceMaterial->UpdateTextureResidency();
            const auto textureIndex{ [&]( material::MapType type ) -> core::i32 {
                // Disabled slots retain their texture but declare no shader dependency.
                return pSurfaceMaterial->IsTextureUsed( type )
                           ? PushTextureID( ctx, pSurfaceMaterial->GetTexture( type ) )
                           : kInvalidTextureID;
            } };
            MeshMaterialInfo material{};
            // Materials
            material.mBaseColorFactor = pSurfaceMaterial->GetBaseColorFactor();
            material.mEmissiveFactor = core::float4{ pSurfaceMaterial->GetEmissiveFactor(), 1.0f };
            material.mDiffuseFactor = pSurfaceMaterial->GetDiffuseFactor();
            material.mSpecularFactor = pSurfaceMaterial->GetSpecularFactor();

            material.mWorkflow = core::as<core::i32>( pSurfaceMaterial->GetWorkflow() );

            material.mMetallicFactor = pSurfaceMaterial->GetMetallicFactor();
            if ( pSurfaceMaterial->GetShadingModel() == material::ShadingModel::eConductor ) {
                // Raster preview keeps its RGB metal BRDF; path tracing uses complex IOR directly.
                material.mBaseColorFactor *= core::float4{ pSurfaceMaterial->GetConductorReflectance(), 1 };
                material.mMetallicFactor = 1;
                material.mWorkflow = core::as<core::i32>( material::Workflow::eMetallicRoughness );
            }
            material.mRoughnessFactor = pSurfaceMaterial->GetRoughnessFactor();
            material.mEmissiveStrength = pSurfaceMaterial->GetEmissiveStrength();
            material.mAlphaMask = core::as<core::f32>( pSurfaceMaterial->GetAlphaMask() );
            material.mAlphaMaskCutoff = pSurfaceMaterial->GetAlphaMaskCutoff();

            // UV Sets
            material.mBaseColorTextureSet = pSurfaceMaterial->GetBaseColorTextureSet();
            material.mMetallicRoughnessTextureSet = pSurfaceMaterial->GetMetallicRoughnessTextureSet();
            material.mSpecularGlossinessSet = pSurfaceMaterial->GetSpecularGlossinessSet();
            material.mNormalTextureSet = pSurfaceMaterial->GetNormalTextureSet();
            material.mClearCoatNormalTextureSet = pSurfaceMaterial->GetClearCoatNormalTextureSet();
            material.mClearCoatNormalScale = pSurfaceMaterial->GetClearCoatNormalScale();
            material.mNormalScale = pSurfaceMaterial->GetNormalScale();
            material.mUvTransform = pSurfaceMaterial->GetUvTransform();
            const auto &height{ pSurfaceMaterial->GetHeightMapping() };
            material.mHeightMapping = { height.mScale, height.mMidlevel, core::as<float>( height.mMode ), core::as<float>( height.mSteps ) };
            material.mHeightIndex = textureIndex( material::MapType::eHeight );
            material.mHeightUVSet = height.mUVSet;

            const auto &checker{ pSurfaceMaterial->GetCheckerboard() };
            material.mCheckerboardColorA = { checker.mColorA, checker.mEnabled ? 1.0f : 0.0f };
            material.mCheckerboardColorB = { checker.mColorB, checker.mCellSize };
            material.mCheckerboardOffset = { checker.mOffset, 0.0f };
            material.mClearCoatNormalIndex = textureIndex( material::MapType::eClearCoatNormal );
            material.mOcclusionTextureSet = pSurfaceMaterial->GetOcclusionTextureSet();
            material.mEmissiveTextureSet = pSurfaceMaterial->GetEmissiveTextureSet();

            // Texture
            material.mAlbedoIndex = textureIndex( material::MapType::eBaseColor );
            material.mDiffuseIndex = textureIndex( material::MapType::eDiffuse );
            material.mNormalIndex = textureIndex( material::MapType::eNormal );
            material.mEmissiveIndex = textureIndex( material::MapType::eEmissive );
            material.mAoIndex = textureIndex( material::MapType::eAmbientOcclusion );
            material.mMetallicIndex = textureIndex( material::MapType::eMetallic );
            material.mRoughnessIndex = textureIndex( material::MapType::eRoughness );
            material.mMetallicRoughnessIndex = textureIndex( material::MapType::eMetallicRoughness );
            material.mSpecularGlossinessIndex = textureIndex( material::MapType::eSpecularGlossiness );

            material.mIsBloomy = pSurfaceMaterial->IsBloomy();
            const auto &subsurface{ pSurfaceMaterial->GetSubsurface() };
            material.mSubsurfaceColorOpacity = { subsurface.mColor, subsurface.mOpacity };
            material.mSubsurface = {
                subsurface.mScatterScale, subsurface.mStrength,
                core::as<core::f32>( pSurfaceMaterial->GetShadingModel() ),
                core::as<core::f32>( pSurfaceMaterial->GetSurfaceShading().mCellBands )
            };

            const auto &shading{ pSurfaceMaterial->GetSurfaceShading() };
            material.mSurfaceShading = {
                shading.mCoatWeight,
                shading.mCoatRoughness,
                shading.mToonThreshold,
                shading.mToonSoftness
            };

            if ( pSurfaceMaterial->GetShadingModel() == material::ShadingModel::eDielectric ) {
                const auto &dielectric{ pSurfaceMaterial->GetDielectric() };
                material.mSurfaceShading = {
                    dielectric.mIndexOfRefraction,
                    dielectric.mTransmission,
                    dielectric.mThickness,
                    dielectric.mAttenuationDistance
                };
                material.mSubsurfaceColorOpacity = { dielectric.mAttenuationColor, 1.0f };
            }

            if ( pSurfaceMaterial->GetShadingModel() == material::ShadingModel::eSubsurfaceScattering ) {
                material.mSurfaceShading = {
                    subsurface.mTranslucency,
                    subsurface.mDepthRejection,
                    core::as<core::f32>( subsurface.mSamples ),
                    subsurface.mThickness
                };
                material.mSubsurfaceTransmission = { BuildSubsurfaceTransmission( subsurface ), 0.0f };
            }

            if ( pSurfaceMaterial->GetShadingModel() == material::ShadingModel::eToonShading ||
                 pSurfaceMaterial->GetShadingModel() == material::ShadingModel::eCellShading ) {
                // SSS and stylized models are mutually exclusive; reuse this model-specific lane.
                material.mSubsurfaceColorOpacity = { shading.mContourColor, shading.mContourWidth };
                mHasStylizedContours |= shading.mContourWidth > 0 && pSurfaceMaterial->GetAlphaMask() == material::AlphaMode::eOpaque;
            }

            // A resumed pass may receive new descriptors even though the material
            // itself is unchanged. Declare dependencies every execution, and only
            // skip the GPU upload when both content and resolved indices match.
            const auto &previous{ mMaterialsList[slot] };

            if ( state.mUploaded && state.mRevision == pSurfaceMaterial->GetRevision() &&
                 material.mAlbedoIndex == previous.mAlbedoIndex &&
                 material.mDiffuseIndex == previous.mDiffuseIndex &&
                 material.mNormalIndex == previous.mNormalIndex &&
                 material.mClearCoatNormalIndex == previous.mClearCoatNormalIndex &&
                 material.mEmissiveIndex == previous.mEmissiveIndex &&
                 material.mAoIndex == previous.mAoIndex &&
                 material.mMetallicIndex == previous.mMetallicIndex &&
                 material.mRoughnessIndex == previous.mRoughnessIndex &&
                 material.mMetallicRoughnessIndex == previous.mMetallicRoughnessIndex &&
                 material.mSpecularGlossinessIndex == previous.mSpecularGlossinessIndex ) {
                continue;
            }

            ++mShadowCasterRevision;
            state.mRevision = pSurfaceMaterial->GetRevision();
            state.mUploaded = true;
            mMaterialsList[slot] = material;

            mMaterialUpload.MarkDirty( slot );

            if ( pSurfaceMaterial->GetShadingModel() == material::ShadingModel::eSubsurfaceScattering ) {
                mSubsurfaceKernels.resize( eastl::max( mSubsurfaceKernels.size(), core::as<core::usize>( slot ) + 1 ) );
                mSubsurfaceKernels[slot] = BuildSubsurfaceKernel( subsurface );

                mSubsurfaceUpload.MarkDirty( slot );
            }
        }
    }

    auto GeometryCullModule::RetireUnusedSlots() -> void {
        // Slots are only referenced through the current view's draw indirection.
        // Reuse records an ordered GPU write, never a host overwrite of memory
        // still being read. Frame-graph history orders it after prior consumers.
        for ( core::u32 slot{}; slot < mInstanceStates.size(); ++slot ) {
            auto &instance{ mInstanceStates[slot] };

            if ( instance.mEntity != entt::null && instance.mLastSeen != mFrame ) {
                mInstanceSlots[entt::to_entity( instance.mEntity )] = kInvalidSlot;
                instance = {};
                mFreeInstances.push_back( slot );
            }
        }

        for ( auto it{ mMaterialSlots.begin() }; it != mMaterialSlots.end(); ) {
            if ( mMaterialStates[it->second].mLastSeen != mFrame ) {
                mMaterialStates[it->second].mSource = {};
                mFreeMaterials.push_back( it->second );
                it = mMaterialSlots.erase( it );
            }
            else {
                ++it;
            }
        }

        if ( mSharedCache && mScene ) {
            for ( auto &entry: mSharedCache->mEntries ) {
                if ( entry.mEntity != entt::null && ( !mScene->GetRegistry().valid( entry.mEntity ) ||
                                                        !mScene->GetRegistry().all_of<scene::MeshComponent>( entry.mEntity ) ||
                                                        !mScene->GetRegistry().get<scene::MeshComponent>( entry.mEntity ).HasMesh() ) ) {
                    entry = {};
                }
            }
        }
    }

    auto GeometryCullModule::UpdateInstance( core::u32 slot, const SceneGeometryCache::Entry &derived,
        core::u32 objectID, bool highlighted, core::i32 skinningPalette ) -> void {
        auto &instance{ mInstanceStates[slot] };
        auto &geometry{ mGeometryList[slot] };
        const auto &material{ mMaterialStates[instance.mMaterialSlot] };
        const core::u32 faceFlags{ material.mFaceFlags | derived.mMirrored };
        const core::u32 highlight{ highlighted ? MKT_SHADER_TRUE : MKT_SHADER_FALSE };

        core::float4 foliageMotion{};
        core::float4 foliageShape{};
        core::float4 foliageWind{};
        const auto *surfaceMaterial{ dynamic_cast<const material::SurfaceMaterial *>( material.mSource.GetPtr() ) };

        if ( mFoliageWindEnabled && surfaceMaterial && surfaceMaterial->GetFoliageWind().mEnabled && !instance.mMesh->IsSkinned() ) {
            const auto &settings{ surfaceMaterial->GetFoliageWind() };
            foliageMotion = { glm::clamp( settings.mHeight, .01f, 100.0f ), glm::clamp( settings.mTrunkBend, 0.0f, .25f ),
                glm::clamp( settings.mBranchMotion, 0.0f, 2.0f ), glm::clamp( settings.mLeafFlutter, 0.0f, .5f ) };
            foliageShape = { settings.mRootHeight, glm::clamp( settings.mFrequency, 0.0f, 10.0f ), 0.0f, 0.0f };
            const auto root{ core::float3{ derived.mTransform * core::float4{ 0, settings.mRootHeight, 0, 1 } } };
            const auto &field{ mFoliageWindField };
            foliageWind = { scene::SampleWind( field, root ), field.mTime };
        }

        const bool foliageSettingsChanged{ geometry.mFoliageMotion != foliageMotion || geometry.mFoliageShape != foliageShape };
        const bool foliageChanged{ foliageSettingsChanged || geometry.mFoliageWind != foliageWind };
        if ( foliageChanged ) {
            ++mShadowCasterRevision;
        }
        const bool transformChanged{ !instance.mUploaded || instance.mDerivedRevision != derived.mRevision };
        const bool drawChanged{ !instance.mUploaded || geometry.mGeometryIndex != instance.mGeometryIndex ||
                                geometry.mObjectID != objectID || geometry.mAlphaMode != material.mAlphaMode };
        const bool instanceChanged{ transformChanged || drawChanged || geometry.mMaterialIndex != instance.mMaterialSlot ||
                                    geometry.mFaceFlags != faceFlags || geometry.mIsHighlighted != highlight ||
                                    geometry.mSkinningMatricesID != skinningPalette };

        if ( transformChanged ) {
            ++mShadowCasterRevision;
            geometry.mTransform = derived.mTransform;
            instance.mDerivedRevision = derived.mRevision;
        }

        if ( transformChanged || drawChanged || foliageSettingsChanged ) {
            auto &bounds{ instance.mBounds };
            bounds.mCenter = derived.mCenter;
            bounds.mExtent = derived.mExtent;
            // Cull against the full motion envelope, including shadow views.
            const float worldScale{ glm::max( glm::length( core::float3{ derived.mTransform[0] } ),
                glm::max( glm::length( core::float3{ derived.mTransform[1] } ), glm::length( core::float3{ derived.mTransform[2] } ) ) ) };
            const float motionExtent{ ( foliageMotion.x * foliageMotion.y * 2.0f + foliageMotion.z * 2.0f + foliageMotion.w * 2.0f ) * worldScale };
            bounds.mExtent += core::float3{ motionExtent };
            // Bind-pose bounds do not safely contain arbitrary animation or IK.
            bounds.mCullable = !instance.mMesh->IsSkinned() &&
                               geometry.mTransform[0].w == 0.0f && geometry.mTransform[1].w == 0.0f &&
                               geometry.mTransform[2].w == 0.0f && geometry.mTransform[3].w == 1.0f;

            for ( core::u32 axis{}; axis < 3; ++axis ) {
                bounds.mCullable &= bounds.mExtent[axis] >= 0.0f;
            }

            mMeshBatchInfos[instance.mGeometryIndex].mBoundsDirty = true;
        }

        geometry.mGeometryIndex = instance.mGeometryIndex;
        geometry.mObjectID = objectID;
        geometry.mIsHighlighted = highlight;
        geometry.mMaterialIndex = instance.mMaterialSlot;
        geometry.mAlphaMode = material.mAlphaMode;
        geometry.mFaceFlags = faceFlags;
        geometry.mFoliageMotion = foliageMotion;
        geometry.mFoliageShape = foliageShape;
        geometry.mFoliageWind = foliageWind;
        geometry.mSkinningMatricesID = skinningPalette;

        // Palette assignments and compute-cache offsets can change independently
        // of the world transform. Animation still schedules deformation each frame.
        const auto previousPositions{ geometry.mSkinnedPositions };
        const auto previousNormals{ geometry.mSkinnedNormals };
        CacheSkinning( geometry, *instance.mMesh );
        const auto *heightMaterial{ dynamic_cast<const material::SurfaceMaterial *>( material.mSource.GetPtr() ) };
        if ( heightMaterial && heightMaterial->GetHeightMapping().mMode == material::HeightMappingMode::eDisplacement ) {
            instance.mBounds.mCullable = false;
            mMeshBatchInfos[instance.mGeometryIndex].mBoundsDirty = true;
        }

        if ( instanceChanged || foliageChanged || previousPositions != geometry.mSkinnedPositions || previousNormals != geometry.mSkinnedNormals ) {
            mInstanceUpload.MarkDirty( slot );
            instance.mUploaded = true;
        }

        mCommandsDirty |= drawChanged;
    }

    auto GeometryCullModule::InitGeometryData( CommandContext &ctx, core::Blackboard &b ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        mFoliageWindField = mScene && mFoliageWindEnabled ? mScene->GetWindField() : scene::WindField{};
        ++mFrame;
        mSkinningJobs.clear();
        mSkinningJobIndices.clear();
        mSkinningCacheBytes = 0;
        mSkinningCacheAddress = ctx.GetDeviceBufferAddress( b.Get<GeometryCullModuleInfo>().mSkinningCacheBuffer );

        if ( mScene ) {
            const auto skinnedView{ mScene->GetRegistry().view<scene::SkinnedMeshRenderer, scene::MeshComponent, scene::TagComponent, scene::MaterialComponent, scene::TransformComponent>() };
            const auto availableSkinningSlots{ mFreeSkinningSlots.size() + kMaxRenderableEntities - mSkinningStates.size() };

            if ( !mSkinningSlots.empty() && skinnedView.size_hint() > availableSkinningSlots ) {
                // Normally the main traversal marks palettes and retirement follows it.
                // Only near capacity, reclaim departed bindings first so replacements
                // can reuse slots without evicting live meshes we have not visited yet.
                // size_hint is an upper bound, so it cannot underestimate new demand.
                for ( const auto &[entity, skinned, mesh, tag, material, transform]:
                    skinnedView.each() ) {
                    if ( tag.IsActive() && mesh.HasMesh() && material.HasMaterial() && animation::AnimationSystem::Get()->GetAnimator( skinned.GetAnimatorID() ) ) {
                        const auto found{ mSkinningSlots.find( std::make_pair( skinned.GetAnimatorID(), mesh.GetMesh() ) ) };

                        if ( found != mSkinningSlots.end() ) {
                            mSkinningStates[found->second].mLastSeen = mFrame;
                        }
                    }
                }

                RetireSkinningSlots();
            }
        }

        if ( !mScene || !mCamera ) {
            mIndirectCmds.clear();
            mTransparentDraws.clear();
            mOpaqueDrawCount = 0;
            mCommandsDirty = true;
            return;
        }

        auto &registry{ mScene->GetRegistry() };
        const auto renderables{ registry.view<scene::TagComponent, scene::TransformComponent, scene::MaterialComponent, scene::MeshComponent>() };
        // Reclaim removed/reassigned records before allocation when the table
        // is full. Replacing an entity must not require a spare GPU slot.
        if ( mInstanceStates.size() >= kMaxRenderableEntities || mMaterialStates.size() >= kMaxRenderableEntities ) {
            core::usize retained{};

            for ( const auto &[entity, tag, transform, material, mesh]: renderables.each() ) {
                if ( !tag.IsActive() || !mesh.HasMesh() || !material.HasMaterial() ) {
                    continue;
                }

                if ( retained++ >= kMaxRenderableEntities ) {
                    break;
                }

                if ( const auto slot{ FindInstance( entity ) }; slot != kInvalidSlot ) {
                    mInstanceStates[slot].mLastSeen = mFrame;
                }

                const auto *videoComponent{ registry.try_get<scene::VideoComponent>( entity ) };
                const auto videoData{ videoComponent ? videoComponent->GetRenderData() : VideoRenderDataHandle{} };
                const auto videoMaterial{ videoData ? videoData->GetMaterial() : material::MaterialHandle{} };
                const auto *capture{ registry.try_get<scene::TextureRenderComponent>( entity ) };
                const auto capturedMaterial{ capture ? capture->GetMaterial() : material::MaterialHandle{} };
                const auto renderMaterial{ capturedMaterial ? capturedMaterial : videoMaterial ? videoMaterial
                                                                                               : material.GetMaterialReference() };

                if ( const auto found{ mMaterialSlots.find( renderMaterial.GetPtr() ) }; found != mMaterialSlots.end() ) {
                    if ( mMaterialStates[found->second].mLastSeen != mFrame ) {
                        UpdateMaterialClassification( found->second );
                    }

                    mMaterialStates[found->second].mLastSeen = mFrame;
                }
            }

            RetireUnusedSlots();
        }

        core::usize acceptedInstances{};

        for ( auto &batch: mMeshBatchInfos ) {
            batch.mInstanceCount = 0;
        }

        for ( const auto &[entity,
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

            if ( acceptedInstances >= kMaxRenderableEntities ) {
                MKT_CORE_LOGGER_WARN( "Renderable entity capacity exceeded; remaining instances are skipped." );
                break;
            }

            ++acceptedInstances;

            const asset::MeshNode *pMeshNode{ meshComponent.GetMesh() };

            const auto *deformed{ registry.try_get<scene::DeformedMeshComponent>( entity ) };
            const MeshRenderDataHandle renderData{ deformed && deformed->mMesh ? MeshRenderDataHandle{ deformed->mMesh } : pMeshNode->GetRenderData() };

            if ( !renderData ) {
                MKT_THROW_RUNTIME_ERROR( "Mesh is missing packed render attributes. Create it through MeshFactory." );
            }

            const auto instanceSlot{ ResolveInstance( entity ) };
            ResolveMesh( instanceSlot, renderData );
            auto &instance{ mInstanceStates[instanceSlot] };
            auto &meshBatchDrawInfo{ mMeshBatchInfos[instance.mGeometryIndex] };
            const auto batchIndex{ meshBatchDrawInfo.mInstanceCount };

            if ( batchIndex == meshBatchDrawInfo.mSlots.size() ) {
                meshBatchDrawInfo.mSlots.push_back( instanceSlot );
                mCommandsDirty = true;
            }
            else if ( meshBatchDrawInfo.mSlots[batchIndex] != instanceSlot ) {
                meshBatchDrawInfo.mSlots[batchIndex] = instanceSlot;
                mCommandsDirty = true;
            }

            core::i32 skinningPalette{ -1 };

            if ( const auto *sm{ registry.try_get<scene::SkinnedMeshRenderer>( entity ) }; sm && meshComponent.HasMesh() ) {
                skinningPalette = ResolveSkinningPalette( sm->GetAnimatorID(), *pMeshNode, meshComponent.GetModel() );
            }

            auto &derived{ mSharedCache->Resolve( entity ) };
            const bool posed{ skinningPalette > 0 };

            if ( derived.mRevision == 0 || derived.mWorldRevision != transformComponent.GetWorldRevision() ||
                 derived.mMesh != renderData || derived.mMeshRevision != renderData->GetRevision() || derived.mNode != pMeshNode || derived.mPosed != posed ) {
                ++derived.mRevision;
                derived.mWorldRevision = transformComponent.GetWorldRevision();
                derived.mMesh = renderData;
                derived.mMeshRevision = renderData->GetRevision();
                derived.mModel = meshComponent.GetModel();
                derived.mNode = pMeshNode;
                derived.mPosed = posed;
                const core::float4x4 bindPose{ posed ? core::float4x4{ 1.0f } : pMeshNode->GetSkinBinding().mBindPoseTransform };
                derived.mTransform = transformComponent.GetWorldTransform() * bindPose;
                derived.mMirrored = glm::determinant( glm::mat3{ derived.mTransform } ) < 0.0f ? 2u : 0u;
                derived.mCenter = core::float3{ derived.mTransform * core::float4{ renderData->GetSortCenter(), 1.0f } };
                const auto extent{ renderData->GetBoundsExtent() };
                derived.mExtent = glm::abs( core::float3{ derived.mTransform[0] } ) * extent.x +
                                  glm::abs( core::float3{ derived.mTransform[1] } ) * extent.y + glm::abs( core::float3{ derived.mTransform[2] } ) * extent.z;
            }

            // Dynamic presentations override rendering while leaving authored material bindings intact.
            const auto *videoComponent{ registry.try_get<scene::VideoComponent>( entity ) };
            const auto videoData{ videoComponent ? videoComponent->GetRenderData() : VideoRenderDataHandle{} };
            const auto videoMaterial{ videoData ? videoData->GetMaterial() : material::MaterialHandle{} };
            const auto *capture{ registry.try_get<scene::TextureRenderComponent>( entity ) };
            const auto capturedMaterial{ capture ? capture->GetMaterial() : material::MaterialHandle{} };
            const auto renderMaterial{ capturedMaterial ? capturedMaterial : videoMaterial ? videoMaterial
                                                                                           : materialComponent.GetMaterialReference() };
            instance.mMaterialSlot = ResolveMaterial( renderMaterial, instance.mMaterialSlot );
            const auto *highlight{ registry.try_get<HighlightComponent>( entity ) };
            UpdateInstance( instanceSlot, derived, tagComponent.GetGuid(), highlight && highlight->IsHighlighted(),
                skinningPalette );

            meshBatchDrawInfo.mInstanceCount += 1;
        }

        for ( auto &batch: mMeshBatchInfos ) {
            if ( batch.mSlots.size() != batch.mInstanceCount ) {
                batch.mSlots.resize( batch.mInstanceCount );
                mCommandsDirty = true;
            }
        }

        // Discover all meshes before recording the upload. Flushing inside Get()
        // repeatedly overwrote the same buffer range within this pass, where the
        // graph's pass-boundary barriers cannot order those transfer writes.
        mGeometryAllocator.CollectUnused( mFrame );
        mGeometryAllocator.Prepare( ctx, b );
        PrepareSkinningData( ctx, b );
        UpdateMaterials( ctx );
        PrepareIndirectDraw( ctx, b );
        RetireUnusedSlots();
    }

    auto GeometryCullModule::BuildDrawCommands() -> void {
        ++mShadowCasterRevision;
        core::u32 packed{};
        mIndirectCmds.clear();
        mTransparentDraws.clear();

        for ( const auto &batch: mMeshBatchInfos ) {
            if ( batch.mSlots.empty() ) {
                continue;
            }

            const auto indexCount{ mGeometryAllocator.Get( batch.mGeometryIndex ).mIndexCount };
            bool continuing{};

            for ( const auto slot: batch.mSlots ) {
                DrawIndirectCommand command{};
                command.mVertexCount = indexCount;
                command.mInstanceCount = 1;
                command.mFirstInstance = packed;

                if ( packed == mDrawInstances.size() ) {
                    mDrawInstances.push_back( { slot, 0 } );
                    mDrawInstanceUpload.MarkDirty( packed );
                }
                else if ( mDrawInstances[packed].mSlot != slot ) {
                    mDrawInstances[packed] = { slot, 0 };
                    mDrawInstanceUpload.MarkDirty( packed );
                }

                ++packed;

                if ( mGeometryList[slot].mAlphaMode == core::as<core::u32>( material::AlphaMode::eBlend ) ) {
                    mTransparentDraws.push_back( { 0, command } );
                    continuing = false;
                }
                else if ( continuing ) {
                    ++mIndirectCmds.back().mInstanceCount;
                }
                else {
                    // Never merge across a transparent gap or a mesh boundary.
                    mIndirectCmds.push_back( command );
                    continuing = true;
                }
            }
        }

        mOpaqueDrawCount = core::as<core::u32>( mIndirectCmds.size() );
        mDrawInstances.resize( packed );

        for ( const auto &draw: mTransparentDraws ) {
            mIndirectCmds.push_back( draw.mCommand );
        }
    }

    auto GeometryCullModule::SortTransparentDraws( bool force ) -> core::u32 {
        const auto &view{ mCamera->GetViewMatrix() };
        bool changed{ force };

        for ( auto &draw: mTransparentDraws ) {
            const auto instance{ draw.mCommand.mFirstInstance };
            const auto center{ view * core::float4{ GetInstanceBounds( instance ).mCenter, 1.0f } };
            const core::f32 depth{ center.z != 0.0f ? -center.z : 0.0f };
            const auto bits{ eastl::bit_cast<core::u32>( depth ) };
            const auto ascending{ bits ^ ( ( bits & 0x80000000u ) ? 0xFFFFFFFFu : 0x80000000u ) };
            const auto objectID{ mGeometryList[mDrawInstances[instance].mSlot].mObjectID };
            const auto key{ ( core::u64{ ascending ^ 0xFFFFFFFFu } << 32 ) | objectID };
            changed |= draw.mKey != key;
            draw.mKey = key;
        }

        auto firstChanged{ core::as<core::u32>( mIndirectCmds.size() ) };

        if ( !changed ) {
            return firstChanged;
        }

        eastl::sort( mTransparentDraws.begin(), mTransparentDraws.end(), []( const TransparentDraw &left, const TransparentDraw &right ) {
            return left.mKey != right.mKey ? left.mKey < right.mKey
                                           : left.mCommand.mFirstInstance < right.mCommand.mFirstInstance;
        } );

        for ( core::u32 index{}; index < mTransparentDraws.size(); ++index ) {
            const auto draw{ mOpaqueDrawCount + index };
            const auto &command{ mTransparentDraws[index].mCommand };

            if ( mIndirectCmds[draw].mFirstInstance != command.mFirstInstance ) {
                firstChanged = eastl::min( firstChanged, draw );
                mIndirectCmds[draw] = command;
            }
        }

        return firstChanged;
    }

    auto GeometryCullModule::GetInstanceBounds( core::u32 drawInstance ) const -> const InstanceBounds & {
        return mInstanceStates[mDrawInstances[drawInstance].mSlot].mBounds;
    }

    auto GeometryCullModule::UpdateDrawBounds( CommandContext &context, FGBufferHandle buffer, core::u32 firstReordered ) -> void {
        // Camera culling tests one union per draw; shadows use the instance bounds.
        mDirtyDrawBounds.clear();
        mDrawBounds.resize( mIndirectCmds.size() );

        if ( mDrawBounds.empty() ) {
            return;
        }

        if ( context.EnsureBufferCapacity( buffer, mDrawBounds.size() * sizeof( DrawBounds ) ) ) {
            firstReordered = 0;
        }

        for ( core::usize draw{}; draw < mIndirectCmds.size(); ++draw ) {
            const auto &command{ mIndirectCmds[draw] };
            const auto slot{ mDrawInstances[command.mFirstInstance].mSlot };

            if ( draw < firstReordered && !mMeshBatchInfos[mGeometryList[slot].mGeometryIndex].mBoundsDirty ) {
                continue;
            }

            const auto &first{ GetInstanceBounds( command.mFirstInstance ) };
            auto minimum{ first.mCenter - first.mExtent };
            auto maximum{ first.mCenter + first.mExtent };
            bool cullable{ first.mCullable };

            for ( core::u32 index{ 1 }; index < command.mInstanceCount; ++index ) {
                const auto &bounds{ GetInstanceBounds( command.mFirstInstance + index ) };
                minimum = glm::min( minimum, bounds.mCenter - bounds.mExtent );
                maximum = glm::max( maximum, bounds.mCenter + bounds.mExtent );
                cullable &= bounds.mCullable;
            }

            mDrawBounds[draw] = { core::float4{ ( minimum + maximum ) * 0.5f, 0.0f },
                core::float4{ ( maximum - minimum ) * 0.5f, cullable ? 1.0f : 0.0f } };
            mDirtyDrawBounds.push_back( core::as<core::u32>( draw ) );
        }
    }

    auto GeometryCullModule::UpdateDrawDebugFlags() -> void {
        const auto rows{ glm::transpose( mCullingDebugCamera ? mCullingDebugCamera->GetViewProjection() : core::float4x4{ 1.0f } ) };
        const eastl::array<core::float4, 6> planes{
            rows[3] + rows[0], rows[3] - rows[0], rows[3] + rows[1],
            rows[3] - rows[1], rows[2], rows[3] - rows[2]
        };

        for ( core::usize draw{}; draw < mIndirectCmds.size(); ++draw ) {
            const auto &command{ mIndirectCmds[draw] };
            core::u32 flags{};

            if ( mCullingDebugCamera ) {
                const core::float3 center{ mDrawBounds[draw].mCenter };
                const core::float3 extent{ mDrawBounds[draw].mExtent };
                bool cullable{ mDrawBounds[draw].mExtent.w > 0.0f };
                core::u32 classification{ cullable ? 1u : 3u };

                if ( cullable ) {
                    for ( const auto &plane: planes ) {
                        const core::float3 normal{ plane };
                        const float distance{ glm::dot( normal, center ) + plane.w };
                        const float radius{ glm::dot( glm::abs( normal ), extent ) };
                        const float tolerance{ 1e-5f * ( glm::dot( glm::abs( normal ), glm::abs( center ) ) + glm::abs( plane.w ) + radius + 1.0f ) };

                        if ( distance + radius < -tolerance ) {
                            classification = 2;
                            break;
                        }
                    }
                }

                flags = classification << 2;
            }

            for ( core::u32 index{}; index < command.mInstanceCount; ++index ) {
                const auto packed{ command.mFirstInstance + index };
                auto &instance{ mDrawInstances[packed] };

                if ( instance.mDebugFlags != flags ) {
                    instance.mDebugFlags = flags;
                    mDrawInstanceUpload.MarkDirty( packed );
                }
            }
        }
    }

    auto GeometryCullModule::PrepareIndirectDraw( CommandContext &context, core::Blackboard &b ) -> void {
        MKT_BEGIN_PROFILER_NAMED();
        const bool rebuild{ mCommandsDirty };
        const bool updateDebug{ rebuild || mCullingDebugCamera || mHadCullingDebugCamera };

        if ( rebuild ) {
            BuildDrawCommands();
        }

        const auto reordered{ SortTransparentDraws( rebuild ) };
        const auto firstChanged{ rebuild ? 0u : reordered };
        auto &info{ b.Get<GeometryCullModuleInfo>() };
        mFirstChangedDraw = firstChanged;

        if ( firstChanged < mIndirectCmds.size() ) {
            const auto bytes{ mIndirectCmds.size() * sizeof( DrawIndirectCommand ) };
            context.EnsureBufferCapacity( mIndirectBuffer, bytes );

            if ( context.EnsureBufferCapacity( mUnculledIndirectBuffer, bytes ) ) {
                mFirstChangedDraw = 0;
            }
        }

        UpdateDrawBounds( context, info.mDrawBoundsBuffer, firstChanged );

        if ( updateDebug ) {
            UpdateDrawDebugFlags();
        }

        for ( auto &batch: mMeshBatchInfos ) {
            batch.mBoundsDirty = false;
        }

        mHadCullingDebugCamera = mCullingDebugCamera != nullptr;
        mCommandsDirty = false;
    }

    auto GeometryCullModule::UploadIndirectDraw( CommandContext &context, core::Blackboard &b ) -> void {
        if ( mFirstChangedDraw < mIndirectCmds.size() ) {
            context.Copy( mUnculledIndirectBuffer, core::usize{ mFirstChangedDraw } * sizeof( DrawIndirectCommand ),
                mIndirectCmds.data() + mFirstChangedDraw, ( mIndirectCmds.size() - mFirstChangedDraw ) * sizeof( DrawIndirectCommand ) );
        }

        const auto target{ b.Get<GeometryCullModuleInfo>().mDrawBoundsBuffer };

        for ( core::usize i{}; i < mDirtyDrawBounds.size(); ) {
            const auto first{ mDirtyDrawBounds[i] };
            core::usize count{ 1 };

            while ( i + count < mDirtyDrawBounds.size() && mDirtyDrawBounds[i + count] == first + count ) {
                ++count;
            }

            context.Copy( target, core::usize{ first } * sizeof( DrawBounds ), mDrawBounds.data() + first, count * sizeof( DrawBounds ) );
            i += count;
        }

        mDirtyDrawBounds.clear();
    }

    auto GeometryCullModule::AppendShadowDraws( const core::float4x4 &viewProjection,
        eastl::vector<DrawIndirectCommand> &commands, core::usize capacity,
        core::u32 &testedInstances, core::u32 &culledInstances ) const -> bool {
        MKT_BEGIN_PROFILER();
        const core::usize first{ commands.size() };
        testedInstances = 0;
        culledInstances = 0;

        if ( !mMeshesVisible ) {
            return true;
        }
        // Extract inward planes from matrix rows. Vulkan/D3D clip depth is [0,w],
        // so the near plane is row 2, not row 3 + row 2 as in OpenGL.
        const auto rows{ glm::transpose( viewProjection ) };
        const eastl::array<core::float4, 6> planes{
            rows[3] + rows[0], rows[3] - rows[0], rows[3] + rows[1],
            rows[3] - rows[1], rows[2], rows[3] - rows[2]
        };

        for ( core::u32 draw{}; draw < mIndirectCmds.size(); ++draw ) {
            const auto &source{ mIndirectCmds[draw] };
            bool continuing{};

            for ( core::u32 offset{}; offset < source.mInstanceCount; ++offset ) {
                const core::u32 instance{ source.mFirstInstance + offset };
                const auto &bounds{ GetInstanceBounds( instance ) };
                ++testedInstances;
                bool visible{ true };

                if ( bounds.mCullable ) {
                    for ( const auto &plane: planes ) {
                        const core::float3 normal{ plane };
                        const core::f32 distance{ glm::dot( normal, bounds.mCenter ) + plane.w };
                        const core::f32 radius{ glm::dot( glm::abs( normal ), bounds.mExtent ) };
                        // AABB support along the plane normal: only reject a box entirely
                        // outside. Leave a relative rounding margin at cascade/face seams.
                        const core::f32 tolerance{ 1e-5f * ( glm::dot( glm::abs( normal ), glm::abs( bounds.mCenter ) ) +
                                                               glm::abs( plane.w ) + radius + 1.0f ) };

                        if ( distance + radius < -tolerance ) {
                            visible = false;
                            break;
                        }
                    }
                }

                if ( !visible ) {
                    ++culledInstances;
                    continuing = false;
                    continue;
                }

                if ( continuing ) {
                    ++commands.back().mInstanceCount;
                }
                else {
                    if ( commands.size() >= capacity ) {
                        commands.resize( first );
                        culledInstances = 0;
                        // The caller draws the complete original list for this view.
                        testedInstances = 0;

                        for ( core::u32 original{}; original < mIndirectCmds.size(); ++original ) {
                            testedInstances += mIndirectCmds[original].mInstanceCount;
                        }

                        return false;
                    }

                    auto command{ source };
                    command.mFirstInstance = instance;
                    command.mInstanceCount = 1;
                    commands.push_back( command );
                    continuing = true;
                }
            }
        }

        return true;
    }

    auto GeometryCullModule::AppendHighlightedDraws( eastl::vector<DrawIndirectCommand> &commands ) const -> void {
        MKT_BEGIN_PROFILER();

        if ( !mMeshesVisible ) {
            return;
        }

        for ( const auto &source: mIndirectCmds ) {
            bool continuing{};

            for ( core::u32 offset{}; offset < source.mInstanceCount; ++offset ) {
                const auto instance{ source.mFirstInstance + offset };
                const auto slot{ mDrawInstances[instance].mSlot };

                if ( !mGeometryList[slot].mIsHighlighted ) {
                    continuing = false;
                    continue;
                }

                if ( continuing ) {
                    ++commands.back().mInstanceCount;
                }
                else {
                    auto command{ source };
                    command.mFirstInstance = instance;
                    command.mInstanceCount = 1;
                    commands.push_back( command );
                    continuing = true;
                }
            }
        }
    }

    auto GeometryCullModule::GetShadowCasterBatchCount() const -> core::u32 {
        MKT_BEGIN_PROFILER();
        return core::as<core::u32>( mIndirectCmds.size() );
    }

    auto GeometryCullModule::PushTextureID( CommandContext &ctx, renderer::rhi::TextureHandle handle ) -> core::i32 {
        MKT_BEGIN_PROFILER();

        if ( handle.IsEmpty() ) {
            return kInvalidTextureID;
        }

        const auto texture{ ctx.ImportSampledTexture( eastl::move( handle ) ) };
        return core::as<core::i32>( ctx.UseImportedTexture( texture ) );
    }

    auto GeometryCullModule::DrawInstancesIndirect( CommandContext &context, GeometryDrawGroup group, bool cameraCulled ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( !mMeshesVisible ) {
            return;
        }
        const auto first{ group == GeometryDrawGroup::eTransparent ? mOpaqueDrawCount : 0 };
        const auto count{ group == GeometryDrawGroup::eOpaqueAndMasked ? mOpaqueDrawCount : core::as<core::u32>( mIndirectCmds.size() ) - first };
        context.DrawIndirect( DrawIndirectState{}
                .SetFirstDraw( first )
                .SetDrawCount( count )
                .SetBuffer( cameraCulled ? mIndirectBuffer : mUnculledIndirectBuffer ) );
    }

    auto GeometryCullModule::HasStylizedContours() const -> bool {
        return mHasStylizedContours;
    }

}// namespace mikoto::renderer
