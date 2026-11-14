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

#ifndef MIKOTO_RENDERER_RHI_D3D12_D3D12_MEMORY_ALLOCATOR_HH
#define MIKOTO_RENDERER_RHI_D3D12_D3D12_MEMORY_ALLOCATOR_HH

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Rhi/DeviceObject.hh>
#include <Renderer/Rhi/GpuAllocator.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )

#include <mutex>

#include <Platform/Windows/Windows.hh>

#include <D3D12MemAlloc.h>
#include <directx/d3d12.h>
#include <directx/d3dx12.h>
#include <wrl.h>

namespace mikoto::renderer::d3d12 {

    struct BufferAllocation {
        D3D12_RESOURCE_DESC mDesc{};
        Microsoft::WRL::ComPtr<ID3D12Resource> mResource{};
        D3D12_RESOURCE_STATES mInitialState{ D3D12_RESOURCE_STATE_COMMON };

        D3D12MA::Allocation* mAllocation{};
        D3D12MA::ALLOCATION_DESC mAllocDesc{};
    };

    struct ImageAllocation {
        D3D12_RESOURCE_DESC mDesc{};
        Microsoft::WRL::ComPtr<ID3D12Resource> mResource{};
        D3D12_RESOURCE_STATES mInitialState{ D3D12_RESOURCE_STATE_COMMON };

        const D3D12_CLEAR_VALUE* mOptimizedClearValue{ nullptr };

        D3D12MA::Allocation* mAllocation{};
        D3D12MA::ALLOCATION_DESC mAllocDesc{};
    };

    class GpuMemoryAllocator final : public rhi::IGpuAllocator {
    public:
    private:
        rhi::IGpuDevice* mDevice{};

    public:
        explicit GpuMemoryAllocator( rhi::IGpuDevice* device );

        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto BeginFrame() -> void override;

        auto FreeImage( ImageAllocation& allocation ) -> void;
        auto AllocateImage( ImageAllocation& allocation ) -> HRESULT ;

        auto FreeBuffer( BufferAllocation& allocation ) -> void;
        auto AllocateBuffer( BufferAllocation& allocation ) -> HRESULT ;

        auto MapBuffer( BufferAllocation& allocation ) -> void*;
        auto UnmapBuffer( BufferAllocation& allocation ) -> void;
        MKT_NODISCARD auto GetVideoBitstreamPool() -> D3D12MA::Pool*;

        /** Uses D3D12MA's process budget accounting, including allocations outside the allocator.
         * D3D12MA uses DXGI when available and otherwise supplies an estimate.
         */
        MKT_NODISCARD auto GetProcessMemoryUsage() const -> rhi::ProcessGpuMemory override;

        // These are slow use for debug only
        MKT_NODISCARD auto GetMemoryUsage() const -> core::usize override;
        MKT_NODISCARD auto GetMemoryTotal() const -> core::usize override;
        MKT_NODISCARD auto GetMemoryAvailable() const -> core::usize override;

    private:
        D3D12MA::Allocator* mAllocator{};
        std::mutex mVideoPoolMutex{};
        Microsoft::WRL::ComPtr<D3D12MA::Pool> mVideoBitstreamPool{};
        core::u32 mFrameIndex{};
        D3D12MA::TotalStatistics mStatistics{};
    };

}

#endif

#endif// MIKOTO_RENDERER_RHI_D3D12_D3D12_MEMORY_ALLOCATOR_HH

