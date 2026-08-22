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

    struct SignalInfo {
        core::u64 mSignalValue{};
        FenceHandle mSinalFence{};
    };

    struct SubmitInfo {
        eastl::fixed_vector<CommandListHandle, 5> mCommands{};

        eastl::fixed_vector<SignalInfo, 5> mWaits{};
        eastl::fixed_vector<SignalInfo, 5> mSignals{};

        auto AddCommandList(CommandListHandle cmd) -> SubmitInfo&;
        auto AddWait(FenceHandle fence, core::u64 value) -> SubmitInfo&;
        auto AddSignal(FenceHandle fence, core::u64 value) -> SubmitInfo&;

        auto AddCommandLists(eastl::span<CommandListHandle> commands) -> SubmitInfo&;
        auto AddWaits(eastl::span<SignalInfo> signals) -> SubmitInfo&;
        auto AddSignals(eastl::span<SignalInfo> signals) -> SubmitInfo&;
    };

    // Queue waits and signals are specified via the SubmitInfo struct
    // The list of signals specify a list of fences upon which we queue a signal
    // on the Device side (GPU will change the value to the specified one when done
    // processing the given batch of commands); on the other hand the list of waits
    // queue a wait on Device to hold execution of commands until the specified
    // value has been reached on the provided fences. For Host side wait/signal
    // use the Fence interface instead.
    class IQueue : public DeviceObject {
    public:
        MKT_NODISCARD auto GetType() const -> QueueType;
        MKT_NODISCARD auto GetOpSupportFlags() const -> QueueOpSupportFlags;

        virtual auto ExecuteCommandLists( const SubmitInfo& submitInfo ) -> void = 0;

        ~IQueue() override = default;

        using DeviceObject::Initialize;

    protected:
        explicit IQueue( QueueType type, QueueOpSupportFlags flags );

    protected:
        QueueType mType{ QueueType::eInvalid };
        QueueOpSupportFlags mOpSupportFlags{ QueueOpSupportFlagsBits::kGraphics };
    };

    using QueueHandle = core::Ref<IQueue>;
}

#endif//MIKOTO_RHI_COMMAND_QUEUE_HH
