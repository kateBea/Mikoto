/**
 * VulkanBuffer.cc
 * Created by kate on 8/13/2023.
 * */

#include <cstddef>

// Project Headers
#include <Library/Math/Math.hh>
#include <Renderer/Vulkan/VulkanBuffer.hh>
#include <Renderer/Vulkan/VulkanDevice.hh>
#include <Renderer/Vulkan/VulkanHelpers.hh>
#include <Renderer/Vulkan/VulkanMemoryAllocator.hh>

namespace Mikoto {


    auto VulkanBuffer::Release() -> void {
        if ( !m_IsAllocated ) {
            return;
        }

        PersistentUnmap();

        auto* allocator{ MKT_VMA_ALLOC_PTR(m_Device) };
        allocator->FreeBuffer( m_Allocation );

        m_Allocation.Buffer = VK_NULL_HANDLE;
        m_Allocation.Allocation = nullptr;

        if (IsUsage( BufferUsage::BUFFER_USAGE_VERTEX ) || IsUsage(BufferUsage::BUFFER_USAGE_INDEX)) {
            allocator->UnmapBuffer( m_StagingAllocation );
            allocator->FreeBuffer( m_StagingAllocation );
        }

        if (m_Data) {
            delete[] m_Data;
            m_Data = nullptr;
        }

        m_IsAllocated = false;
    }

    auto VulkanBuffer::Initialize() -> void {
        if (IsUsage( BufferUsage::BUFFER_USAGE_SHADER_STORAGE ) ||
            IsUsage(BufferUsage::BUFFER_USAGE_UNIFORM) ||
            IsUsage(BufferUsage::BUFFER_USAGE_STAGING)) {
            // When description has specified size and element count we need to apply alignment if needed for this GPU buffer
            // We allocate space for a buffer large enough to contain ElementCount objects of ElementSize size (in bytes)
            if (m_ElementSize != 0 && m_ElementCount != 0) {

                if (IsUsage( BufferUsage::BUFFER_USAGE_SHADER_STORAGE )) {
                    VkDeviceSize minOffsetAlignment{ TO_VK_DEVICE(m_Device)->GetStorageBufferMinOffsetAlignment() };
                    m_MinPaddedSize = VulkanHelpers::GetStorageBufferPadding( m_ElementSize, minOffsetAlignment );

                } else if (IsUsage(BufferUsage::BUFFER_USAGE_UNIFORM)) {
                    VkDeviceSize minOffsetAlignment{ TO_VK_DEVICE(m_Device)->GetUniformBufferMinOffsetAlignment() };
                    m_MinPaddedSize = VulkanHelpers::GetUniformBufferPadding( m_ElementSize, minOffsetAlignment );
                }

                m_SizeBytes = m_ElementCount * m_MinPaddedSize;
                m_Allocation.BufferCreateInfo.size = static_cast<UInt32>( m_SizeBytes );
            }

            InitMainBuffer();
            UploadHostData();
        }

        if (IsUsage( BufferUsage::BUFFER_USAGE_VERTEX ) || IsUsage(BufferUsage::BUFFER_USAGE_INDEX)) {
            InitMainBuffer();
            InitStaging();

            UploadHostDataToStaging();

            CommandListHandle cmd{ m_Device->CreateCommandList( QueueType::TRANSFER_QUEUE, true ) };
            cmd->Begin();

            VkBufferCopy copy{
                .srcOffset{ 0 },
                .dstOffset{ 0 },
                .size{ m_SizeBytes },
            };

            vkCmdCopyBuffer(
                cmd->GetNativeHandle( ObjectType::Vk_CmdBuffer ),
                m_StagingAllocation.Buffer,
                m_Allocation.Buffer,
                1,
                std::addressof(copy));

            VkAccessFlags accessFlags{ VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT };

            // It is either vertex or index
            if (IsUsage(BufferUsage::BUFFER_USAGE_INDEX)) {
                accessFlags = VK_ACCESS_INDEX_READ_BIT;
            }

            // VK_QUEUE_FAMILY_IGNORED Because queue family indices are
            // unified see https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples
            // Otherwise we would need to specify the indices explicitly

            const VkBufferMemoryBarrier barrier{
                .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .dstAccessMask = accessFlags,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .buffer = m_Allocation.Buffer,
                .offset = 0,
                .size = m_SizeBytes
            };

            vkCmdPipelineBarrier(
                cmd->GetNativeHandle(ObjectType::Vk_CmdBuffer),
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                VK_FLAGS_NONE,
                0, nullptr,
                1, &barrier,
                0, nullptr
            );

            cmd->End();
            m_Device->SubmitCommands( cmd );
        }

        SetDebugInfo();

        m_IsAllocated = true;
    }

