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

#ifndef MIKOTO_RENDERER_RHI_OPENGL_OPENGL_CONTEXT_HH
#define MIKOTO_RENDERER_RHI_OPENGL_OPENGL_CONTEXT_HH

#include <mutex>

#include <EASTL/vector.h>
#include <EASTL/functional.h>

#include <Renderer/Core/RenderContext.hh>

namespace mikoto::platform {
    class IOpenGLWindowContext;
}

namespace mikoto::renderer::opengl {
    class Context final : public RenderContext {
    public:
        explicit Context( const RenderContextCreateInfo &info );

        auto Init() -> bool override;
        auto Shutdown() -> void override;

        auto SubmitFrame() -> void override;
        auto PrepareFrame() -> void override;

        auto Update() -> void override;
        auto Present() -> void override;

        auto SetRefreshRate( rhi::RefreshRate rate ) -> void override;
        auto SetPresentTarget( rhi::TextureHandle texture ) -> void override;

        auto BatchSubmission( rhi::SubmitInfo &&info, rhi::QueueType queue ) -> void override;

        auto SetOverlay( eastl::function<void()> draw ) -> void;

        /** @brief Restores the main window's GL context on the render thread. */
        auto MakeCurrent() -> void;

    private:
        struct Submission {
            rhi::SubmitInfo mInfo{};
            rhi::QueueType mQueue{};
        };


    private:
        // [Internal usage]
        auto SetupDebugCallback() -> void;

    private:
        std::mutex mMutex{};

        eastl::vector<Submission> mSubmissions{};
        eastl::function<void()> mOverlay{};

        platform::IOpenGLWindowContext* mWindowContext{};
        rhi::TextureHandle mPresentTarget{};
        core::u32 mPresentFramebuffer{};
    };
}// namespace mikoto::renderer::opengl

#endif// MIKOTO_RENDERER_RHI_OPENGL_OPENGL_CONTEXT_HH
