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
        eastl::fixed_vector<SignalInfo, 5> mSignals{};

        auto AddCommandList(CommandListHandle cmd) -> SubmitInfo&;
        auto AddSignal(FenceHandle fence, core::u64 value) -> SubmitInfo&;

        auto AddCommandLists(eastl::span<CommandListHandle> commands) -> SubmitInfo&;
        auto AddSignals(eastl::span<SignalInfo> signals) -> SubmitInfo&;
    };

    class IQueue : public DeviceObject {
    public:
        MKT_NODISCARD auto GetType() const -> QueueType;
        MKT_NODISCARD auto GetOpSupportFlags() const -> QueueOpSupportFlags;

        // Don't execute subsequent work until this synchronization object has reached value
        virtual auto Wait( IFence* fence, core::u64 value ) -> void = 0;

        // Queue execution will eventually advance this synchronization object to value
        virtual auto Signal( IFence* fence, core::u64 value ) -> void = 0;

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
