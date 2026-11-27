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

// =================================================
// Refs:
// https://stoleckipawel.dev/posts/frame-graph-theory/
// https://stoleckipawel.dev/posts/frame-graph-build-it/
// https://stoleckipawel.dev/posts/frame-graph-advanced/
// https://stoleckipawel.dev/posts/frame-graph-production/
// https://traverse.nl/blog/render-graph-101/#dead-stripping
// =================================================

#ifndef MIKOTO_RENDERER_CORE_FRAME_GRAPH_HH
#define MIKOTO_RENDERER_CORE_FRAME_GRAPH_HH

#include <source_location>

#include <ankerl/unordered_dense.h>

#include <EASTL/fixed_hash_map.h>
#include <EASTL/fixed_vector.h>
#include <EASTL/functional.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/unordered_map.h>
#include <EASTL/vector.h>

#include <taskflow/taskflow.hpp>

#include <Assets/AssetRegistryService.hh>

#include <Core/Blackboard.hh>
#include <Core/Core.hh>
#include <Core/Profiler.hh>
#include <Core/Resource.hh>
#include <Core/Types.hh>

#include <Memory/Allocator.hh>
#include <Memory/FreeListAllocator.hh>
#include <Memory/MemoryArena.hh>

#include <Renderer/Core/FrameGraphCompiler.hh>
#include <Renderer/Core/FrameGraphDescriptorAllocator.hh>
#include <Renderer/Core/FrameGraphPass.hh>
#include <Renderer/Core/FrameGraphResourceManager.hh>
#include <Renderer/Core/FrameGraphScheduler.hh>
#include <Renderer/Core/FrameGraphTypes.hh>
#include <Renderer/Core/GpuQueryProfiler.hh>
#include <Renderer/Rhi/CommandList.hh>
#include <Renderer/Rhi/Descriptor.hh>
#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Rhi/Types.hh>

namespace mikoto::renderer {

    // Ref:
    // https://stoleckipawel.dev/posts/frame-graph-theory/
    // https://godotengine.org/article/rendering-acyclic-graph/
    // https://levelup.gitconnected.com/organizing-gpu-work-with-directed-acyclic-graphs-f3fd5f2c2af3

    class CommandContext;

    struct FrameGraphStatistics {
        eastl::string_view mGraphicsDeviceName{};

        rhi::CommandListStatistics mCommands{};

        core::u32 mPassCount{};
        core::u32 mExecutedPassCount{};
        core::u32 mPipelineCount{};
        core::u32 mImageCount{};
        core::u32 mBufferCount{};
    };

    struct FGNodeStatistics {
        rhi::CommandListStatistics mCommands{};

        // Per call to Execute(...) not the total
        core::u32 mDrawCallCount{};
        core::u32 mDispatchCallCount{};

        core::u32 mTextureSRVCount{};
        core::u32 mTextureUAVCount{};

        core::u32 mBufferSRVCount{};
        core::u32 mBufferUAVCount{};

        // CPU time spent in the pass callback,
        // in milliseconds (not GPU time).
        core::u64 mExecutionCount{};
        core::f64 mMaxExecutionTime{};
        core::f64 mMinExecutionTime{};
        core::f64 mLastExecutionTime{};

        bool mExecutedThisFrame{};
    };

    class FGStatisticsManager {
    public:
        explicit FGStatisticsManager() = default;

        MKT_NODISCARD auto GetNode( eastl::string_view name ) -> FGNodeStatistics *;
        MKT_NODISCARD auto GetNode( eastl::string_view name ) const -> const FGNodeStatistics *;

        MKT_NODISCARD auto RegisterNode( eastl::string_view name ) -> FGNodeStatistics *;

        MKT_NODISCARD auto GetStatistics() const -> const ankerl::unordered_dense::map<eastl::string, FGNodeStatistics> &;

    private:
        // unique_ptr for stable ptrs
        ankerl::unordered_dense::map<eastl::string, FGNodeStatistics> mGraphNodes{};
    };

    struct FGReadbackTask {
        using Callback = eastl::function<void( core::Blackboard &, const FGResourceManager & )>;
        Callback mCallback{};
        core::u64 mFenceValue{};

        // It starts as ready so we can update
        // the fence value for the first submission
        bool mIsReady{ true };

