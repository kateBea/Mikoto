//
// Created by zanet on 10/6/2025.
//

// Define VMA implementation in one source file
#define VMA_IMPLEMENTATION

#include "Renderer/Vulkan/VulkanMemoryAllocator.hh"

#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanDevice.hh>
#include <memory>

namespace Mikoto {

    auto VulkanMemoryAllocator::Init() -> void {
        VmaAllocatorCreateInfo allocInfo{};
        allocInfo.physicalDevice = dynamic_cast<VulkanDevice *>( m_Device )->GetPhysicalDevice();
        allocInfo.device = dynamic_cast<VulkanDevice *>( m_Device )->GetLogicalDevice();
        allocInfo.instance = VulkanContext::Get()->GetInstance();
        allocInfo.vulkanApiVersion = VulkanContext::Get()->GetApiVersion();

        //  VMA tries to fetch remaining pointers that are still null
        //  by calling vkGetInstanceProcAddr and vkGetDeviceProcAddr on its own.
        //  You need to only fill in VmaVulkanFunctions::vkGetInstanceProcAddr and
        //  VmaVulkanFunctions::vkGetDeviceProcAddr. Other pointers will be fetched automatically.
        VmaVulkanFunctions vulkanFuncs{
            .vkGetInstanceProcAddr{ vkGetInstanceProcAddr },
            .vkGetDeviceProcAddr{ vkGetDeviceProcAddr },
        };
        const VkResult resImportVkFunctions{ vmaImportVulkanFunctionsFromVolk( std::addressof( allocInfo ), std::addressof( vulkanFuncs ) ) };
        if ( resImportVkFunctions != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "Failed to import Vulkan functions into VMA!" );
        }

        allocInfo.pVulkanFunctions = std::addressof( vulkanFuncs );

        const VkResult resCreateAlloc{ vmaCreateAllocator( &allocInfo, &m_Allocator ) };
        if ( resCreateAlloc != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "Failed to create VMA Allocator!" );
        }
    }

    auto VulkanMemoryAllocator::Shutdown() -> void {
        // Check if we have stuff without freeing it first

        // Shutdown VMA here
        vmaDestroyAllocator( m_Allocator );
    }

    auto VulkanMemoryAllocator::AllocateImage( VulkanTexture *texture ) const -> VkResult {
        return vmaCreateImage(
                m_Allocator,
                texture->GetImageCreateInfo(),
                texture->GetAllocationCreateInfo(),
                texture->GetImage(),
                texture->GetVMAllocation(),
                texture->GetVMAllocationInfo() );
    }

    auto VulkanMemoryAllocator::AllocateBuffer( VulkanBuffer *buffer ) const -> VkResult {
        return vmaCreateBuffer(
                m_Allocator,
                buffer->GetBufferCreateInfo(),
                buffer->GetAllocationCreateInfo(),
                buffer->GetBuffer(),
                buffer->GetVmaAllocation(),
                buffer->GetVmaAllocationInfo() );
    }

    auto VulkanMemoryAllocator::FreeImage( VulkanTexture *texture ) const -> void {
        vmaDestroyImage( m_Allocator, *texture->GetImage(), *texture->GetVMAllocation() );
    }

    auto VulkanMemoryAllocator::FreeBuffer( VulkanBuffer *buffer ) const -> void {
        vmaDestroyBuffer( m_Allocator, *buffer->GetBuffer(), *buffer->GetVmaAllocation() );
    }

    auto VulkanMemoryAllocator::MapBuffer( VulkanBuffer *buffer ) const -> void {
        MapBuffer( buffer, true );
    }

    auto VulkanMemoryAllocator::UnmapBuffer( VulkanBuffer *buffer ) const -> void {
        MapBuffer( buffer, false );
    }

    auto VulkanMemoryAllocator::MapBuffer(VulkanBuffer* buffer, const bool map) const -> void {
        MKT_ASSERT(buffer != nullptr, "VulkanMemoryAllocator::MapBuffer - buffer is null!");

        if (map) {
            // Map buffer memory to CPU
            void* mapped{ nullptr };

            const VkResult result{ vmaMapMemory(
                m_Allocator,
                *buffer->GetVmaAllocation(),
                std::addressof( mapped )
            )};

            if (result != VK_SUCCESS) {
                MKT_THROW_RUNTIME_ERROR("Failed to map Vulkan buffer memory!");
            }

            // Update buffer’s allocation info
            buffer->GetVmaAllocationInfo()->pMappedData = mapped;
        }
        else {
            // Unmap buffer memory from CPU
            if (buffer->GetVmaAllocation() && buffer->GetVmaAllocationInfo()->pMappedData) {
                vmaUnmapMemory(m_Allocator, *buffer->GetVmaAllocation());
                buffer->GetVmaAllocationInfo()->pMappedData = nullptr;
            }
        }
    }


}// namespace Mikoto