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
#include <Renderer/Vulkan/VulkanBuffer.hh>
#include <Renderer/Vulkan/VulkanTexture.hh>

namespace Mikoto {

    class VulkanMemoryAllocator final : public GpuAllocator {
    public:
        explicit VulkanMemoryAllocator( GpuDevice* device )
            : GpuAllocator{ device }
        {}

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto AllocateImage(VulkanTexture* texture ) -> VkResult;
        auto AllocateBuffer(VulkanBuffer* buffer ) -> VkResult;

        auto FreeImage(VulkanTexture* texture ) -> void;
        auto FreeBuffer(VulkanBuffer* buffer ) -> void;

        auto MapBuffer(VulkanBuffer* buffer ) const -> void;
        auto UnmapBuffer(VulkanBuffer* buffer ) const -> void;

    private:
        // Map/Unmap Device memory to cpu accessible memory (map = true to map, false to unmap)
        auto MapBuffer( VulkanBuffer* buffer, bool map) const -> void;

    private:

#if !defined(NDEBUG)
        //std::unordered_set<std::string> m_DebugNames{};
#endif


        VmaAllocator m_Allocator{};
    };
}



#endif //VULKANMEMORYALLOCATOR_H