        bool mRunsEveryFrame{ false };
    };

    // Manages GPU to CPU readbacks
    class FGReadbackManager final {
    public:
        static constexpr core::u32 kMaxReadbacks{ 15 };

        explicit FGReadbackManager( core::Blackboard &blackboard, FGResourceManager &manager );

        auto ExecuteCallbacks( core::u64 fenceValue ) -> void;

        auto UpdateTaskFenceValue( core::u32 taskID, core::u64 fenceValue ) -> void;
        auto RegisterCallback( const FGReadbackTask::Callback &callback, bool runEveryFrame = false ) -> core::u32;

    private:
        core::Blackboard *mBlackboard{};
        FGResourceManager *mResourceManager{};
        ankerl::unordered_dense::map<core::u32, FGReadbackTask> mReadbackTasks{};
        core::u32 mNextTaskID{ 1 };
    };

    struct FGNodeControl final : public core::INonCopyable, core::INonMoveable {
        ankerl::unordered_dense::map<eastl::string, FrameGraphPass> mNodes;
        ankerl::unordered_dense::map<FGResourceHandle, FGNodeResource> mResources;

        FGNodeControl();
        ~FGNodeControl();

        auto Clear() -> void;
        auto RegisterContext( const eastl::string &name, FrameGraphPass &node,
            FGResourceManager &resources, FrameGraphDescriptorAllocator &descriptors,
            rhi::PipelineLayoutHandle pipelineLayout, FGStatisticsManager &statistics ) -> void;

        MKT_NODISCARD auto GetContext( const eastl::string &name ) -> CommandContext &;

    private:
        // Keep the by-value context map in the implementation, where its value
        // type is complete. One storage allocation per graph, not per pass.
        struct ContextStorage;
        eastl::unique_ptr<ContextStorage> mContexts;
    };

    class FGNodeBuilder final {
    public:
        explicit FGNodeBuilder( FrameGraphPass &node, FGNodeControl &control );

        auto UseResource( FGTextureHandle handle, FGPipelineStage state, FGResourceAccess access ) -> void;
        auto UseResource( FGBufferHandle handle, FGPipelineStage state, FGResourceAccess access ) -> void;
        auto UseResource( FGVideoHandle handle, FGPipelineStage state, FGResourceAccess access ) -> void;
        auto UseResource( FGAccelerationStructureHandle handle, FGPipelineStage state, FGResourceAccess access ) -> void;

        /** Declares the current images of a runtime video batch.
         * The batch handle supplies graph dependencies; the resolver supplies concrete
         * images immediately before recording this pass's barriers and commands.
         * Declare multi-planar storage once, rather than its individual sampling views.
         */
        auto UseTextures( FGVideoHandle batch, FGPipelineStage stage, FGResourceAccess access,
            const eastl::function<void( eastl::vector<rhi::TextureHandle> & )> &resolveTextures ) -> void;

    private:
        auto UseResource( FGResourceHandle handle, FGPipelineStage stage, FGResourceAccess access, FGResourceType type ) -> void;
        auto TrackRead( FGResourceHandle handle ) -> void;
        auto TrackWrite( FGResourceHandle handle ) -> void;

    private:
        FrameGraphPass *mGraphNode{};
        FGNodeControl *mNodeControl{};
    };

    class FrameGraph final {
    public:
        explicit FrameGraph( rhi::IGpuDevice *device, asset::ShaderLibrary *shaderLibrary );

        auto Compile() -> void;
        auto Execute() -> void;

        auto ExecuteReadbacks() -> void;

        auto SetExecutionPolicy( eastl::string_view passName, FGExecutionPolicy policy ) -> void;

        auto DisablePass( eastl::string_view passName ) -> void;
        auto EnablePass( eastl::string_view passName ) -> void;

        auto DumpHierarchy() -> void;

        MKT_NODISCARD auto IsPassPresent( eastl::string_view passName ) const -> bool;

        MKT_NODISCARD auto GetNodeControl() const -> const FGNodeControl &;
        MKT_NODISCARD auto GetNodeControl() -> FGNodeControl &;
        MKT_NODISCARD auto GetStatisticsManager() const -> const FGStatisticsManager &;
        MKT_NODISCARD auto GetStatistics() const -> FrameGraphStatistics;
        MKT_NODISCARD auto GetGpuProfiler() -> GpuQueryProfiler &;

