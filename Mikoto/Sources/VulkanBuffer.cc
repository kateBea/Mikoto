//    Copyright 2025 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstddef>
#include <memory>

#include <Library/Math/Math.hh>

#include <Renderer/Core/RenderService.hh>

#include <Renderer/Vulkan/VulkanBuffer.hh>
#include <Renderer/Vulkan/VulkanDevice.hh>
#include <Renderer/Vulkan/VulkanHelpers.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanMemoryAllocator.hh>

namespace Mikoto {

    auto VulkanBuffer::Release() -> void {
        if (!m_IsAllocated) {
            return;
        }

        TO_VK_DEVICE( m_Device )->SubmitDeletion( [allocation = m_Allocation, inFlightAllocations = m_InFlightBuffers, staging = m_StagingAllocation]( GpuDevice* device ) mutable -> void {
            auto* allocator{ MKT_VMA_ALLOC_PTR(device) };

            if ( allocation.Buffer != VK_NULL_HANDLE ) {
                if (!(allocation.AllocationCreateInfo.flags & VMA_ALLOCATION_CREATE_MAPPED_BIT)) {
                    allocator->UnmapBuffer( allocation );
                }

                allocator->FreeBuffer( allocation );
            }

            if ( staging.Buffer != VK_NULL_HANDLE ) {
                if (!(staging.AllocationCreateInfo.flags & VMA_ALLOCATION_CREATE_MAPPED_BIT)) {
                    allocator->UnmapBuffer( staging );
                }
                allocator->FreeBuffer( staging );
            }

            for ( auto& inFlightBuffer: inFlightAllocations ) {
                if ( inFlightBuffer.Buffer != VK_NULL_HANDLE ) {
                    allocator->FreeBuffer( inFlightBuffer );
                }
            }
        } );

        if (m_Data) {
            delete[] m_Data;
            m_Data = nullptr;
        }

        m_IsAllocated = false;
    }

    auto VulkanBuffer::Initialize() -> void {
        if (IsUsage(BufferUsage::STAGING)) {
            InitMainBuffers();
            UploadHostData();

            m_IsAllocated = true;
            return;
        }

        if (IsResourceUsage( ResourceUsageType::RESOURCE_USAGE_DYNAMIC ) || IsResourceUsage( ResourceUsageType::RESOURCE_USAGE_STREAM )) {
            InitializeInFlightBuffers();
        }

        if (IsResourceUsage( ResourceUsageType::RESOURCE_USAGE_STATIC ) ) {
            InitializeAttributesBuffers();
        }

        //SetDebugInfo();

        m_IsAllocated = true;
    }

