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
    auto CreateBuffer(VkBuffer& buffer, VkDeviceMemory& bufferMemory, VkDeviceSize size, VkMemoryPropertyFlags properties, VkBufferUsageFlags usage) -> void {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(VulkanContext::GetPrimaryLogicalDevice(), &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
            throw std::runtime_error("failed to create vertex buffer!");

        VkMemoryRequirements memRequirements{};
        vkGetBufferMemoryRequirements(VulkanContext::GetPrimaryLogicalDevice(), buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = VulkanContext::FindMemoryType(memRequirements.memoryTypeBits, properties, VulkanContext::GetPrimaryPhysicalDevice());

        /**
         * NOTE:
         * It should be noted that in a real world application, you're not supposed to actually call
         * vkAllocateMemory for every individual buffer. The maximum number of simultaneous memory
         * allocations is limited by the maxMemoryAllocationCount physical device limit, which may
         * be as low as 4096 even on high end hardware like an NVIDIA GTX 1080
         * See: https://vulkan-tutorial.com/Vertex_buffers/Staging_buffer
         * */
        if (vkAllocateMemory(VulkanContext::GetPrimaryLogicalDevice(), &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
            throw std::runtime_error("failed to allocate vertex buffer memory!");

        vkBindBufferMemory(VulkanContext::GetPrimaryLogicalDevice(), buffer, bufferMemory, 0);
    }

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

    auto WaitOnQueue(VkQueue queue) -> void {
        vkQueueWaitIdle(queue);
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
}