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

#ifndef MIKOTO_RENDERER_RHI_D3D12_D3D12_QUERY_POOL_HH
#define MIKOTO_RENDERER_RHI_D3D12_D3D12_QUERY_POOL_HH

#include <Core/Platform.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )
#include <EASTL/vector.h>
#include <Platform/Windows/Windows.hh>

#include <directx/d3d12.h>
#include <wrl.h>

#include <Renderer/Rhi/QueryPool.hh>

namespace mikoto::renderer::d3d12 {
    class Device;
    class QueryPool final : public rhi::IQueryPool {
        friend class CommandList;

    public:
        QueryPool( const rhi::QueryPoolDescription& description );


        MKT_NODISCARD auto GetDescription() const -> const rhi::QueryPoolDescription& override;
        MKT_NODISCARD auto GetCapabilities() const -> const rhi::QueryCapabilities& override;
        MKT_NODISCARD auto GetElapsedMilliseconds( core::u64 begin, core::u64 end ) const -> core::f64 override;
        MKT_NODISCARD auto TryGetResult( core::u32 index, const rhi::IFence& completion, core::u64 value, rhi::QueryResult& result ) -> bool override;

        ~QueryPool() override;

    private:
        // CPU recording state, separate from completion of the GPU submission.
        enum class SlotState : core::ubyte {
            eNeedsReset,
            eReady,
            eActive,
            eEnded,
            eResolved
        };

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;
        auto ReadCompleted( core::u32 index, rhi::QueryResult& result ) -> bool override;

        auto ValidateRange( core::u32 first, core::u32 count ) const -> void override;

    private:
        D3D12_QUERY_TYPE mType{};

        Microsoft::WRL::ComPtr<ID3D12QueryHeap> mHeap{};
        Microsoft::WRL::ComPtr<ID3D12Resource> mReadback{};

        core::u32 mStride{};
        eastl::vector<SlotState> mStates{};
        core::u32 mActive{ ~0u };
        const rhi::ICommandList* mActiveCommands{};

        rhi::QueryPoolDescription mDescription{};
        rhi::QueryCapabilities mCapabilities{};
    };
}// namespace mikoto::renderer::d3d12
#endif

#endif// MIKOTO_RENDERER_RHI_D3D12_D3D12_QUERY_POOL_HH

