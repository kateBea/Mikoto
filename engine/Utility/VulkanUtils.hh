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
#include <Utility/Types.hh>
#include <Utility/Common.hh>
#include <Renderer/Material/Shader.hh>

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
     * Wait on the host for the completion of outstanding queue
     * operations for all queues of the given device
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

    /**
     *  Returns the minimum required alignment, in bytes, for the offset member of the
     *  VkDescriptorBufferInfo structure for uniform buffers.
     *  @returns minimum offset required in bytes for uniform buffers
     * */
    auto GetDeviceMinimumOffsetAlignment(VkPhysicalDevice physicalDevice) -> VkDeviceSize;

    /**
     * Returns the size in bytes required for a uniform buffer structure to be compatible
     * with the an device required minimum offset alignment
     * @param bufferOriginalSize uniform buffer size
     * @param deviceMinOffsetAlignment device minimum offset alignment
     * @returns required size to meet device minimum offset alignment requirements
     * */
    auto GetUniformBufferPadding(VkDeviceSize bufferOriginalSize, VkDeviceSize deviceMinOffsetAlignment) -> VkDeviceSize;

    /**
     * Returns the corresponding VkShaderStageFlagBits flag for the given stage
     * @param stage shader module stage
     * @returns corresponding flag
     * */
    auto GetVulkanShaderStageFlag(ShaderStage stage) -> VkShaderStageFlagBits;


} // MIKOTO::VULKAN_UTILS

namespace Mikoto::VulkanUtils::Initializers {
    /**
     * Returns a default initialized VkApplicationInfo structure
     * @returns default initialized VkApplicationInfo
     * */
    inline auto ApplicationInfo() -> VkApplicationInfo {
        VkApplicationInfo ret{};
        ret.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;

        return ret;
    }

    /**
     * Returns a default initialized VkInstanceCreateInfo structure
     * @returns default initialized VkInstanceCreateInfo
     * */
    inline auto InstanceCreateInfo() -> VkInstanceCreateInfo {
        VkInstanceCreateInfo ret{};
        ret.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

        return ret;
    }

    /**
     * Returns a default initialized VkDebugUtilsMessengerCreateInfoEXT structure
     * @returns default initialized VkDebugUtilsMessengerCreateInfoEXT
     * */
    inline auto DebugUtilsMessengerCreateInfoEXT() -> VkDebugUtilsMessengerCreateInfoEXT {
        VkDebugUtilsMessengerCreateInfoEXT ret{};
        ret.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

        return ret;
    }

    /**
     * Returns a default initialized VkDeviceCreateInfo structure
     * @returns default initialized VkDeviceCreateInfo
     * */
    inline auto DeviceCreateInfo() -> VkDeviceCreateInfo {
        VkDeviceCreateInfo ret{};
        ret.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        return ret;
    }

    /**
     * Returns a default initialized VkDeviceQueueCreateInfo structure
     * @returns default initialized VkDeviceQueueCreateInfo
     * */
    inline auto DeviceQueueCreateInfo() -> VkDeviceQueueCreateInfo {
        VkDeviceQueueCreateInfo ret{};
        ret.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;

        return ret;
    }

    /**
     * Returns a default initialized VkSwapchainCreateInfoKHR structure
     * @returns default initialized VkSwapchainCreateInfoKHR
     * */
    inline auto SwapchainCreateInfoKHR() -> VkSwapchainCreateInfoKHR {
        VkSwapchainCreateInfoKHR ret{};
        ret.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;

        return ret;
    }

    /**
     * Returns a default initialized VkImageViewCreateInfo structure
     * @returns default initialized VkImageViewCreateInfo
     * */
    inline auto ImageViewCreateInfo() -> VkImageViewCreateInfo {
        VkImageViewCreateInfo ret{};
        ret.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;

        return ret;
    }

    /**
     * Returns a default initialized VkImageCreateInfo structure
     * @returns default initialized VkImageCreateInfo
     * */
    inline auto ImageCreateInfo() -> VkImageCreateInfo {
        VkImageCreateInfo ret{};
        ret.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;

        return ret;
    }

    /**
     * Returns a default initialized VkSemaphoreCreateInfo structure
     * @returns default initialized VkSemaphoreCreateInfo
     * */
    inline auto SemaphoreCreateInfo() -> VkSemaphoreCreateInfo {
        VkSemaphoreCreateInfo ret{};
        ret.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        return ret;
    }

    /**
     * Returns a default initialized VkFenceCreateInfo structure
     * @returns default initialized VkFenceCreateInfo
     * */
    inline auto FenceCreateInfo() -> VkFenceCreateInfo {
        VkFenceCreateInfo ret{};
        ret.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

        return ret;
    }

    /**
     * Returns a default initialized VkMemoryAllocateInfo structure
     * @returns default initialized VkMemoryAllocateInfo
     * */
    inline auto MemoryAllocateInfo() -> VkMemoryAllocateInfo {
        VkMemoryAllocateInfo ret{};
        ret.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

        return ret;
    }
}

#endif // MIKOTO_VULKAN_UTILS_HH
