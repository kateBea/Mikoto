//
// Created by kate on 6/4/23.
//

// C++ Standard Library
#include <any>

// Third-Party Libraries
#include "GL/glew.h"
#include "GLFW/glfw3.h"

// Project Headers
#include "../Common/Common.hh"

#include "Core/Assert.hh"
#include "Core/Logger.hh"

#include "Renderer/OpenGL/OpenGLContext.hh"

namespace Mikoto::OpenGLContext {

    auto Init(const std::shared_ptr<Window>& windowHandle) -> void {
        try {
            // We expect the native window for Linux Window to be a pointer to a GLFW window
            s_Handle = std::any_cast<GLFWwindow*>(windowHandle->GetNativeWindow());
            MKT_ASSERT(s_Handle, "Window handle for OpenGL context initialization is NULL");
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, MKT_OPENGL_VERSION_MAJOR);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, MKT_OPENGL_VERSION_MINOR);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

            glfwMakeContextCurrent(s_Handle);
            glewExperimental = GL_TRUE;

            // Using temporal variable because MKT_CORE_LOGGER_ERROR gets stripped
            // in non-DEBUG builds, so glewInit() would not be executed
            s_GLEWInitSuccess = glewInit() == GLEW_OK;
            MKT_ASSERT(s_GLEWInitSuccess, "Failed to initialize GLEW");

            MKT_CORE_LOGGER_INFO("OpenGL Target Version:  {}.{}", MKT_OPENGL_VERSION_MAJOR, MKT_OPENGL_VERSION_MINOR);
            MKT_CORE_LOGGER_INFO("OpenGL Drivers Version {}", (const char*)glGetString(GL_VERSION));
            MKT_CORE_LOGGER_INFO("OpenGL Vendor {}", (const char*)glGetString(GL_VENDOR));
            MKT_CORE_LOGGER_INFO("OpenGL Device {}", (const char*)glGetString(GL_RENDERER));
        }
        catch (const std::bad_any_cast& exception) {
            MKT_APP_LOGGER_ERROR("Exception thrown std::any_cast at OpenGLContext::Init(). What: {}", exception.what());
        }
    }

    auto EnableVSync() -> void {
        glfwSwapInterval(1);
        s_VSync = true;
    }

    auto DisableVSync() -> void {
        glfwSwapInterval(0);
        s_VSync = false;
    }

    auto Shutdown() -> void {

    }

    auto Present() -> void {
        glfwSwapBuffers(s_Handle);
    }

    auto IsVSyncActive() -> bool {
        return s_VSync;
    }
}
