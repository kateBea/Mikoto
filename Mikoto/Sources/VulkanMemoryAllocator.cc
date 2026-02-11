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


#include <memory>

#include <volk.h>
#include <fmt/format.h>

// Define VMA implementation in one source file
#define VMA_IMPLEMENTATION

#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanDevice.hh>
#include <Renderer/Vulkan/VulkanMemoryAllocator.hh>

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

        // Note for public API:
        // By default, all calls to functions that take VmaAllocator as first parameter are safe to call
        // from multiple threads simultaneously because they are synchronized internally when needed.
        // This includes allocation and deallocation from default memory pool, as well as custom VmaPool.
        // https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/general_considerations.html
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

    auto VulkanMemoryAllocator::AllocateImage( ImageAllocation& allocation ) -> VkResult {
        VkResult res{
            vmaCreateImage(
                m_Allocator,
                std::addressof( allocation.ImageCreateInfo ),
                std::addressof( allocation.AllocationCreateInfo ),
                std::addressof( allocation.Image ),
                std::addressof( allocation.Allocation ),
                std::addressof( allocation.AllocationInfo ) ) };

        return res;
    }

    auto VulkanMemoryAllocator::AllocateBuffer( BufferAllocation& allocation ) -> VkResult {
        VkResult res{ vmaCreateBuffer(
                m_Allocator,
                std::addressof( allocation.BufferCreateInfo ),
                std::addressof( allocation.AllocationCreateInfo ),
                std::addressof( allocation.Buffer ),
                std::addressof( allocation.Allocation ),
                std::addressof( allocation.AllocationInfo ) ) };


        return res;
    }

    auto VulkanMemoryAllocator::FreeBuffer( BufferAllocation& allocation ) -> void {
        vmaDestroyBuffer( m_Allocator, allocation.Buffer, allocation.Allocation );
    }

    auto VulkanMemoryAllocator::FreeImage( ImageAllocation& allocation ) -> void {
        vmaDestroyImage( m_Allocator, allocation.Image, allocation.Allocation );
    }

    auto VulkanMemoryAllocator::MapBuffer( BufferAllocation& allocation ) const -> void {
        MapBuffer( allocation, true );
    }

    auto VulkanMemoryAllocator::UnmapBuffer( BufferAllocation& allocation ) const -> void {
        MapBuffer( allocation, false );
    }

    auto VulkanMemoryAllocator::IsValidAllocation( BufferAllocation& allocation ) const -> bool {
        return allocation.Buffer != VK_NULL_HANDLE; // TODO: better vaidation
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

    auto VulkanMemoryAllocator::MapBuffer( BufferAllocation& allocation, const bool map ) const -> void {
        if (map) {
            const VkResult result{ vmaMapMemory(
                m_Allocator,
                allocation.Allocation,
                std::addressof( allocation.AllocationInfo.pMappedData )
            )};

            if (result != VK_SUCCESS) {
                MKT_THROW_RUNTIME_ERROR("Failed to map Vulkan buffer memory!");
            }
        }
        else {
            // Unmap buffer memory from CPU
            if (allocation.AllocationInfo.pMappedData) {
                vmaUnmapMemory(m_Allocator, allocation.Allocation);
                allocation.AllocationInfo.pMappedData = nullptr;
            }
        }
    }


}// namespace Mikoto