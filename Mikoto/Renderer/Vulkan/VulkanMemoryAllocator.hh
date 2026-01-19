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

#include <Memory/GpuAllocator.hh>

namespace Mikoto {

    struct BufferAllocation {
        VkBuffer Buffer{};

        VmaAllocation Allocation{};
        VmaAllocationInfo AllocationInfo{};

        VkBufferCreateInfo BufferCreateInfo{};
        VmaAllocationCreateInfo AllocationCreateInfo{};
    };

    struct ImageAllocation {
        VkImage Image{ VK_NULL_HANDLE };

        VkImageCreateInfo ImageCreateInfo{};
        VmaAllocationInfo AllocationInfo{};
        VmaAllocation Allocation{ VK_NULL_HANDLE };

        VmaAllocationCreateInfo AllocationCreateInfo{};
    };

    class VulkanMemoryAllocator final : public GpuAllocator {
    public:
        explicit VulkanMemoryAllocator( GpuDevice* device );

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto FreeImage( ImageAllocation& allocation ) -> void;
        auto AllocateImage( ImageAllocation& allocation ) -> VkResult;

        auto FreeBuffer( BufferAllocation& allocation ) -> void;
        auto AllocateBuffer( BufferAllocation& allocation ) -> VkResult;

        auto MapBuffer( BufferAllocation& allocation ) const -> void;
        auto UnmapBuffer( BufferAllocation& allocation ) const -> void;

        // These are slow use for debug only
        MKT_NODISCARD auto GetMemoryUsage() const -> Size override;
        MKT_NODISCARD auto GetMemoryTotal() const -> Size override;
        MKT_NODISCARD auto GetMemoryAvailable() const -> Size override;

    private:
        // Map/Unmap Device memory to cpu accessible memory (map = true to map, false to unmap)
        auto MapBuffer( BufferAllocation& allocation, bool map ) const -> void;

    private:
        VmaAllocator m_Allocator{};
        mutable VmaTotalStatistics m_Stats{};
    };

#define MKT_VMA_ALLOC_PTR( GPU_DEVICE ) dynamic_cast<VulkanMemoryAllocator*>( TO_VK_DEVICE( GPU_DEVICE )->GetAllocator() )
}// namespace Mikoto


#endif//MIKOTO_VULKAN_MEMORY_ALLOCATOR_H
