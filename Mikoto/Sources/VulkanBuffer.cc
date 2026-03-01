//    Copyright 2026 ケイト
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

        TO_VK_DEVICE( m_Device )->SubmitDeletion( [allocation = m_Allocation]( GpuDevice* device ) mutable -> void {
            auto* allocator{ MKT_VMA_ALLOC_PTR(device) };
            if ( allocation.Buffer != VK_NULL_HANDLE ) {
                if (!(allocation.AllocationCreateInfo.flags & VMA_ALLOCATION_CREATE_MAPPED_BIT)) {
                    allocator->UnmapBuffer( allocation );
                }

                allocator->FreeBuffer( allocation );
            }
        } );

        if (m_Data) {
            delete[] m_Data;
            m_Data = nullptr;
        }

        m_IsAllocated = false;
    }
    
    auto VulkanBuffer::Initialize() -> void {
        ComputeAllocationSize();
        
        // Allocate memory
        auto* allocator{ MKT_VMA_ALLOC_PTR(m_Device) };
        if ( const VkResult result{ allocator->AllocateBuffer( m_Allocation ) }; result != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("VulkanBuffer::InitBuffer - Failed to allocate Vulkan buffer!");
        }
        
        // Staging
        if (IsUsage( BufferUsage::UNDEFINED )) {
            if (m_Data) {
                allocator->MapBuffer( m_Allocation );
                std::memcpy( m_Allocation.AllocationInfo.pMappedData, m_Data, m_SizeBytes );
                allocator->UnmapBuffer( m_Allocation );
            }
        } else {
            InitializeAttributesBuffers();
        }

        if (m_Data) {
            delete[] m_Data;
            m_Data = nullptr;
        }

        SetDebugInfo();

        m_IsAllocated = true;
    }

    auto VulkanBuffer::InitializeAttributesBuffers() -> void {
        if (IsUsage( BufferUsage::VERTEX ) || IsUsage(BufferUsage::INDEX)) {
            CommandListHandle cmd{ m_Device->CreateCommandList( QueueType::TRANSFER_QUEUE, true ) };
            cmd->Begin();

            cmd->CopyBuffer( m_Data, m_SizeBytes, this );

            cmd->End();
            m_Device->SubmitCommands( cmd );
        }
    }

    auto VulkanBuffer::SetDebugInfo() -> void {
        if (m_DebugName == GetDefaultDebugName()) {
            m_DebugName = fmt::format( "MikotoBuffer {}, Pool ID: {}", reinterpret_cast<UInt64>( m_Allocation.Buffer ), GetHandle() );
        }

        VulkanHelpers::SetObjectDebugName(VK_DEVICE( m_Device ),VK_OBJECT_TYPE_BUFFER, reinterpret_cast<UInt64>( m_Allocation.Buffer ),m_DebugName.c_str() );
    }

    auto VulkanBuffer::ComputeAllocationSize() -> void {
        // Size of ONE dynamic slice (aligned)
        if (m_ElementSize != 0 && m_ElementCount != 0) {
            if (TO_VK_DEVICE( m_Device )->IsScalarBlockLayoutEnabled()) {
                // Non padded structs (VK_EXT_scalar_block_layout)
                m_SizeBytes = m_ElementCount * m_ElementSize;
                m_UsesScalarBlockLayout = true;
            } else {
                MKT_ASSERT( false, "Mikoto does not support yet arbitrary padding for GPU buffers" );
            }
        }

        if (IsResourceUsage( ResourceUsageType::RESOURCE_USAGE_STREAMING )) {
            auto& physicalProperties{ TO_VK_DEVICE( m_Device )->GetPhysicalDeviceProperties() };

            UInt32 alignment{};

            if (m_Usage == BufferUsage::UNIFORM) {
                alignment = physicalProperties.limits.minUniformBufferOffsetAlignment;
            } else {
                alignment = physicalProperties.limits.minStorageBufferOffsetAlignment;
            }

            // Align helper
            const auto AlignUp = [](UInt64 value, UInt64 alignment) {
                return (value + alignment - 1) & ~(alignment - 1);
            };

            m_AlignedSizeBytes = AlignUp(m_SizeBytes, alignment);

            // Allocate enough for all frames-in-flight
            auto totalSizeBytes{ m_AlignedSizeBytes * VulkanContext::Get()->GetMaxFramesInFlight() };

            // Override the buffer allocation size before creating actual Vulkan buffer
            m_SizeBytes = static_cast<VkDeviceSize>(totalSizeBytes);
        }

        m_Allocation.BufferCreateInfo.size = static_cast<VkDeviceSize>( m_SizeBytes );
    }

    auto VulkanBuffer::CompuetAlignedSizeMaxFrames( Size sliceSize, Size bufferOffsetAligment ) -> Size {
        VkDeviceSize alignment{ bufferOffsetAligment };
        sliceSize = ( sliceSize + alignment - 1 ) & ~( alignment - 1 );

        return sliceSize * m_Context->GetMaxFramesInFlight();
    }

    auto VulkanBuffer::SetupUniformBuffer(const BufferDescription& createInfo) -> void {
        m_Allocation.AllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
        m_Allocation.AllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        m_Allocation.BufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }

    auto VulkanBuffer::SetupStorageBuffer(const BufferDescription& createInfo) -> void {
        m_Allocation.AllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
        m_Allocation.AllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        m_Allocation.BufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
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

    VulkanBuffer::VulkanBuffer( const void* src, Size size )
        :   Buffer{ nullptr, size, BufferUsage::UNDEFINED, ResourceUsageType::RESOURCE_USAGE_STATIC, BufferDataType::BUFFER_DATA_TYPE_UNKNOWN } {

        m_Allocation.BufferCreateInfo = VulkanHelpers::Initializers::BufferCreateInfo();

        // Data must be alive until call to Initialize()
        if ( src ) {
            m_Data = new Byte[m_SizeBytes];
            std::memcpy( m_Data, src, m_SizeBytes );
        }

        m_Allocation.AllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
        m_Allocation.AllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
            VMA_ALLOCATION_CREATE_MAPPED_BIT;

        m_Allocation.BufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

         m_Context = MKT_VK_CTX( RenderService::Get()->GetContext() );
    }

    VulkanBuffer::VulkanBuffer( const BufferDescription& createInfo )
        : Buffer{ createInfo.Data, createInfo.SizeBytes, createInfo.Usage, createInfo.UsageType, createInfo.Type } {
        m_Allocation.BufferCreateInfo = VulkanHelpers::Initializers::BufferCreateInfo();

        // Data must be alive until call to Initialize()
        if ( createInfo.Data ) {
            m_Data = new Byte[m_SizeBytes];
            std::memcpy( m_Data, createInfo.Data, m_SizeBytes );
        }

        m_ElementSize = createInfo.ElementSize;
        m_ElementCount = createInfo.ElementCount;

        m_Allocation.AllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
        m_Allocation.AllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
            VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        switch (m_Usage) {
            case BufferUsage::VERTEX:
                SetupVertexBuffer( createInfo );
                break;
            case BufferUsage::INDEX:
                SetupIndexBuffer( createInfo );
                break;
            case BufferUsage::UNIFORM:
                SetupUniformBuffer(createInfo);
                break;
            case BufferUsage::SHADER_STORAGE:
                SetupStorageBuffer(createInfo);
                break;
            default:;
        }

        m_Context = MKT_VK_CTX( RenderService::Get()->GetContext() );
    }

    auto VulkanBuffer::CopyToHost( void* ptr, const Size size ) -> void {
        // Do a check that this is CPU visible memory

        PersistentMap();

        const Size copySize{ Math::Min(size, m_Allocation.AllocationInfo.size) };
        std::memcpy( ptr, m_Allocation.AllocationInfo.pMappedData, copySize );
    }

    auto VulkanBuffer::Copy( const void* ptr, Size size, CommandListHandle cmd ) -> void {
        auto& physicalProperties{ TO_VK_DEVICE( m_Device )->GetPhysicalDeviceProperties() };

        Size offsetAligment{};
        if ( m_Usage == BufferUsage::UNIFORM ) {
            offsetAligment = physicalProperties.limits.minUniformBufferOffsetAlignment;
        } else {
            offsetAligment = physicalProperties.limits.minStorageBufferOffsetAlignment;
        }

        Size bufferSize{ CompuetAlignedSizeMaxFrames( size, offsetAligment ) };

        bool needsResizing { m_StagingSliceSize != 0 && bufferSize > m_StagingForCopies->GetSizeBytes() };
        if ( m_StagingForCopies.IsEmpty() || needsResizing ) {
            // To avoid frequent allocations
            bufferSize *= 1.5f;

            m_StagingSliceSize = bufferSize / m_Context->GetMaxFramesInFlight();
            m_StagingForCopies = TO_VK_DEVICE( m_Device )->CreateStaging( nullptr, bufferSize );
        }

        VkDeviceSize offset{ m_StagingSliceSize * m_Context->GetCurrentFrameIndex() };

        m_StagingForCopies->CopyToDevice( ptr, size, offset );

        // Copy region
        VkBufferCopy region{
            .srcOffset = offset,
            .dstOffset = 0,
            .size = size
        };

        std::array regions{ region };

        vkCmdCopyBuffer( cmd->GetNativeHandle( ObjectType::Vk_CmdBuffer ), 
            m_StagingForCopies->GetNativeHandle( ObjectType::Vk_Buffer ), 
            GetNativeHandle( ObjectType::Vk_Buffer ), 
            static_cast<UInt32>( regions.size() ), regions.data() );

        VkBufferMemoryBarrier2 barrier{
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT,
            .buffer = GetNativeHandle( ObjectType::Vk_Buffer ),
            .offset = 0,
            .size = VK_WHOLE_SIZE
        };

        VkDependencyInfo depInfo{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .bufferMemoryBarrierCount = 1,
            .pBufferMemoryBarriers = &barrier,
        };

        vkCmdPipelineBarrier2( cmd->GetNativeHandle( ObjectType::Vk_CmdBuffer ), &depInfo );
    }

    auto VulkanBuffer::CopyToDevice( const void* ptr, const Size size ) -> void {
        // Do a check that this is CPU visible memory

        PersistentMap();
        CopyToDevice(ptr, size, 0);
    }

    auto VulkanBuffer::CopyToDevice( const void* ptr, const Size size, const Size offset ) -> void {
        // Do a check that this is CPU visible memory

        PersistentMap();

        if (IsResourceUsage( ResourceUsageType::RESOURCE_USAGE_STREAMING )) {
            if (m_UsesScalarBlockLayout) {
                const UInt32 currentFrame{ m_Context->GetCurrentFrameIndex() };

                std::memcpy(static_cast<std::byte*>(
                    m_Allocation.AllocationInfo.pMappedData ) + // Base address of this buffer
                    (currentFrame * m_AlignedSizeBytes) + // Offset that "selects" the slice for the current frame
                    offset // Caller offset within this slice
                    , ptr, size);
            } else {
                MKT_ASSERT( false, "Mikoto only supports scalar block for now" );
            }
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

    auto VulkanBuffer::GetAlignedSize() const -> UInt32 {
        return m_AlignedSizeBytes;
    }
}// namespace Mikoto