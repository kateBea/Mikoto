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

#include <EASTL/memory.h>

// Needs to go before VMA for
// vmaImportVulkanFunctionsFromVolk to be defined
#include <volk.h>

// Define VMA implementation in one source file
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <Memory/Allocator.hh>
#include <Memory/GpuAllocator.hh>

#include <Renderer/Core/RenderSystem.hh>

#include <Renderer/Rhi/Vulkan/VulkanContext.hh>
#include <Renderer/Rhi/Vulkan/VulkanDevice.hh>
#include <Renderer/Rhi/Vulkan/VulkanHelpers.hh>
#include <Renderer/Rhi/Vulkan/VulkanMemoryAllocator.hh>

namespace mikoto::renderer::vulkan {

    GpuMemoryAllocator::GpuMemoryAllocator( IGpuDevice* device )
        : IGpuAllocator{ device } {}

    auto GpuMemoryAllocator::Init() -> void {
        Device* device{ as<Device*>( mDevice ) };
        Context* ctx{ as<Context*>( RenderSystem::Get()->GetContext() ) };

        VmaAllocatorCreateInfo allocInfo{};
        allocInfo.instance = ctx->GetInstance().mInstance;
        allocInfo.vulkanApiVersion = ctx->GetApiVersion();
        allocInfo.flags = MKT_VK_FLAGS_NONE;

        if (device->GetActive12Features().bufferDeviceAddress == VK_TRUE) {
            allocInfo.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
        }

        allocInfo.device = device->GetDevice();
        allocInfo.physicalDevice = device->GetPhysicalDevice()->mPhysicalDevice;

        //  VMA tries to fetch remaining pointers that are still null
        //  by calling vkGetInstanceProcAddr and vkGetDeviceProcAddr on its own.
        //  You need to only fill in VmaVulkanFunctions::vkGetInstanceProcAddr and
        //  VmaVulkanFunctions::vkGetDeviceProcAddr. Other pointers will be fetched automatically.
        VmaVulkanFunctions vulkanFuncs{
            .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
            .vkGetDeviceProcAddr = vkGetDeviceProcAddr };
        MKT_VK_CHECK( vmaImportVulkanFunctionsFromVolk( MKT_ADDRESSOF( allocInfo ), MKT_ADDRESSOF( vulkanFuncs ) ) );

        allocInfo.pVulkanFunctions = MKT_ADDRESSOF( vulkanFuncs );

        MKT_VK_CHECK( vmaCreateAllocator( MKT_ADDRESSOF( allocInfo ) , MKT_ADDRESSOF( mAllocator ) ) );

        // Note for public API:
        // By default, all calls to functions that take VmaAllocator as first parameter are safe to call
        // from multiple threads simultaneously because they are synchronized internally when needed.
        // This includes allocation and deallocation from default memory pool, as well as custom VmaPool.
        // https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/general_considerations.html
    }

    auto GpuMemoryAllocator::Shutdown() -> void {
        vmaDestroyAllocator( mAllocator );
    }

    auto GpuMemoryAllocator::AllocateImage( ImageAllocation& allocation ) -> VkResult {
        VkResult res{
            vmaCreateImage(
                    mAllocator,
                    MKT_ADDRESSOF( allocation.mImageCreateInfo ),
                    MKT_ADDRESSOF( allocation.mAllocationCreateInfo ),
                    MKT_ADDRESSOF( allocation.mImage ),
                    MKT_ADDRESSOF( allocation.mAllocation ),
                    MKT_ADDRESSOF( allocation.mAllocationInfo ) )
        };

        return res;
    }

    auto GpuMemoryAllocator::AllocateBuffer( BufferAllocation& allocation ) -> VkResult {
        VkResult res{ vmaCreateBuffer(
                mAllocator,
                MKT_ADDRESSOF( allocation.mBufferCreateInfo ),
                MKT_ADDRESSOF( allocation.mAllocationCreateInfo ),
                MKT_ADDRESSOF( allocation.mBuffer ),
                MKT_ADDRESSOF( allocation.mAllocation ),
                MKT_ADDRESSOF( allocation.mAllocationInfo ) ) };


        return res;
    }

