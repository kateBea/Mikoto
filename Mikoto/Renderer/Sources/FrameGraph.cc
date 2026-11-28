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

#include <EASTL/algorithm.h>
#include <EASTL/sort.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/vector.h>

#include <taskflow/taskflow.hpp>

#include <Assets/AssetRegistryService.hh>
#include <Assets/ImageProcessing.hh>
#include <Assets/ShaderLibrary.hh>
#include <Assets/Utility.hh>

#include <Core/Core.hh>
#include <Core/DotGraph.hh>
#include <Core/Exception.hh>
#include <Core/Profiler.hh>
#include <Core/Timer.hh>
#include <Core/Types.hh>

#include <Filesystem/File.hh>
#include <Filesystem/FileService.hh>
#include <Filesystem/Path.hh>

#include <Logging/Assert.hh>
#include <Logging/Logger.hh>

#include <Memory/Allocator.hh>
#include <Memory/FreeListAllocator.hh>
#include <Memory/MemoryArena.hh>

#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/FrameGraphTypes.hh>
#include <Renderer/Core/RenderSystem.hh>

#include <Threading/TaskService.hh>
#include <Threading/Utility.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::asset;
    using namespace mikoto::memory;
    using namespace mikoto::material;
    using namespace mikoto::filesystem;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;

    MKT_NODISCARD static constexpr auto GetShaderFlagsFromStage( FGStageType type ) -> renderer::rhi::ShaderType {
        switch ( type ) {
            case FGStageType::eVertex:
                return renderer::rhi::ShaderType::eVertex;
            case FGStageType::ePixel:
                return renderer::rhi::ShaderType::ePixel;
            case FGStageType::eCompute:
                return renderer::rhi::ShaderType::eCompute;
            default:;
        }

        return renderer::rhi::ShaderType::eInvalid;
    }

    struct FGNodeControl::ContextStorage {
        ankerl::unordered_dense::map<eastl::string, CommandContext> mValues{};
    };

    auto FGStatisticsManager::GetNode( eastl::string_view name ) -> FGNodeStatistics * {
        MKT_BEGIN_PROFILER();
        MKT_ASSERT( mGraphNodes.contains( name.data() ), "Node does not exists" );
        return MKT_ADDRESSOF( mGraphNodes.at( name.data() ) );
    }

    auto FGStatisticsManager::GetNode( eastl::string_view name ) const -> const FGNodeStatistics * {
        MKT_BEGIN_PROFILER();
        MKT_ASSERT( mGraphNodes.contains( name.data() ), "Node does not exists" );
        return MKT_ADDRESSOF( mGraphNodes.at( name.data() ) );
    }

    auto FGStatisticsManager::RegisterNode( eastl::string_view name ) -> FGNodeStatistics * {
        MKT_BEGIN_PROFILER();
        return MKT_ADDRESSOF( mGraphNodes[name.data()] );
    }

    auto FGStatisticsManager::GetStatistics() const -> const ankerl::unordered_dense::map<eastl::string, FGNodeStatistics> & {
        MKT_BEGIN_PROFILER();
        return mGraphNodes;
    }

    FGReadbackManager::FGReadbackManager( core::Blackboard &blackboard, FGResourceManager &manager )
        : mBlackboard{ MKT_ADDRESSOF( blackboard ) }, mResourceManager{ MKT_ADDRESSOF( manager ) } {
    }

    auto FGReadbackManager::ExecuteCallbacks( core::u64 fenceValue ) -> void {
        MKT_BEGIN_PROFILER();
        auto it{ mReadbackTasks.begin() };

        while ( it != mReadbackTasks.end() ) {
            auto &pair{ *it };

            if ( !pair.second.mIsReady && fenceValue >= pair.second.mFenceValue ) {
                pair.second.mCallback( *mBlackboard, *mResourceManager );
                pair.second.mIsReady = true;

                if ( !pair.second.mRunsEveryFrame ) {
                    it = mReadbackTasks.erase( it );
                }
                else {
                    ++it;
                }
            }
            else {
                ++it;
            }
        }
    }

    auto FGReadbackManager::UpdateTaskFenceValue( core::u32 taskID, core::u64 fenceValue ) -> void {
        MKT_BEGIN_PROFILER();
        MKT_ASSERT( mReadbackTasks.contains( taskID ), "Task with specified ID does not exist" );

        if ( mReadbackTasks[taskID].mIsReady ) {
            mReadbackTasks[taskID].mIsReady = false;
            mReadbackTasks[taskID].mFenceValue = fenceValue;
        }
    }

    auto FGReadbackManager::RegisterCallback( const FGReadbackTask::Callback &callback, bool runEveryFrame ) -> core::u32 {
        MKT_BEGIN_PROFILER();
        const core::u32 taskID{ mNextTaskID++ };
        mReadbackTasks[taskID] = FGReadbackTask{
            .mCallback = callback,
            .mRunsEveryFrame = runEveryFrame,
        };

        return taskID;
    }


    FGNodeControl::FGNodeControl()
        : mContexts{ eastl::make_unique<ContextStorage>() } {
    }

    // Destruction must also see ContextStorage and CommandContext's definitions.
    FGNodeControl::~FGNodeControl() = default;

    auto FGNodeControl::GetContext( const eastl::string &name ) -> CommandContext & {
        return mContexts->mValues.at( name );
    }

    auto FGNodeControl::RegisterContext( const eastl::string &name, FrameGraphPass &node,
        FGResourceManager &resources, FrameGraphDescriptorAllocator &descriptors,
        rhi::PipelineLayoutHandle pipelineLayout, FGStatisticsManager &statistics ) -> void {
        // Recompilation can replace GPU generations; recreate contexts to discard
        // cached native pointers and BLASes before the next execution.
        mContexts->mValues.insert_or_assign( name, CommandContext{
                                                       &node, &resources, &descriptors, eastl::move( pipelineLayout ), &statistics } );
    }

    auto FGNodeControl::Clear() -> void {
        MKT_BEGIN_PROFILER();
        mContexts->mValues.clear();
        mResources.clear();
        mNodes.clear();
    }

    FGNodeBuilder::FGNodeBuilder( FrameGraphPass &node, FGNodeControl &control )
        : mGraphNode{ MKT_ADDRESSOF( node ) }, mNodeControl{ MKT_ADDRESSOF( control ) } {
    }

    auto FGNodeBuilder::UseResource( FGTextureHandle handle, FGPipelineStage stage, FGResourceAccess access ) -> void {
        MKT_BEGIN_PROFILER();
        UseResource( handle.mHandle, stage, access, FGResourceType::eTexture );
    }

    auto FGNodeBuilder::UseResource( FGDenoiserHandle handle, FGPipelineStage stage, FGResourceAccess access ) -> void {
        UseResource( handle.mHandle, stage, access, FGResourceType::eDenoiser );
    }

    auto FGNodeBuilder::UseResource( FGAccelerationStructureHandle handle, FGPipelineStage stage, FGResourceAccess access ) -> void {
        UseResource( handle.mHandle, stage, access, FGResourceType::eAccelerationStructure );
    }

    auto FGNodeBuilder::UseResource( FGBufferHandle handle, FGPipelineStage stage, FGResourceAccess access ) -> void {
        MKT_BEGIN_PROFILER();
        UseResource( handle.mHandle, stage, access, FGResourceType::eBuffer );
    }

    auto FGNodeBuilder::UseResource( FGVideoHandle handle, FGPipelineStage stage, FGResourceAccess access ) -> void {
        UseResource( handle.mHandle, stage, access, FGResourceType::eVideo );
    }

    auto FGNodeBuilder::UseTextures( FGVideoHandle batch, FGPipelineStage stage, FGResourceAccess access,
        const eastl::function<void( eastl::vector<rhi::TextureHandle> & )> &resolveTextures ) -> void {
        if ( !resolveTextures ) {
            MKT_THROW_INVALID_ARGUMENT( "A runtime texture declaration requires a resolver." );
        }

        UseResource( batch, stage, access );
        mGraphNode->mRuntimeTextureUses.push_back( FGRuntimeTextureUse{
            .mUsage = { .mStages = { stage }, .mAccess = access },
            .mResolveTextures = resolveTextures } );
    }

    auto FGNodeBuilder::UseResource( FGResourceHandle handle, FGPipelineStage stage, FGResourceAccess access, FGResourceType type ) -> void {
        MKT_BEGIN_PROFILER();

        if ( access == FGResourceAccess::eNone || stage == FGPipelineStage::eUnknown ) {
            MKT_THROW_RUNTIME_ERROR( "A graph resource use needs a pipeline stage and access." );
        }

        const auto resource{ mNodeControl->mResources.find( handle ) };
        if ( handle == 0 || resource == mNodeControl->mResources.end() || resource->second.mVersions.empty() ) {
            MKT_THROW_RUNTIME_ERROR( string::Format( "Frame graph pass '{}' uses an unregistered resource (handle {}).",
                mGraphNode->mName.c_str(), handle )
                    .c_str() );
        }

        if ( const auto existing{ mGraphNode->mResourceStates.find( handle ) }; existing != mGraphNode->mResourceStates.end() ) {
            auto &usage{ existing->second };
            // A pass-boundary barrier cannot represent conflicting layouts or
            // order a read followed by a write inside the pass. Reject that case
            // instead of silently replacing an earlier declaration.
            if ( usage.mAccess != access ||
                 ( type == FGResourceType::eTexture &&
                     GetResourceState( usage.mStages.front(), access ) != GetResourceState( stage, access ) ) ) {
                MKT_THROW_RUNTIME_ERROR( "Conflicting resource uses in one graph pass; split the operations into separate passes." );
            }

            if ( eastl::find( usage.mStages.begin(), usage.mStages.end(), stage ) == usage.mStages.end() ) {
                usage.mStages.push_back( stage );
            }
            // Dependencies belong to resource accesses, not to shader stages.
            // Registering a repeated write again would create a self-dependency.
            return;
        }

        if ( access == FGResourceAccess::eWrite ) {
            TrackWrite( handle );
        }
        else {
            TrackRead( handle );
        }

        mGraphNode->mResourceStates.emplace( handle, FGResourceTrack{
                                                         .mStages = { stage },
                                                         .mAccess = access } );
    }

    auto FGNodeBuilder::TrackRead( FGResourceHandle handle ) -> void {
        MKT_BEGIN_PROFILER();
        MKT_ASSERT( handle != 0, "Invalid resource handle" );
        auto &resourceLatestVersion{ mNodeControl->mResources[handle].mVersions.back() };

        // Let writer finish first before we start reading to it (READ-AFTER-WRITE)
        if ( resourceLatestVersion.HasWriter() ) {
            mGraphNode->mDependsOn.emplace( resourceLatestVersion.mWriterPass );
        }

        // Register myself as reader for this resource
        resourceLatestVersion.mReaderPasses.push_back( mGraphNode->mName );

        // record for barrier insertion
        mGraphNode->mReadResources.push_back( handle );
    }

    auto FGNodeBuilder::TrackWrite( FGResourceHandle handle ) -> void {
        MKT_BEGIN_PROFILER();
        MKT_ASSERT( handle != 0, "Invalid resource handle" );
        auto &resourceLatestVersion{ mNodeControl->mResources[handle].mVersions.back() };

        // We depend on previous writer, let
        // it finish first (WRITE-AFTER-WRITE)
        if ( resourceLatestVersion.HasWriter() ) {
            mGraphNode->mDependsOn.emplace( resourceLatestVersion.mWriterPass );
        }

        // Readers must finish before this pass writes to the resource (WRITE-AFTER-READ)
        for ( const auto &reader: resourceLatestVersion.mReaderPasses ) {
            mGraphNode->mDependsOn.emplace( reader );
        }

        // Make this pass as the last writer for this resource
        if ( resourceLatestVersion.HasWriter() ) {
            mNodeControl->mResources[handle].mVersions.push_back( {} );
            mNodeControl->mResources[handle].mVersions.back().mWriterPass = mGraphNode->mName;
        }
        else {
            resourceLatestVersion.mWriterPass = mGraphNode->mName;
        }

        // record for barrier insertion
        mGraphNode->mWriteResources.push_back( handle );
    }

    FrameGraph::FrameGraph( renderer::rhi::IGpuDevice *device, asset::ShaderLibrary *library )
        : mDevice{ device }, mShaderLibrary{ library } {
        MKT_BEGIN_PROFILER();
        mNodeControl = eastl::make_unique<FGNodeControl>();
        mResourceManager = eastl::make_unique<FGResourceManager>( mDevice );
        mReadbackManager = eastl::make_unique<FGReadbackManager>( mBlackboard, *mResourceManager );

        // Every graph pipeline uses the same bindless layout and table.
        auto layoutDescription{ rhi::DescriptorTableLayoutDescription{}
            .SetVisibility( rhi::ShaderFlagsBits::All )
            .SetRegisterSpace( MKT_DEFAULT_REGISTER_SPACE )
            .AddBindlessItem( rhi::BindlessLayoutItem::Samplers( MKT_SAMPLER_BINDING, 1024 ) )
            .AddBindlessItem( rhi::BindlessLayoutItem::TextureSRV( MKT_TEXTURE_SRV_BINDING, 4096 ) )
            .AddBindlessItem( rhi::BindlessLayoutItem::TextureUAV( MKT_TEXTURE_UAV_BINDING, 4096 ) )
            .AddBindlessItem( rhi::BindlessLayoutItem::RawBufferSRV( MKT_STRUCTURED_SRV_BINDING, 4096 ) )
            .AddBindlessItem( rhi::BindlessLayoutItem::RawBufferUAV( MKT_STRUCTURED_UAV_BINDING, 4096 ) ) };

        if ( mDevice->SupportsRayTracing() ) {
            layoutDescription.AddBindlessItem( rhi::BindlessLayoutItem::AccelerationStructure( MKT_ACCELERATION_STRUCTURE_BINDING, 4096 ) );
        }

        mDescriptorAllocator = eastl::make_unique<FrameGraphDescriptorAllocator>(
            mDevice, mDevice->CreateDescriptorTableLayout( layoutDescription ) );
        mPipelineLayout = mDevice->CreatePipelineLayout( rhi::PipelineLayoutCreateDescription{}
            .AddBindingLayout( mDescriptorAllocator->GetBindlessLayout() ) );

        mFence = mDevice->CreateFence( mFenceValue++ );

        mGraphicsCommands = mDevice->CreateCommandList( renderer::rhi::QueueType::eGraphics );
        mComputeCommands = mDevice->CreateCommandList( renderer::rhi::QueueType::eCompute );
        mTransferCommands = mDevice->CreateCommandList( renderer::rhi::QueueType::eTransfer );

        mGraphicsCommands->SetEnableAutomaticBarriers( false );
        mGraphicsCommands->SetDebugName( "FG GraphicsCommands" );

        mComputeCommands->SetEnableAutomaticBarriers( false );
        mComputeCommands->SetDebugName( "FG ComputeCommands" );

        mTransferCommands->SetEnableAutomaticBarriers( false );
        mTransferCommands->SetDebugName( "FG TransferCommands" );

        mStatisticsManager = eastl::make_unique<FGStatisticsManager>();
        mGpuProfiler = eastl::make_unique<GpuQueryProfiler>( mDevice );

        mScheduler = eastl::make_unique<FGScheduler>( mDevice );
        mGraphCompiler = eastl::make_unique<FGCompiler>( mDevice );
    }

    auto FrameGraph::GetNodeControl() -> FGNodeControl & {
        return *mNodeControl;
    }

    auto FrameGraph::Compile() -> void {
        MKT_BEGIN_PROFILER_NAMED();
        mExecutionPlan = mGraphCompiler->Compile( *this );

        // The compiler owns the plan; recording contexts use this graph's runtime objects.
        for ( auto &[passName, node]: mNodeControl->mNodes ) {
            if ( node.mType == FGPassType::eVideo && !mVideoCommands ) {
                mVideoCommands = mDevice->CreateCommandList( rhi::QueueType::eVideoDecode );
                mVideoCommands->SetEnableAutomaticBarriers( false );
            }

            node.mCommandList = node.mType == FGPassType::eVideo ? mVideoCommands : mGraphicsCommands;
            mNodeControl->RegisterContext( passName, node, *mResourceManager,
                *mDescriptorAllocator, mPipelineLayout, *mStatisticsManager );
            mNodeControl->GetContext( passName ).SetVideoRecording( mScheduler.get() );
        }

#if !defined( NDEBUG )
        DumpHierarchy();
#endif
    }

    auto FrameGraph::Execute() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( !mExecutionPlan ) {
            MKT_THROW_RUNTIME_ERROR( "Frame graph must be compiled before execution." );
        }

        CollectRetiredResources();

        mExecutedPassCount = 0;
        mGraphicsRecordedThisFrame = false;
        mVideoRecordedThisFrame = false;

        for ( auto &[texture, entry]: mRuntimeTextures ) {
            entry->mUsedThisFrame = false;
        }

        // Reset stats
        for ( const auto &[name, node]: mNodeControl->mNodes ) {
            if ( auto *stats = mStatisticsManager->GetNode( name ) ) {
                stats->mExecutedThisFrame = false;
                stats->mCommands = {};
                stats->mDrawCallCount = stats->mDispatchCallCount = 0;
            }
        }

        // Decode first, then restore graphics ownership
        // before any image consumers. Keep the existing
        // single graphics batch for every other pass.
        RecordVideoCommands();
        mVideoRecordedThisFrame = mScheduler->HasVideoWork();
        RecordGraphicsCommands();

        SubmitRecordedCommands();

        // Keep history for live pictures without retaining every allocation from past seeks.
        // Native recordings retain the images they reference until their submission completes.
        for ( auto entry{ mRuntimeTextures.begin() }; entry != mRuntimeTextures.end(); ) {
            if ( !entry->second->mUsedThisFrame ) {
                entry = mRuntimeTextures.erase( entry );
            }
            else {
                ++entry;
            }
        }
    }

    auto FrameGraph::RecordVideoCommands() -> void {
        mScheduler->BeginVideoWork();

        if ( mVideoCommands.IsEmpty() ) {
            return;
        }


        for ( const auto &name: mExecutionPlan->mSortedPasses ) {
            auto &pass{ mNodeControl->mNodes.at( name ) };

            if ( pass.mType != FGPassType::eVideo || !pass.mIsAlive ) {
                continue;
            }

            for ( const auto &dependency: pass.mDependsOn ) {
                if ( mNodeControl->mNodes.at( dependency ).mType != FGPassType::eVideo ) {
                    MKT_THROW_RUNTIME_ERROR( "A video prelude cannot depend on a graphics pass in the same graph." );
                }
            }

            auto &context{ mNodeControl->GetContext( name ) };
            // The first decode may open a new command buffer inside its callback.
            context.ResetCallCounts( !mScheduler->HasVideoWork() );
            pass.mExecuteCallback( context, mBlackboard );
            ++mExecutedPassCount;

            auto &statistics{ *mStatisticsManager->GetNode( pass.mName ) };
            statistics.mCommands = mScheduler->HasVideoWork()
                                       ? context.GetStatistics()
                                       : rhi::CommandListStatistics{};
            statistics.mDrawCallCount = core::as<core::u32>( statistics.mCommands.mDrawCallCount );
            statistics.mDispatchCallCount = core::as<core::u32>( statistics.mCommands.mDispatchCallCount );
            statistics.mExecutedThisFrame = true;

            if ( pass.mExecutionPolicy != FGExecutionPolicy::ePerFrame ) {
                pass.mIsAlive = false;
            }
        }
    }

    auto FrameGraph::RecordGraphicsCommands() -> void {
        mGraphicsCommands->Open( { .mScopeName = "TaskGraph Graphics Commands" } );
        mGraphicsRecordedThisFrame = true;

#if defined( MIKOTO_ENABLE_TRACY )
        mGraphicsCommands->CollectGpuProfile();
#endif
        mGpuProfiler->BeginFrame( *mGraphicsCommands, mFence );

        RestoreGraphicsResources();

        if ( mScheduler->HasVideoWork() ) {
            mVideoCommands->Close();
            mScheduler->PushVideoCommandList( mVideoCommands );
        }
        BindResources( mGraphicsCommands );

        for ( const auto &passName: mExecutionPlan->mSortedPasses ) {
            if ( !mNodeControl->mNodes[passName].mIsAlive ) {
                continue;
            }

            auto &pass{ mNodeControl->mNodes[passName] };
            if ( pass.mType == FGPassType::eVideo ) {
                continue;
            }

            auto &ctx{ mNodeControl->GetContext( passName ) };

#if defined( MIKOTO_ENABLE_TRACY )
            // Attribute CPU command recording to the pass declaration.
            // https://github.com/wolfpld/tracy/blob/v0.14.1/public/client/TracyScoped.hpp
            const eastl::string_view file{ pass.mSourceLocation.file_name() };
            const eastl::string_view function{ pass.mSourceLocation.function_name() };
            const tracy::ScopedZone cpuZone{ pass.mSourceLocation.line(), file.data(), file.size(),
                function.data(), function.size(), pass.mName.data(), pass.mName.size(), core::u32{}, MIKOTO_TRACY_CALLSTACK_DEPTH, true };
#endif

            // Off load this work to workers threads?
            pass.mCommandList->BeginDebugLabel( pass.mDebugLabel, kColorTransparent );

            // Include resource transitions in the GPU cost of the pass.
#if defined( MIKOTO_ENABLE_TRACY )
            mGraphicsCommands->BeginGpuProfileZone( pass.mName, pass.mSourceLocation );
#endif
            mGpuProfiler->BeginPass( *mGraphicsCommands, pass.mName );
            const auto &barriers{ mExecutionPlan->mBarriers.at( passName ) };
            ctx.ResetCallCounts();
            ctx.SubmitBarriers( barriers );
            SubmitRuntimeTextureBarriers( pass, ctx );

            Timer timer{ false };

            // Run pass execute callback
            pass.mExecuteCallback( ctx, mBlackboard );
            mGpuProfiler->EndPass( *mGraphicsCommands );
#if defined( MIKOTO_ENABLE_TRACY )
            mGraphicsCommands->EndGpuProfileZone();
#endif
            const double elapsed{ timer.GetCurrentProgress( core::TimeUnit::eMilliseconds ) };

            FGNodeStatistics &stats{ *mStatisticsManager->GetNode( pass.mName ) };

            stats.mLastExecutionTime = elapsed;
            stats.mCommands = ctx.GetStatistics();
            ++mExecutedPassCount;
            stats.mDrawCallCount = ctx.GetDrawCallCount();
            stats.mDispatchCallCount = ctx.GetDispatchCallCount();

            if ( stats.mExecutionCount == 0 || elapsed < stats.mMinExecutionTime ) {
                stats.mMinExecutionTime = elapsed;
            }

            if ( stats.mExecutionCount == 0 || elapsed > stats.mMaxExecutionTime ) {
                stats.mMaxExecutionTime = elapsed;
            }

            ++stats.mExecutionCount;
            stats.mExecutedThisFrame = true;

            // End pass
            pass.mCommandList->EndDebugLabel();

            if ( pass.mExecutionPolicy == FGExecutionPolicy::eOnce ||
                 pass.mExecutionPolicy == FGExecutionPolicy::eOnWake ) {
                pass.mIsAlive = false;
            }
        }

        mGpuProfiler->EndFrame( *mGraphicsCommands );
        mGraphicsCommands->Close();
    }

    auto FrameGraph::SubmitRuntimeTextureBarriers( const FrameGraphPass &pass, CommandContext &context ) -> void {
        if ( pass.mRuntimeTextureUses.empty() ) {
            return;
        }

        ankerl::unordered_dense::map<const rhi::ITexture *, FGBarrier> uses{};
        eastl::vector<rhi::TextureHandle> textures{};

        for ( const auto &declaration: pass.mRuntimeTextureUses ) {
            textures.clear();
            declaration.mResolveTextures( textures );

            for ( auto &texture: textures ) {
                if ( !texture ) {
                    continue;
                }

                auto [use, added]{ uses.try_emplace( texture.GetPtr(), FGBarrier{
                                                                           .mNewStages = declaration.mUsage.mStages,
                                                                           .mNewAccess = declaration.mUsage.mAccess } ) };

                if ( !added ) {
                    if ( use->second.mNewAccess != declaration.mUsage.mAccess ||
                         GetResourceState( use->second.mNewStages.front(), use->second.mNewAccess ) !=
                             GetResourceState( declaration.mUsage.mStages.front(), declaration.mUsage.mAccess ) ) {
                        MKT_THROW_RUNTIME_ERROR( "Conflicting runtime texture uses; split the operations into separate passes." );
                    }

                    for ( const auto stage: declaration.mUsage.mStages ) {
                        if ( eastl::find( use->second.mNewStages.begin(), use->second.mNewStages.end(), stage ) == use->second.mNewStages.end() ) {
                            use->second.mNewStages.push_back( stage );
                        }
                    }
                }

                auto &entry{ mRuntimeTextures[texture.GetPtr()] };

                if ( !entry ) {
                    entry = eastl::make_unique<RuntimeTexture>();
                    entry->mResource.mResource = texture;
                    entry->mResource.mType = FGResourceType::eTexture;
                    entry->mResource.mNeedsRebuild = false;
                }

                entry->mUsedThisFrame = true;
            }
        }

        eastl::vector<eastl::pair<FGResource *, FGBarrier>> barriers{};
        barriers.reserve( uses.size() );

        for ( const auto &[texture, usage]: uses ) {
            barriers.emplace_back( &mRuntimeTextures.at( texture )->mResource, usage );
        }

        context.SubmitBarriers( barriers );
    }

    auto FrameGraph::SubmitRecordedCommands() -> void {
        const core::u64 submissionFenceValue{ mFenceValue++ };
        auto submitInfoGraphics{ renderer::rhi::SubmitInfo{}
                .AddSignal( mFence, submissionFenceValue )
                .AddCommandList( mGraphicsCommands ) };

        mScheduler->PushRenderCommandList( submitInfoGraphics );
        mScheduler->Execute();

        // A replacement may happen during this recording. Its old descriptor
        // stays intact until the fence covering the entire recording completes.
        // Cached indices for unchanged resources remain allocated.
        for ( auto &resource: mResourceManager->GetReplacedResources() ) {
            mRetiredResources.push_back( { eastl::move( resource ), submissionFenceValue } );
        }

        mGpuProfiler->Submitted( submissionFenceValue );

        ProcessReadbackTasks( submissionFenceValue );
    }

    auto FrameGraph::CollectRetiredResources() -> void {
        const auto completed{ mFence->GetCompletionValue() };

        for ( auto entry{ mRetiredResources.begin() }; entry != mRetiredResources.end(); ) {
            if ( entry->mFenceValue <= completed ) {
                // A physical allocation can be aliased or republished while
                // this fence is pending. Its live descriptor must stay cached.
                if ( !mResourceManager->IsResourceReferenced( entry->mResource.GetPtr() ) ) {
                    mDescriptorAllocator->ReleaseResource( entry->mResource.GetPtr() );
                }
                entry = mRetiredResources.erase( entry );
            }
            else {
                ++entry;
            }
        }
    }

    auto FrameGraph::RestoreGraphicsResources() -> void {
        MKT_BEGIN_PROFILER();

        if ( mScheduler->HasVideoWork() ) {
            mScheduler->RestoreGraphicsResources( mGraphicsCommands.GetPtr(), *mVideoCommands );
        }

        // Ordinary graph images keep their last layout and access history across frames.
        // SubmitBarriers transitions directly from that history to each actual consumer;
        // a blanket GENERAL round trip adds full-pipeline dependencies and discards useful scopes.
        // https://docs.vulkan.org/guide/latest/synchronization_examples.html
    }

    auto FrameGraph::ProcessReadbackTasks( core::u64 submissionFenceValue ) -> void {
        MKT_BEGIN_PROFILER();
        auto it{ mReadbackTasks.begin() };

        while ( it != mReadbackTasks.end() ) {
            if ( !it->mHasBeenRegistered ) {
                it->mHasBeenRegistered = true;
                it->mTaskID = mReadbackManager->RegisterCallback( it->mTask, it->mRunsPerFrame );
            }

            // Wait for the command list containing the copy, not the next
            // unused timeline value.
            mReadbackManager->UpdateTaskFenceValue( it->mTaskID, submissionFenceValue );

            if ( !it->mRunsPerFrame ) {
                // Runs only once, delete to not
                // update submission fence value again
                it = mReadbackTasks.erase( it );
            }
            else {
                ++it;
            }
        }
    }

    auto FrameGraph::ExecuteReadbacks() -> void {
        MKT_BEGIN_PROFILER();
        const core::u64 currentValue{ mFence->GetCompletionValue() };
        mReadbackManager->ExecuteCallbacks( currentValue );
    }

    auto FrameGraph::SetExecutionPolicy( eastl::string_view passName, FGExecutionPolicy policy ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !mNodeControl->mNodes.contains( passName.data() ) ) {
            MKT_CORE_LOGGER_WARN( "Pass '{}' does not exist", passName.data() );
            return;
        }

        mNodeControl->mNodes[passName.data()].mExecutionPolicy = policy;
    }

    auto FrameGraph::DisablePass( eastl::string_view passName ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !mNodeControl->mNodes.contains( passName.data() ) ) {
            MKT_CORE_LOGGER_WARN( "Pass '{}' does not exist", passName.data() );
            return;
        }

        mNodeControl->mNodes[passName.data()].mIsAlive = false;
    }

    auto FrameGraph::EnablePass( eastl::string_view passName ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !mNodeControl->mNodes.contains( passName.data() ) ) {
            MKT_CORE_LOGGER_WARN( "Pass '{}' does not exist", passName.data() );
            return;
        }

        mNodeControl->mNodes[passName.data()].mIsAlive = true;
    }

    auto FrameGraph::DumpHierarchy() -> void {
        MKT_BEGIN_PROFILER();
        core::DotGraph graph{ "FrameGraph dependencies" };
        ankerl::unordered_dense::map<eastl::string_view, core::DotGraph::NodeID> nodes{};
        eastl::vector<eastl::string_view> names{};
        names.reserve( mNodeControl->mNodes.size() );
        nodes.reserve( mNodeControl->mNodes.size() );

        for ( const auto &entry: mNodeControl->mNodes ) {
            names.emplace_back( entry.first.data(), entry.first.size() );
        }

        // Hash-table iteration order is not a stable dump order. Sort names
        // and dependencies so captures can be compared without unrelated noise.
        eastl::sort( names.begin(), names.end() );

        for ( const auto name: names ) {
            const auto &pass{ mNodeControl->mNodes.at( eastl::string{ name.data(), name.size() } ) };
            auto label{ eastl::string{ name.data(), name.size() } };

            if ( !pass.mIsAlive ) {
                label += "\n[inactive]";
            }

            nodes.emplace( name, graph.AddNode( { label.data(), label.size() } ) );
        }

        eastl::vector<eastl::string_view> dependencies{};

        for ( const auto name: names ) {
            const auto &pass{ mNodeControl->mNodes.at( eastl::string{ name.data(), name.size() } ) };
            dependencies.clear();

            for ( const auto &dependency: pass.mDependsOn ) {
                dependencies.emplace_back( dependency.data(), dependency.size() );
            }

            eastl::sort( dependencies.begin(), dependencies.end() );

            for ( const auto dependency: dependencies ) {
                auto source{ nodes.find( dependency ) };

                if ( source == nodes.end() ) {
                    // A dump requested before compilation can contain an
                    // unresolved dependency; make that visible instead of
                    // inserting a pass into the real graph or dropping the edge.
                    auto label{ eastl::string{ dependency.data(), dependency.size() } };
                    label += "\n[missing pass]";
                    source = nodes.emplace( dependency, graph.AddNode( { label.data(), label.size() } ) ).first;
                }
                // An arrow means the destination depends on the source.
                ( void )graph.AddEdge( source->second, nodes.at( name ) );
            }
        }

        graph.Dump();
    }

    auto FrameGraph::IsPassPresent( eastl::string_view passName ) const -> bool {
        MKT_BEGIN_PROFILER();
        return mNodeControl->mNodes.contains( passName.data() );
    }

    auto FrameGraph::GetNodeControl() const -> const FGNodeControl & {
        MKT_BEGIN_PROFILER();
        return *mNodeControl;
    }

    auto FrameGraph::GetStatistics() const -> FrameGraphStatistics {
        FrameGraphStatistics statistics{};
        statistics.mGraphicsDeviceName = mDevice->GetDeviceName();
        statistics.mPassCount = core::as<core::u32>( mNodeControl->mNodes.size() );
        statistics.mExecutedPassCount = mExecutedPassCount;

        if ( mGraphicsRecordedThisFrame ) {
            statistics.mCommands += mGraphicsCommands->GetStatistics();
        }

        if ( mVideoRecordedThisFrame ) {
            statistics.mCommands += mVideoCommands->GetStatistics();
        }

        for ( const auto &[handle, node]: mNodeControl->mResources ) {
            switch ( mResourceManager->Get( handle ).mType ) {
                case FGResourceType::ePipeline:
                    ++statistics.mPipelineCount;
                    break;
                case FGResourceType::eTexture:
                    ++statistics.mImageCount;
                    break;
                case FGResourceType::eBuffer:
                    ++statistics.mBufferCount;
                    break;
                default:
                    break;
            }
        }

        return statistics;
    }
    auto FrameGraph::GetStatisticsManager() const -> const FGStatisticsManager & {
        MKT_BEGIN_PROFILER();
        return *mStatisticsManager;
    }

    auto FrameGraph::GetGpuProfiler() -> GpuQueryProfiler & {
        return *mGpuProfiler;
    }

    auto FrameGraph::GetDescriptorTable() const -> rhi::DescriptorTableHandle {
        return mDescriptorAllocator->GetDescriptorTable();
    }

    auto FrameGraph::GetDescriptorAllocator() -> FrameGraphDescriptorAllocator & {
        return *mDescriptorAllocator;
    }

    auto FrameGraph::GetDescriptorAllocator() const -> const FrameGraphDescriptorAllocator & {
        return *mDescriptorAllocator;
    }

    auto FrameGraph::GetTexture( FGTextureHandle handle ) const -> renderer::rhi::TextureHandle {
        MKT_BEGIN_PROFILER();
        return mResourceManager->Get( handle.mHandle ).mResource;
    }

    auto FrameGraph::GetBuffer( FGBufferHandle handle ) const -> renderer::rhi::BufferHandle {
        MKT_BEGIN_PROFILER();
        return mResourceManager->Get( handle.mHandle ).mResource;
    }

    auto FrameGraph::GetBlackBoard() const -> const core::Blackboard & {
        MKT_BEGIN_PROFILER();
        return mBlackboard;
    }

    auto FrameGraph::Create( renderer::rhi::IGpuDevice *device, asset::ShaderLibrary *shaderLibrary ) -> eastl::unique_ptr<FrameGraph> {
        MKT_BEGIN_PROFILER_NAMED();
        return eastl::make_unique<FrameGraph>( device, shaderLibrary );
    }

    auto FrameGraph::BindResources( renderer::rhi::CommandListHandle commandList ) -> void {
        MKT_BEGIN_PROFILER();
        renderer::rhi::PipelineType bindPoint{ renderer::rhi::PipelineType::eInvalid };

        switch ( commandList->GetQueueType() ) {
            case renderer::rhi::QueueType::eGraphics:
                bindPoint = renderer::rhi::PipelineType::eGraphics;
                break;
            case renderer::rhi::QueueType::eCompute:
                bindPoint = renderer::rhi::PipelineType::eCompute;
                break;
            default:;
        }

        // Just sanity check
        MKT_ASSERT( bindPoint != renderer::rhi::PipelineType::eInvalid, "Invalid pipeline bind point." );

        renderer::rhi::DescriptorTableHandle table{ mDescriptorAllocator->GetDescriptorTable() };
        renderer::rhi::PipelineLayoutHandle layout{ mPipelineLayout };

        // Resource layout is predefined and fixed for all pipelines
        // see this graph's layout creation and descriptor allocator
        commandList->BindPipelineResources( renderer::rhi::BindResourcesDescription{}
                .AddResourceSet( 0, table.GetPtr() )
                .SetPipelineLayout( layout.GetPtr() )
                .SetBindPoint( bindPoint ) );

        if ( bindPoint == renderer::rhi::PipelineType::eGraphics ) {
            // Graphics command lists also execute compute passes. The two
            // bind points have independent descriptor/root-signature state.
            commandList->BindPipelineResources( renderer::rhi::BindResourcesDescription{}
                    .AddResourceSet( 0, table.GetPtr() )
                    .SetPipelineLayout( layout.GetPtr() )
                    .SetBindPoint( renderer::rhi::PipelineType::eCompute ) );
        }
    }

    auto FrameGraph::CreateShaderTable( FGPipelineHandle pipeline ) -> FGShaderTableHandle {
        MKT_BEGIN_PROFILER();
        const auto &pipelineDescription{ mNodeControl->mResources.at( pipeline.mHandle ) };
        auto &resource{ mResourceManager->Allocate( FGResourceType::eShaderTable ) };
        const auto name{ string::Format( "{} Shader Table", pipelineDescription.mName ) };
        mNodeControl->mResources[resource.mResourceID] = FGNodeResource{
            .mName = name,
            .mDescription = FGShaderTableDescription{ name, pipeline },
            .mVersions = { ResourceVersion{} }
        };
        return { resource.mResourceID };
    }

    auto FrameGraph::Create( const FGDenoiserDescription &desc ) -> FGDenoiserHandle {
        auto &resource{ mResourceManager->Allocate( FGResourceType::eDenoiser ) };
        mNodeControl->mResources[resource.mResourceID] = FGNodeResource{
            .mName = desc.mName,
            .mDescription = static_cast<const rhi::DenoiserDescription &>( desc ),
            .mVersions = { ResourceVersion{} },
        };
        return { resource.mResourceID };
    }

    auto FrameGraph::Create( const FGPipelineDescription &desc ) -> FGPipelineHandle {
        MKT_BEGIN_PROFILER();
        auto &resource{ mResourceManager->Allocate( FGResourceType::ePipeline ) };
        mNodeControl->mResources[resource.mResourceID] = FGNodeResource{
            .mName = desc.mName,
            .mDescription = desc,
            .mVersions = { ResourceVersion{} }
        };
        return { resource.mResourceID };
    }

    auto FrameGraph::Create( const FGAccelerationStructureDescription &desc ) -> FGAccelerationStructureHandle {
        MKT_BEGIN_PROFILER();
        auto &resource{ mResourceManager->Allocate( FGResourceType::eAccelerationStructure ) };
        mNodeControl->mResources[resource.mResourceID] = FGNodeResource{
            .mName = desc.mName,
            .mDescription = desc,
            .mVersions = { ResourceVersion{} }
        };
        return { resource.mResourceID };
    }

    auto FrameGraph::Create( const FGVideoDescription &description ) -> FGVideoHandle {
        auto &resource{ mResourceManager->Allocate( FGResourceType::eVideo ) };
        resource.mNeedsRebuild = false;
        mNodeControl->mResources[resource.mResourceID] = FGNodeResource{
            .mName = description.mName,
            .mVersions = { ResourceVersion{} }
        };
        return { resource.mResourceID };
    }

    auto FrameGraph::Create( const FGBufferDescription &desc ) -> FGBufferHandle {
        MKT_BEGIN_PROFILER();
        auto &resource{ mResourceManager->Allocate( FGResourceType::eBuffer ) };
        auto bufferDesc{ renderer::rhi::BufferCreateDescription{}
            .SetBufferUsage( desc.mBufferUsageFlags )
            .SetName( desc.mName )
            .ForElement( desc.mElementSizeBytes, desc.mElementCount )
            .SetHeapType( desc.mHeapType )
            .SetCpuAccessType( desc.mHeapType == renderer::rhi::HeapType::eUpload ? renderer::rhi::AccessType::eWrite : renderer::rhi::AccessType::eRead )
            .SetByteSize( desc.mElementSizeBytes ) };

        if ( !desc.mInitialContents.IsEmpty() ) {
            bufferDesc.SetInitialData( desc.mInitialContents );
        }

        mNodeControl->mResources[resource.mResourceID] = FGNodeResource{
            .mName{ desc.mName },
            .mDescription = bufferDesc,
            .mVersions{ ResourceVersion{
                .mWriterPass{},  // No writers
                .mReaderPasses{},// No readers
            } },
            .mIsImported = false
        };

        return FGBufferHandle{ .mHandle = resource.mResourceID };
    }

    auto FrameGraph::ImportTexture( const filesystem::Path &path ) -> FGTextureHandle {
        MKT_BEGIN_PROFILER();
        asset::ImageHandle image{ asset::ProcessImage2D( path ) };
        auto textureDesc{ renderer::rhi::TextureCreateDescription{}
                .SetName( path.GetPath() )
                .SetImageData( image )
                .SetMipCount( image->mMipCount )
                .SetWidth( core::as<core::i32>( image->mWidth ) )
                .SetHeight( core::as<core::i32>( image->mHeight ) )
                .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
                .SetMultisampling( renderer::rhi::Multisampling::eMsaaX1 )
                .SetUsage( renderer::rhi::TextureUsageFlagsBits::ShaderResource )
                .SetFormat( asset::GetTextureFormat( image->mFormat ) ) };

        auto &resource{ mResourceManager->Allocate( FGResourceType::eTexture ) };

        mNodeControl->mResources[resource.mResourceID] = FGNodeResource{
            .mName{ string::Format( "FG Loaded Texture {}", path.GetC_Str() ) },
            .mDescription = textureDesc,// Retains the image until deferred GPU creation.
            .mVersions{ ResourceVersion{
                .mWriterPass{},  // No writers
                .mReaderPasses{},// No readers
            } },
            .mIsImported = false,
            .mSourcePath = path
        };

        return FGTextureHandle{ .mHandle = resource.mResourceID };
    }

    auto FrameGraph::ImportTexture( renderer::rhi::TextureHandle handle ) -> FGTextureHandle {
        MKT_BEGIN_PROFILER();

        if ( handle.IsEmpty() ) {
            return {};
        }

        auto &resource{ mResourceManager->Allocate( FGResourceType::eTexture, handle.GetPtr() ) };
        resource.mResource = handle;

        mNodeControl->mResources[resource.mResourceID] = FGNodeResource{
            .mName{ string::Format( "FG External Texture {}", handle->GetDebugName() ) },
            .mVersions{ ResourceVersion{
                .mWriterPass{},  // No writers
                .mReaderPasses{},// No readers
            } },
            .mIsImported = true
        };

        return FGTextureHandle{ .mHandle = resource.mResourceID };
    }

    auto FrameGraph::ImportBuffer( renderer::rhi::BufferHandle handle ) -> FGBufferHandle {
        MKT_BEGIN_PROFILER();

        if ( !handle ) {
            return {};
        }

        auto &resource{ mResourceManager->Allocate( FGResourceType::eBuffer, handle.GetPtr() ) };
        resource.mResource = handle;
        // Shared mesh storage can be imported repeatedly without discarding
        // the read/write dependencies already declared for this graph handle.
        mNodeControl->mResources.try_emplace( resource.mResourceID, FGNodeResource{
                                                                        .mName = string::Format( "FG External Buffer {}", handle->GetDebugName() ),
                                                                        .mVersions = { ResourceVersion{} },
                                                                        .mIsImported = true } );
        return FGBufferHandle{ .mHandle = resource.mResourceID };
    }

    auto FrameGraph::Create( const FGTextureDescription &desc ) -> FGTextureHandle {
        MKT_BEGIN_PROFILER();
        auto &resource{ mResourceManager->Allocate( FGResourceType::eTexture ) };
        auto textureDesc{ renderer::rhi::TextureCreateDescription{}
            .SetName( desc.mName )
            .SetWidth( desc.mWidth )
            .SetHeight( desc.mHeight )
            .SetDepth( desc.mDepth )
            .SetMipCount( desc.mMipCount )
            .SetDimensions( desc.mDimension )
            .SetHeapType( desc.mHeapType )
            .SetMultisampling( desc.mMultisampling )
            .SetUsage( desc.mUsage )
            .SetFormat( desc.mFormat ) };

        mNodeControl->mResources[resource.mResourceID] = FGNodeResource{
            .mName{ desc.mName },
            .mDescription = textureDesc,
            .mVersions{ ResourceVersion{
                .mWriterPass{},  // No writers
                .mReaderPasses{},// No readers
            } },
            .mIsImported = false
        };

        return FGTextureHandle{ .mHandle = resource.mResourceID };
    }

    auto FrameGraph::Create( const FGSamplerDescription &desc ) -> FGSamplerHandle {
        MKT_BEGIN_PROFILER();
        auto &resource{ mResourceManager->Allocate( FGResourceType::eSampler ) };
        auto samplerDesc{ renderer::rhi::SamplerCreateDescription{}
            .SetBorderColor( desc.mBorderColor )
            .SetMagFilter( desc.mMagFilter )
            .SetMinFilter( desc.mMinFilter )
            .SetMipLevels( desc.mMipLevels )
            .SetMaxAnisotropy( desc.mMaxAnisotropy )
            .SetWrapU( desc.mWrapU )
            .SetWrapV( desc.mWrapV )
            .SetWrapW( desc.mWrapW ) };

        mNodeControl->mResources[resource.mResourceID] = FGNodeResource{
            .mName{ desc.mName },
            .mDescription = samplerDesc,
            .mVersions{ ResourceVersion{
                .mWriterPass{},  // No writers
                .mReaderPasses{},// No readers
            } },
            .mIsImported = false
        };

        return FGSamplerHandle{ .mHandle = resource.mResourceID };
    }

    auto FrameGraph::CreateResource( FGResourceType type, const FGNodeResource &node, rhi::PipelineHandle pipeline ) -> core::Ref<core::Resource> {
        MKT_BEGIN_PROFILER();
        const auto &description{ node.mDescription };
        core::Ref<core::Resource> resource{};

        switch ( type ) {
            case FGResourceType::eDenoiser: {
                resource = mDevice->CreateDenoiser( eastl::any_cast<const rhi::DenoiserDescription &>( description ) );
                break;
            }
            case FGResourceType::eBuffer: {
                const auto &buffer{ eastl::any_cast<const rhi::BufferCreateDescription &>( description ) };

                if ( node.mInitializersReleased && !buffer.mSpanHandle ) {
                    MKT_THROW_RUNTIME_ERROR( string::Format(
                        "Supply fresh initializer data before rebuilding buffer '{}'.", node.mName ) );
                }

                resource = mDevice->CreateBuffer( buffer );
                break;
            }

            case FGResourceType::eTexture: {
                auto texture{ eastl::any_cast<const rhi::TextureCreateDescription &>( description ) };

                if ( !texture.mImageHandle && !texture.mBufferSpan ) {
                    const auto &source{ node.mSourcePath };

                    if ( !source.IsEmpty() ) {
                        auto image{ asset::ProcessImage2D( source ) };

                        if ( !image || image->mWidth != texture.mWidth || image->mHeight != texture.mHeight ||
                             image->mMipCount != texture.mMipCount || asset::GetTextureFormat( image->mFormat ) != texture.mFormat ) {
                            MKT_THROW_RUNTIME_ERROR( string::Format(
                                "Cannot reload texture '{}': source is unavailable or its dimensions, format or mip count changed. Reimport the texture.",
                                source.GetPath() ) );
                        }

                        texture.SetImageData( eastl::move( image ) );
                    }
                    else if ( node.mInitializersReleased ) {
                        MKT_THROW_RUNTIME_ERROR( string::Format(
                            "Supply fresh initializer data before rebuilding texture '{}'.", node.mName ) );
                    }
                }

                resource = mDevice->CreateTexture( texture );
                break;
            }

            case FGResourceType::eSampler:
                resource = mDevice->CreateSampler( eastl::any_cast<const rhi::SamplerCreateDescription &>( description ) );
                break;
            case FGResourceType::ePipeline:
                resource = CreatePipeline( eastl::any_cast<const FGPipelineDescription &>( description ) );
                break;
            case FGResourceType::eShaderTable:
                if ( !pipeline || pipeline->GetPipelineType() != rhi::PipelineType::eRayTracing ) {
                    MKT_THROW_INVALID_ARGUMENT( "A graph shader table requires a compiled ray-tracing pipeline." );
                }

                resource = mDevice->CreateShaderTable( rhi::ShaderTableDescription{}.SetPipeline( pipeline.GetPtr() ) );
                break;
            default:
                MKT_THROW_INVALID_ARGUMENT( "Cannot build this frame graph resource type." );
        }

        if ( !resource ) {
            MKT_THROW_RUNTIME_ERROR( string::Format( "Failed to create frame graph resource '{}'.", node.mName ).c_str() );
        }

        core::checked_cast<rhi::DeviceObject *>( resource.GetPtr() )->SetDebugName( node.mName );
        return resource;
    }

    auto FrameGraph::CreatePipeline( const FGPipelineDescription &desc ) -> rhi::PipelineHandle {
        if ( !mShaderLibrary ) {
            MKT_THROW_INVALID_ARGUMENT( "Frame graph pipeline creation requires a shader library." );
        }

        const auto loadShader{ [this]( const filesystem::Path &path, rhi::ShaderType stage, eastl::string_view entry ) {
            return mShaderLibrary->LoadShader( path.GetPath(), stage, entry );
        } };

        const auto entryPoint{ [&]( FGStageType stage ) -> eastl::string_view {
            const auto found{ desc.mEntryPoints.find( stage ) };
            return found != desc.mEntryPoints.end() ? eastl::string_view{ found->second } : eastl::string_view{ "main" };
        } };

        rhi::PipelineHandle pipeline{};
        if ( desc.mPipelineType == renderer::rhi::PipelineType::eGraphics ) {
            MKT_ASSERT( !desc.mShaders.empty(), "Creating graphics pipeline without shaders." );
            auto graphicsPipelineDesc{ renderer::rhi::GraphicsPipelineDescription{}
                .SetPipelineLayout( mPipelineLayout )
                .SetTopology( desc.mTopology )
                .SetCullMode( desc.mCullMode )
                .SetMultisampling( desc.mMultisampling )
                .SetPolygonMode( desc.mPolygonMode )
                .SetAlphaBlendState( desc.mAlphaBlendState )
                .SetDepthTest( desc.mEnableDepthTest )
                .SetDepthWrite( desc.mEnableDepthWrite )
                .SetDepthFormat( desc.mDepthFormat ) };

            // Color formats
            for ( const auto &format: desc.mColorFormats ) {
                graphicsPipelineDesc.AddColorFormat( format );
            }

            // Shaders
            for ( const auto &shader: desc.mShaders ) {
                graphicsPipelineDesc.AddShader( loadShader( shader.second, GetShaderFlagsFromStage( shader.first ), entryPoint( shader.first ) ) );
            }

            pipeline = mDevice->CreatePipeline( graphicsPipelineDesc );
        }
        else if ( desc.mPipelineType == renderer::rhi::PipelineType::eCompute ) {
            MKT_ASSERT( desc.mShaders.contains( FGStageType::eCompute ), "Creating compute pipeline without compute shader." );
            auto computePipelineDesc{ renderer::rhi::ComputePipelineDescription{}
                .SetPipelineLayout( mPipelineLayout )
                .SetComputeStage( loadShader( desc.mShaders.at( FGStageType::eCompute ),
                        renderer::rhi::ShaderType::eCompute, entryPoint( FGStageType::eCompute ) ) ) };

            pipeline = mDevice->CreatePipeline( computePipelineDesc );
        }
        else if ( desc.mPipelineType == rhi::PipelineType::eRayTracing ) {
            if ( !desc.mShaders.contains( FGStageType::eRayGen ) || !desc.mShaders.contains( FGStageType::eMiss ) ||
                 !desc.mShaders.contains( FGStageType::eClosestHit ) ) {
                MKT_THROW_RUNTIME_ERROR( "Ray-tracing graph pipelines require ray-generation, miss and closest-hit shaders." );
            }

            rhi::RayTracingPipelineDescription rayDescription{};
            rayDescription.mPipelineLayout = mPipelineLayout;
            rayDescription.mPayloadBytes = desc.mRayPayloadBytes;
            rayDescription.mRecursionDepth = desc.mRayRecursionDepth;
            rayDescription.mRayGenerationEntry = entryPoint( FGStageType::eRayGen );
            rayDescription.mMissEntry = entryPoint( FGStageType::eMiss );
            rayDescription.mClosestHitEntry = entryPoint( FGStageType::eClosestHit );
            rayDescription.mRayGeneration = loadShader( desc.mShaders.at( FGStageType::eRayGen ),
                rhi::ShaderType::eRayGeneration, rayDescription.mRayGenerationEntry );
            rayDescription.mMiss = loadShader( desc.mShaders.at( FGStageType::eMiss ),
                rhi::ShaderType::eMiss, rayDescription.mMissEntry );
            rayDescription.mClosestHit = loadShader( desc.mShaders.at( FGStageType::eClosestHit ),
                rhi::ShaderType::eClosestHit, rayDescription.mClosestHitEntry );
            pipeline = mDevice->CreatePipeline( rayDescription );
        }

        if ( !pipeline ) {
            MKT_THROW_RUNTIME_ERROR( "Failed to create framegraph pipeline." );
        }

        return pipeline;
    }

    auto FrameGraph::RegisterReadback( FGReadbackTask::Callback &&execute, bool runEveryFrame ) -> void {
        MKT_BEGIN_PROFILER();
        mReadbackTasks.emplace_back( ReadbackTask{
            .mTask = eastl::move( execute ),
            .mRunsPerFrame = runEveryFrame,
            .mHasBeenRegistered = false,
        } );
    }
}// namespace mikoto::renderer
