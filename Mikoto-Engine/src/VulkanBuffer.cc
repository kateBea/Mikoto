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

        DeletionQueue::Push([isMapped = allocInfo.IsMapped, vulkanBufferHandle = m_AllocationInfo.Buffer, allocationHandle =  m_AllocationInfo.Allocation]() -> void {
            if (isMapped) {
                vmaUnmapMemory(VulkanContext::GetDefaultAllocator(), allocationHandle);
            }

            vmaDestroyBuffer(VulkanContext::GetDefaultAllocator(), vulkanBufferHandle, allocationHandle);
        });
    }
}
