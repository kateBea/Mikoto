//
// Created by zanet on 1/26/2025.
//

#ifndef RENDERSYSTEM_HH
#define RENDERSYSTEM_HH

#include <Core/Engine.hh>
#include <Renderer/Core/RenderContext.hh>
#include <Platform/Window/Window.hh>
#include <Renderer/Core/RendererBackend.hh>

namespace Mikoto {
    class RenderSystem final : public IEngineSystem {
    public:
        explicit RenderSystem() = default;
        explicit RenderSystem(const EngineConfig& options) {
            m_Options = options;
        }

        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto Update() -> void override;

        ~RenderSystem() override = default;

        auto PrepareFrame() const -> void;
        auto EndFrame() const -> void;

        MKT_NODISCARD auto GetContext() -> RenderContext* { return m_Context.get(); }
        MKT_NODISCARD auto GetContext() const -> const RenderContext* { return m_Context.get(); }
        MKT_NODISCARD auto GetDefaultApi() const -> GraphicsAPI { return m_Context->GetContextData().GraphicsAPI; }

    private:
        auto Flush() const -> void;

    private:
        EngineConfig m_Options{};
        Scope_T<RenderContext> m_Context{ nullptr };
    };
}
#endif //RENDERSYSTEM_HH
