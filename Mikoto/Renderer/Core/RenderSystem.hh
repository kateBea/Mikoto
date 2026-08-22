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

#ifndef MIKOTO_RENDER_SERVICE_HH
#define MIKOTO_RENDER_SERVICE_HH

#include <slang.h>
#include <slang-com-ptr.h>

#include <EASTL/unique_ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Subsystem.hh>

#include <Platform/Window.hh>

#include <Material/ShaderLibrary.hh>

#include <ImGui/ImGuiService.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Core/RenderContext.hh>

namespace mikoto::renderer {

    using namespace mikoto::gui;
    using namespace mikoto::core;
    using namespace mikoto::platform;
    using namespace mikoto::material;
    using namespace mikoto::renderer::rhi;

    struct RenderSystemCreateInfo {
        Window* mWindow{ nullptr };
        RefreshRate mRefreshRate{ RefreshRate::eSync };
        GraphicsAPI mApi{ GraphicsAPI::eVulkan };

        bool mEnableImGui{ false };
    };

    class RenderSystem final : public ISubsystem, public Singleton<RenderSystem> {
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

        ~RenderSystem() override = default;

        auto PrepareFrame() const -> void;
        auto SubmitFrame() -> void;

        // Request presentation toi the submitted window if any
        auto PresentFrame() -> void;

        // Must be called once per frame for now
        auto SetPresentTarget(TextureHandle texture) -> void;

        MKT_NODISCARD auto GetContext() -> RenderContext*;
        MKT_NODISCARD auto GetContext() const -> const RenderContext*;

        MKT_NODISCARD auto GetGpuDevice() -> IGpuDevice*;
        MKT_NODISCARD auto GetGpuDevice() const -> const IGpuDevice*;

        MKT_NODISCARD auto IsApiActive( GraphicsAPI api ) const -> bool;
        MKT_NODISCARD auto GetActiveGraphicsApi() const -> GraphicsAPI;

        MKT_NODISCARD auto GetSlangCurrentSession() const -> Slang::ComPtr<slang::ISession>;

    private:
        auto InitContext() -> void;

        auto InitializeSlang() -> void; 

        auto InitGuiService() -> void;

        auto PrepareSlangForD3D11() -> void;
        auto PrepareSlangForD3D12() -> void;
        auto PrepareSlangForVulkan() -> void;

    private:
        Window* mWindow{ nullptr };
        GraphicsAPI mApi{ GraphicsAPI::eVulkan };
        RefreshRate mRefreshRate{ RefreshRate::eSync };

        eastl::unique_ptr<RenderContext> mContext{};
        eastl::unique_ptr<ImGuiService> mImguiService{};

        // Stores specific configuration, like target
        // configuration (SPIR-V, DXIL, CPU, etc.)
        Slang::ComPtr<slang::ISession> mSlangCurrentSession{};
        Slang::ComPtr<slang::IGlobalSession> mSlangGlobalSession{};

        bool mEnableImGui{ false };
    };
}
#endif //MIKOTO_RENDER_SERVICE_HH
