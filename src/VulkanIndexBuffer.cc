//
// Created by kate on 7/3/23.
//

#include <volk.h>

#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanIndexBuffer.hh>

namespace kaTe {

    VulkanIndexBuffer::VulkanIndexBuffer(const std::vector<UInt32_T>& indices) {
        SetIndicesData(indices);
    }

    auto VulkanIndexBuffer::Bind(VkCommandBuffer commandBuffer) const -> void {
        vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
    }

    auto VulkanIndexBuffer::SetIndicesData(const std::vector<UInt32_T> &indices) -> void {
        m_Count = indices.size();

        // TODO: use staging buffers
        VkDeviceSize bufferSize{ m_Count * sizeof(UInt32_T) };
        CreateBuffer(
                bufferSize,
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        void* data{};
        if (vkMapMemory(VulkanContext::GetPrimaryLogicalDevice(), m_IndexBufferMemory, 0, bufferSize, 0, &data) != VK_SUCCESS)
            throw std::runtime_error("Failed to map memory");

        std::memcpy(data, static_cast<const void*>(indices.data()), static_cast<std::size_t>(bufferSize));
        vkUnmapMemory(VulkanContext::GetPrimaryLogicalDevice(), m_IndexBufferMemory);
    }

    auto VulkanIndexBuffer::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) -> void {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(VulkanContext::GetPrimaryLogicalDevice(), &bufferInfo, nullptr, &m_IndexBuffer) != VK_SUCCESS)
            throw std::runtime_error("failed to create vertex buffer!");

        VkMemoryRequirements memRequirements{};
        vkGetBufferMemoryRequirements(VulkanContext::GetPrimaryLogicalDevice(), m_IndexBuffer, &memRequirements);

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
        if (vkAllocateMemory(VulkanContext::GetPrimaryLogicalDevice(), &allocInfo, nullptr, &m_IndexBufferMemory) != VK_SUCCESS)
            throw std::runtime_error("failed to allocate vertex buffer memory!");

        vkBindBufferMemory(VulkanContext::GetPrimaryLogicalDevice(), m_IndexBuffer, m_IndexBufferMemory, 0);
    }

    auto VulkanIndexBuffer::OnRelease() const -> void {
        vkDeviceWaitIdle(VulkanContext::GetPrimaryLogicalDevice());
        vkDestroyBuffer(VulkanContext::GetPrimaryLogicalDevice(), m_IndexBuffer, nullptr);
        vkFreeMemory(VulkanContext::GetPrimaryLogicalDevice(), m_IndexBufferMemory, nullptr);
    }
}