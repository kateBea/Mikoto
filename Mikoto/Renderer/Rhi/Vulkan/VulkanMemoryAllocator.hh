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

#ifndef MIKOTO_RENDERER_RHI_VULKAN_VULKAN_MEMORY_ALLOCATOR_HH
#define MIKOTO_RENDERER_RHI_VULKAN_VULKAN_MEMORY_ALLOCATOR_HH

#include <vk_mem_alloc.h>
#include <volk.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Rhi/GpuAllocator.hh>

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
    class GpuMemoryAllocator final : public rhi::IGpuAllocator {
    public:
        explicit GpuMemoryAllocator( rhi::IGpuDevice* device );

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto FreeImage( ImageAllocation& allocation ) -> void;
        auto AllocateImage( ImageAllocation& allocation ) -> VkResult;

        auto FreeBuffer( BufferAllocation& allocation ) -> void;
        auto AllocateBuffer( BufferAllocation& allocation ) -> VkResult;

        auto BeginFrame() -> void override;
        auto GetProcessMemoryUsage() const -> rhi::ProcessGpuMemory override;

        // Allocates memory for objects such as video sessions that have no buffer or image.
        auto FreeMemory( VmaAllocation allocation ) -> void;
        auto AllocateMemory( const VkMemoryRequirements& requirements, const VmaAllocationCreateInfo& description, VmaAllocation& allocation, VmaAllocationInfo& info ) -> VkResult;

        auto MapBuffer( BufferAllocation& allocation ) const -> void;
        auto UnmapBuffer( BufferAllocation& allocation ) const -> void;

        auto InvalidateBuffer( const BufferAllocation& allocation ) const -> void;
        auto FlushBuffer( const BufferAllocation& allocation, VkDeviceSize offset, VkDeviceSize size ) const -> void;

        auto SetAllocationName( VmaAllocation allocation, eastl::string_view name) -> void;

        MKT_NODISCARD auto GetStatsString() -> eastl::string;

        // Byte counts for device-local heaps only. Available is remaining VMA
        // budget, not physical capacity minus usage. Without VK_EXT_memory_budget
        // enabled in VMA, usage and budget are allocator estimates.
        MKT_NODISCARD auto GetMemoryUsage() const -> core::usize override;
        MKT_NODISCARD auto GetMemoryTotal() const -> core::usize override;
        MKT_NODISCARD auto GetMemoryAvailable() const -> core::usize override;

    private:
        // Map/Unmap Device memory to cpu accessible memory (map = true to map, false to unmap)
        auto MapBuffer( BufferAllocation& allocation, bool map ) const -> void;

    private:
        rhi::IGpuDevice* mDevice{};
        VmaAllocator mAllocator{};
    };
}// namespace mikoto::renderer::vulkan

#endif// MIKOTO_RENDERER_RHI_VULKAN_VULKAN_MEMORY_ALLOCATOR_HH

