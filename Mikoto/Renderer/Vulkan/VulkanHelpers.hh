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

#ifndef MIKOTO_VULKAN_UTILS_HH
#define MIKOTO_VULKAN_UTILS_HH

#include <volk.h>

#include <cpptrace/cpptrace.hpp>

#include <Core/Core.hh>
#include <Core/String.hh>
#include <Core/Types.hh>
#include <Core/Exception.hh>

#include <Logging/Logger.hh>

#include <Renderer/Core/GpuDevice.hh>

#define MKT_VK_FLAGS_NONE 0

namespace mikoto::renderer::vulkan {

    using namespace mikoto::core;

    MKT_NODISCARD auto GetGpuDeviceType( GpuDeviceType type ) -> VkPhysicalDeviceType;
    MKT_NODISCARD auto GetResultString( VkResult result ) -> const char*;
    MKT_NODISCARD auto GetFormat( Format format ) -> VkFormat;
    MKT_NODISCARD auto GetAspectMask( VkFormat format) -> VkImageAspectFlags;
    MKT_NODISCARD auto GetAspectMask( Format format ) -> VkImageAspectFlags;

    MKT_NODISCARD auto GetShaderModuleStage(ShaderType stage) -> VkShaderStageFlagBits;
    MKT_NODISCARD auto GetShaderModuleStage(VkShaderStageFlagBits stage) -> ShaderType;

    MKT_NODISCARD auto GetTopology(PrimitiveTopology topology) -> VkPrimitiveTopology;
    MKT_NODISCARD auto GetSampleCount(Multisampling msaa) -> VkSampleCountFlagBits;
    MKT_NODISCARD auto GetCullMode(CullMode mode) -> VkCullModeFlags;
    MKT_NODISCARD auto GetCompareOp(DepthCompareOp op) -> VkCompareOp;

    MKT_NODISCARD auto GetInputRate(InputRate rate) -> VkVertexInputRate;

    MKT_NODISCARD auto GetSamplerFilter( SamplerFilter filter ) -> VkFilter;
    MKT_NODISCARD auto GetSamplerWrap( SamplerWrapMode wrap ) -> VkSamplerAddressMode;

    MKT_NODISCARD auto GetQueueName( QueueType type ) -> eastl::string_view;

    MKT_NODISCARD auto GetViewType( TextureDimension dimensions ) -> VkImageViewType;
    MKT_NODISCARD auto GetTextureType( TextureDimension dimensions ) -> VkImageType;

    MKT_NODISCARD auto GetImageLayout( ResourceStates state ) -> VkImageLayout;
    MKT_NODISCARD auto GetResourceState( VkImageLayout layout ) -> ResourceStates;
    MKT_NODISCARD auto GetStageMask(ResourceStates state) -> VkPipelineStageFlags2;
    MKT_NODISCARD auto GetAccessMask(ResourceStates state) -> VkAccessFlags2;

    MKT_NODISCARD auto GetIndexType(Format format) -> VkIndexType;

    MKT_NODISCARD auto GetShaderStageFlags( ShaderStage visibility ) -> VkShaderStageFlags;
    MKT_NODISCARD auto GetDescriptorType( ResourceType type ) -> VkDescriptorType;

    MKT_NODISCARD auto GetArraLayerCount( TextureDimension dimension, u32 requestedLayers = 1 ) -> u32;

    MKT_NODISCARD auto GetImageUsage(TextureUsageFlags flags ) -> VkImageUsageFlags;

#define MKT_VK_CHECK( expr )                                               \
    do {                                                                   \
        VkResult _vk_result{ ( expr ) };                                   \
        if ( _vk_result != VK_SUCCESS ) {                                  \
            MKT_FILE_LOGGER_ERROR(                                         \
                    "Vulkan error: {} (code: {}) at {}:{}",                \
                    GetResultString( _vk_result ), as<i32>( _vk_result ),  \
                    __FILE__, __LINE__ );                                  \
                                                                           \
            cpptrace::generate_trace().print();                            \
            throw mikoto::core::RuntimeException( string::Format(          \
                    "Vulkan call failed: {}\nFile: {}\nLine: {}",          \
                    GetResultString( _vk_result ), __FILE__, __LINE__ ) ); \
        }                                                                  \
    } while ( 0 )

}// namespace mikoto::renderer::vulkan

