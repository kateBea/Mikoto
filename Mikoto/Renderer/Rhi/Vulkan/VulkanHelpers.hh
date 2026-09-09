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

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuDevice.hh>

#define MKT_VK_FLAGS_NONE 0

namespace mikoto::renderer::vulkan {

    MKT_NODISCARD auto GetGpuDeviceType( rhi::GpuDeviceType type ) -> VkPhysicalDeviceType;
    MKT_NODISCARD auto GetResultString( VkResult result ) -> const char*;
    MKT_NODISCARD auto GetFormat( rhi::Format format ) -> VkFormat;
    MKT_NODISCARD auto GetAspectMask( VkFormat format ) -> VkImageAspectFlags;
    MKT_NODISCARD auto GetAspectMask( rhi::Format format ) -> VkImageAspectFlags;

    MKT_NODISCARD auto GetShaderModuleStage( rhi::ShaderType stage ) -> VkShaderStageFlagBits;
    MKT_NODISCARD auto GetShaderModuleStage( VkShaderStageFlagBits stage ) -> rhi::ShaderType;

    MKT_NODISCARD auto GetTopology( rhi::PrimitiveTopology topology ) -> VkPrimitiveTopology;
    MKT_NODISCARD auto GetSampleCount( rhi::Multisampling msaa ) -> VkSampleCountFlagBits;
    MKT_NODISCARD auto GetCullMode( rhi::CullMode mode ) -> VkCullModeFlags;
    MKT_NODISCARD auto GetWindingOrder( rhi::WindingOrder order ) -> VkFrontFace;
    MKT_NODISCARD auto GetCompareOp( rhi::CompareOp op ) -> VkCompareOp;

    MKT_NODISCARD auto GetInputRate( rhi::InputRate rate ) -> VkVertexInputRate;

    MKT_NODISCARD auto GetSamplerFilter( rhi::SamplerFilter filter ) -> VkFilter;
    MKT_NODISCARD auto GetSamplerWrap( rhi::SamplerWrapMode wrap ) -> VkSamplerAddressMode;

    MKT_NODISCARD auto GetQueueName( rhi::QueueType type ) -> eastl::string_view;

    MKT_NODISCARD auto GetViewType( rhi::TextureDimension dimensions ) -> VkImageViewType;
    MKT_NODISCARD auto GetTextureType( rhi::TextureDimension dimensions ) -> VkImageType;

    MKT_NODISCARD auto GetImageLayout( rhi::ResourceStates state ) -> VkImageLayout;
    MKT_NODISCARD auto GetResourceState( VkImageLayout layout ) -> rhi::ResourceStates;
    MKT_NODISCARD auto GetStageMask( rhi::ResourceStates state ) -> VkPipelineStageFlags2;

    /**
     * Converts every selected RHI pipeline stage to a Synchronization2 stage.
     * @param stages Pipeline stages to include; None produces an empty scope.
     * @returns The combined Vulkan stage mask.
     */
    MKT_NODISCARD auto GetStageMask( rhi::PipelineStageFlags stages ) -> VkPipelineStageFlags2;

    /**
     * Converts explicit RHI memory accesses without widening their scope.
     * @param accesses Memory accesses to include; None produces an execution-only scope.
     * @returns The combined Vulkan access mask.
     */
    MKT_NODISCARD auto GetAccessMask( rhi::BarrierAccessFlags accesses ) -> VkAccessFlags2;

    MKT_NODISCARD auto GetMipmapMode( rhi::SamplerMipmapMode mode ) -> VkSamplerMipmapMode;

    MKT_UNUSED_FUNC MKT_NODISCARD auto GetAccessMask( rhi::ResourceStates state ) -> VkAccessFlags2;

    MKT_NODISCARD auto GetIndexType( rhi::Format format ) -> VkIndexType;

    MKT_NODISCARD auto GetPolygonMode( rhi::PolygonMode mode ) -> VkPolygonMode;

    MKT_NODISCARD auto GetShaderStageFlags( rhi::ShaderFlags visibility ) -> VkShaderStageFlags;
    MKT_NODISCARD auto GetDescriptorType( rhi::ResourceType type ) -> VkDescriptorType;

    MKT_NODISCARD auto GetArraLayerCount( rhi::TextureDimension dimension, core::u32 requestedLayers = 1 ) -> core::u32;

    MKT_NODISCARD auto GetImageUsage( rhi::TextureUsageFlags flags ) -> VkImageUsageFlags;

#define MKT_VK_CHECK( expr )                                                          \
    do {                                                                              \
        VkResult _vk_result{ ( expr ) };                                              \
        if ( _vk_result != VK_SUCCESS ) {                                             \
            MKT_FILE_LOGGER_ERROR(                                                    \
                    "Vulkan error: {} (code: {}) at {}:{}",                           \
                    GetResultString( _vk_result ), core::as<core::i32>( _vk_result ), \
                    __FILE__, __LINE__ );                                             \
                                                                                      \
            cpptrace::generate_trace().print();                                       \
            throw mikoto::core::RuntimeException( string::Format(                     \
                    "Vulkan call failed: {}\nFile: {}\nLine: {}",                     \
                    GetResultString( _vk_result ), __FILE__, __LINE__ ) );            \
        }                                                                             \
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
    MKT_NODISCARD auto PipelineCacheCreateInfo() -> VkPipelineCacheCreateInfo;

}// namespace mikoto::renderer::vulkan::initializers

#endif// MIKOTO_VULKAN_UTILS_HH
