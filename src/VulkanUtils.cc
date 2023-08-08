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

namespace kaTe {
    auto VulkanUtils::CreateBuffer(VkBuffer &buffer, VkDeviceMemory &bufferMemory, VkDeviceSize size, VkMemoryPropertyFlags properties, VkBufferUsageFlags usage) -> void {
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

    auto VulkanUtils::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkCommandBuffer commandBuffer) -> void {
        VkBufferCopy copyRegion{};
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
    }

    auto VulkanUtils::UploadBuffer(AllocatedBuffer& allocatedBufferData) -> void {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = allocatedBufferData.Size;             // this is the total size, in bytes, of the buffer we are allocating
        bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;   //this buffer is going to be used as a Vertex Buffer

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;          // Buffer is writeable by host but readable by device

        if (vmaCreateBuffer(VulkanContext::GetDefaultAllocator(), &bufferInfo, &allocInfo,&allocatedBufferData.Buffer, &allocatedBufferData.Allocation, nullptr) != VK_SUCCESS)
            throw std::runtime_error("Failed to create VMA buffer");
    }
}