namespace mikoto::renderer::vulkan::initializers {

    MKT_NODISCARD auto ApplicationInfo() -> VkApplicationInfo;
    MKT_NODISCARD auto InstanceCreateInfo() -> VkInstanceCreateInfo;
    MKT_NODISCARD auto SemaphoreCreateInfo() -> VkSemaphoreCreateInfo;
    MKT_NODISCARD auto CommandPoolCreateInfo() -> VkCommandPoolCreateInfo;
    MKT_NODISCARD auto FenceCreateInfo( VkFenceCreateFlags flags ) -> VkFenceCreateInfo;
    MKT_NODISCARD auto CommandBufferAllocateInfo() -> VkCommandBufferAllocateInfo;
    MKT_NODISCARD auto DebugUtilsMessengerCreateInfoEXT() -> VkDebugUtilsMessengerCreateInfoEXT;
    MKT_NODISCARD auto DynamicRenderingFeature() -> VkPhysicalDeviceDynamicRenderingFeatures;
    MKT_NODISCARD auto PhysicalDeviceFeatures2() -> VkPhysicalDeviceFeatures2;
    MKT_NODISCARD auto DeviceCreateInfo() -> VkDeviceCreateInfo;
    MKT_NODISCARD auto PhysicalDeviceVulkan13Features() -> VkPhysicalDeviceVulkan13Features;
    MKT_NODISCARD auto PhysicalDeviceVulkan12Features() -> VkPhysicalDeviceVulkan12Features;
    MKT_NODISCARD auto PhysicalDeviceVulkan11Features() -> VkPhysicalDeviceVulkan11Features;
    MKT_NODISCARD auto DeviceQueueCreateInfo() -> VkDeviceQueueCreateInfo;
    MKT_NODISCARD auto DescriptorPoolCreateInfo() -> VkDescriptorPoolCreateInfo;
    MKT_NODISCARD auto SwapchainCreateInfoKHR() -> VkSwapchainCreateInfoKHR;
    MKT_NODISCARD auto ImageViewCreateInfo() -> VkImageViewCreateInfo;
    MKT_NODISCARD auto RenderingAttachmentInfo() -> VkRenderingAttachmentInfo;
    MKT_NODISCARD auto PresentInfoKHR() -> VkPresentInfoKHR;
    MKT_NODISCARD auto SamplerCreateInfo() -> VkSamplerCreateInfo;
    MKT_NODISCARD auto ImageCreateInfo() -> VkImageCreateInfo;
    MKT_NODISCARD auto RenderingInfo() -> VkRenderingInfo;
    MKT_NODISCARD auto CommandBufferBeginInfo() -> VkCommandBufferBeginInfo;
    MKT_NODISCARD auto SemaphoreTypeCreateInfo() -> VkSemaphoreTypeCreateInfo;
    MKT_NODISCARD auto ImageBlit2() -> VkImageBlit2;
    MKT_NODISCARD auto ImageCopy2() -> VkImageCopy2;
    MKT_NODISCARD auto ShaderModuleCreateInfo() -> VkShaderModuleCreateInfo;
    MKT_NODISCARD auto PipelineShaderStageCreateInfo() -> VkPipelineShaderStageCreateInfo;
    MKT_NODISCARD auto FramebufferCreateInfo() -> VkFramebufferCreateInfo;
    MKT_NODISCARD auto PipelineRenderingCreateInfo() -> VkPipelineRenderingCreateInfo;
    MKT_NODISCARD auto GraphicsPipelineCreateInfo() -> VkGraphicsPipelineCreateInfo;
    MKT_NODISCARD auto PipelineVertexInputStateCreateInfo() -> VkPipelineVertexInputStateCreateInfo;
    MKT_NODISCARD auto PipelineLayoutCreateInfo() -> VkPipelineLayoutCreateInfo;
    MKT_NODISCARD auto ComputePipelineCreateInfo() -> VkComputePipelineCreateInfo;
    MKT_NODISCARD auto BufferCreateInfo() -> VkBufferCreateInfo;
    MKT_NODISCARD auto WriteDescriptorSet() -> VkWriteDescriptorSet;
}// namespace mikoto::renderer::vulkan::initializers

