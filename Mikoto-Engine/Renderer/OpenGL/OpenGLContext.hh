/**
 * OpenFLContext.hh
 * Created by kate on 6/4/23.
 * */

#ifndef MIKOTO_OPENGL_CONTEXT_HH
#define MIKOTO_OPENGL_CONTEXT_HH

// C++ Standard Library
#include <any>

// Project Headers
#include "Platform/GlfwWindow.hh"
#include "Renderer/RenderContext.hh"

namespace Mikoto::OpenGLContext {
    /*************************************************************
    * CONTEXT DATA
    * ***********************************************************/
    inline GLFWwindow* s_Handle{};
    inline bool s_GLEWInitSuccess{ false };
    inline bool s_VSync{};

    /*************************************************************
    * CONTEXT INTERFACE
    * ***********************************************************/
    auto Init(const std::shared_ptr<Window>& windowHandle) -> void;
    auto Shutdown() -> void;
    auto Present() -> void;
    auto EnableVSync() -> void;
    auto DisableVSync() -> void;
    auto IsVSyncActive() -> bool;
}


#endif // MIKOTO_OPENGL_CONTEXT_HH
