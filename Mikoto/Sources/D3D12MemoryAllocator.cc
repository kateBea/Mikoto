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

#include <Core/Types.hh>
#include <Core/Core.hh>
#include <Core/Platform.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Rhi/GpuAllocator.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )

#include <directx/d3d12.h>
#include <D3D12MemAlloc.h>

#include <Renderer/Rhi/D3D12/D3D12Device.hh>
#include <Renderer/Rhi/D3D12/Direct3D12Helpers.hh>
#include <Renderer/Rhi/D3D12/D3D12MemoryAllocator.hh>

// https://gpuopen-librariesandsdks.github.io/D3D12MemoryAllocator/html/quick_start.html

namespace mikoto::renderer::d3d12 {

    using namespace mikoto::core;
    using namespace mikoto::renderer::rhi;

    GpuMemoryAllocator::GpuMemoryAllocator( IGpuDevice *device )
        : IGpuAllocator{ device }
    {

    }

    auto GpuMemoryAllocator::Init() -> void {
        Device* device{ checked_cast<Device*>( mDevice ) };

        D3D12MA::ALLOCATOR_DESC allocatorDesc{};
        allocatorDesc.pDevice = device->GetDevice();
        allocatorDesc.pAdapter = device->GetAdapter();
        allocatorDesc.Flags = (D3D12MA::ALLOCATOR_FLAGS)D3D12MA_RECOMMENDED_ALLOCATOR_FLAGS;

        ThrowIfFailed(D3D12MA::CreateAllocator(&allocatorDesc, &mAllocator));
    }

    auto GpuMemoryAllocator::Shutdown() -> void {

    }

    auto GpuMemoryAllocator::FreeImage( ImageAllocation &allocation ) -> void {

    }

    auto GpuMemoryAllocator::AllocateImage( ImageAllocation &allocation ) -> HRESULT {
        HRESULT result{ mAllocator->CreateResource(
            &allocation.mAllocDesc, &allocation.mDesc,
            D3D12_RESOURCE_STATE_COMMON, allocation.mOptimizedClearValue,
            &allocation.mAllocation, IID_PPV_ARGS(&allocation.mResource)) };

        return result;
    }

    auto GpuMemoryAllocator::FreeBuffer( BufferAllocation &allocation ) -> void {

    }

    auto GpuMemoryAllocator::AllocateBuffer( BufferAllocation &allocation ) -> HRESULT {
        HRESULT result{ mAllocator->CreateResource(
            &allocation.mAllocDesc, &allocation.mDesc,
            D3D12_RESOURCE_STATE_COMMON, nullptr,
            &allocation.mAllocation, IID_PPV_ARGS(&allocation.mResource)) };

        return result;
    }

    auto GpuMemoryAllocator::MapBuffer( BufferAllocation &allocation ) -> void* {
        void* mapped{ nullptr };
        D3D12_RANGE readRange{ 0, 0 };
        ThrowIfFailed( allocation.mResource->Map(0, &readRange, rc_cast<void**>(&mapped)));

        return mapped;
    }

    auto GpuMemoryAllocator::UnmapBuffer( BufferAllocation &allocation ) -> void {
        allocation.mResource->Unmap(0, nullptr);
    }

    auto GpuMemoryAllocator::GetMemoryUsage() const -> usize {
        // https://gpuopen-librariesandsdks.github.io/D3D12MemoryAllocator/html/optimal_allocation.html
        D3D12MA::Budget videoMemBudget{};
        mAllocator->GetBudget(&videoMemBudget, NULL);

        return videoMemBudget.UsageBytes;
    }

    auto GpuMemoryAllocator::GetMemoryTotal() const -> usize {
        // https://gpuopen-librariesandsdks.github.io/D3D12MemoryAllocator/html/class_d3_d12_m_a_1_1_allocator.html#a434ae3147209953253da26687bfd62dc
        auto totalVideoMemory{ mAllocator->GetMemoryCapacity(DXGI_MEMORY_SEGMENT_GROUP_LOCAL ) };
        return totalVideoMemory;
    }

    auto GpuMemoryAllocator::GetMemoryAvailable() const -> usize {
        // The full capacity of the memory can be queried using function D3D12MA::Allocator::GetMemoryCapacity.
        // However, it is not recommended, because the amount of memory available to the application is
        // typically smaller than the full capacity, as some portion of it is reserved by the operating
        // system or used by other processes.
        D3D12MA::Budget videoMemBudget{};
        mAllocator->GetBudget(&videoMemBudget, NULL);

        return as<usize>(videoMemBudget.BudgetBytes - videoMemBudget.UsageBytes);
    }
}// namespace mikoto::renderer::d3d12

#endif