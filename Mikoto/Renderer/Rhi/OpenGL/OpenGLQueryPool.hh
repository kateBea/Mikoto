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

#ifndef MIKOTO_RENDERER_RHI_OPENGL_OPENGL_QUERY_POOL_HH
#define MIKOTO_RENDERER_RHI_OPENGL_OPENGL_QUERY_POOL_HH

#include <Renderer/Rhi/OpenGL/OpenGLDevice.hh>

namespace mikoto::renderer::opengl {

    class QueryPool final : public renderer::rhi::IQueryPool {
        friend class CommandList;

    public:
        QueryPool( const renderer::rhi::QueryPoolDescription &desc, eastl::shared_ptr<DeviceState> state );

        MKT_NODISCARD auto GetDescription() const -> const rhi::QueryPoolDescription & override;
        MKT_NODISCARD auto GetCapabilities() const -> const rhi::QueryCapabilities & override;
        MKT_NODISCARD auto GetElapsedMilliseconds( core::u64 begin, core::u64 end ) const -> core::f64 override;

        MKT_NODISCARD auto TryGetResult( core::u32 index, const rhi::IFence &completion, core::u64 value, rhi::QueryResult &result ) -> bool override;

        static auto Capabilities( renderer::rhi::QueryType type, renderer::rhi::QueueType queue ) -> renderer::rhi::QueryCapabilities;

        ~QueryPool() override;

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

        auto ReadCompleted( core::u32 index, renderer::rhi::QueryResult &result ) -> bool override;

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
        auto ValidateRange( core::u32 first, core::u32 count ) const -> void override;

    private:
        eastl::shared_ptr<DeviceState> mState{};

        eastl::vector<GLuint> mQueries{};
        eastl::vector<GLenum> mTargets{};
        eastl::vector<core::u32> mResultIndices{};
        eastl::vector<SlotState> mSlots{};

        core::u32 mActive{ ~0u };
        const rhi::ICommandList *mActiveCommands{};

        rhi::QueryPoolDescription mDescription{};
        rhi::QueryCapabilities mCapabilities{};
    };
}// namespace mikoto::renderer::opengl

#endif// MIKOTO_RENDERER_RHI_OPENGL_OPENGL_QUERY_POOL_HH
