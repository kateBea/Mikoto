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

#include <EASTL/array.h>
#include <EASTL/memory.h>
#include <EASTL/fixed_vector.h>

#include <Core/Profiler.hh>
#include <Core/Exception.hh>

// Needs to go before VMA for
// vmaImportVulkanFunctionsFromVolk to be defined
#include <volk.h>

// Define VMA implementation in one source file
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Memory/Allocator.hh>

#include <Renderer/Rhi/GpuAllocator.hh>
#include <Renderer/Core/RenderSystem.hh>
#include <Renderer/Rhi/Vulkan/VulkanDevice.hh>
#include <Renderer/Rhi/Vulkan/VulkanContext.hh>
#include <Renderer/Rhi/Vulkan/VulkanHelpers.hh>
#include <Renderer/Rhi/Vulkan/VulkanMemoryAllocator.hh>

namespace mikoto::renderer::vulkan {

    using namespace mikoto::core;

    GpuMemoryAllocator::GpuMemoryAllocator( renderer::rhi::IGpuDevice* device )
        : mDevice{ device } {
    }

    auto GpuMemoryAllocator::Init() -> void {
        MKT_BEGIN_PROFILER();
        Device* device{ core::checked_cast<Device*>( mDevice ) };
        Context* ctx{ core::checked_cast<Context*>( RenderSystem::Get()->GetContext() ) };

        VmaAllocatorCreateInfo allocInfo{};
        allocInfo.instance = ctx->GetInstance().mInstance;
        allocInfo.vulkanApiVersion = ctx->GetApiVersion();
        allocInfo.flags = MKT_VK_FLAGS_NONE;

        if ( device->GetActive12Features().bufferDeviceAddress == VK_TRUE ) {
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
            .vkGetDeviceProcAddr = vkGetDeviceProcAddr
        };
        MKT_VK_CHECK( vmaImportVulkanFunctionsFromVolk( MKT_ADDRESSOF( allocInfo ), MKT_ADDRESSOF( vulkanFuncs ) ) );

        allocInfo.pVulkanFunctions = MKT_ADDRESSOF( vulkanFuncs );

        MKT_VK_CHECK( vmaCreateAllocator( MKT_ADDRESSOF( allocInfo ), MKT_ADDRESSOF( mAllocator ) ) );

        // Note for public API:
        // By default, all calls to functions that take VmaAllocator as first parameter are safe to call
        // from multiple threads simultaneously because they are synchronized internally when needed.
        // This includes allocation and deallocation from default memory pool, as well as custom VmaPool.
        // https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/general_considerations.html
    }

    auto GpuMemoryAllocator::Shutdown() -> void {
        MKT_BEGIN_PROFILER();
        vmaDestroyAllocator( mAllocator );
        mAllocator = nullptr;
    }

    auto GpuMemoryAllocator::AllocateImage( ImageAllocation& allocation ) -> VkResult {
        MKT_BEGIN_PROFILER();
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
        MKT_BEGIN_PROFILER();
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
        MKT_BEGIN_PROFILER();
        vmaDestroyBuffer( mAllocator, allocation.mBuffer, allocation.mAllocation );
    }

    auto GpuMemoryAllocator::FreeImage( ImageAllocation& allocation ) -> void {
        MKT_BEGIN_PROFILER();
        vmaDestroyImage( mAllocator, allocation.mImage, allocation.mAllocation );
    }

    auto GpuMemoryAllocator::AllocateMemory( const VkMemoryRequirements& requirements, const VmaAllocationCreateInfo& description,
        VmaAllocation& allocation, VmaAllocationInfo& info ) -> VkResult {
        MKT_BEGIN_PROFILER();

        // General allocations preserve the size, alignment and memory types required by a video session.
        // https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/group__group__alloc.html
        return vmaAllocateMemory( mAllocator, &requirements, &description, &allocation, &info );
    }

    auto GpuMemoryAllocator::FreeMemory( VmaAllocation allocation ) -> void {
        MKT_BEGIN_PROFILER();
        vmaFreeMemory( mAllocator, allocation );
    }

    auto GpuMemoryAllocator::MapBuffer( BufferAllocation& allocation ) const -> void {
        MKT_BEGIN_PROFILER();
        MapBuffer( allocation, true );
    }

    auto GpuMemoryAllocator::UnmapBuffer( BufferAllocation& allocation ) const -> void {
        MKT_BEGIN_PROFILER();
        MapBuffer( allocation, false );
    }

    auto GpuMemoryAllocator::InvalidateBuffer( const BufferAllocation& allocation ) const -> void {
        MKT_BEGIN_PROFILER();
        MKT_VK_CHECK( vmaInvalidateAllocation( mAllocator, allocation.mAllocation, 0, VK_WHOLE_SIZE ) );
    }

    auto GpuMemoryAllocator::SetAllocationName( VmaAllocation allocation, eastl::string_view name ) -> void {
        vmaSetAllocationName( mAllocator, allocation, name.data() );
    }

    auto GpuMemoryAllocator::GetStatsString() -> eastl::string {
        char* statsString{ nullptr };
        vmaBuildStatsString( mAllocator, &statsString, VK_TRUE );

        eastl::string result{ statsString };
        vmaFreeStatsString( mAllocator, statsString );

        return result;
    }

