/**
 * VulkanBuffer.cc
 * Created by kate on 8/13/2023.
 * */

// Project Headers
#include <Renderer/Vulkan/DeletionQueue.hh>
#include <Renderer/Vulkan/VulkanBuffer.hh>
#include <Renderer/Vulkan/VulkanContext.hh>

namespace Mikoto {

    auto VulkanBuffer::OnCreate(const BufferAllocateInfo& allocInfo) -> void {
        m_AllocationInfo = allocInfo;
        VulkanUtils::UploadBuffer(m_AllocationInfo);

        if (m_AllocationInfo.WantMapping) {
            PersistentMap();
        }

        DeletionQueue::Push([isMapped = m_IsMapped, vulkanBufferHandle = m_AllocationInfo.Buffer, allocationHandle =  m_AllocationInfo.Allocation]() -> void {
            if (isMapped) {
                // We get validation errors if we destroy a buffer that is mapped
                vmaUnmapMemory(VulkanContext::GetDefaultAllocator(), allocationHandle);
            }

            vmaDestroyBuffer(VulkanContext::GetDefaultAllocator(), vulkanBufferHandle, allocationHandle);
        });
    }

    auto VulkanBuffer::PersistentMap() -> void {
        if (vmaMapMemory(VulkanContext::GetDefaultAllocator(), m_AllocationInfo.Allocation, &m_MappedAddress ) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("Failed to map memory for uniform buffer in default material!");
        }

        m_IsMapped = true;
    }

    auto VulkanBuffer::PersistentUnmap() -> void {
        vmaUnmapMemory(VulkanContext::GetDefaultAllocator(), m_AllocationInfo.Allocation);

        m_IsMapped = false;
    }
}
