/**
 * VulkanBuffer.cc
 * Created by kate on 8/13/2023.
 * */

// Project Headers
#include <Renderer/Vulkan/VulkanBuffer.hh>
#include <Renderer/Vulkan/VulkanDevice.hh>
#include <Renderer/Vulkan/VulkanMemoryAllocator.hh>

namespace Mikoto {


    auto VulkanBuffer::Release() -> void {
        if ( m_Buffer == VK_NULL_HANDLE ) {
            return;// nothing to free
        }

        const auto* allocator{ dynamic_cast<VulkanMemoryAllocator*>(TO_VK_DEVICE( m_Device )->GetAllocator()) };
        MKT_ASSERT(allocator != nullptr, "Allocator is null in VulkanBuffer::Release!");

        allocator->FreeBuffer(this);

        m_Buffer = VK_NULL_HANDLE;
        m_VmaAllocation = nullptr;

        m_IsAllocated = false;
    }

    auto VulkanBuffer::Allocate() -> void {
        // Ensure allocator exists
        const auto* allocator{ dynamic_cast<VulkanMemoryAllocator*>(TO_VK_DEVICE( m_Device )->GetAllocator()) };
        MKT_ASSERT(allocator != nullptr, "Allocator is null in VulkanBuffer::Allocate!");

        const VkResult result{ allocator->AllocateBuffer(this) };
        if (result != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("Failed to allocate Vulkan buffer!");
        }

        m_IsAllocated = true;
    }

    VulkanBuffer::~VulkanBuffer() {
        Release();
    }

    VulkanBuffer::VulkanBuffer( const BufferDescription& createInfo )
        : Buffer{ createInfo.Data, createInfo.SizeBytes, createInfo.Usage, createInfo.UsageType }
    {
        m_BufferCreateInfo = VulkanHelpers::Initializers::BufferCreateInfo();
        m_BufferCreateInfo.pNext = nullptr;

        m_BufferCreateInfo.size = static_cast<UInt32>( m_SizeBytes);
        m_BufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

        //let the VMA library know that this data should be on CPU RAM
        m_AllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
        m_AllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }

    auto VulkanBuffer::PersistentMap() -> void {
        const auto* allocator{ dynamic_cast<VulkanMemoryAllocator*>(TO_VK_DEVICE( m_Device )->GetAllocator()) };
        allocator->MapBuffer( this );
    }

    auto VulkanBuffer::PersistentUnmap() -> void {
        const auto* allocator{ dynamic_cast<VulkanMemoryAllocator*>(TO_VK_DEVICE( m_Device )->GetAllocator()) };
        allocator->UnmapBuffer( this );
    }
}
