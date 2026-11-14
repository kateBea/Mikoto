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

#ifndef MIKOTO_RENDERER_FRAME_GRAPH_SCHEDULER_HH
#define MIKOTO_RENDERER_FRAME_GRAPH_SCHEDULER_HH

#include <mutex>

#include <EASTL/vector.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Rhi/CommandQueue.hh>

namespace mikoto::renderer {

    class FGScheduler final {
    public:

        explicit FGScheduler( rhi::IGpuDevice* device );

        // Pushes retain closed command lists and fence handles until Execute().
        // They may be called from recording workers after those lists are closed.
        auto PushRenderCommandList( const rhi::SubmitInfo& info ) -> void;
        auto PushComputeCommandList( const rhi::SubmitInfo& info ) -> void;
        auto PushTransferCommandList( const rhi::SubmitInfo& info ) -> void;

        auto BeginVideoWork() -> void;
        auto HasVideoWork() const -> bool;
        auto UseVideoResource( rhi::ICommandList* commands, rhi::TextureHandle texture,
                               bool reference, bool graphicsOutput ) -> void;
        auto RestoreGraphicsResources( rhi::ICommandList* graphics, rhi::ICommandList& video ) -> void;
        auto PushVideoCommandList( rhi::CommandListHandle commands ) -> void;

        // Resolve this batch before enqueueing it for RenderContext::SubmitFrame().
        // Waits without a producer in this batch remain external GPU dependencies.
        // Cross-queue dependencies on previous Execute() calls must be supplied by the caller.
        // Resource declarations come from the graph; this scheduler records queue handoffs.
        // Call on the graph owner after its recording workers have finished pushing.
        auto Execute() -> void;

    private:
        struct Submission {
            rhi::IQueue* mQueue{};
            rhi::QueueType mQueueType{ rhi::QueueType::eInvalid };
            rhi::SubmitInfo mInfo{};
        };

        auto PushCommandList( const rhi::SubmitInfo& info, rhi::IQueue* queue, rhi::QueueType type ) -> void;
        auto ResolveDependencies() -> void;

    private:
        std::mutex mMutex{};
        eastl::vector<Submission> mSubmissions{};
        eastl::vector<core::usize> mExecutionOrder{};

    private:
        rhi::IQueue* mGraphicsQueue{ nullptr };
        rhi::IQueue* mTransferQueue{ nullptr };
        rhi::IQueue* mComputeQueue{ nullptr };
        rhi::IQueue *mVideoDecodeQueue{ nullptr };
        rhi::IQueue *mVideoEnCodeQueue{ nullptr };

        rhi::CommandListHandle mGraphicsRelease{};

        core::u64 mVideoReady{};
        core::u64 mVideoFenceValue{};
        rhi::FenceHandle mVideoFence{};
        eastl::vector<rhi::TextureHandle> mVideoOutputs{};
        
        bool mVideoUsed{};
        bool mReleaseRecording{};

        rhi::IGpuDevice* mDevice{ nullptr };
    };
}// namespace mikoto::renderer

#endif//MIKOTO_RENDERER_FRAME_GRAPH_SCHEDULER_HH
