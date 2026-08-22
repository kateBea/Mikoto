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

    class IFence : public DeviceObject {
    public:

        MKT_NODISCARD virtual auto GetCompletionValue() const -> core::u64 = 0;

        MKT_NODISCARD virtual auto Signal( core::u64 fenceValue ) -> bool = 0;
        MKT_NODISCARD virtual auto Wait( core::u64 fenceValue, core::u64 timeoutMs ) -> bool = 0;

        using DeviceObject::Initialize;

    protected:
        auto Initialize() -> void override = 0;
        auto Release() -> void override = 0;
    };

    using FenceHandle = core::Ref<IFence>;
}


#endif//MIKOTO_RHI_FENCE_HH