    auto GpuMemoryAllocator::GetMemoryUsage() const -> core::usize {
        MKT_BEGIN_PROFILER();

        if ( !mAllocator ) {
            return 0;
        }

        const VkPhysicalDeviceMemoryProperties* properties{};
        vmaGetMemoryProperties( mAllocator, &properties );
        eastl::array<VmaBudget, VK_MAX_MEMORY_HEAPS> budgets{};
        vmaGetHeapBudgets( mAllocator, budgets.data() );

        // Match GetMemoryTotal's device-local heap set. Including host-only
        // heaps here mixes system RAM into a GPU-capacity comparison. Count
        // heaps, not memory types: several types may refer to the same heap.
        VkDeviceSize usage{};

        for ( core::u32 index{}; index < properties->memoryHeapCount; ++index ) {
            if ( properties->memoryHeaps[index].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT ) {
                usage += budgets[index].usage;
            }
        }

        return core::as<core::usize>( usage );
    }

    auto GpuMemoryAllocator::GetMemoryTotal() const -> core::usize {
        MKT_BEGIN_PROFILER();

        if ( !mAllocator ) {
            return 0;
        }

        const VkPhysicalDeviceMemoryProperties* properties{};
        vmaGetMemoryProperties( mAllocator, &properties );
        VkDeviceSize total{};

        for ( core::u32 index{}; index < properties->memoryHeapCount; ++index ) {
            const auto& heap{ properties->memoryHeaps[index] };
            // On integrated GPUs, device-local memory can also be shared RAM.
            if ( heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT ) {
                total += heap.size;
            }
        }

        return core::as<core::usize>( total );
    }

    auto GpuMemoryAllocator::GetMemoryAvailable() const -> core::usize {
        MKT_BEGIN_PROFILER();

        if ( !mAllocator ) {
            return 0;
        }

        const VkPhysicalDeviceMemoryProperties* properties{};
        vmaGetMemoryProperties( mAllocator, &properties );
        eastl::array<VmaBudget, VK_MAX_MEMORY_HEAPS> budgets{};
        vmaGetHeapBudgets( mAllocator, budgets.data() );

        // VmaBudget::budget is a spending limit, not free memory. Subtract
        // usage per heap and clamp before unsigned subtraction. An over-budget
        // heap contributes zero rather than wrapping or consuming another
        // heap's headroom. The budget may be below physical heap capacity.
        VkDeviceSize available{};

        for ( core::u32 index{}; index < properties->memoryHeapCount; ++index ) {
            if ( !( properties->memoryHeaps[index].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT ) ) {
                continue;
            }

            const auto& budget{ budgets[index] };

            if ( budget.budget > budget.usage ) {
                available += budget.budget - budget.usage;
            }
        }

        return core::as<core::usize>( available );
    }

    auto GpuMemoryAllocator::MapBuffer( BufferAllocation& allocation, bool map ) const -> void {
        MKT_BEGIN_PROFILER();

        if ( map ) {
            MKT_VK_CHECK( vmaMapMemory( mAllocator, allocation.mAllocation, MKT_ADDRESSOF( allocation.mAllocationInfo.pMappedData ) ) );
        }
        else {
            if ( allocation.mAllocationInfo.pMappedData ) {
                vmaUnmapMemory( mAllocator, allocation.mAllocation );
                allocation.mAllocationInfo.pMappedData = nullptr;
            }
        }
    }

    auto GpuMemoryAllocator::FlushBuffer( const BufferAllocation& allocation, VkDeviceSize offset, VkDeviceSize size ) const -> void {
        MKT_VK_CHECK( vmaFlushAllocation( mAllocator, allocation.mAllocation, offset, size ) );
    }

    auto GpuMemoryAllocator::BeginFrame() -> void {
    }

    auto GpuMemoryAllocator::GetProcessMemoryUsage() const -> rhi::ProcessGpuMemory {
        if ( !mAllocator ) {
            return {};
        }

        Device* pDevice{ core::checked_cast<Device*>( mDevice ) };

        const PhysicalDevice* pPhysicalDevice{ pDevice->GetPhysicalDevice() };
        const VkPhysicalDeviceMemoryProperties& memProps{ pPhysicalDevice->mMemoryProperties };

        u32 heapCount{ memProps.memoryHeapCount };
        eastl::fixed_vector<VmaBudget, 10> budgets( heapCount );
        vmaGetHeapBudgets( mAllocator, budgets.data() );

        VkDeviceSize totalDeviceBudget{ 0 };// Pure GPU VRAM (No CPU access)
        VkDeviceSize totalDeviceUsage{ 0 };

        VkDeviceSize totalSharedBudget{ 0 };// Shared GPU-CPU memory
        VkDeviceSize totalSharedUsage{ 0 };

        VkDeviceSize totalHostBudget{ 0 };// Standard System RAM (Host only)
        VkDeviceSize totalHostUsage{ 0 };

        for ( u32 i{}; i < heapCount; ++i ) {
            const auto& heap{ memProps.memoryHeaps[i] };
            bool isDeviceLocal{ ( heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT ) != 0 };

            // Check if any memory types mapping to this heap are host-visible
            bool isHostVisible{ false };

            for ( u32 t{}; t < memProps.memoryTypeCount; ++t ) {
                if ( memProps.memoryTypes[t].heapIndex == i &&
                     ( memProps.memoryTypes[t].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT ) ) {
                    isHostVisible = true;
                    break;
                }
            }

            // Categorize and accumulate budget and usage
            if ( isDeviceLocal && !isHostVisible ) {
                totalDeviceBudget += budgets[i].budget;
                totalDeviceUsage += budgets[i].usage;
            }
            else if ( isDeviceLocal && isHostVisible ) {
                totalSharedBudget += budgets[i].budget;
                totalSharedUsage += budgets[i].usage;
            }
            else {
                totalHostBudget += budgets[i].budget;
                totalHostUsage += budgets[i].usage;
            }
        }


        return { core::as<core::usize>( totalDeviceUsage ), core::as<core::usize>( totalSharedUsage ), true };
    }

}// namespace mikoto::renderer::vulkan
