/**
 * VulkanUtils.hh
 * Created by kate on 8/5/2023.
 * */

#ifndef MIKOTO_VULKAN_UTILS_HH
#define MIKOTO_VULKAN_UTILS_HH

// C++ Standard Library
#include <vector>

// Third-Party Libraries
#include <volk.h>
#include <vk_mem_alloc.h>

// Project Headers
#include <Utility/Common.hh>

namespace Mikoto {
    struct BufferAllocateInfo {
        VkBuffer Buffer{};
        VkDeviceSize Size{};
        VmaAllocation Allocation{};
        VkBufferCreateInfo BufferCreateInfo{};
        VmaAllocationCreateInfo AllocationCreateInfo{};
    };

    struct ImageAllocateInfo {
        VkImage Image{};
        VkImageCreateInfo ImageCreateInfo{};
        VmaAllocation Allocation{};
        VmaAllocationCreateInfo AllocationCreateInfo{};
    };

    struct QueuePresentInfo {

    };

    struct CommandBuffersSubmitInfo {
        VkSubmitInfo SubmitInfo{};
        VkQueue QueueDst{};
        std::vector<VkSemaphore> WaitSemaphores{};
        std::vector<VkPipelineStageFlags> WaitStages{};
        std::vector<VkFence> InFlightFences{};
    };

} // NAMESPACE MIKOTO

namespace Mikoto::VulkanUtils {

    /**
         * Allocates a block of memory in the device's heap memory and binds given block to the given buffer
         * @param buffer
         * @param bufferMemory
         * @param size
         * @param properties
         * @param usage
         * @deprecated Prefer allocating memory buffers via VMA with the default allocator, number of allocations is pretty limited even in modern hardware
         * */
    auto CreateBuffer(VkBuffer& buffer, VkDeviceMemory& bufferMemory, VkDeviceSize size, VkMemoryPropertyFlags properties, VkBufferUsageFlags usage) -> void;

    /**
         * Copies a block of data to a different location
         * @param srcBuffer
         * @param dstBuffer
         * @param size
         * @param commandBuffer
         * @deprecated Previously used in conjunction with Create() to create staging buffers
         * */
    auto CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkCommandBuffer commandBuffer) -> void;

    /**
         * Uploads CPU accessible data to GPU readable memory
         * @param allocatedBufferData
         * */
    auto UploadBuffer(BufferAllocateInfo& allocatedBufferData) -> void;

    /**
         * Allocates an image
         * @param allocatedImageData
         * */
    auto AllocateImage(ImageAllocateInfo& allocatedImageData) -> void;

    /**
         * Wait on the host for the completion of outstanding queue operations for all queues the given device
         * @param device logical device to wait on
         * */
    auto WaitOnDevice(VkDevice device) -> void;

    /**
         * Wait on the specified queue to finish commands execution
         * @param device queue to wait on
         * */
    auto WaitOnQueue(VkQueue queue) -> void;

    /**
         * @param submitInfo
         * @returns
         * */
    auto SubmitCommandBuffers(const CommandBuffersSubmitInfo& submitInfo) -> VkResult;

    /**
         * @param presentInfo
         * @returns
         * */
    auto QueueImageForPresentation(const QueuePresentInfo& presentInfo) -> VkResult;

} // MIKOTO::VULKAN_UTILS

#endif // MIKOTO_VULKAN_UTILS_HH