    auto VulkanBuffer::InitializeAttributesBuffers() -> void {
        if (IsUsage( BufferUsage::VERTEX ) || IsUsage(BufferUsage::INDEX)) {
            InitMainBuffers();
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
            if (IsUsage(BufferUsage::INDEX)) {
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
    }

    auto VulkanBuffer::InitializeInFlightBuffers() -> void {
        // Here we initialize the buffer assuming we will update it frequently per frame
        if (IsUsage( BufferUsage::VERTEX ) || IsUsage(BufferUsage::INDEX) || IsUsage(BufferUsage::STAGING)) {
            return;
        }

        // When description has specified size and element count we need to apply alignment if needed for this GPU buffer
        // We allocate space for a buffer large enough to contain ElementCount objects of ElementSize size (in bytes)
        if (m_ElementSize != 0 && m_ElementCount != 0) {

            if (TO_VK_DEVICE( m_Device )->IsScalarBlockLayoutEnabled()) {
                // Non padded structs (VK_EXT_scalar_block_layout)
                m_SizeBytes = m_ElementCount * m_ElementSize;
                m_UsesScalarBlockLayout = true;
            } else {
                if (IsUsage( BufferUsage::SSBO )) {
                    VkDeviceSize minOffsetAlignment{ TO_VK_DEVICE(m_Device)->GetStorageBufferMinOffsetAlignment() };
                    m_MinPaddedSize = VulkanHelpers::GetStorageBufferPadding( m_ElementSize, minOffsetAlignment );

                } else if (IsUsage(BufferUsage::UNIFORM)) {
                    VkDeviceSize minOffsetAlignment{ TO_VK_DEVICE(m_Device)->GetUniformBufferMinOffsetAlignment() };
                    m_MinPaddedSize = VulkanHelpers::GetUniformBufferPadding( m_ElementSize, minOffsetAlignment );
                }
            }

            auto allocator{ MKT_VMA_ALLOC_PTR(m_Device) };

            for (auto& inFlightBuffer : m_InFlightBuffers) {
                inFlightBuffer.BufferCreateInfo.size = static_cast<UInt32>( m_SizeBytes );

                if ( const VkResult result{ allocator->AllocateBuffer( inFlightBuffer ) }; result != VK_SUCCESS) {
                    MKT_THROW_RUNTIME_ERROR("VulkanBuffer::InitializeInFlightBuffers - Failed to allocate inFlightBuffer Vulkan buffer!");
                }
            }
        }
    }

    auto VulkanBuffer::SetDebugInfo() -> void {
        if (IsResourceUsage( ResourceUsageType::RESOURCE_USAGE_DYNAMIC ) || IsResourceUsage( ResourceUsageType::RESOURCE_USAGE_STREAM )) {
            UInt32 index{};
            for (const auto& inFlightBuffer : m_InFlightBuffers) {
                if (m_DebugName == GetDefaultDebugName()) {
                    m_DebugName = fmt::format( "MikotoBuffer {}, Pool ID: {}", reinterpret_cast<UInt64>( m_Allocation.Buffer ), GetHandle() );
                    const auto inFlightNane { fmt::format( "MikotoBufferInFlight [{}]. Index: {}", reinterpret_cast<UInt64>( inFlightBuffer.Buffer ), index++ ) };
                    VulkanHelpers::SetObjectDebugName(VK_DEVICE( m_Device ),VK_OBJECT_TYPE_BUFFER, reinterpret_cast<UInt64>( inFlightBuffer.Buffer ),inFlightNane.c_str() );
                }
            }

            return;
        }

        if (m_DebugName == GetDefaultDebugName()) {
            m_DebugName = fmt::format( "MikotoBuffer {}, Pool ID: {}", reinterpret_cast<UInt64>( m_Allocation.Buffer ), GetHandle() );
        }

        VulkanHelpers::SetObjectDebugName(VK_DEVICE( m_Device ),VK_OBJECT_TYPE_BUFFER, reinterpret_cast<UInt64>( m_Allocation.Buffer ),m_DebugName.c_str() );
    }

    auto VulkanBuffer::UploadHostData() -> void {
        if (m_Data) {
            CopyToDevice( m_Data, m_SizeBytes );

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

    auto VulkanBuffer::InitMainBuffers() -> void {
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

    auto VulkanBuffer::SetupUniformBuffer(const BufferDescription& createInfo) -> void {
        m_Allocation.BufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

        // When description has specified size and element count we need to apply alignment if needed for this GPU buffer
        // We allocate space for a buffer large enough to contain ElementCount objects of ElementSize size (in bytes)
        if (createInfo.ElementSize != 0 && createInfo.ElementCount != 0) {
            m_ElementSize = createInfo.ElementSize;
            m_ElementCount = createInfo.ElementCount;
        }

        if (IsResourceUsage( ResourceUsageType::RESOURCE_USAGE_DYNAMIC ) || IsResourceUsage( ResourceUsageType::RESOURCE_USAGE_STREAM)) {
            const UInt32 framesInFlight{ MKT_VK_CTX(RenderService::Get()->GetContext())->GetMaxFramesInFlight() };
            for (UInt32 count{}; count < framesInFlight; ++count) { m_InFlightBuffers.emplace_back(m_Allocation); }
        }
    }

    auto VulkanBuffer::SetupStorageBuffer(const BufferDescription& createInfo) -> void {
        m_Allocation.AllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
        m_Allocation.AllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        m_Allocation.BufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        // When description has specified size and element count we need to apply alignment if needed for this GPU buffer
        // We allocate space for a buffer large enough to contain ElementCount objects of ElementSize size (in bytes)
        if (createInfo.ElementSize != 0 && createInfo.ElementCount != 0) {
            m_ElementSize = createInfo.ElementSize;
            m_ElementCount = createInfo.ElementCount;
        }

        if (IsResourceUsage( ResourceUsageType::RESOURCE_USAGE_DYNAMIC ) || IsResourceUsage( ResourceUsageType::RESOURCE_USAGE_STREAM)) {
            const UInt32 framesInFlight{ MKT_VK_CTX(RenderService::Get()->GetContext())->GetMaxFramesInFlight() };
            for (UInt32 count{}; count < framesInFlight; ++count) { m_InFlightBuffers.emplace_back(m_Allocation); }
        }
    }

    auto VulkanBuffer::SetupStagingBuffer(const BufferDescription&) -> void {
        m_Allocation.AllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
        m_Allocation.AllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
            VMA_ALLOCATION_CREATE_MAPPED_BIT;

        m_Allocation.BufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    }

    auto VulkanBuffer::SetupVertexBuffer(const BufferDescription&) -> void {
        m_Allocation.BufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        // Because vertices are not often modified/write from CPU
        m_Allocation.AllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        m_Allocation.AllocationCreateInfo.priority = 1.0f;
    }

    auto VulkanBuffer::SetupIndexBuffer(const BufferDescription&) -> void {
        m_Allocation.BufferCreateInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        // Because indices are not often modified/write from CPU
        m_Allocation.AllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        m_Allocation.AllocationCreateInfo.priority = 1.0f;
    }

    VulkanBuffer::~VulkanBuffer() {
        if (m_IsAllocated) {
            Release();
        }
    }

    VulkanBuffer::VulkanBuffer( const BufferDescription& createInfo )
        : Buffer{ createInfo.Data, createInfo.SizeBytes, createInfo.Usage, createInfo.UsageType, createInfo.Type } {
        m_Allocation.BufferCreateInfo = VulkanHelpers::Initializers::BufferCreateInfo();

        // Data must be alive until call to Initialize()
        if ( createInfo.Data ) {
            m_Data = new Byte[m_SizeBytes];
            std::memcpy( m_Data, createInfo.Data, m_SizeBytes );
        }

        m_Allocation.AllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
        m_Allocation.AllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
            VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        m_Allocation.BufferCreateInfo.size = static_cast<VkDeviceSize>( m_SizeBytes );

        switch (m_Usage) {
            case BufferUsage::VERTEX:
                SetupVertexBuffer( createInfo );
                break;
            case BufferUsage::INDEX:
                SetupIndexBuffer( createInfo );
                break;
            case BufferUsage::STAGING:
                SetupStagingBuffer(createInfo);
                break;
            case BufferUsage::UNIFORM:
                SetupUniformBuffer(createInfo);
                break;
            case BufferUsage::SSBO:
                SetupStorageBuffer(createInfo);
                break;
        }
    }

    auto VulkanBuffer::CopyToHost( void* ptr, const Size size ) -> void {
        // Do a check that this is CPU visible memory

        const Size copySize{ Math::Min(size, m_Allocation.AllocationInfo.size) };
        std::memcpy( ptr, m_Allocation.AllocationInfo.pMappedData, copySize );
    }

    auto VulkanBuffer::CopyToDevice( const void* ptr, const Size size ) -> void {
        // Do a check that this is CPU visible memory

        if (IsUsage(BufferUsage::STAGING)) {
            std::memcpy( m_Allocation.AllocationInfo.pMappedData, ptr, size );
        } else if (IsResourceUsage( ResourceUsageType::RESOURCE_USAGE_DYNAMIC ) || IsResourceUsage( ResourceUsageType::RESOURCE_USAGE_STREAM)) {
            const UInt32 currentFrame{ MKT_VK_CTX(RenderService::Get()->GetContext())->GetCurrentFrameIndex() };
            std::memcpy( m_InFlightBuffers[currentFrame].AllocationInfo.pMappedData, ptr, size );
        } else {
            std::memcpy( m_Allocation.AllocationInfo.pMappedData, ptr, size );
        }
    }

    auto VulkanBuffer::CopyToDevice( const void* ptr, const Size size, const Size offset ) -> void {
        if (IsResourceUsage( ResourceUsageType::RESOURCE_USAGE_DYNAMIC ) || IsResourceUsage( ResourceUsageType::RESOURCE_USAGE_STREAM)) {
            if (m_UsesScalarBlockLayout) {
                const UInt32 currentFrame{ MKT_VK_CTX(RenderService::Get()->GetContext())->GetCurrentFrameIndex() };
                std::memcpy(static_cast<std::byte*>(m_InFlightBuffers[currentFrame].AllocationInfo.pMappedData) + offset, ptr, size);
            } else {
                MKT_ASSERT( false, "Unhandled copy case" );
            }

            return;
        }

        // Vertices and indices are not often copied by these means, usually via a copy command on the GPU
        // We check for scalar block because that way we can directly copy the contents without account for any padding
        if (m_UsesScalarBlockLayout || IsUsage( BufferUsage::STAGING )
            || IsUsage( BufferUsage::VERTEX ) || IsUsage( BufferUsage::INDEX )) {
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
        if (m_Allocation.Buffer == VK_NULL_HANDLE || m_Allocation.AllocationInfo.pMappedData != nullptr) {
            return;
        }

        auto allocator{ MKT_VMA_ALLOC_PTR( m_Device ) };
        allocator->MapBuffer( m_Allocation );
    }

    auto VulkanBuffer::PersistentUnmap() -> void {
        if (m_Allocation.Buffer == VK_NULL_HANDLE || m_Allocation.AllocationInfo.pMappedData != nullptr) {
            return;
        }

        auto allocator{ MKT_VMA_ALLOC_PTR( m_Device ) };
        allocator->UnmapBuffer(  m_Allocation );

        m_Allocation.AllocationInfo.pMappedData = nullptr;
    }

    auto VulkanBuffer::GetBufferFrameIndex(UInt32 index) -> VkBuffer {
        const UInt32 framesInFlight{ MKT_VK_CTX(RenderService::Get()->GetContext())->GetMaxFramesInFlight() };

        MKT_ASSERT( index < framesInFlight, "Index out of bounds" );
        if (IsResourceUsage( ResourceUsageType::RESOURCE_USAGE_DYNAMIC ) || IsResourceUsage( ResourceUsageType::RESOURCE_USAGE_STREAM)) {
            return m_InFlightBuffers[index].Buffer;
        }

        return m_Allocation.Buffer;
    }
}