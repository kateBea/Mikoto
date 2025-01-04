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
#include "vk_mem_alloc.h"
#include "volk.h"

// Project Headers
#include "Common/Common.hh"
#include "Renderer/Buffer/VertexBuffer.hh"
#include "Material/Core/Shader.hh"
#include "STL/Utility/Types.hh"

// Vulkan version
#define MKT_VULKAN_VERSION_VARIANT 0
#define MKT_VULKAN_VERSION_MAJOR 1
#define MKT_VULKAN_VERSION_MINOR 3
#define MKT_VULKAN_VERSION_PATCH 0

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

    // TODO: move to VulkanImage.hh
    struct ImageAllocateInfo {
        VkImage Image{};
        VmaAllocation Allocation{};
        VkImageCreateInfo ImageCreateInfo{};
        VmaAllocationCreateInfo AllocationCreateInfo{};
    };

    // TODO: move to swap chain file
    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR Capabilities{};
        std::vector<VkSurfaceFormatKHR> Formats{};
        std::vector<VkPresentModeKHR> PresentModes{};
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


    MKT_NODISCARD inline auto ImageSubresourceRange(VkImageAspectFlags aspectMask) -> VkImageSubresourceRange {
        VkImageSubresourceRange subImage {};
        subImage.aspectMask = aspectMask;
        subImage.baseMipLevel = 0;
        subImage.levelCount = 1;
        subImage.baseArrayLayer = 0;
        subImage.layerCount = 1;

        return subImage;
    }

    MKT_NODISCARD inline auto FindQueueFamilies( VkPhysicalDevice device, VkSurfaceKHR surface ) -> QueuesData {
        QueuesData indices{};

        UInt32_T queueFamilyCount{};
        vkGetPhysicalDeviceQueueFamilyProperties( device, &queueFamilyCount, nullptr );

        std::vector<VkQueueFamilyProperties> queueFamilies( queueFamilyCount );
        vkGetPhysicalDeviceQueueFamilyProperties( device, &queueFamilyCount, queueFamilies.data() );

        Int32_T i{};
        for ( const auto& queueFamily: queueFamilies ) {
            if ( ( queueFamily.queueCount > 0 ) && ( queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT ) ) {
                indices.GraphicsFamilyIndex = i;
                indices.GraphicsFamilyHasValue = true;
            }

            VkBool32 presentSupport{ VK_FALSE };
            vkGetPhysicalDeviceSurfaceSupportKHR( device, i, surface, &presentSupport );
            if ( queueFamily.queueCount > 0 && presentSupport ) {
                indices.PresentFamilyIndex = i;
                indices.PresentFamilyHasValue = true;
            }

            if ( indices.IsComplete() )
                break;

            i++;
        }

        return indices;
    }

    MKT_NODISCARD auto CommandBufferBeginInfo(VkCommandBufferUsageFlags flags = 0) -> VkCommandBufferBeginInfo;

    MKT_NODISCARD auto SubmitInfo(VkCommandBuffer& command) -> VkSubmitInfo;

    auto PerformImageLayoutTransition(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkCommandBuffer cmd) -> void;

    auto TransitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout) -> void;

    MKT_NODISCARD auto CreateDescriptorSetLayoutBinding(VkDescriptorType type, VkShaderStageFlags stageFlags, UInt32_T binding) -> VkDescriptorSetLayoutBinding;

    auto CopyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent3D imageSize) -> void;

    MKT_NODISCARD MKT_UNUSED_FUNC static inline auto QueueFamilySupportsOperation(VkQueueFamilyProperties queueFamily, VkQueueFlagBits flags) -> bool { return queueFamily.queueFlags & flags; }
    MKT_NODISCARD MKT_UNUSED_FUNC static inline auto QueueFamilySupportsGraphicsOperations(VkQueueFamilyProperties queueFamily, VkQueueFlagBits flags) -> bool { return queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT; }
    MKT_NODISCARD MKT_UNUSED_FUNC static inline auto QueueFamilySupportsComputeOperations(VkQueueFamilyProperties queueFamily, VkQueueFlagBits flags) -> bool { return queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT; }
    MKT_NODISCARD MKT_UNUSED_FUNC static inline auto QueueFamilySupportsTransferOperations(VkQueueFamilyProperties queueFamily, VkQueueFlagBits flags) -> bool { return queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT; }

    /**
         * Returns the VkFormat corresponding to the shader data type as follows:
         * float: VK_FORMAT_R32_SFLOAT
         * vec2: VK_FORMAT_R32G32_SFLOAT
         * vec3: VK_FORMAT_R32G32B32_SFLOAT
         * vec4: VK_FORMAT_R32G32B32A32_SFLOAT
         * ivec2: VK_FORMAT_R32G32_SINT, a 2-component vector of 32-bit signed integers
         * uvec4: VK_FORMAT_R32G32B32A32_UINT, a 4-component vector of 32-bit unsigned integers
         * double: VK_FORMAT_R64_SFLOAT, a double-precision (64-bit) float
         * @param type represents the shader data type
         * */
    MKT_NODISCARD inline auto GetVulkanAttributeDataType(ShaderDataType type) -> VkFormat {
        switch(type) {
            case ShaderDataType::FLOAT_TYPE: return VK_FORMAT_R32_SFLOAT;
            case ShaderDataType::FLOAT2_TYPE: return VK_FORMAT_R32G32_SFLOAT;
            case ShaderDataType::FLOAT3_TYPE: return VK_FORMAT_R32G32B32_SFLOAT;
            case ShaderDataType::FLOAT4_TYPE: return VK_FORMAT_R32G32B32A32_SFLOAT;

            case ShaderDataType::MAT3_TYPE: return VK_FORMAT_UNDEFINED; //temporary
            case ShaderDataType::MAT4_TYPE: return VK_FORMAT_UNDEFINED; //temporary

            case ShaderDataType::INT_TYPE:  return VK_FORMAT_R32_SINT;
            case ShaderDataType::INT2_TYPE: return VK_FORMAT_R32G32_SINT;
            case ShaderDataType::INT3_TYPE: return VK_FORMAT_R32G32B32_SINT;
            case ShaderDataType::INT4_TYPE: return VK_FORMAT_R32G32B32A32_SINT;
            case ShaderDataType::BOOL_TYPE: return VK_FORMAT_R32_SINT;

            case ShaderDataType::NONE:
            case ShaderDataType::COUNT: [[fallthrough]];
            default:
                MKT_ASSERT(false, "Invalid shader data type");
        }
    }

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

    /**
     * Returns a default initialized VkDescriptorSetAllocateInfo structure
     * @returns default initialized VkDescriptorSetAllocateInfo
     * */
    inline auto DescriptorSetAllocateInfo() -> VkDescriptorSetAllocateInfo {
        VkDescriptorSetAllocateInfo ret{};
        ret.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;

        return ret;
    }

    /**
     * Returns a default initialized VkWriteDescriptorSet structure
     * @returns default initialized VkWriteDescriptorSet
     * */
    inline auto WriteDescriptorSet() -> VkWriteDescriptorSet {
        VkWriteDescriptorSet ret{};
        ret.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

        return ret;
    }

    /**
     * Returns a default initialized VkPhysicalDeviceVulkan13Features structure
     * @returns default initialized VkPhysicalDeviceVulkan13Features
     * */
    inline auto PhysicalDeviceVulkan13Features() -> VkPhysicalDeviceVulkan13Features {
        VkPhysicalDeviceVulkan13Features ret{};
        ret.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

        return ret;
    }

    /**
     * Returns a default initialized VkPhysicalDeviceFeatures2 structure
     * @returns default initialized VkPhysicalDeviceFeatures2
     * */
    inline auto PhysicalDeviceFeatures2() -> VkPhysicalDeviceFeatures2 {
        VkPhysicalDeviceFeatures2 ret{};
        ret.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

        return ret;
    }
}

#endif // MIKOTO_VULKAN_UTILS_HH