    auto GpuMemoryAllocator::FreeBuffer( BufferAllocation& allocation ) -> void {
        vmaDestroyBuffer( mAllocator, allocation.mBuffer, allocation.mAllocation );
    }

    auto GpuMemoryAllocator::FreeImage( ImageAllocation& allocation ) -> void {
        vmaDestroyImage( mAllocator, allocation.mImage, allocation.mAllocation );
    }

    auto GpuMemoryAllocator::MapBuffer( BufferAllocation& allocation ) const -> void {
        MapBuffer( allocation, true );
    }

    auto GpuMemoryAllocator::UnmapBuffer( BufferAllocation& allocation ) const -> void {
        MapBuffer( allocation, false );
    }

    auto GpuMemoryAllocator::GetMemoryUsage() const -> usize {
        // Too slow to call per frame
        //vmaCalculateStatistics( mAllocator, MKT_ADDRESSOF( mStats ) );

        Device* device{ checked_cast<Device*>( mDevice ) };
        eastl::fixed_vector<VmaBudget, VK_MAX_MEMORY_HEAPS> budgets{};
        budgets.resize( device->GetPhysicalDevice()->mMemoryProperties.memoryHeapCount );

        // https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/staying_within_budget.html
        // https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/struct_vma_budget.html
        vmaGetHeapBudgets( mAllocator, budgets.data() );

        // Sum up the consumption in each heap
        VkDeviceSize budgetSize{ 0 };
        for (const auto& budget : budgets ) {
            budgetSize += budget.usage;
        }

        return as<u64>(budgetSize);
    }

    auto GpuMemoryAllocator::GetMemoryTotal() const -> usize {
        // Too slow to call per frame
        // vmaCalculateStatistics( mAllocator, MKT_ADDRESSOF( mStats ) );

        Device* device{ checked_cast<Device*>( mDevice ) };

        VkDeviceSize total{ 0 };
        for (usize heapIndex{}; heapIndex < device->GetPhysicalDevice()->mMemoryProperties.memoryHeapCount; ++heapIndex ) {
            VkMemoryHeap heap = device->GetPhysicalDevice()->mMemoryProperties.memoryHeaps[heapIndex];

            // Check if the heap belongs physically to the GPU device
            if (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                total += heap.size;
            }
        }

        return as<u64>(total);
    }

    auto GpuMemoryAllocator::GetMemoryAvailable() const -> usize {
        // Too slow to call per frame
        // vmaCalculateStatistics( mAllocator, MKT_ADDRESSOF( mStats ) );

        Device* device{ checked_cast<Device*>( mDevice ) };
        eastl::fixed_vector<VmaBudget, VK_MAX_MEMORY_HEAPS> budgets{};
        budgets.resize( device->GetPhysicalDevice()->mMemoryProperties.memoryHeapCount );

        // https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/struct_vma_budget.html
        vmaGetHeapBudgets( mAllocator, budgets.data() );

        // Sum up the consumption in each heap
        VkDeviceSize available{ 0 };
        for (const auto& budget : budgets ) {
            available += budget.budget;
        }

        return as<u64>(available);
    }

    auto GpuMemoryAllocator::MapBuffer( BufferAllocation& allocation, bool map ) const -> void {
        if ( map ) {
            MKT_VK_CHECK( vmaMapMemory( mAllocator, allocation.mAllocation, MKT_ADDRESSOF( allocation.mAllocationInfo.pMappedData ) ) );
        } else {
            if ( allocation.mAllocationInfo.pMappedData ) {
                vmaUnmapMemory( mAllocator, allocation.mAllocation );
                allocation.mAllocationInfo.pMappedData = nullptr;
            }
        }
    }


}// namespace mikoto::renderer::vulkan