        MKT_NODISCARD auto GetTexture( FGTextureHandle handle ) const -> rhi::TextureHandle;
        MKT_NODISCARD auto GetBuffer( FGBufferHandle handle ) const -> rhi::BufferHandle;

        MKT_NODISCARD auto GetDescriptorTable() const -> rhi::DescriptorTableHandle;

        /** Descriptor releases require completion of every recording using the entry. */
        MKT_NODISCARD auto GetDescriptorAllocator() -> FrameGraphDescriptorAllocator &;
        MKT_NODISCARD auto GetDescriptorAllocator() const -> const FrameGraphDescriptorAllocator &;

        MKT_NODISCARD auto GetBlackBoard() const -> const core::Blackboard &;

        MKT_NODISCARD auto ImportTexture( const filesystem::Path &path ) -> FGTextureHandle;
        MKT_NODISCARD auto ImportTexture( rhi::TextureHandle handle ) -> FGTextureHandle;
        MKT_NODISCARD auto ImportBuffer( rhi::BufferHandle handle ) -> FGBufferHandle;

        // TODO: Explore multilayered textures, this could be interesting for bloom which
        // due to resource usage limitations it implies multiple passes on same resources but different usages.
        // For instance within the same pass a resource can be used as render target but another mip maybe as shader read.
        // https://godotengine.org/article/rendering-acyclic-graph/
        /** Registers descriptions only; GPU objects are available after Compile(). */
        MKT_NODISCARD auto Create( const FGTextureDescription &desc ) -> FGTextureHandle;
        MKT_NODISCARD auto Create( const FGBufferDescription &desc ) -> FGBufferHandle;
        MKT_NODISCARD auto Create( const FGVideoDescription &desc ) -> FGVideoHandle;
        MKT_NODISCARD auto Create( const FGPipelineDescription &desc ) -> FGPipelineHandle;
        MKT_NODISCARD auto Create( const FGSamplerDescription &desc ) -> FGSamplerHandle;
        MKT_NODISCARD auto Create( const FGAccelerationStructureDescription &desc ) -> FGAccelerationStructureHandle;

        MKT_NODISCARD auto CreateShaderTable( FGPipelineHandle pipeline ) -> FGShaderTableHandle;

        template<typename PassData, typename SetupFunc, typename ExecuteFunc>
        auto RegisterPass( eastl::string_view name, FGPassType nodeType, SetupFunc &&setup, ExecuteFunc &&execute,
            std::source_location source = std::source_location::current() ) -> void {
            MKT_BEGIN_PROFILER();
            FrameGraphPass &node{ mNodeControl->mNodes[name.data()] = FrameGraphPass{
                                      .mName{ name },
                                      .mDebugLabel{ string::Format( "Pass: {}", name ) },
                                      .mSourceLocation = source,
                                      .mType = nodeType,
                                      .mExecuteCallback = [execute]( CommandContext &ctx, core::Blackboard &blackboard ) -> void {
                                          execute( ctx, blackboard );
                                      },
                                  } };
            PassData &data{ GetOrCreate<PassData>() };
            node.mBuilderCallback = [&]() -> void {
                FGNodeBuilder builder{ node, *mNodeControl };
                setup( builder, data );
            };

            node.mBuilderCallback();
            ( void )mStatisticsManager->RegisterNode( name );
        }

        template<typename SetupFunc, typename ExecuteFunc>
        auto RegisterPass( eastl::string_view name, FGPassType nodeType, SetupFunc &&setup, ExecuteFunc &&execute,
            std::source_location source = std::source_location::current() ) -> void {
            MKT_BEGIN_PROFILER();
            FrameGraphPass &node{ mNodeControl->mNodes[name.data()] = FrameGraphPass{
                                      .mName{ name },
                                      .mDebugLabel{ string::Format( "Pass: {}", name ) },
                                      .mSourceLocation = source,
                                      .mType = nodeType,
                                      .mExecuteCallback = [execute]( CommandContext &ctx, core::Blackboard &blackboard ) -> void {
                                          execute( ctx, blackboard );
                                      },
                                  } };
            node.mBuilderCallback = [&]() -> void {
                FGNodeBuilder builder{ node, *mNodeControl };
                setup( builder, mBlackboard );
            };

            node.mBuilderCallback();
            ( void )mStatisticsManager->RegisterNode( name );
        }

