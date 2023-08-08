/**
 * VulkanUtils.hh
 * Created by kate on 8/5/2023.
 * */

#ifndef KATE_ENGINE_VULKAN_UTILS_HH
#define KATE_ENGINE_VULKAN_UTILS_HH

// C++ Standard Library
#include <vector>

// Third-Party Libraries
#include <volk.h>
#include <vk_mem_alloc.h>

namespace kaTe {
    struct AllocatedBuffer {
        VkBuffer Buffer{};
        VkDeviceSize Size{}; // In Bytes of the buffer
        VmaAllocation Allocation{};
    };

    class VulkanUtils {
    public:
        /**
         * Allocates a block of memory in the device's heap memory and binds given block to the given buffer
         * @param buffer
         * @param bufferMemory
         * @param size
         * @param properties
         * @param usage
         * @deprecated Prefer allocating memory buffers via VMA with the default allocator, number of allocations is pretty limited
         * */
        static auto CreateBuffer(VkBuffer& buffer, VkDeviceMemory& bufferMemory, VkDeviceSize size, VkMemoryPropertyFlags properties, VkBufferUsageFlags usage) -> void;

        /**
         * Copies a block of data to a different location
         * @param srcBuffer
         * @param dstBuffer
         * @param size
         * @param commandBuffer
         * @deprecated Previously used in conjunction with CreateBuffer() to create staging buffers
         * */
        static auto CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkCommandBuffer commandBuffer) -> void;

        /**
         * Uploads CPU accessible data to GPU readable memory
         * @param allocatedBufferData
         * */
        static auto UploadBuffer(AllocatedBuffer &allocatedBufferData) -> void;
    };
}
#endif//KATE_ENGINE_VULKAN_UTILS_HH
