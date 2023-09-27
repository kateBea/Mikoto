/**
 * VulkanUtils.hh
 * Created by kate on 8/5/2023.
 * */

// C++ Standard Library
#include <stdexcept>

// Third-Party Libraries
#include <volk.h>
#include <vk_mem_alloc.h>

// Project Header
#include <Utility/VulkanUtils.hh>
#include <Renderer/Vulkan/VulkanContext.hh>

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

    auto CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkCommandBuffer commandBuffer) -> void {
        VkBufferCopy copyRegion{};
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
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

        if (result != VK_SUCCESS)
            throw std::runtime_error("Failed to allocate VMA Image!");
    }

    auto WaitOnDevice(VkDevice device) -> void {
        vkDeviceWaitIdle(device);
    }

    auto WaitOnQueue(VkQueue queue) -> void {
        vkQueueWaitIdle(queue);
    }

    auto SubmitCommandBuffers(const CommandBuffersSubmitInfo& submitInfo) -> VkResult {

        return VK_SUCCESS;
    }

    auto QueueImageForPresentation(const QueuePresentInfo& presentInfo) -> VkResult {

        return VK_SUCCESS;
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

    auto ImmediateSubmit(const std::function<void()> &task, VkDevice device) -> void {

    }
}