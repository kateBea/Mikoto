/**
 * RenderContext.hh
 * Created by kate on 6/4/23.
 * */

#ifndef MIKOTO_RENDER_CONTEXT_HH
#define MIKOTO_RENDER_CONTEXT_HH

// C++ Standard Library
#include <any>
#include <memory>

// Project Headers
#include "Platform/Window.hh"
#include <Renderer/Renderer.hh>
#include <Renderer/RenderingUtilities.hh>
#include <Utility/Common.hh>

namespace Mikoto {
    struct RenderContextSpec {
        GraphicsAPI Backend{};
        std::shared_ptr<Window> WindowHandle{};
    };

    class RenderContext {
    public:
        static auto Init(RenderContextSpec&& spec) -> void;
        static auto ShutDown() -> void;
        static auto Present() -> void;

        static auto EnableVSync() -> void;
        static auto DisableVSync() -> void;

        MKT_UNUSED_FUNC static auto IsVSyncActive() -> bool;

    public:
        DISABLE_COPY_AND_MOVE_FOR(RenderContext);

    private:
        inline static RenderContextSpec s_Spec{};
    };
}

#endif // MIKOTO_RENDER_CONTEXT_HH
