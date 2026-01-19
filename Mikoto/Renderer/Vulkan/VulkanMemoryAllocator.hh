//
// Created by zanet on 10/6/2025.
//

#ifndef VULKANMEMORYALLOCATOR_H
#define VULKANMEMORYALLOCATOR_H

#include <unordered_set>

// Volk must be included before VMA
#include <volk.h>
#include <vk_mem_alloc.h>

#include <Memory/GpuAllocator.hh>
#include <Renderer/Vulkan/VulkanTexture.hh>

namespace Mikoto {

    struct BufferAllocation {
        VkBuffer Buffer{};

        VmaAllocation Allocation{};
        VmaAllocationInfo AllocationInfo{};

        VkBufferCreateInfo BufferCreateInfo{};
        VmaAllocationCreateInfo AllocationCreateInfo{};
    };

    struct ImageAllocation {

    };

    class VulkanMemoryAllocator final : public GpuAllocator {
    public:
        explicit VulkanMemoryAllocator( GpuDevice* device );

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto AllocateImage(VulkanTexture* texture ) -> VkResult;
        auto AllocateImage(VulkanTextureCube* texture ) -> VkResult;

        auto FreeImage(VulkanTexture* texture ) -> void;
        auto FreeImage(VulkanTextureCube* texture ) -> void;

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
        auto MapBuffer( BufferAllocation& allocation, bool map) const -> void;

    private:
        mutable VmaTotalStatistics m_Stats{};

        VmaAllocator m_Allocator{};
    };


#define MKT_VMA_ALLOC_PTR(GPU_DEVICE) dynamic_cast<VulkanMemoryAllocator*>(TO_VK_DEVICE( GPU_DEVICE )->GetAllocator())
}



#endif //VULKANMEMORYALLOCATOR_H
