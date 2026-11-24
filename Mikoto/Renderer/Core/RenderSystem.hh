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

#ifndef MIKOTO_RENDERER_CORE_RENDER_SYSTEM_HH
#define MIKOTO_RENDERER_CORE_RENDER_SYSTEM_HH

#include <mutex>
#include <condition_variable>

#include <EASTL/vector.h>
#include <EASTL/unique_ptr.h>

#include <slang.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Service.hh>

#include <Platform/Window.hh>

#include <Assets/ShaderLibrary.hh>

#include <Renderer/Video/Video.hh>
#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Core/RenderContext.hh>
#include <Renderer/Core/SlangCompiler.hh>

#include <ImGui/ImGuiService.hh>

namespace mikoto::renderer {

    class SlangSessionLease;

    struct RenderSystemCreateInfo {
        platform::IWindow* mWindow{ nullptr };
        renderer::rhi::RefreshRate mRefreshRate{ renderer::rhi::RefreshRate::eSync };
        renderer::rhi::GraphicsAPI mApi{ renderer::rhi::GraphicsAPI::eVulkan };

        bool mEnableImGui{ false };
    };

    class RenderSystem final : public core::ISubsystem, public core::Singleton<RenderSystem> {
    public:
        explicit RenderSystem(const RenderSystemCreateInfo& options);

        auto Initialize() -> void override;
        auto Shutdown() -> void override;
        auto Update(float ts) -> void override;

        // The render system allows for command batching, clients can batch commands alongside a fence
        // to signal on completion to one of main device queues to minimize API calls to queue
        // submission for tasks that do not need to be executed in parallel during the frame.
        // Commands are all submitted after a call to SubmitFrame()
        auto BatchSubmission( rhi::SubmitInfo&& submitInfo, rhi::QueueType queue ) -> void;

        auto PrepareFrame() -> void;
        MKT_NODISCARD auto AcquireVideoBitstream( const core::Ref<asset::VideoClip>& clip,
            rhi::IVideoDecoder* decoder ) -> core::Ref<VideoBitstream>;
        auto SubmitFrame() -> void;

        // Request presentation to the submitted window if any
        auto PresentFrame() -> void;

        // Must be called once per frame for now
        auto SetPresentTarget( renderer::rhi::TextureHandle texture ) -> void;

        MKT_NODISCARD auto GetContext() -> RenderContext*;
        MKT_NODISCARD auto GetContext() const -> const RenderContext*;

        MKT_NODISCARD auto GetGpuDevice() -> renderer::rhi::IGpuDevice*;
        MKT_NODISCARD auto GetGpuDevice() const -> const renderer::rhi::IGpuDevice*;

        MKT_NODISCARD auto IsApiActive( renderer::rhi::GraphicsAPI api ) const -> bool;
        MKT_NODISCARD auto GetActiveGraphicsApi() const -> renderer::rhi::GraphicsAPI;

        /** Waits for an available slot. Compilation runs without the pool lock. */
        MKT_NODISCARD auto AcquireSlangSession() -> SlangSessionLease;

        auto PrepareSlangSession( SlangCompilerContext &context ) -> void;
        auto ReleaseSlangSession( SlangCompilerContext &context ) -> void;

    private:
        auto InitContext() -> void;
        auto InitializeSlang() -> void; 
        auto InitGuiService() -> void;

    private:
        platform::IWindow* mWindow{ nullptr };
        renderer::rhi::GraphicsAPI mApi{ renderer::rhi::GraphicsAPI::eVulkan };
        renderer::rhi::RefreshRate mRefreshRate{ renderer::rhi::RefreshRate::eSync };

        eastl::unique_ptr<RenderContext> mContext{};
        ankerl::unordered_dense::map<const asset::VideoClip*, core::Ref<VideoBitstream>> mVideoBitstreams{};
        eastl::unique_ptr<imgui::ImGuiService> mImguiService{};

        // Shared Slang object graphs are non-reentrant. Every leased slot owns
        // its own global session, including the normal and precise vertex sessions.
        // https://docs.shader-slang.org/en/latest/external/slang/docs/user-guide/08-compiling.html#multithreading
        eastl::vector<eastl::unique_ptr<SlangCompilerContext>> mSlangCompilers{};

        std::mutex mSlangMutex{};
        core::usize mSlangSessionLimit{ 1 };
        std::condition_variable mSlangAvailable{};

        bool mSlangStopping{ true };
        bool mEnableImGui{ false };
    };

}

#endif// MIKOTO_RENDERER_CORE_RENDER_SYSTEM_HH

