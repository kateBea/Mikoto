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

#ifndef MIKOTO_RHI_FENCE_HH
#define MIKOTO_RHI_FENCE_HH

#include <EASTL/string.h>
#include <EASTL/numeric.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/ResourcePool.hh>

#include <Memory/BufferSpan.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Utility.hh>
#include <Renderer/Rhi/DeviceObject.hh>

namespace mikoto::renderer::rhi {

    /**
     * Synchronization primitive shared by the host and GPU queues.
     *
     * @ref Signal and @ref Wait operate on monotonically increasing fence values.
     */
    class IFence : public DeviceObject {
    public:

        /**
         * Returns the most recently completed fence value.
         * @returns The result of GetCompletionValue.
         */
        MKT_NODISCARD virtual auto GetCompletionValue() const -> core::u64 = 0;

        /**
         * Signals the fence from the host with @p fenceValue.
         *
         * @param fenceValue Input value used by this operation.
         * @returns The result of Signal.
         */
        MKT_NODISCARD virtual auto Signal( core::u64 fenceValue ) -> bool = 0;

        /**
         * Blocks the host until @p fenceValue completes or @p timeoutMs expires.
         *
         * @param fenceValue Input value used by this operation.
         * @param timeoutMs Input value used by this operation.
         * @returns The result of Wait.
         */
        MKT_NODISCARD virtual auto Wait( core::u64 fenceValue, core::u64 timeoutMs ) -> bool = 0;

        using DeviceObject::Initialize;

    protected:

        /**
         * Creates the native fence or timeline synchronization primitive.
         */
        auto Initialize() -> void override = 0;

        /**
         * Releases the native fence or timeline synchronization primitive.
         */
        auto Destroy() -> void override = 0;
    };

    using FenceHandle = core::Ref<IFence>;
}


#endif//MIKOTO_RHI_FENCE_HH