// namespace mikoto::VulkanHelpers::Initializers {
//
//     MKT_NODISCARD inline auto ImageSubresourceRange(VkImageAspectFlags aspectMask) -> VkImageSubresourceRange {
//         VkImageSubresourceRange subImage {};
//         subImage.aspectMask = aspectMask;
//         subImage.baseMipLevel = 0;
//         subImage.levelCount = 1;
//         subImage.baseArrayLayer = 0;
//         subImage.layerCount = 1;
//
//         return subImage;
//     }
//
//     MKT_NODISCARD inline auto CreateDescriptorSetLayoutBinding(VkDescriptorType type, VkShaderStageFlags stageFlags, UInt32 binding) -> VkDescriptorSetLayoutBinding {
//         VkDescriptorSetLayoutBinding layoutBinding{};
//         layoutBinding.binding = binding;
//         layoutBinding.descriptorCount = 1;
//         layoutBinding.descriptorType = type;
//         layoutBinding.pImmutableSamplers = nullptr;
//         layoutBinding.stageFlags = stageFlags;
//
//         return layoutBinding;
//     }
//
//     /**
//      * Returns a default initialized VkApplicationInfo structure
//      * @returns default initialized VkApplicationInfo
//      * */
//     inline auto ApplicationInfo() -> VkApplicationInfo {
//         VkApplicationInfo ret{};
//         ret.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
//
//         return ret;
//     }
//
//     /**
//      * Returns a default initialized VkApplicationInfo structure
//      * @returns default initialized VkApplicationInfo
//      * */
//     inline auto ComputePipelineCreateInfo() -> VkComputePipelineCreateInfo {
//         VkComputePipelineCreateInfo ret{};
//         ret.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
//
//         return ret;
//     }
//
//     /**
//      * Returns a default initialized VkInstanceCreateInfo structure
//      * @returns default initialized VkInstanceCreateInfo
//      * */
//     inline auto InstanceCreateInfo() -> VkInstanceCreateInfo {
//         VkInstanceCreateInfo ret{};
//         ret.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
//
//         return ret;
//     }
//
//     /**
//      * Returns a default initialized VkDebugUtilsMessengerCreateInfoEXT structure
//      * @returns default initialized VkDebugUtilsMessengerCreateInfoEXT
//      * */
//     inline auto DebugUtilsMessengerCreateInfoEXT() -> VkDebugUtilsMessengerCreateInfoEXT {
//         VkDebugUtilsMessengerCreateInfoEXT ret{};
//         ret.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
//
//         return ret;
//     }
//
//     /**
//      * Returns a default initialized VkDeviceCreateInfo structure
//      * @returns default initialized VkDeviceCreateInfo
//      * */
//     inline auto DeviceCreateInfo() -> VkDeviceCreateInfo {
//         VkDeviceCreateInfo ret{};
//         ret.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
//
//         return ret;
//     }
//
//     /**
//      * Returns a default initialized VkDeviceQueueCreateInfo structure
//      * @returns default initialized VkDeviceQueueCreateInfo
//      * */
//     inline auto DeviceQueueCreateInfo() -> VkDeviceQueueCreateInfo {
//         VkDeviceQueueCreateInfo ret{};
//         ret.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
//
//         return ret;
//     }
//
//     /**
//      * Returns a default initialized VkSwapchainCreateInfoKHR structure
//      * @returns default initialized VkSwapchainCreateInfoKHR
//      * */
//     inline auto SwapchainCreateInfoKHR() -> VkSwapchainCreateInfoKHR {
//         VkSwapchainCreateInfoKHR ret{};
//         ret.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
//
//         return ret;
//     }
//
//     /**
//      * Returns a default initialized VkImageViewCreateInfo structure
//      * @returns default initialized VkImageViewCreateInfo
//      * */
//     inline auto ImageViewCreateInfo() -> VkImageViewCreateInfo {
//         VkImageViewCreateInfo ret{};
//         ret.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
//
//         return ret;
//     }
//
//     /**
//      * Returns a default initialized VkImageCreateInfo structure
//      * @returns default initialized VkImageCreateInfo
//      * */
//     inline auto ImageCreateInfo() -> VkImageCreateInfo {
//         VkImageCreateInfo ret{};
//         ret.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
//
//         return ret;
//     }
//
//     /**
//      * Returns a default initialized VkSemaphoreCreateInfo structure
//      * @returns default initialized VkSemaphoreCreateInfo
//      * */
//     inline auto SemaphoreCreateInfo() -> VkSemaphoreCreateInfo {
//         VkSemaphoreCreateInfo ret{};
//         ret.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
//
//         return ret;
//     }
//
//     /**
//      * Returns a default initialized VkFenceCreateInfo structure
//      * @returns default initialized VkFenceCreateInfo
//      * */
//     inline auto FenceCreateInfo() -> VkFenceCreateInfo {
//         VkFenceCreateInfo ret{};
//         ret.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
//
//         return ret;
//     }
//
//     /**
//      * Returns a default initialized VkMemoryAllocateInfo structure
//      * @returns default initialized VkMemoryAllocateInfo
//      * */
//     inline auto MemoryAllocateInfo() -> VkMemoryAllocateInfo {
//         VkMemoryAllocateInfo ret{};
//         ret.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
//
//         return ret;
//     }
//
//     /**
//      * Returns a default initialized VkDescriptorPoolCreateInfo structure
//      * @returns default initialized VkDescriptorPoolCreateInfo
//      * */
//     inline auto DescriptorPoolCreateInfo() -> VkDescriptorPoolCreateInfo {
//         VkDescriptorPoolCreateInfo ret{};
//         ret.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
//
//         return ret;
//     }
//
//     /**
//      * Returns a default initialized VkCommandPoolCreateInfo structure
//      * @returns default initialized VkCommandPoolCreateInfo
//      * */
//     inline auto CommandPoolCreateInfo() -> VkCommandPoolCreateInfo {
//         VkCommandPoolCreateInfo ret{};
//         ret.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
//
//         return ret;
//     }
//
//     /**
//      * Returns a default initialized VkCommandBufferAllocateInfo structure
//      * @returns default initialized VkCommandBufferAllocateInfo
//      * */
//     inline auto CommandBufferAllocateInfo() -> VkCommandBufferAllocateInfo {
//         VkCommandBufferAllocateInfo ret{};
//         ret.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
//
//         return ret;
//     }
//
//     /**
//      * Returns a default initialized VkCommandBufferBeginInfo structure
//      * @returns default initialized VkCommandBufferBeginInfo
//      * */
//     inline auto CommandBufferBeginInfo() -> VkCommandBufferBeginInfo {
//         VkCommandBufferBeginInfo ret{};
//         ret.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
//
//         return ret;
//     }
//
//     /**
//      * Returns a default initialized VkRenderPassBeginInfo structure
//      * @returns default initialized VkRenderPassBeginInfo
//      * */
//     inline auto RenderPassBeginInfo() -> VkRenderPassBeginInfo {
//         VkRenderPassBeginInfo ret{};
//         ret.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
//
//         return ret;
//     }
//
//     /**
//      * Returns a default initialized VkRenderPassCreateInfo structure
//      * @returns default initialized VkRenderPassCreateInfo
//      * */
//     inline auto RenderPassCreateInfo() -> VkRenderPassCreateInfo {
//         VkRenderPassCreateInfo ret{};
//         ret.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
//
//         return ret;
//     }
//
//     /**
//      * Returns a default initialized VkFramebufferCreateInfo structure
//      * @returns default initialized VkFramebufferCreateInfo
//      * */
//     inline auto FramebufferCreateInfo() -> VkFramebufferCreateInfo {
//         VkFramebufferCreateInfo ret{};
//         ret.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
//
//         return ret;
//     }
//
//     /**
//      * Returns a default initialized VkDescriptorSetLayoutCreateInfo structure
//      * @returns default initialized VkDescriptorSetLayoutCreateInfo
//      * */
//     inline auto DescriptorSetLayoutCreateInfo() -> VkDescriptorSetLayoutCreateInfo {
//         VkDescriptorSetLayoutCreateInfo ret{};
//         ret.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
//
//         return ret;
//     }
//
//     /**
//      * Returns a default initialized VkPipelineLayoutCreateInfo structure
//      * @returns default initialized VkPipelineLayoutCreateInfo
//      * */
//     inline auto PipelineLayoutCreateInfo() -> VkPipelineLayoutCreateInfo {
//         VkPipelineLayoutCreateInfo ret{};
//         ret.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
//
//         return ret;
//     }
//
//     /**
//      * Returns a default initialized VkSubmitInfo structure
//      * @returns default initialized VkSubmitInfo
//      * */
//     inline auto SubmitInfo() -> VkSubmitInfo {
//         VkSubmitInfo ret{};
//         ret.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
//
//         return ret;
//     }
//
//     /**
//      * Returns a default initialized VkShaderModuleCreateInfo structure
//      * @returns default initialized VkShaderModuleCreateInfo
//      * */
//     inline auto ShaderModuleCreateInfo() -> VkShaderModuleCreateInfo {
//         VkShaderModuleCreateInfo ret{};
//         ret.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
//
//         return ret;
//     }
//
//     /**
//      * Returns a default initialized VkPipelineShaderStageCreateInfo structure
//      * @returns default initialized VkPipelineShaderStageCreateInfo
//      * */
//     inline auto PipelineShaderStageCreateInfo() -> VkPipelineShaderStageCreateInfo {
//         VkPipelineShaderStageCreateInfo ret{};
//         ret.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
//
//         return ret;
//     }
//
//     /**
//      * Returns a default initialized VkGraphicsPipelineCreateInfo structure
//      * @returns default initialized VkGraphicsPipelineCreateInfo
//      * */
//     inline auto GraphicsPipelineCreateInfo() -> VkGraphicsPipelineCreateInfo {
//         VkGraphicsPipelineCreateInfo ret{};
//         ret.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
//
//         return ret;
//     }
//
//     /**
//      * Returns a default initialized VkPipelineVertexInputStateCreateInfo structure
//      * @returns default initialized VkPipelineVertexInputStateCreateInfo
//      * */
//     inline auto PipelineVertexInputStateCreateInfo() -> VkPipelineVertexInputStateCreateInfo {
//         VkPipelineVertexInputStateCreateInfo ret{};
//         ret.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
//
//         return ret;
//     }
//
//     /**
//      * Returns a default initialized VkSamplerCreateInfo structure
//      * @returns default initialized VkSamplerCreateInfo
//      * */
//     inline auto SamplerCreateInfo() -> VkSamplerCreateInfo {
//         VkSamplerCreateInfo ret{};
//         ret.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
//
//         return ret;
//     }
//
//     inline auto PushConstantRange(VkShaderStageFlags stageFlags, UInt32 size, UInt32 offset) -> VkPushConstantRange {
//         const VkPushConstantRange pushConstantRange {
//             .stageFlags{ stageFlags },
//             .offset{ offset },
//             .size{ size }
//         };
//
//         return pushConstantRange;
//     }
//
//     /**
//      * Returns a default initialized VkPresentInfoKHR structure
//      * @returns default initialized VkPresentInfoKHR
//      * */
//     inline auto PresentInfoKHR() -> VkPresentInfoKHR {
//         VkPresentInfoKHR ret{};
//         ret.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
//
//         return ret;
//     }
//
//     /**
//      * Returns a default initialized VkBufferCreateInfo structure
//      * @returns default initialized VkBufferCreateInfo
//      * */
//     inline auto BufferCreateInfo() -> VkBufferCreateInfo {
//         VkBufferCreateInfo ret{};
//         ret.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
//
//         return ret;
//     }
//
//     /**
//      * Returns a default initialized VkImageMemoryBarrier structure
//      * @returns default initialized VkImageMemoryBarrier
//      * */
//     inline auto ImageMemoryBarrier() -> VkImageMemoryBarrier {
//         VkImageMemoryBarrier ret{};
//         ret.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
//
//         return ret;
//     }
//
//     /**
//      * Returns a default initialized VkDescriptorSetAllocateInfo structure
//      * @returns default initialized VkDescriptorSetAllocateInfo
//      * */
//     inline auto DescriptorSetAllocateInfo() -> VkDescriptorSetAllocateInfo {
//         VkDescriptorSetAllocateInfo ret{};
//         ret.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
//
//         return ret;
//     }
//
//     /**
//      * Returns a default initialized VkWriteDescriptorSet structure
//      * @returns default initialized VkWriteDescriptorSet
//      * */
//     inline auto WriteDescriptorSet() -> VkWriteDescriptorSet {
//         VkWriteDescriptorSet ret{};
//         ret.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
//
//         return ret;
//     }
//
//     /**
//      * Returns a default initialized VkPhysicalDeviceVulkan13Features structure
//      * @returns default initialized VkPhysicalDeviceVulkan13Features
//      * */
//     inline auto PhysicalDeviceVulkan13Features() -> VkPhysicalDeviceVulkan13Features {
//         VkPhysicalDeviceVulkan13Features ret{};
//         ret.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
//
//         return ret;
//     }
//
//     /**
//      * Returns a default initialized VkPhysicalDeviceVulkan12Features structure
//      * @returns default initialized VkPhysicalDeviceVulkan12Features
//      * */
//     inline auto PhysicalDeviceVulkan12Features() -> VkPhysicalDeviceVulkan12Features {
//         VkPhysicalDeviceVulkan12Features ret{};
//         ret.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
//
//         return ret;
//     }
//
//     /**
//      * Returns a default initialized VkPhysicalDeviceFeatures2 structure
//      * @returns default initialized VkPhysicalDeviceFeatures2
//      * */
//     inline auto PhysicalDeviceFeatures2() -> VkPhysicalDeviceFeatures2 {
//         VkPhysicalDeviceFeatures2 ret{};
//         ret.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
//
//         return ret;
//     }
//
//     /**
//      * Returns a default initialized VkRenderingAttachmentInfo structure
//      * @returns default initialized VkRenderingAttachmentInfo
//      * */
//     inline auto RenderingAttachmentInfo() -> VkRenderingAttachmentInfo {
//         VkRenderingAttachmentInfo ret{};
//         ret.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
//
//         return ret;
//     }
//
//     /**
//      * Returns a default initialized VkRenderingInfo structure
//      * @returns default initialized VkRenderingInfo
//      * */
//     inline auto RenderingInfo() -> VkRenderingInfo {
//         VkRenderingInfo ret{};
//         ret.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
//
//         return ret;
//     }
// }

#endif// MIKOTO_VULKAN_UTILS_HH
