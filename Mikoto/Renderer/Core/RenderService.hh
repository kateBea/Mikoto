//    Copyright 2025 ケイト
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

#include <Common/Common.hh>
#include <Common/Subsystem.hh>
#include <Material/ShaderLibrary.hh>
#include <Platform/Window.hh>

#include <Assets/Texture.hh>
#include <ImGui/ImGuiService.hh>
#include <Renderer/Core/GpuDevice.hh>
#include <Renderer/Core/GraphicsContext.hh>
#include <Renderer/Core/RenderUtility.hh>

namespace Mikoto {

    struct RenderContextCreateInfo {
        GraphicsAPI Api{ GraphicsAPI::UNKNOWN };
        const Window* TargetWindow{ nullptr };
    };

    class RenderContext {
    public:
        virtual ~RenderContext() = default;

        virtual auto Init() -> bool = 0;
        virtual auto Shutdown() -> void = 0;

        virtual auto SubmitFrame() -> void = 0;
        virtual auto PrepareFrame() -> void = 0;

        virtual auto Update() -> void = 0;

        virtual auto Present() -> void = 0;

        virtual auto SetPresentTarget(TextureHandle texture) -> void = 0;


        MKT_NODISCARD auto GetGpuDevice() -> GpuDevice* { return m_Device.get(); }
        MKT_NODISCARD auto GetGpuDevice() const -> const GpuDevice* { return m_Device.get(); }

        virtual auto EnableVSync() -> void = 0;
        virtual auto DisableVSync() -> void = 0;

        static auto Create(const RenderContextCreateInfo& config) -> Unique<RenderContext>;

    protected:

        explicit RenderContext(const RenderContextCreateInfo& createInfo)
            :   m_TargetWindow{ createInfo.TargetWindow }
        { }

        Unique<GpuDevice> m_Device{ nullptr };

        const Window* m_TargetWindow{ nullptr };
    };

    struct RenderServiceCreateInfo {
        Window* TargetWindow{ nullptr };
        GraphicsAPI RendererAPI{ GraphicsAPI::VULKAN_API };

        bool EnableImGui{ true };
    };

    class RenderService final : public Subsystem, public Singleton<RenderService> {
    public:
        explicit RenderService(const RenderServiceCreateInfo& options);

        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto Update(float ts) -> void override;

        ~RenderService() override = default;

        auto PrepareFrame() const -> void;
        auto EndFrame() -> void;

        // Request presentation toi the submitted window if any
        auto PresentFrame() -> void;

        // Must be called once per frame for now
        auto SetPresentTarget(TextureHandle texture) -> void;

        MKT_NODISCARD auto GetContext() -> RenderContext* { return m_Context.get(); }
        MKT_NODISCARD auto GetContext() const -> const RenderContext* { return m_Context.get(); }

        MKT_NODISCARD auto GetGpuDevice() -> GpuDevice* { return m_Context->GetGpuDevice(); }
        MKT_NODISCARD auto GetGpuDevice() const -> const GpuDevice* { return m_Context->GetGpuDevice(); }

        MKT_NODISCARD auto IsGraphicsActive( GraphicsAPI api ) const -> bool;
        MKT_NODISCARD auto GetActiveGraphicsApi() const -> GraphicsAPI { return m_ActiveAPI; }

    private:
        auto InitContext() -> void;
        auto InitShaderLibrary() -> void;

        auto InitGuiService() -> void;

    private:
        RenderServiceCreateInfo m_Options{};

        Unique<RenderContext> m_Context{};

        // Move to the graphics context
        Unique<ShaderLibrary> m_ShaderLibrary{};

        Unique<ImGuiService> m_ImguiService{};

        GraphicsAPI m_ActiveAPI{ GraphicsAPI::VULKAN_API };
    };
}
#endif //MIKOTO_RENDER_SERVICE_HH
