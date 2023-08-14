//
// Created by kate on 6/4/23.
//

// C++ Standard Library
#include <any>

// Third-Party Libraries
#include <GL/glew.h>
#include <GLFW/glfw3.h>

// Project Headers
#include <Utility/Common.hh>

#include <Core/Assert.hh>
#include <Core/Logger.hh>

#include <Renderer/OpenGL/OpenGLContext.hh>

namespace Mikoto {

    auto OpenGLContext::Init(const std::shared_ptr<Window>& windowHandle) -> void {
        try {
            // We expect the native window for Linux Window to be a pointer to a GLFW window
            s_Handle = std::any_cast<GLFWwindow*>(windowHandle->GetNativeWindow());
            KT_ASSERT(s_Handle, "Window handle for OpenGL context initialization is NULL");
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, KT_OPENGL_VERSION_MAJOR);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, KT_OPENGL_VERSION_MINOR);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

            glfwMakeContextCurrent(s_Handle);
            glewExperimental = GL_TRUE;

            // Using temporal variable because KATE_CORE_LOGGER_ERROR gets stripped
            // in non-DEBUG builds, so glewInit() would not be executed
            s_GLEWInitSuccess = glewInit() == GLEW_OK;
            KT_ASSERT(s_GLEWInitSuccess, "Failed to initialize GLEW");

            KATE_CORE_LOGGER_INFO("OpenGL target  {}.{}", KT_OPENGL_VERSION_MAJOR, KT_OPENGL_VERSION_MINOR);
            KATE_CORE_LOGGER_INFO("OpenGL available {}", (const char*)glGetString(GL_VERSION));
            KATE_CORE_LOGGER_INFO("OpenGL vendor {}", (const char*)glGetString(GL_VENDOR));
            KATE_CORE_LOGGER_INFO("OpenGL renderer {}", (const char*)glGetString(GL_RENDERER));

        }
        catch (const std::bad_any_cast& exception) {
            KATE_APP_LOGGER_ERROR("Exception thrown std::any_cast at OpenGLContext::Init(). What: {}", exception.what());
        }
    }

    auto OpenGLContext::EnableVSync() -> void {
        glfwSwapInterval(1);
        s_VSync = true;
    }

    auto OpenGLContext::DisableVSync() -> void {
        glfwSwapInterval(0);
        s_VSync = false;
    }

    auto OpenGLContext::ShutDown() -> void {

    }

    auto OpenGLContext::Present() -> void {
        glfwSwapBuffers(s_Handle);
    }
}
