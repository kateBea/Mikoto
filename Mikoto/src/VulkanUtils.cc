/**
 * VulkanUtils.hh
 * Created by kate on 8/5/2023.
 * */

// C++ Standard Library
#include <stdexcept>

// Third-Party Libraries
#include "vk_mem_alloc.h"
#include "volk.h"

// Project Header
#include "../Common/VulkanUtils.hh"
#include "Renderer/Vulkan/VulkanContext.hh"

namespace Mikoto::VulkanUtils {

    auto UploadBuffer(BufferAllocateInfo& allocatedBufferData) -> void {
        auto result { vmaCreateBuffer(VulkanContext::GetDefaultAllocator(),
                            &allocatedBufferData.BufferCreateInfo,
                            &allocatedBufferData.AllocationCreateInfo,
                                    &allocatedBufferData.Buffer,
                            &allocatedBufferData.Allocation,
                            nullptr) };

        if (result  != VK_SUCCESS)
            throw std::runtime_error("Failed to create VMA buffer");
    }

    auto AllocateImage(ImageAllocateInfo& allocatedImageData) -> void {
        auto result{ vmaCreateImage(VulkanContext::GetDefaultAllocator(),
                                     &allocatedImageData.ImageCreateInfo,
                                     &allocatedImageData.AllocationCreateInfo,
                                     &allocatedImageData.Image,
                                     &allocatedImageData.Allocation,
                                     nullptr) };

        if (result != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("Failed to allocate VMA Image!");
        }
    }

    auto WaitOnDevice(VkDevice device) -> void {
        vkDeviceWaitIdle(device);
    }

    auto GetDeviceMinimumOffsetAlignment(VkPhysicalDevice physicalDevice) -> VkDeviceSize {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);

        return properties.limits.minUniformBufferOffsetAlignment;
    }

    auto GetUniformBufferPadding(VkDeviceSize bufferOriginalSize, VkDeviceSize deviceMinOffsetAlignment) -> VkDeviceSize {
        VkDeviceSize alignedSize{ bufferOriginalSize };

        if (deviceMinOffsetAlignment > 0)
            alignedSize = (alignedSize + deviceMinOffsetAlignment - 1) & ~(deviceMinOffsetAlignment - 1);

        return alignedSize;
    }

    auto GetVulkanShaderStageFlag(ShaderStage stage) -> VkShaderStageFlagBits {
        switch (stage) {
            case ShaderStage::SHADER_VERTEX_STAGE: return VK_SHADER_STAGE_VERTEX_BIT;
            case ShaderStage::SHADER_FRAGMENT_STAGE: return VK_SHADER_STAGE_FRAGMENT_BIT;

            // TODO: temporary (unused stages)
            case SHADER_GEOMETRY_STAGE:
            case SHADER_TESSELATION_STAGE:
            default: return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
        }
    }

    auto PerformImageLayoutTransition(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkCommandBuffer cmd) -> void {
        VkImageSubresourceRange range{};
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.baseMipLevel = 0;
        range.levelCount = 1;
        range.baseArrayLayer = 0;
        range.layerCount = 1;

        VkImageMemoryBarrier imageBarrierToTransfer{ Initializers::ImageMemoryBarrier() };
        imageBarrierToTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageBarrierToTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageBarrierToTransfer.oldLayout = oldLayout;
        imageBarrierToTransfer.newLayout = newLayout;
        imageBarrierToTransfer.image = image;
        imageBarrierToTransfer.subresourceRange = range;

        VkPipelineStageFlags sourceStage{};
        VkPipelineStageFlags destinationStage{};

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
            newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            imageBarrierToTransfer.srcAccessMask = 0;
            imageBarrierToTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
                 newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            imageBarrierToTransfer.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imageBarrierToTransfer.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else {
            MKT_THROW_RUNTIME_ERROR("Unsupported layout transition!");
        }

        vkCmdPipelineBarrier(cmd, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, std::addressof(imageBarrierToTransfer));
    }

    auto TransitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout) -> void {
        VkImageMemoryBarrier2 imageBarrier {};
        imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        imageBarrier.pNext = nullptr;

        imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
        imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

        imageBarrier.oldLayout = currentLayout;
        imageBarrier.newLayout = newLayout;

        VkImageAspectFlags aspectMask = (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        imageBarrier.subresourceRange = ImageSubresourceRange(aspectMask);
        imageBarrier.image = image;

        VkDependencyInfo depInfo {};
        depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        depInfo.pNext = nullptr;

        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers = std::addressof(imageBarrier);

        vkCmdPipelineBarrier2(cmd, std::addressof(depInfo));
    }

    auto CommandBufferBeginInfo(VkCommandBufferUsageFlags flags) -> VkCommandBufferBeginInfo {
        VkCommandBufferBeginInfo info{ Initializers::CommandBufferBeginInfo() };
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        info.pNext = nullptr;

        info.pInheritanceInfo = nullptr;
        info.flags = flags;

        return info;
    }

    auto SubmitInfo(VkCommandBuffer& command) -> VkSubmitInfo {
        VkSubmitInfo info{ Initializers::SubmitInfo() };
        info.pNext = nullptr;

        info.pWaitDstStageMask = nullptr;

        info.waitSemaphoreCount = 0;
        info.pWaitSemaphores = nullptr;

        info.signalSemaphoreCount = 0;
        info.pSignalSemaphores = nullptr;

        info.commandBufferCount = 1;
        info.pCommandBuffers = std::addressof(command);

        return info;
    }

    auto CreateDescriptorSetLayoutBinding(VkDescriptorType type, VkShaderStageFlags stageFlags, UInt32_T binding) -> VkDescriptorSetLayoutBinding {
        VkDescriptorSetLayoutBinding layoutBinding{};
        layoutBinding.binding = binding;
        layoutBinding.descriptorCount = 1;
        layoutBinding.descriptorType = type;
        layoutBinding.pImmutableSamplers = nullptr;
        layoutBinding.stageFlags = stageFlags;

        return layoutBinding;
    }

    auto CopyImageToImage( VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent3D imageSize ) -> void {
        VkImageBlit2 blitRegion{};
        blitRegion.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
        blitRegion.pNext = nullptr;

        blitRegion.srcOffsets[1].x = imageSize.width;
        blitRegion.srcOffsets[1].y = imageSize.height;
        blitRegion.srcOffsets[1].z = 1;

        blitRegion.dstOffsets[1].x = imageSize.width;
        blitRegion.dstOffsets[1].y = imageSize.height;
        blitRegion.dstOffsets[1].z = 1;

        blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blitRegion.srcSubresource.baseArrayLayer = 0;
        blitRegion.srcSubresource.layerCount = 1;
        blitRegion.srcSubresource.mipLevel = 0;

        blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blitRegion.dstSubresource.baseArrayLayer = 0;
        blitRegion.dstSubresource.layerCount = 1;
        blitRegion.dstSubresource.mipLevel = 0;

        VkBlitImageInfo2 blitInfo{};
        blitInfo.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
        blitInfo.pNext = nullptr;
        blitInfo.dstImage = destination;
        blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        blitInfo.srcImage = source;
        blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        blitInfo.filter = VK_FILTER_NEAREST;
        blitInfo.regionCount = 1;
        blitInfo.pRegions = &blitRegion;

        vkCmdBlitImage2(cmd, &blitInfo);
    }
}