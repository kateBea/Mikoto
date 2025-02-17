/**
 * VulkanIndexBuffer.cc
 * Created by kate on 7/3/23.
 * */

// C++ Standard Library
#include <vector>

// Third-Party Library
#include <volk.h>
#include <vk_mem_alloc.h>

// Project Headers
#include <Common/Common.hh>
#include <Renderer/Vulkan/VulkanHelpers.hh>
#include <Renderer/Vulkan/VulkanDeletionQueue.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanIndexBuffer.hh>

namespace Mikoto {

    VulkanIndexBuffer::VulkanIndexBuffer(const VulkanIndexBufferCreateInfo& createInfo)
    {
        LoadIndices(createInfo.Indices);
    }

    auto VulkanIndexBuffer::Bind( const VkCommandBuffer commandBuffer ) const -> void {
        // VK_INDEX_TYPE_UINT32 because the indices are created from UInt32_T types
        vkCmdBindIndexBuffer( commandBuffer, m_Buffer->Get(), 0, VK_INDEX_TYPE_UINT32 );
    }

    auto VulkanIndexBuffer::Release() -> void {
        m_Buffer = nullptr;
    }

    VulkanIndexBuffer::~VulkanIndexBuffer() {
        if ( !m_IsReleased ) {
            Release();
            Invalidate();
        }
    }

    auto VulkanIndexBuffer::Create( const VulkanIndexBufferCreateInfo& createInfo ) -> Scope_T<VulkanIndexBuffer> {
        return CreateScope<VulkanIndexBuffer>( createInfo );
    }

    auto VulkanIndexBuffer::LoadIndices(const std::span<const UInt32_T>& indices) -> void {
        m_Count = indices.size();

        const VulkanBufferCreateInfo allocaInfo{
            .BufferCreateInfo{
                .sType{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO },
                .size{ m_Count * sizeof(UInt32_T) },
                .usage{ VK_BUFFER_USAGE_INDEX_BUFFER_BIT }
            },
            .AllocationCreateInfo{
                // Buffer is writeable by host and readable by device
                // DEPRECATED: look docs for details
                .usage{ VMA_MEMORY_USAGE_CPU_TO_GPU },
            },
            .WantMapping{ true },
        };

        m_Buffer = VulkanBuffer::Create( allocaInfo );

        // Copy data to CPU readable memory
        std::memcpy(m_Buffer->GetMappedPtr(), indices.data(), m_Buffer->GetSize() );
        m_Buffer->PersistentUnmap();
    }
}