    auto VulkanBuffer::SetDebugInfo() -> void {
        if (m_DebugName == GetDefaultDebugName()) {
            m_DebugName = fmt::format( "MikotoBuffer {}, Pool ID: {}", reinterpret_cast<UInt64>( m_Allocation.Buffer ), GetHandle() );
        }

        VulkanHelpers::SetObjectDebugName(VK_DEVICE( m_Device ),VK_OBJECT_TYPE_BUFFER, reinterpret_cast<UInt64>( m_Allocation.Buffer ),m_DebugName.c_str() );
    }

    auto VulkanBuffer::UploadHostData() -> void {
        if (m_Data) {
            CopyFromBlock( m_Data, m_SizeBytes );

            // Free retained data after upload
            delete[] m_Data;
            m_Data = nullptr;
        }
    }

    auto VulkanBuffer::UploadHostDataToStaging() -> void {
        if (m_Data) {
            const auto* allocator{ MKT_VMA_ALLOC_PTR(m_Device) };
            allocator->MapBuffer( m_StagingAllocation );

            std::memcpy( m_StagingAllocation.AllocationInfo.pMappedData, m_Data, m_SizeBytes );

            delete[] m_Data;
            m_Data = nullptr;

            allocator->UnmapBuffer( m_StagingAllocation );
        }
    }

    auto VulkanBuffer::InitMainBuffer() -> void {
        auto* allocator{ MKT_VMA_ALLOC_PTR(m_Device) };
        if ( const VkResult result{ allocator->AllocateBuffer( m_Allocation ) }; result != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("VulkanBuffer::InitBuffer - Failed to allocate Vulkan buffer!");
        }
    }

    auto VulkanBuffer::InitStaging() -> void {
        m_StagingAllocation.BufferCreateInfo = VulkanHelpers::Initializers::BufferCreateInfo();
        m_StagingAllocation.BufferCreateInfo.size = m_SizeBytes;
        m_StagingAllocation.BufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        m_StagingAllocation.AllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
        m_StagingAllocation.AllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
            | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        auto* allocator{ MKT_VMA_ALLOC_PTR(m_Device) };
        if ( const VkResult result{ allocator->AllocateBuffer( m_StagingAllocation ) }; result != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("VulkanBuffer::InitStaging - Failed to allocate staging buffer!");
        }
    }

    VulkanBuffer::~VulkanBuffer() {
        if (m_IsAllocated) {
            Release();
        }
    }

    VulkanBuffer::VulkanBuffer( const BufferDescription& createInfo )
        : Buffer{ createInfo.Data, createInfo.SizeBytes, createInfo.Usage, createInfo.UsageType, createInfo.Type } {
        m_Allocation.BufferCreateInfo = VulkanHelpers::Initializers::BufferCreateInfo();

        // Data must be alive until initialization
        if ( createInfo.Data ) {
            m_Data = new Byte[m_SizeBytes];
            std::memcpy( m_Data, createInfo.Data, m_SizeBytes );
        }

        // Let a VMA library select the optimal memory type unless specified
        m_Allocation.AllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
        m_Allocation.BufferCreateInfo.size = static_cast<UInt32>( m_SizeBytes );

        // For buffers, we copy CPU data and later use to transfer its data to other CPU buffer/image
        if ( m_Usage == BufferUsage::BUFFER_USAGE_STAGING ) {
            m_Allocation.BufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

            m_Allocation.AllocationCreateInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        }

        // Uniform buffers for shaders
        if ( m_Usage == BufferUsage::BUFFER_USAGE_UNIFORM ) {
            m_Allocation.BufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            m_Allocation.AllocationCreateInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

            // When description has specified size and element count we need to apply alignment if needed for this GPU buffer
            // We allocate space for a buffer large enough to contain ElementCount objects of ElementSize size (in bytes)
            if (createInfo.ElementSize != 0 && createInfo.ElementCount != 0) {
                m_ElementSize = createInfo.ElementSize;
                m_ElementCount = createInfo.ElementCount;
            }
            // Fill VMA specific structs
            // Let the VMA library know that this data should be on CPU RAM
            m_Allocation.AllocationCreateInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        }

        // Storage buffers
        if ( m_Usage == BufferUsage::BUFFER_USAGE_SHADER_STORAGE ) {
            m_Allocation.BufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            m_Allocation.AllocationCreateInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

            // When description has specified size and element count we need to apply alignment if needed for this GPU buffer
            // We allocate space for a buffer large enough to contain ElementCount objects of ElementSize size (in bytes)
            if (createInfo.ElementSize != 0 && createInfo.ElementCount != 0) {
                m_ElementSize = createInfo.ElementSize;
                m_ElementCount = createInfo.ElementCount;
            }

            // Fill VMA specific structs
            // Let the VMA library know that this data should be on CPU RAM
            m_Allocation.AllocationCreateInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        }

        // Vertex buffers
        if ( m_Usage == BufferUsage::BUFFER_USAGE_VERTEX ) {
            m_Allocation.BufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            m_Allocation.AllocationCreateInfo.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        }

        // Index buffers
        if ( m_Usage == BufferUsage::BUFFER_USAGE_INDEX ) {
            m_Allocation.BufferCreateInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            m_Allocation.AllocationCreateInfo.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        }
    }

