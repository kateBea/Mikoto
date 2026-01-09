//
// Created by zanet on 10/6/2025.
//
#include <memory>

#include <volk.h>
#include <fmt/format.h>

// Define VMA implementation in one source file
#define VMA_IMPLEMENTATION

#include <Renderer/Vulkan/VulkanMemoryAllocator.hh>

#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanDevice.hh>

namespace Mikoto {

    VulkanMemoryAllocator::VulkanMemoryAllocator( GpuDevice *device )
        : GpuAllocator{ device } {}

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
        // if (m_DebugNames.size() != 0) {
        //     MKT_CORE_LOGGER_WARN( "VulkanMemoryAllocator::Shutdown - All resources were not freed previous to this allocator shutdown. Left {}", m_DebugNames.size() );
        //
        //     for (const auto& name : m_DebugNames) {
        //         MKT_CORE_LOGGER_WARN( "VulkanMemoryAllocator::Shutdown - Did no free {}", name );
        //     }
        // }

        // Shutdown VMA
        vmaDestroyAllocator( m_Allocator );
    }

    auto VulkanMemoryAllocator::AllocateImage( VulkanTexture *texture ) -> VkResult {
        VkResult res{
            vmaCreateImage(
                m_Allocator,
                texture->GetImageCreateInfo(),
                texture->GetAllocationCreateInfo(),
                texture->GetImage(),
                texture->GetVMAllocation(),
                texture->GetVMAllocationInfo() ) };

        if ( res == VK_SUCCESS ) {
            //m_DebugNames.insert( texture->GetDebugName() );
        }

        return res;
    }

    auto VulkanMemoryAllocator::AllocateImage( VulkanTextureCube *texture ) -> VkResult {
        VkResult res{
            vmaCreateImage(
                m_Allocator,
                texture->GetImageCreateInfo(),
                texture->GetAllocationCreateInfo(),
                texture->GetImage(),
                texture->GetVMAllocation(),
                texture->GetVMAllocationInfo() ) };

        if ( res == VK_SUCCESS ) {
            //m_DebugNames.insert( texture->GetDebugName() );
        }

        return res;
    }

    auto VulkanMemoryAllocator::FreeImage( VulkanTextureCube *texture ) -> void {
        vmaDestroyImage( m_Allocator, *texture->GetImage(), *texture->GetVMAllocation() );

        //m_DebugNames.erase( texture->GetDebugName() );
    }

    auto VulkanMemoryAllocator::AllocateBuffer( VulkanBuffer *buffer ) -> VkResult {
        VkResult res{ vmaCreateBuffer(
                m_Allocator,
                buffer->GetBufferCreateInfo(),
                buffer->GetAllocationCreateInfo(),
                buffer->GetBuffer(),
                buffer->GetVmaAllocation(),
                buffer->GetVmaAllocationInfo() ) };

        if ( res == VK_SUCCESS ) {
            //m_DebugNames.insert( buffer->GetDebugName() );
        }

        return res;
    }

    auto VulkanMemoryAllocator::FreeImage( VulkanTexture *texture ) -> void {
        vmaDestroyImage( m_Allocator, *texture->GetImage(), *texture->GetVMAllocation() );

        //m_DebugNames.erase( texture->GetDebugName() );
    }

    auto VulkanMemoryAllocator::FreeBuffer( VulkanBuffer *buffer ) -> void {
        vmaDestroyBuffer( m_Allocator, *buffer->GetBuffer(), *buffer->GetVmaAllocation() );

        //m_DebugNames.erase( buffer->GetDebugName());
    }

    auto VulkanMemoryAllocator::MapBuffer( VulkanBuffer *buffer ) const -> void {
        MapBuffer( buffer, true );
    }

    auto VulkanMemoryAllocator::UnmapBuffer( VulkanBuffer *buffer ) const -> void {
        MapBuffer( buffer, false );
    }

    auto VulkanMemoryAllocator::GetMemoryUsage() const -> Size {
        vmaCalculateStatistics( m_Allocator, std::addressof( m_Stats ) );
        return m_Stats.total.statistics.allocationBytes;
    }

    auto VulkanMemoryAllocator::GetMemoryTotal() const -> Size {
        vmaCalculateStatistics( m_Allocator, std::addressof( m_Stats ) );
        return 0;
    }

    auto VulkanMemoryAllocator::GetMemoryAvailable() const -> Size {
        vmaCalculateStatistics( m_Allocator, std::addressof( m_Stats ) );
        return m_Stats.total.unusedRangeSizeMax;
    }

    auto VulkanMemoryAllocator::MapBuffer( VulkanBuffer *buffer, const bool map ) const -> void {
        MKT_ASSERT(buffer != nullptr, "VulkanMemoryAllocator::MapBuffer - buffer is null!");

        if (map) {
            const VkResult result{ vmaMapMemory(
                m_Allocator,
                *buffer->GetVmaAllocation(),
                std::addressof( buffer->GetVmaAllocationInfo()->pMappedData )
            )};

            if (result != VK_SUCCESS) {
                MKT_THROW_RUNTIME_ERROR("Failed to map Vulkan buffer memory!");
            }
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