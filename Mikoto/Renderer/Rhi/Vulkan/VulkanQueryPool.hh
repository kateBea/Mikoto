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

#ifndef MIKOTO_RENDERER_RHI_VULKAN_VULKAN_QUERY_POOL_HH
#define MIKOTO_RENDERER_RHI_VULKAN_VULKAN_QUERY_POOL_HH

#include <EASTL/vector.h>

#include <volk.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Renderer/Rhi/QueryPool.hh>

namespace mikoto::renderer::vulkan {

    class QueryPool final : public rhi::IQueryPool {
        friend class CommandList;

    public:
        QueryPool( const rhi::QueryPoolDescription& description );

        auto SetDebugName( eastl::string_view name ) -> void override;

        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) -> rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object override;

        MKT_NODISCARD auto GetDescription() const -> const rhi::QueryPoolDescription& override;
        MKT_NODISCARD auto GetCapabilities() const -> const rhi::QueryCapabilities& override;
        MKT_NODISCARD auto GetElapsedMilliseconds( core::u64 begin, core::u64 end ) const -> core::f64 override;
        MKT_NODISCARD auto TryGetResult( core::u32 index, const rhi::IFence& completion, core::u64 value, rhi::QueryResult& result ) -> bool override;

        ~QueryPool() override;

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

        auto ReadCompleted( core::u32 index, rhi::QueryResult& result ) -> bool override;
        auto ValidateRange( core::u32 first, core::u32 count ) const -> void override;

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
        VkQueryPool mPool{};

        core::u32 mActive{ ~0u };
        eastl::vector<SlotState> mStates{};

        const rhi::ICommandList* mActiveCommands{};

        rhi::QueryPoolDescription mDescription{};
        rhi::QueryCapabilities mCapabilities{};
    };
}// namespace mikoto::renderer::vulkan

#endif// MIKOTO_RENDERER_RHI_VULKAN_VULKAN_QUERY_POOL_HH