    auto VulkanBuffer::CopyToBlock( void* ptr, const Size size ) -> void {
        // Do a check that this is CPU visible memory

        const Size copyAmount{ Math::Min(size, m_Allocation.AllocationInfo.size) };

        PersistentMap();
        std::memcpy( ptr, m_Allocation.AllocationInfo.pMappedData, copyAmount );
    }

    auto VulkanBuffer::CopyFromBlock( const void* ptr, const Size size ) -> void {
        // Do a check that this is CPU visible memory

        PersistentMap();
        std::memcpy( m_Allocation.AllocationInfo.pMappedData, ptr, size );
    }

    auto VulkanBuffer::CopyFromBlock( const void* ptr, const Size size, const Size offset ) -> void {
        PersistentMap();

        if (m_ElementSize != 0 && (m_Usage == BufferUsage::BUFFER_USAGE_SHADER_STORAGE || m_Usage == BufferUsage::BUFFER_USAGE_UNIFORM)) {
            // if this buffer is supposed to hold elements we need to handle element padding depending on whether this is uniform or storage
            const Size minJumps{ offset / m_MinPaddedSize };
            Size newOffset{ (minJumps + 1 ) * m_MinPaddedSize };

            if (offset == 0) {
                newOffset = 0;
            }

            std::memcpy(static_cast<std::byte*>(m_Allocation.AllocationInfo.pMappedData) + newOffset, ptr, size);
        } else {
            std::memcpy(static_cast<std::byte*>(m_Allocation.AllocationInfo.pMappedData) + offset, ptr, size);
        }
    }

    auto VulkanBuffer::GetNativeHandle( ObjectType object ) -> Object {
        switch (object) {

            case ObjectType::Vk_Buffer:
                return Object( m_Allocation.Buffer );
            default:;
        }

        return Object(nullptr);
    }

    auto VulkanBuffer::GetNativeHandle( ObjectType type ) const -> Object {
        return const_cast<VulkanBuffer*>(this)->GetNativeHandle( type );
    }

    auto VulkanBuffer::PersistentMap() -> void {
        // If it is not null then it has been mapped
        if (m_Allocation.AllocationInfo.pMappedData != nullptr) {
            return;
        }

        const auto* allocator{ dynamic_cast<VulkanMemoryAllocator*>(TO_VK_DEVICE( m_Device )->GetAllocator()) };
        allocator->MapBuffer( m_Allocation );
    }

    auto VulkanBuffer::PersistentUnmap() -> void {
        // If it is not null, then we have something to unmap
        if (m_Allocation.AllocationInfo.pMappedData != nullptr) {
            return;
        }

        const auto* allocator{ dynamic_cast<VulkanMemoryAllocator*>(TO_VK_DEVICE( m_Device )->GetAllocator()) };
        allocator->UnmapBuffer(  m_Allocation );

        m_Allocation.AllocationInfo.pMappedData = nullptr;
    }
}