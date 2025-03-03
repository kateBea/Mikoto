/**
 * VulkanHelpers.hh
 * Created by kate on 8/5/2023.
 * */

#ifndef MIKOTO_VULKAN_UTILS_HH
#define MIKOTO_VULKAN_UTILS_HH

// C++ Standard Library
#include <set>
#include <vector>
#include <memory>
#include <string_view>
#include <optional>

// Third-Party Libraries
#include "vk_mem_alloc.h"
#include "volk.h"

// Project Headers
#include "Common/Common.hh"
#include "Renderer/Buffer/VertexBuffer.hh"
#include "Material/Core/Shader.hh"
#include "Library/Utility/Types.hh"

// Vulkan version
#define MKT_VULKAN_VERSION_VARIANT 0
#define MKT_VULKAN_VERSION_MAJOR 1
#define MKT_VULKAN_VERSION_MINOR 3
#define MKT_VULKAN_VERSION_PATCH 0

namespace Mikoto {

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR Capabilities{};
        std::vector<VkSurfaceFormatKHR> Formats{};
        std::vector<VkPresentModeKHR> PresentModes{};
    };

    struct VulkanQueueData {
        VkQueue Queue{};
        UInt32_T FamilyIndex{};
    };

    struct QueuesData {
        std::optional<VulkanQueueData> Present{};
        std::optional<VulkanQueueData> Graphics{};
        std::optional<VulkanQueueData> Compute{};
    };

    struct FrameSynchronizationPrimitives {
        VkSemaphore PresentSemaphore{ VK_NULL_HANDLE };
        VkSemaphore RenderSemaphore{ VK_NULL_HANDLE };
        VkFence RenderFence{ VK_NULL_HANDLE };
    };

    struct ComputeSynchronizationPrimitives {
        std::vector<VkSemaphore> WaitSemaphores{};
        std::vector<VkSemaphore> SignalSemaphores{};
        VkFence Fence{ VK_NULL_HANDLE };
    };


}

namespace Mikoto::VulkanHelpers {
    // We take a set because for instance graphics queue and present queue could be the same, if u try to create two queues of same index program will crash
    MKT_NODISCARD auto SetupDeviceQueueCreateInfo(const std::set<UInt32_T>& uniqueQueueFamilies) -> std::vector<VkDeviceQueueCreateInfo>;

    auto CopyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent3D imageSize) -> void;

    MKT_NODISCARD auto GetSwapChainSupport( const VkPhysicalDevice& device, const VkSurfaceKHR& surface ) -> SwapChainSupportDetails;
    MKT_NODISCARD auto GetVulkanAttributeDataType(ShaderDataType type) -> VkFormat;
    MKT_NODISCARD auto HasGraphicsQueue( const VkQueueFamilyProperties& queueFamily ) -> bool;
    MKT_NODISCARD auto HasComputeQueue( const VkQueueFamilyProperties& queueFamily ) -> bool;
    MKT_NODISCARD auto HasPresentQueue( const VkPhysicalDevice& device, UInt32_T queueFamilyIndex, const VkSurfaceKHR& surface, const VkQueueFamilyProperties& queueFamilyProperties ) -> bool;
    MKT_NODISCARD auto GetVkStageFromShaderStage(ShaderStage stage) -> VkShaderStageFlagBits;
    MKT_NODISCARD auto GetUniformBufferPadding(VkDeviceSize bufferOriginalSize, VkDeviceSize deviceMinOffsetAlignment) -> VkDeviceSize;

} // MIKOTO::VULKAN_UTILS


namespace Mikoto::VulkanHelpers::Initializers {
    /**
     *
     * @param aspectMask
     * @return
     * */
    MKT_NODISCARD inline auto ImageSubresourceRange(VkImageAspectFlags aspectMask) -> VkImageSubresourceRange {
        VkImageSubresourceRange subImage {};
        subImage.aspectMask = aspectMask;
        subImage.baseMipLevel = 0;
        subImage.levelCount = 1;
        subImage.baseArrayLayer = 0;
        subImage.layerCount = 1;

        return subImage;
    }

    MKT_NODISCARD inline auto CreateDescriptorSetLayoutBinding(VkDescriptorType type, VkShaderStageFlags stageFlags, UInt32_T binding) -> VkDescriptorSetLayoutBinding {
        VkDescriptorSetLayoutBinding layoutBinding{};
        layoutBinding.binding = binding;
        layoutBinding.descriptorCount = 1;
        layoutBinding.descriptorType = type;
        layoutBinding.pImmutableSamplers = nullptr;
        layoutBinding.stageFlags = stageFlags;

        return layoutBinding;
    }

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
     * Returns a default initialized VkApplicationInfo structure
     * @returns default initialized VkApplicationInfo
     * */
    inline auto ComputePipelineCreateInfo() -> VkComputePipelineCreateInfo {
        VkComputePipelineCreateInfo ret{};
        ret.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;

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

    inline auto PushConstantRange(VkShaderStageFlags stageFlags, UInt32_T size, UInt32_T offset) -> VkPushConstantRange {
        const VkPushConstantRange pushConstantRange {
            .stageFlags{ stageFlags },
            .offset{ offset },
            .size{ size }
        };

        return pushConstantRange;
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