        template<typename T, typename... Args>
        MKT_NODISCARD auto GetOrCreate( Args &&...args ) -> T & {
            MKT_BEGIN_PROFILER();
            if ( mBlackboard.Contains<T>() ) {
                return mBlackboard.Get<T>();
            }

            return mBlackboard.Add<T>( eastl::forward<Args>( args )... );
        }

        template<typename T>
        MKT_NODISCARD auto GetData() const -> const T & {
            MKT_BEGIN_PROFILER();
            return mBlackboard.Get<T>();
        }

        template<typename T>
        MKT_NODISCARD auto Exists() const -> bool {
            MKT_BEGIN_PROFILER();
            return mBlackboard.Contains<T>();
        }

        auto RegisterReadback( FGReadbackTask::Callback &&execute, bool runEveryFrame = false ) -> void;

        MKT_NODISCARD static auto Create( renderer::rhi::IGpuDevice *device, asset::ShaderLibrary *shaderLibrary ) -> eastl::unique_ptr<FrameGraph>;

    private:
        friend class FGCompiler;

        // The compiler schedules these factories; creation stays with the graph.
        MKT_NODISCARD auto CreateResource( FGResourceType type, const FGNodeResource &node,
            rhi::PipelineHandle pipeline = {} ) -> core::Ref<core::Resource>;
        MKT_NODISCARD auto CreatePipeline( const FGPipelineDescription &description ) -> rhi::PipelineHandle;

        auto BindResources( rhi::CommandListHandle commandList ) -> void;
        auto RestoreGraphicsResources() -> void;

        auto RecordVideoCommands() -> void;
        auto RecordGraphicsCommands() -> void;
        auto SubmitRuntimeTextureBarriers( const FrameGraphPass &pass, CommandContext &context ) -> void;

        auto SubmitRecordedCommands() -> void;
        auto CollectRetiredResources() -> void;

        auto ProcessReadbackTasks( core::u64 submissionFenceValue ) -> void;

    private:
        struct ReadbackTask {
            core::u32 mTaskID{};
            FGReadbackTask::Callback mTask{};

            bool mRunsPerFrame{};
            bool mHasBeenRegistered{};
        };

        struct RetiredResource {
            core::Ref<core::Resource> mResource{};
            core::u64 mFenceValue{};
        };

        struct RuntimeTexture {
            FGResource mResource{};
            bool mUsedThisFrame{};
        };

    private:
        rhi::IGpuDevice *mDevice{};
        asset::ShaderLibrary *mShaderLibrary{};

        core::Blackboard mBlackboard{};

        // Execution plan
        core::Ref<FGExecutionPlan> mExecutionPlan{};
        eastl::unique_ptr<FGNodeControl> mNodeControl{};

        // Management
        eastl::unique_ptr<FGScheduler> mScheduler{};
        eastl::unique_ptr<FGCompiler> mGraphCompiler{};
        eastl::unique_ptr<FGResourceManager> mResourceManager{};
        eastl::unique_ptr<FrameGraphDescriptorAllocator> mDescriptorAllocator{};
        eastl::unique_ptr<FGReadbackManager> mReadbackManager{};

        rhi::PipelineLayoutHandle mPipelineLayout{};

        // Statistics
        core::u32 mExecutedPassCount{};
        bool mGraphicsRecordedThisFrame{};
        bool mVideoRecordedThisFrame{};

        eastl::unique_ptr<GpuQueryProfiler> mGpuProfiler{};
        eastl::unique_ptr<FGStatisticsManager> mStatisticsManager{};

        eastl::vector<ReadbackTask> mReadbackTasks{};
        ankerl::unordered_dense::map<const rhi::ITexture *, eastl::unique_ptr<RuntimeTexture>> mRuntimeTextures{};

        eastl::vector<RetiredResource> mRetiredResources{};

        core::u64 mFenceValue{};
        rhi::FenceHandle mFence{};

        rhi::CommandListHandle mGraphicsCommands{};
        rhi::CommandListHandle mComputeCommands{};
        rhi::CommandListHandle mTransferCommands{};
        rhi::CommandListHandle mVideoCommands{};
    };
}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_CORE_FRAME_GRAPH_HH
