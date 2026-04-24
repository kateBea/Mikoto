//    Copyright 2025 ケイト
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

#ifndef MIKOTO_VULKAN_BUFFER_HH
#define MIKOTO_VULKAN_BUFFER_HH

#include <volk.h>
#include <vk_mem_alloc.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Renderer/Core/Rhi.hh>
#include <Renderer/Vulkan/VulkanMemoryAllocator.hh>

namespace mikoto::renderer::vulkan {

    class Buffer final : public IBuffer {
    public:
        explicit Buffer( const BufferCreateDescription& createInfo );

        auto PersistentMap() -> void;
        auto PersistentUnmap() -> void;

        MKT_NODISCARD auto GetAlignedSize() const -> u32;
        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) -> Object override;
        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) const -> Object override;

        MKT_NODISCARD auto IsMapped() const -> bool;
        MKT_NODISCARD auto GetMappedAddress() -> void*;
        MKT_NODISCARD auto GetMappedAddress() const -> const void*;

        ~Buffer() override;

    private:
        auto Release() -> void override;
        auto Initialize() -> void override;

    private:
        // If the buffer is dynamic this value contains
        // the size of each frame in flight slice
        size_t mAlignedSizeBytes{};
        size_t mStagingSliceSize{};

        BufferAllocation mAllocation{};

        bool mKeepInitializerResources{ false };
    };
}

#endif // MIKOTO_VULKAN_BUFFER_HH
