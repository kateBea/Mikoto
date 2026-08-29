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

#ifndef MIKOTO_VULKAN_MEMORY_ALLOCATOR_H
#define MIKOTO_VULKAN_MEMORY_ALLOCATOR_H

// Volk must be included before VMA
#include <volk.h>
#include <vk_mem_alloc.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Memory/GpuAllocator.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuDevice.hh>

namespace mikoto::renderer::vulkan {

    struct BufferAllocation {
        VkBuffer mBuffer{ VK_NULL_HANDLE };
        VmaAllocation mAllocation{ VK_NULL_HANDLE };

        VmaAllocationInfo mAllocationInfo{};

        VkBufferCreateInfo mBufferCreateInfo{};
        VmaAllocationCreateInfo mAllocationCreateInfo{};
    };

    struct ImageAllocation {
        VkImage mImage{ VK_NULL_HANDLE };
        VmaAllocation mAllocation{ VK_NULL_HANDLE };

        VkImageCreateInfo mImageCreateInfo{};
        VmaAllocationInfo mAllocationInfo{};

        VmaAllocationCreateInfo mAllocationCreateInfo{};
    };

    // Allocations are sync internally by VMA so it is safe
    // to call Allocation functions from any thread
    class GpuMemoryAllocator final : public memory::IGpuAllocator {
    public:
        explicit GpuMemoryAllocator( rhi::IGpuDevice* device );

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto FreeImage( ImageAllocation& allocation ) -> void;
        auto AllocateImage( ImageAllocation& allocation ) -> VkResult;

        auto FreeBuffer( BufferAllocation& allocation ) -> void;
        auto AllocateBuffer( BufferAllocation& allocation ) -> VkResult;

        auto MapBuffer( BufferAllocation& allocation ) const -> void;
        auto UnmapBuffer( BufferAllocation& allocation ) const -> void;

        // These are slow use for debug only
        MKT_NODISCARD auto GetMemoryUsage() const -> core::usize override;
        MKT_NODISCARD auto GetMemoryTotal() const -> core::usize override;
        MKT_NODISCARD auto GetMemoryAvailable() const -> core::usize override;

    private:
        // Map/Unmap Device memory to cpu accessible memory (map = true to map, false to unmap)
        auto MapBuffer( BufferAllocation& allocation, bool map ) const -> void;

    private:
        VmaAllocator mAllocator{};
        mutable VmaTotalStatistics mStats{};
    };
}// namespace Mikoto

#endif//MIKOTO_VULKAN_MEMORY_ALLOCATOR_H
