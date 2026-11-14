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

#include <cstring>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Rhi/GpuAllocator.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )

#include <Platform/Windows/Windows.hh>

#include <D3D12MemAlloc.h>
#include <directx/d3d12.h>
#include <directx/d3dx12.h>

#include <Renderer/Rhi/D3D12/D3D12Device.hh>
#include <Renderer/Rhi/D3D12/Direct3D12Helpers.hh>
#include <Renderer/Rhi/D3D12/D3D12MemoryAllocator.hh>

// https://gpuopen-librariesandsdks.github.io/D3D12MemoryAllocator/html/quick_start.html

namespace mikoto::renderer::d3d12 {

    using namespace mikoto::core;
    using namespace mikoto::renderer::rhi;

    GpuMemoryAllocator::GpuMemoryAllocator( renderer::rhi::IGpuDevice *device )
        : mDevice{ device } {
    }

    auto GpuMemoryAllocator::Init() -> void {
        MKT_BEGIN_PROFILER();
        Device *device{ core::checked_cast<Device *>( mDevice ) };

        D3D12MA::ALLOCATOR_DESC allocatorDesc{};
        allocatorDesc.pDevice = device->GetDevice();
        allocatorDesc.pAdapter = device->GetAdapter();
        allocatorDesc.Flags = ( D3D12MA::ALLOCATOR_FLAGS )D3D12MA_RECOMMENDED_ALLOCATOR_FLAGS;

        ThrowIfFailed( D3D12MA::CreateAllocator( &allocatorDesc, &mAllocator ) );
    }

    auto GpuMemoryAllocator::Shutdown() -> void {
    }

    auto GpuMemoryAllocator::GetVideoBitstreamPool() -> D3D12MA::Pool* {
        std::lock_guard lock{ mVideoPoolMutex };

        if ( !mVideoBitstreamPool ) {
            // CPU-written bitstreams need VIDEO_DECODE_READ, which fixed-state UPLOAD heaps cannot use.
            // A shared write-combined L0 pool keeps Map/UnMap on the ordinary BufferHandle path.
            // https://github.com/microsoft/DirectX-Specs/blob/master/d3d/ResourceHeaps.md
            D3D12MA::POOL_DESC description{};
            description.HeapProperties = CD3DX12_HEAP_PROPERTIES( D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE, D3D12_MEMORY_POOL_L0 );
            description.HeapFlags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;
            ThrowIfFailed( mAllocator->CreatePool( &description, mVideoBitstreamPool.GetAddressOf() ) );
        }

        return mVideoBitstreamPool.Get();
    }

    auto GpuMemoryAllocator::FreeImage( ImageAllocation &allocation ) -> void {
        allocation.mResource.Reset();

        if ( allocation.mAllocation ) {
            allocation.mAllocation->Release();
            allocation.mAllocation = nullptr;
        }
    }

    auto GpuMemoryAllocator::AllocateImage( ImageAllocation &allocation ) -> HRESULT {
        MKT_BEGIN_PROFILER();
        // https://logins.github.io/graphics/2020/07/31/DX12ResourceHandling.html
        // D3D12 heap resource alignment appears in forms of common fixed constants:
        // 64KB for every buffer and base textures types
        // 4MB for MSAA textures

        HRESULT result{ mAllocator->CreateResource(
            &allocation.mAllocDesc, &allocation.mDesc,
            allocation.mInitialState, allocation.mOptimizedClearValue,
            &allocation.mAllocation, IID_PPV_ARGS( &allocation.mResource ) ) };

        return result;
    }

    auto GpuMemoryAllocator::FreeBuffer( BufferAllocation &allocation ) -> void {
        allocation.mResource.Reset();

        if ( allocation.mAllocation ) {
            allocation.mAllocation->Release();
            allocation.mAllocation = nullptr;
        }
    }

    auto GpuMemoryAllocator::AllocateBuffer( BufferAllocation &allocation ) -> HRESULT {
        MKT_BEGIN_PROFILER();
        HRESULT result{ mAllocator->CreateResource(
            &allocation.mAllocDesc, &allocation.mDesc,
            allocation.mInitialState, nullptr,
            &allocation.mAllocation, IID_PPV_ARGS( &allocation.mResource ) ) };

        return result;
    }

    auto GpuMemoryAllocator::MapBuffer( BufferAllocation &allocation ) -> void * {
        MKT_BEGIN_PROFILER();
        void *mapped{ nullptr };
        D3D12_RANGE readRange{ 0, 0 };
        ThrowIfFailed( allocation.mResource->Map( 0, &readRange, core::rc_cast<void **>( &mapped ) ) );

        return mapped;
    }

    auto GpuMemoryAllocator::UnmapBuffer( BufferAllocation &allocation ) -> void {
        MKT_BEGIN_PROFILER();
        allocation.mResource->Unmap( 0, nullptr );
    }

    auto GpuMemoryAllocator::GetProcessMemoryUsage() const -> rhi::ProcessGpuMemory {
        if ( !mAllocator ) {
            return {};
        }
        // Reuse the allocator's adapter and budget cache. UsageBytes includes
        // process resources outside D3D12MA; Stats.BlockBytes would not.
        D3D12MA::Budget local{}, shared{};
        mAllocator->GetBudget( &local, &shared );
        return { core::as<core::usize>( local.UsageBytes ), core::as<core::usize>( shared.UsageBytes ), true };
    }

    auto GpuMemoryAllocator::BeginFrame() -> void {
        if ( mAllocator ) {
            // Let D3D12MA refresh its OS budget cache even when this frame did
            // not create allocations. Otherwise process usage can remain stale.
            mAllocator->SetCurrentFrameIndex( ++mFrameIndex );
        }
    }

    auto GpuMemoryAllocator::GetMemoryUsage() const -> core::usize {
        MKT_BEGIN_PROFILER();
        // https://gpuopen-librariesandsdks.github.io/D3D12MemoryAllocator/html/optimal_allocation.html
        D3D12MA::Budget videoMemBudget{};
        mAllocator->GetBudget( &videoMemBudget, NULL );

        return videoMemBudget.UsageBytes;
    }

    auto GpuMemoryAllocator::GetMemoryTotal() const -> core::usize {
        MKT_BEGIN_PROFILER();
        // https://gpuopen-librariesandsdks.github.io/D3D12MemoryAllocator/html/class_d3_d12_m_a_1_1_allocator.html#a434ae3147209953253da26687bfd62dc
        auto totalVideoMemory{ mAllocator->GetMemoryCapacity( DXGI_MEMORY_SEGMENT_GROUP_LOCAL ) };
        return totalVideoMemory;
    }

    auto GpuMemoryAllocator::GetMemoryAvailable() const -> core::usize {
        MKT_BEGIN_PROFILER();
        // The full capacity of the memory can be queried using function D3D12MA::Allocator::GetMemoryCapacity.
        // However, it is not recommended, because the amount of memory available to the application is
        // typically smaller than the full capacity, as some portion of it is reserved by the operating
        // system or used by other processes.
        D3D12MA::Budget videoMemBudget{};
        mAllocator->GetBudget( &videoMemBudget, NULL );

        // Exceeding the OS budget must report zero headroom, not unsigned wraparound.
        return videoMemBudget.UsageBytes < videoMemBudget.BudgetBytes
                   ? core::as<core::usize>( videoMemBudget.BudgetBytes - videoMemBudget.UsageBytes )
                   : 0;
    }


}// namespace mikoto::renderer::d3d12

#endif
