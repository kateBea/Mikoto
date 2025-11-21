/**
 * VulkanBuffer.cc
 * Created by kate on 8/13/2023.
 * */

#include <cstddef>

// Project Headers
#include <Library/Math/Math.hh>
#include <Renderer/Vulkan/VulkanBuffer.hh>
#include <Renderer/Vulkan/VulkanDevice.hh>
#include <Renderer/Vulkan/VulkanMemoryAllocator.hh>

namespace Mikoto {


    auto VulkanBuffer::Release() -> void {
        if ( !m_IsAllocated ) {
            return;// nothing to free
        }

        PersistentUnmap();

        auto* allocator{ dynamic_cast<VulkanMemoryAllocator*>(TO_VK_DEVICE( m_Device )->GetAllocator()) };
        MKT_ASSERT(allocator != nullptr, "Allocator is null in VulkanBuffer::Release!");

        allocator->FreeBuffer(this);

        m_Buffer = VK_NULL_HANDLE;
        m_VmaAllocation = nullptr;

        if (m_Data) {
            delete[] m_Data;
            m_Data = nullptr;
        }

        m_IsAllocated = false;
    }

    auto VulkanBuffer::Initialize() -> void {
        auto* allocator{ dynamic_cast<VulkanMemoryAllocator*>(TO_VK_DEVICE( m_Device )->GetAllocator()) };
        MKT_ASSERT(allocator != nullptr, "Allocator is null in VulkanBuffer::Allocate!");

        const VkResult result{ allocator->AllocateBuffer(this) };
        if (result != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("Failed to allocate Vulkan buffer!");
        }

        // Copy data over if created from a block
        if (m_Data) {
            CopyFromBlock( m_Data, m_SizeBytes );

            // Free retained data after upload
            delete[] m_Data;
            m_Data = nullptr;
        }

        m_DebugName = fmt::format( "MikotoBuffer {}, Pool ID: {}", reinterpret_cast<UInt64>( m_Buffer ), GetHandle() );
        VulkanHelpers::SetObjectDebugName(VK_DEVICE( m_Device ),VK_OBJECT_TYPE_BUFFER, reinterpret_cast<UInt64>( m_Buffer ),m_DebugName.c_str() );

        m_IsAllocated = true;
    }

    VulkanBuffer::~VulkanBuffer() {
        if (m_IsAllocated) {
            Release();
        }
    }

    VulkanBuffer::VulkanBuffer( const BufferDescription& createInfo )
        : Buffer{ createInfo.Data, createInfo.SizeBytes, createInfo.Usage, createInfo.UsageType, createInfo.Type } {
        m_BufferCreateInfo = VulkanHelpers::Initializers::BufferCreateInfo();

        // Retain data 
        // FIXME: This is done because data held by m_Data must be valid until after Initialize is called
        if ( createInfo.Data ) {
            m_Data = new Byte[m_SizeBytes];
            std::memcpy( static_cast<void*>(m_Data), static_cast<const void*>(createInfo.Data), m_SizeBytes );
        }

        // Let a VMA library select the optimal memory type unless specified
        m_AllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
        m_BufferCreateInfo.size = static_cast<UInt32>( m_SizeBytes );

        // For buffers, we copy CPU data and later use to transfer its data to other CPU buffer/image
        if ( m_Usage == BufferUsage::BUFFER_USAGE_STAGING ) {
            m_BufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        }

        // Uniform buffers for shaders
        if ( m_Usage == BufferUsage::BUFFER_USAGE_UNIFORM ) {
            m_BufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            m_AllocationCreateInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        }

        // Vertex buffers
        if ( m_Usage == BufferUsage::BUFFER_USAGE_VERTEX ) {
            m_BufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        }

        // Index buffers
        if ( m_Usage == BufferUsage::BUFFER_USAGE_INDEX ) {
            m_BufferCreateInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        }

        // Index buffers
        if ( m_Usage == BufferUsage::BUFFER_USAGE_SHADER_STORAGE ) {
            m_BufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            m_AllocationCreateInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        }

        // Fill VMA specific structs
        //let the VMA library know that this data should be on CPU RAM
        m_AllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }

    auto VulkanBuffer::CopyToBlock( void* ptr, const Size size ) -> void {
        // Do a check that this is CPU visible memory

        const Size copyAmount{ Math::Min(size, m_VmaAllocationInfo.size) };

        PersistentMap();
        std::memcpy( ptr, m_VmaAllocationInfo.pMappedData, copyAmount );
    }

    auto VulkanBuffer::CopyFromBlock( const void* ptr, const Size size ) -> void {
        // Do a check that this is CPU visible memory

        PersistentMap();
        std::memcpy( m_VmaAllocationInfo.pMappedData, ptr, size );
    }

    auto VulkanBuffer::CopyFromBlock( const void* ptr, Size size, Size offset ) -> void {
        PersistentMap();

        std::memcpy(static_cast<std::byte*>(m_VmaAllocationInfo.pMappedData) + offset, ptr, size);
    }

    auto VulkanBuffer::GetNativeHandle( ObjectType object ) -> Object {
        switch (object) {

            case ObjectType::Vk_Buffer:
                return Object(m_Buffer );
            default:;
        }

        return Object(nullptr);
    }

    auto VulkanBuffer::PersistentMap() -> void {
        // If it is not null then it has been mapped
        if (m_VmaAllocationInfo.pMappedData != nullptr) {
            return;
        }

        const auto* allocator{ dynamic_cast<VulkanMemoryAllocator*>(TO_VK_DEVICE( m_Device )->GetAllocator()) };
        allocator->MapBuffer( this );
    }

    auto VulkanBuffer::PersistentUnmap() -> void {
        // If it is not null, then we have something to unmap
        if (m_VmaAllocationInfo.pMappedData != nullptr) {
            return;
        }

        const auto* allocator{ dynamic_cast<VulkanMemoryAllocator*>(TO_VK_DEVICE( m_Device )->GetAllocator()) };
        allocator->UnmapBuffer( this );

        m_VmaAllocationInfo.pMappedData = nullptr;
    }
}