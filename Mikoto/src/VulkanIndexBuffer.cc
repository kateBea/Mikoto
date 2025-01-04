/**
 * VulkanIndexBuffer.cc
 * Created by kate on 7/3/23.
 * */

// C++ Standard Library
#include <vector>

// Third-Party Library
#include "vk_mem_alloc.h"
#include "volk.h"

// Project Headers
#include "Common/Common.hh"
#include "Renderer/Vulkan/VulkanUtils.hh"

#include <Renderer/Vulkan/DeletionQueue.hh>
#include "Renderer/Vulkan/VulkanContext.hh"
#include "Renderer/Vulkan/VulkanIndexBuffer.hh"

namespace Mikoto {

    VulkanIndexBuffer::VulkanIndexBuffer(const std::vector<UInt32_T>& indices) {
        SetIndicesData(indices);
    }

    auto VulkanIndexBuffer::Bind(VkCommandBuffer commandBuffer) const -> void {
        vkCmdBindIndexBuffer(commandBuffer, m_Buffer.Get(), 0, VK_INDEX_TYPE_UINT32);
    }

    auto VulkanIndexBuffer::SetIndicesData(const std::vector<UInt32_T>& indices) -> void {
        m_Count = indices.size();
        BufferAllocateInfo allocaInfo{};
        allocaInfo.Size = m_Count * sizeof(UInt32_T);
        allocaInfo.BufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        allocaInfo.BufferCreateInfo.size = allocaInfo.Size;
        allocaInfo.BufferCreateInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        allocaInfo.AllocationCreateInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;          // Buffer is writeable by host but readable by device

        m_Buffer.OnCreate(allocaInfo);

        // Copy data to CPU readable memory
        void* data{};
        if (vmaMapMemory(VulkanContext::GetDefaultAllocator(), m_Buffer.GetVmaAllocation(), &data) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("Failed to map memory for index buffer!");
        }

        std::memcpy(data, static_cast<const void*>(indices.data()), static_cast<Size_T>(m_Buffer.GetSize()));
        vmaUnmapMemory(VulkanContext::GetDefaultAllocator(), m_Buffer.GetVmaAllocation());
    }
}