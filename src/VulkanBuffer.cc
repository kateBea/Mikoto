/**
 * VulkanBuffer.cc
 * Created by kate on 8/13/2023.
 * */

// Project Headers
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanBuffer.hh>

namespace Mikoto {

    auto VulkanBuffer::OnCreate(const BufferAllocateInfo& allocInfo) -> void {
        m_AllocationInfo = allocInfo;
        VulkanUtils::UploadBuffer(m_AllocationInfo);
    }

    auto VulkanBuffer::OnRelease() const -> void {
        vmaDestroyBuffer(VulkanContext::GetDefaultAllocator(), m_AllocationInfo.Buffer, m_AllocationInfo.Allocation);
    }
}
