/**
 * VulkanBuffer.cc
 * Created by kate on 8/13/2023.
 * */

// Project Headers
#include <Renderer/Vulkan/DeletionQueue.hh>
#include "Renderer/Vulkan/VulkanBuffer.hh"
#include "Renderer/Vulkan/VulkanContext.hh"

namespace Mikoto {

    auto VulkanBuffer::OnCreate(const BufferAllocateInfo& allocInfo) -> void {
        m_AllocationInfo = allocInfo;
        VulkanUtils::UploadBuffer(m_AllocationInfo);

        DeletionQueue::Push([=, this]() -> void {
            vmaDestroyBuffer(VulkanContext::GetDefaultAllocator(), m_AllocationInfo.Buffer, m_AllocationInfo.Allocation);
        });
    }
}
