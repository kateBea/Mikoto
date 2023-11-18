/**
 * VulkanUtils.hh
 * Created by kate on 8/5/2023.
 * */

#ifndef MIKOTO_VULKAN_UTILS_HH
#define MIKOTO_VULKAN_UTILS_HH

// C++ Standard Library
#include <vector>
#include <memory>
#include <string_view>

// Third-Party Libraries
#include <volk.h>
#include <vk_mem_alloc.h>

// Project Headers
#include <Common/Common.hh>
#include <Common/Types.hh>
#include <Renderer/Material/Shader.hh>

namespace Mikoto {
    struct BufferAllocateInfo {
        VkBuffer Buffer{};
        VkDeviceSize Size{};
        VmaAllocation Allocation{};
        VkBufferCreateInfo BufferCreateInfo{};
        VmaAllocationCreateInfo AllocationCreateInfo{};

        // True if the allocation was mapped, false otherwise.
        // This is only used at the moment of creation of the buffer,
        // not to keep track of whether the buffer is mapped or not.
        // Allocation must be unmapped before destruction.
        bool WantMapping{};
    };

    struct ImageAllocateInfo {
        VkImage Image{};
        VmaAllocation Allocation{};
        VkImageCreateInfo ImageCreateInfo{};
        VmaAllocationCreateInfo AllocationCreateInfo{};
    };

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR Capabilities{};
        std::vector<VkSurfaceFormatKHR> Formats{};
        std::vector<VkPresentModeKHR> PresentModes{};
    };


    struct FramebufferAttachmentCreateInfo {
        UInt32_T Width{};
        UInt32_T Height{};
        UInt32_T LayerCount{};

        VkFormat Format{};

        VkImageUsageFlags Usage{};
        VkSampleCountFlagBits ImageSampleCount{ VK_SAMPLE_COUNT_1_BIT };
    };

    /**
         * Queues indices
         * TODO: implement with optionals
         * */
    struct QueuesData {
        VkQueue GraphicsQueue{};
        UInt32_T GraphicsFamilyIndex{};

        VkQueue PresentQueue{};
        UInt32_T PresentFamilyIndex{};

        bool GraphicsFamilyHasValue{false};
        bool PresentFamilyHasValue{false};

        MKT_NODISCARD auto IsComplete() const -> bool { return GraphicsFamilyHasValue && PresentFamilyHasValue; }
    };

    struct FrameSynchronizationPrimitives {
        VkSemaphore PresentSemaphore{ VK_NULL_HANDLE };
        VkSemaphore RenderSemaphore{ VK_NULL_HANDLE };

        VkFence RenderFence{ VK_NULL_HANDLE };
    };

    struct QueueSubmitInfo {
        FrameSynchronizationPrimitives SyncObjects{};
        const VkCommandBuffer *Commands{};
        UInt32_T CommandsCount{};
        VkQueue Queue{};
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

    inline constexpr auto GetErrorStr(VkResult result) -> std::string_view {
        switch(result) {
            case VK_SUCCESS: return "VK_SUCCESS";
            case VK_NOT_READY:
                break;
            case VK_TIMEOUT:
                break;
            case VK_EVENT_SET:
                break;
            case VK_EVENT_RESET:
                break;
            case VK_INCOMPLETE:
                break;
            case VK_ERROR_OUT_OF_HOST_MEMORY:
                break;
            case VK_ERROR_OUT_OF_DEVICE_MEMORY:
                break;
            case VK_ERROR_INITIALIZATION_FAILED:
                break;
            case VK_ERROR_DEVICE_LOST:
                break;
            case VK_ERROR_MEMORY_MAP_FAILED:
                break;
            case VK_ERROR_LAYER_NOT_PRESENT:
                break;
            case VK_ERROR_EXTENSION_NOT_PRESENT:
                break;
            case VK_ERROR_FEATURE_NOT_PRESENT:
                break;
            case VK_ERROR_INCOMPATIBLE_DRIVER:
                break;
            case VK_ERROR_TOO_MANY_OBJECTS:
                break;
            case VK_ERROR_FORMAT_NOT_SUPPORTED:
                break;
            case VK_ERROR_FRAGMENTED_POOL:
                break;
            case VK_ERROR_UNKNOWN:
                break;
            case VK_ERROR_OUT_OF_POOL_MEMORY:
                break;
            case VK_ERROR_INVALID_EXTERNAL_HANDLE:
                break;
            case VK_ERROR_FRAGMENTATION:
                break;
            case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
                break;
            case VK_PIPELINE_COMPILE_REQUIRED:
                break;
            case VK_ERROR_SURFACE_LOST_KHR:
                break;
            case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
                break;
            case VK_SUBOPTIMAL_KHR:
                break;
            case VK_ERROR_OUT_OF_DATE_KHR:
                break;
            case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
                break;
            case VK_ERROR_VALIDATION_FAILED_EXT:
                break;
            case VK_ERROR_INVALID_SHADER_NV:
                break;
            case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT:
                break;
            case VK_ERROR_NOT_PERMITTED_KHR:
                break;
            case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
                break;
            case VK_THREAD_IDLE_KHR:
                break;
            case VK_THREAD_DONE_KHR:
                break;
            case VK_OPERATION_DEFERRED_KHR:
                break;
            case VK_OPERATION_NOT_DEFERRED_KHR:
                break;
            case VK_RESULT_MAX_ENUM:
                break;
        }
    }

    MKT_NODISCARD auto CommandBufferBeginInfo(VkCommandBufferUsageFlags flags = 0) -> VkCommandBufferBeginInfo;

    MKT_NODISCARD auto SubmitInfo(VkCommandBuffer& command) -> VkSubmitInfo;

    auto PerformImageLayoutTransition(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkCommandBuffer cmd) -> void;

    MKT_NODISCARD auto CreateDescriptorSetLayoutBinding(VkDescriptorType type, VkShaderStageFlags stageFlags, UInt32_T binding) -> VkDescriptorSetLayoutBinding;

    auto CopyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent3D imageSize) -> void;

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

    /**
     * Returns a default initialized VkDescriptorPoolCreateInfo structure
     * @returns default initialized VkDescriptorPoolCreateInfo
     * */
    inline auto DescriptorPoolCreateInfo() -> VkDescriptorPoolCreateInfo {
        VkDescriptorPoolCreateInfo ret{};
        ret.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;

        return ret;
    }

    /**
     * Returns a default initialized VkCommandPoolCreateInfo structure
     * @returns default initialized VkCommandPoolCreateInfo
     * */
    inline auto CommandPoolCreateInfo() -> VkCommandPoolCreateInfo {
        VkCommandPoolCreateInfo ret{};
        ret.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

        return ret;
    }

    /**
     * Returns a default initialized VkCommandBufferAllocateInfo structure
     * @returns default initialized VkCommandBufferAllocateInfo
     * */
    inline auto CommandBufferAllocateInfo() -> VkCommandBufferAllocateInfo {
        VkCommandBufferAllocateInfo ret{};
        ret.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;

        return ret;
    }

    /**
     * Returns a default initialized VkCommandBufferBeginInfo structure
     * @returns default initialized VkCommandBufferBeginInfo
     * */
    inline auto CommandBufferBeginInfo() -> VkCommandBufferBeginInfo {
        VkCommandBufferBeginInfo ret{};
        ret.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        return ret;
    }

    /**
     * Returns a default initialized VkRenderPassBeginInfo structure
     * @returns default initialized VkRenderPassBeginInfo
     * */
    inline auto RenderPassBeginInfo() -> VkRenderPassBeginInfo {
        VkRenderPassBeginInfo ret{};
        ret.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;

        return ret;
    }

    /**
     * Returns a default initialized VkRenderPassCreateInfo structure
     * @returns default initialized VkRenderPassCreateInfo
     * */
    inline auto RenderPassCreateInfo() -> VkRenderPassCreateInfo {
        VkRenderPassCreateInfo ret{};
        ret.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

        return ret;
    }

    /**
     * Returns a default initialized VkFramebufferCreateInfo structure
     * @returns default initialized VkFramebufferCreateInfo
     * */
    inline auto FramebufferCreateInfo() -> VkFramebufferCreateInfo {
        VkFramebufferCreateInfo ret{};
        ret.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;

        return ret;
    }

    /**
     * Returns a default initialized VkDescriptorSetLayoutCreateInfo structure
     * @returns default initialized VkDescriptorSetLayoutCreateInfo
     * */
    inline auto DescriptorSetLayoutCreateInfo() -> VkDescriptorSetLayoutCreateInfo {
        VkDescriptorSetLayoutCreateInfo ret{};
        ret.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

        return ret;
    }

    /**
     * Returns a default initialized VkPipelineLayoutCreateInfo structure
     * @returns default initialized VkPipelineLayoutCreateInfo
     * */
    inline auto PipelineLayoutCreateInfo() -> VkPipelineLayoutCreateInfo {
        VkPipelineLayoutCreateInfo ret{};
        ret.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

        return ret;
    }

    /**
     * Returns a default initialized VkSubmitInfo structure
     * @returns default initialized VkSubmitInfo
     * */
    inline auto SubmitInfo() -> VkSubmitInfo {
        VkSubmitInfo ret{};
        ret.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        return ret;
    }

    /**
     * Returns a default initialized VkShaderModuleCreateInfo structure
     * @returns default initialized VkShaderModuleCreateInfo
     * */
    inline auto ShaderModuleCreateInfo() -> VkShaderModuleCreateInfo {
        VkShaderModuleCreateInfo ret{};
        ret.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

        return ret;
    }

    /**
     * Returns a default initialized VkPipelineShaderStageCreateInfo structure
     * @returns default initialized VkPipelineShaderStageCreateInfo
     * */
    inline auto PipelineShaderStageCreateInfo() -> VkPipelineShaderStageCreateInfo {
        VkPipelineShaderStageCreateInfo ret{};
        ret.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;

        return ret;
    }

    /**
     * Returns a default initialized VkGraphicsPipelineCreateInfo structure
     * @returns default initialized VkGraphicsPipelineCreateInfo
     * */
    inline auto GraphicsPipelineCreateInfo() -> VkGraphicsPipelineCreateInfo {
        VkGraphicsPipelineCreateInfo ret{};
        ret.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

        return ret;
    }

    /**
     * Returns a default initialized VkPipelineVertexInputStateCreateInfo structure
     * @returns default initialized VkPipelineVertexInputStateCreateInfo
     * */
    inline auto PipelineVertexInputStateCreateInfo() -> VkPipelineVertexInputStateCreateInfo {
        VkPipelineVertexInputStateCreateInfo ret{};
        ret.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        return ret;
    }

    /**
     * Returns a default initialized VkSamplerCreateInfo structure
     * @returns default initialized VkSamplerCreateInfo
     * */
    inline auto SamplerCreateInfo() -> VkSamplerCreateInfo {
        VkSamplerCreateInfo ret{};
        ret.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

        return ret;
    }

    /**
     * Returns a default initialized VkPresentInfoKHR structure
     * @returns default initialized VkPresentInfoKHR
     * */
    inline auto PresentInfoKHR() -> VkPresentInfoKHR {
        VkPresentInfoKHR ret{};
        ret.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        return ret;
    }

    /**
     * Returns a default initialized VkBufferCreateInfo structure
     * @returns default initialized VkBufferCreateInfo
     * */
    inline auto BufferCreateInfo() -> VkBufferCreateInfo {
        VkBufferCreateInfo ret{};
        ret.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;

        return ret;
    }

    /**
     * Returns a default initialized VkImageMemoryBarrier structure
     * @returns default initialized VkImageMemoryBarrier
     * */
    inline auto ImageMemoryBarrier() -> VkImageMemoryBarrier {
        VkImageMemoryBarrier ret{};
        ret.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;

        return ret;
    }
}

#endif // MIKOTO_VULKAN_UTILS_HH
