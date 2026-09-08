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

#ifndef MIKOTO_RHI_COMMAND_QUEUE_HH
#define MIKOTO_RHI_COMMAND_QUEUE_HH

#include <EASTL/span.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/ResourcePool.hh>
#include <Core/String.hh>
#include <Core/Types.hh>

#include <Memory/BufferSpan.hh>

#include <Renderer/Rhi/Fence.hh>
#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Utility.hh>
#include <Renderer/Rhi/CommandList.hh>
#include <Renderer/Rhi/DeviceObject.hh>

namespace mikoto::renderer::rhi {

    /**
     * A fence value used to synchronize queue submission.
     */
    struct SignalInfo {
        core::u64 mSignalValue{};
        FenceHandle mSignalFence{};
    };

    /**
     * Batch of command lists and queue-level fence waits/signals.
     */
    struct SubmitInfo {
        eastl::fixed_vector<CommandListHandle, 5> mCommands{};

        eastl::fixed_vector<SignalInfo, 5> mWaits{};
        eastl::fixed_vector<SignalInfo, 5> mSignals{};

        /**
         * Adds the supplied value through AddCommandList.
         *
         * @param cmd Input value used by this operation.
         * @returns The result of AddCommandList.
         */
        auto AddCommandList(CommandListHandle cmd) -> SubmitInfo&;

        /**
         * Adds the supplied value through AddWait.
         *
         * @param fence Input value used by this operation.
         * @param value Input value used by this operation.
         * @returns The result of AddWait.
         */
        auto AddWait(FenceHandle fence, core::u64 value) -> SubmitInfo&;

        /**
         * Adds the supplied value through AddSignal.
         *
         * @param fence Input value used by this operation.
         * @param value Input value used by this operation.
         * @returns The result of AddSignal.
         */
        auto AddSignal(FenceHandle fence, core::u64 value) -> SubmitInfo&;

        /**
         * Adds the supplied value through AddCommandLists.
         *
         * @param commands Input value used by this operation.
         * @returns The result of AddCommandLists.
         */
        auto AddCommandLists(eastl::span<CommandListHandle> commands) -> SubmitInfo&;

        /**
         * Adds the supplied value through AddWaits.
         *
         * @param signals Input value used by this operation.
         * @returns The result of AddWaits.
         */
        auto AddWaits(eastl::span<SignalInfo> signals) -> SubmitInfo&;

        /**
         * Adds the supplied value through AddSignals.
         *
         * @param signals Input value used by this operation.
         * @returns The result of AddSignals.
         */
        auto AddSignals(eastl::span<SignalInfo> signals) -> SubmitInfo&;
    };

    /**
     * Queue that executes submitted command lists.
     *
     * Queue waits and signals are encoded in @ref SubmitInfo. They are GPU-side
     * dependencies; use @ref IFence directly for host-side synchronization.
     */
    class IQueue : public DeviceObject {
    public:

        /**
         * Returns the queue family represented by this queue.
         * @returns The result of GetType.
         */
        MKT_NODISCARD auto GetType() const -> QueueType;

        /**
         * Returns the operations supported by this queue.
         * @returns The result of GetOpSupportFlags.
         */
        MKT_NODISCARD auto GetOpSupportFlags() const -> QueueOpSupportFlags;

        /**
         * Submits command lists and their GPU-side fence dependencies.
         *
         * @param submitInfo Input value used by this operation.
         */
        virtual auto ExecuteCommandLists( const SubmitInfo& submitInfo ) -> void = 0;

        /**
         * Destroys the queue.
         */
        ~IQueue() override = default;

        using DeviceObject::Initialize;

    protected:

        /**
         * Constructs a queue with its type and supported operations.
         *
         * @param type Queue type.
         * @param flags Supported queue operations.
         */
        explicit IQueue( QueueType type, QueueOpSupportFlags flags );

    protected:
        QueueType mType{ QueueType::eInvalid };
        QueueOpSupportFlags mOpSupportFlags{ QueueOpSupportFlagsBits::Graphics };
    };

    using QueueHandle = core::Ref<IQueue>;
}

#endif//MIKOTO_RHI_COMMAND_QUEUE_HH
