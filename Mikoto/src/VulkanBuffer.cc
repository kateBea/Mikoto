/**
 * VulkanBuffer.cc
 * Created by kate on 8/13/2023.
 * */

// Project Headers
#include <Renderer/Vulkan/VulkanBuffer.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanDeletionQueue.hh>

namespace Mikoto {


    auto VulkanBuffer::Release() -> void {
        PersistentUnmap();

        auto& device{ VulkanContext::Get().GetDevice() };
        device.WaitIdle();

        vmaDestroyBuffer( device.GetAllocator(), m_Buffer, m_VmaAllocation );
    }

    VulkanBuffer::~VulkanBuffer() {

        if ( !m_IsReleased ) {
            Release();
            Invalidate();
        }
    }

    VulkanBuffer::VulkanBuffer( const VulkanBufferCreateInfo& createInfo )
        : m_Size{ createInfo.BufferCreateInfo.size },
        m_BufferCreateInfo{ createInfo.BufferCreateInfo },
        m_AllocationCreateInfo{ createInfo.AllocationCreateInfo }
    {
        VulkanDevice& device{ VulkanContext::Get().GetDevice() };
        device.CreateBuffer( createInfo, m_Buffer, m_VmaAllocation, m_VmaAllocationInfo );

        if ( createInfo.WantMapping ) {
            PersistentMap();
        }
    }

    auto VulkanBuffer::Create( const VulkanBufferCreateInfo& createInfo ) -> Scope_T<VulkanBuffer> {
        return CreateScope<VulkanBuffer>( createInfo );
    }

    auto VulkanBuffer::PersistentMap() -> void {
        VulkanDevice& device{ VulkanContext::Get().GetDevice() };
        if (vmaMapMemory(device.GetAllocator(), m_VmaAllocation, std::addressof( m_MappedAddress ) ) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("Failed to map memory for uniform buffer in default material!");
        }

        m_IsMapped = true;
    }

    auto VulkanBuffer::PersistentUnmap() -> void {
        VulkanDevice& device{ VulkanContext::Get().GetDevice() };

        if (m_IsMapped) {
            vmaUnmapMemory(device.GetAllocator(), m_VmaAllocation);
            m_IsMapped = false;
        }
    }
}
