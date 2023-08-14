//
// Created by kate on 6/4/23.
//

#ifndef KATE_ENGINE_RENDER_CONTEXT_HH
#define KATE_ENGINE_RENDER_CONTEXT_HH

#include <any>
#include <memory>

#include <Renderer/Renderer.hh>

#include <Platform/Window/Window.hh>

namespace Mikoto {
    class RenderContext {
    public:
        explicit RenderContext() = default;

        static auto Init(std::shared_ptr<Window> windowHandle) -> void;
        static auto ShutDown() -> void;
        static auto Present() -> void;

        static auto EnableVSync() -> void;
        static auto DisableVSync() -> void;
        static auto IsVSyncActive() -> bool;

        virtual ~RenderContext() = default;
    public:
        RenderContext(const RenderContext&) = delete;
        auto operator=(const RenderContext&) -> RenderContext& = delete;

        RenderContext(RenderContext&&) = delete;
        auto operator=(RenderContext&&) -> RenderContext& = delete;

    private:
        inline static Renderer::GraphicsAPI s_ActiveAPI{ Renderer::GetActiveGraphicsAPI() };
        inline static std::shared_ptr<Window> s_WindowHandle{};
    };
}


#endif//KATE_ENGINE_RENDER_CONTEXT_HH
