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

#ifndef MIKOTOROOT_D3D12_MEMORY_ALLOCATOR_HH
#define MIKOTOROOT_D3D12_MEMORY_ALLOCATOR_HH


#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>

#include <Memory/GpuAllocator.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )

#include <directx/d3d12.h>
#include <D3D12MemAlloc.h>

#include <Renderer/Core/Rhi.hh>

namespace mikoto::renderer::d3d12 {

    struct BufferAllocation {
        D3D12_RESOURCE_DESC mDesc{};
        ID3D12Resource* mResource{};
        D3D12MA::Allocation* mAllocation{};
    };

    struct ImageAllocation {
        D3D12_RESOURCE_DESC mDesc{};
        ID3D12Resource* mResource{};
        D3D12MA::Allocation* mAllocation{};
    };

    class GpuMemoryAllocator final : public memory::IGpuAllocator {
    public:
        explicit GpuMemoryAllocator( IGpuDevice* device );

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto FreeImage( ImageAllocation& allocation ) -> void;
        auto AllocateImage( ImageAllocation& allocation ) -> HRESULT ;

        auto FreeBuffer( BufferAllocation& allocation ) -> void;
        auto AllocateBuffer( BufferAllocation& allocation ) -> HRESULT ;

        auto MapBuffer( BufferAllocation& allocation ) const -> void;
        auto UnmapBuffer( BufferAllocation& allocation ) const -> void;

        // These are slow use for debug only
        MKT_NODISCARD auto GetMemoryUsage() const -> size_t override;
        MKT_NODISCARD auto GetMemoryTotal() const -> size_t override;
        MKT_NODISCARD auto GetMemoryAvailable() const -> size_t override;

    private:
        // Map/Unmap Device memory to cpu accessible memory (map = true to map, false to unmap)
        auto MapBuffer( BufferAllocation& allocation, bool map ) const -> void;

    private:
        D3D12MA::Allocator* mAllocator{};
        D3D12MA::TotalStatistics mStatistics{};
    };
}

#endif

#endif//MIKOTOROOT_D3D12_MEMORY_ALLOCATOR_HH
