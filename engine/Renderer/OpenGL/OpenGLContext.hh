/**
 * OpenFLContext.hh
 * Created by kate on 6/4/23.
 * */

#ifndef MIKOTO_OPENGL_CONTEXT_HH
#define MIKOTO_OPENGL_CONTEXT_HH

// C++ Standard Library
#include <any>

// Project Headers
#include <Platform/Window/MainWindow.hh>
#include <Renderer/RenderContext.hh>

// TODO: convert into namespace
namespace Mikoto {
    class OpenGLContext {
    public:
        static auto Init(const std::shared_ptr<Window>& windowHandle) -> void;
        static auto ShutDown() -> void;
        static auto Present() -> void;

        static auto EnableVSync() -> void;
        static auto DisableVSync() -> void;
        static auto IsVSyncActive() -> bool { return s_VSync; }

    public:
        // Forbidden operations on Contexts
        OpenGLContext(const OpenGLContext&) = delete;
        auto operator=(const OpenGLContext&) -> OpenGLContext& = delete;

        OpenGLContext(OpenGLContext&&) = delete;
        auto operator=(OpenGLContext&&) -> OpenGLContext& = delete;

    private:
        inline static GLFWwindow* s_Handle{};
        inline static bool s_GLEWInitSuccess{ false };
        inline static bool s_VSync{};
    };
}


#endif // MIKOTO_OPENGL_CONTEXT_HH
