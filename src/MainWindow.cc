/**
 * MainWindow.cc
 * Created by kate on 5/26/23.
 * */

// C++ Standard Library
#include <any>
#include <memory>

// Third-Party Libraries
#include <volk.h>
#include <GLFW/glfw3.h>

// Projects headers
#include <Utility/Common.hh>
#include <Core/Logger.hh>
#include <Core/Assert.hh>
#include <Core/Events/AppEvents.hh>
#include <Core/Events/KeyEvents.hh>
#include <Core/Events/MouseEvents.hh>
#include <Renderer/Renderer.hh>
#include <Renderer/RenderContext.hh>
#include <Platform/Window/MainWindow.hh>

namespace Mikoto {
    MainWindow::MainWindow(const WindowProperties& properties)
        :   Window{ properties }, m_Window{ nullptr }, m_Callback{} {}

    auto MainWindow::OnUpdate() -> void {
        glfwPollEvents();

        // not be done when the window is minimized, it CPU waste
        RenderContext::Present();
    }

    auto MainWindow::Init() -> void {
        KATE_CORE_LOGGER_INFO("Main Window initialization");
        InitGLFW();

        switch(Renderer::GetActiveGraphicsAPI()) {
            case Renderer::GraphicsAPI::OPENGL_API:
                m_CurrentGraphicsAPIIsOpenGL = true;
                m_Properties.SetTitle(fmt::format("Mikoto (OpenGL Version {}.{}.0)", KT_OPENGL_VERSION_MAJOR, KT_OPENGL_VERSION_MINOR));
                break;
            case Renderer::GraphicsAPI::VULKAN_API:
                UInt32_T major{ KT_VULKAN_VERSION_MAJOR };
                UInt32_T minor{ KT_VULKAN_VERSION_MINOR };
                m_Properties.SetTitle(fmt::format("Mikoto (Vulkan Version {}.{})", major, minor));
                break;
        }

        KATE_CORE_LOGGER_INFO("Creating Window GLFW. Name '{}'. Dimensions [{}, {}]",
                              m_Properties.GetName(), m_Properties.GetWidth(), m_Properties.GetHeight());

        if (!m_CurrentGraphicsAPIIsOpenGL) {
            // Because GLFW was originally designed to create an OpenGL context,
            // we need to tell it to not create an OpenGL context with a subsequent call
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        }

        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        
        m_Window = glfwCreateWindow(m_Properties.GetWidth(), m_Properties.GetHeight(), m_Properties.GetName().c_str(), nullptr, nullptr);
        m_WindowCreateSuccess = m_Window != nullptr;
        KT_ASSERT(m_WindowCreateSuccess, "Failed to create the Window GLFW");

        SpawnOnCenter();
        SetCallbacks();
    }

    auto MainWindow::ShutDown() -> void {
        KATE_CORE_LOGGER_DEBUG("Shutting down Window GLFW. Name '{}'. Dimensions [{}, {}]",
                               m_Properties.GetName(), m_Properties.GetWidth(), m_Properties.GetHeight());

        // Might have an internal window counter If we want to spawn multiple windows,
        // so the last one alive shuts down the GLFW library
        glfwDestroyWindow(m_Window);
        glfwTerminate();
    }

    auto MainWindow::SetCallbacks() -> void {
        glfwSetWindowUserPointer(m_Window, this);

        glfwSetWindowSizeCallback(m_Window,
            [](GLFWwindow* window, Int32_T width, Int32_T height) {
                                      MainWindow* data{static_cast<MainWindow *>(glfwGetWindowUserPointer(window)) };
                                      data->m_Properties.SetWidth(width);
                                      data->m_Properties.SetHeight(height);

                WindowResizedEvent wre{width, height};
                data->m_Callback(wre);
            }
        );

        glfwSetWindowCloseCallback(m_Window,
            [](GLFWwindow* window) {
                                       MainWindow * data{static_cast<MainWindow *>(glfwGetWindowUserPointer(window)) };
                WindowCloseEvent wce{};
                data->m_Callback(wce);
            }
        );

        glfwSetKeyCallback(m_Window,
            [](GLFWwindow *window, std::int32_t key, [[maybe_unused]] Int32_T  scancode, Int32_T action, Int32_T mods) {
                               MainWindow * data{ static_cast<MainWindow *>(glfwGetWindowUserPointer(window)) };

                switch (action) {
                    case GLFW_PRESS: {
                        KeyPressedEvent kpeNoRepeat{ key, false, mods };
                        data->m_Callback(kpeNoRepeat);
                        break;
                    }
                    case GLFW_RELEASE: {
                        KeyReleasedEvent kre{ key };
                        data->m_Callback(kre);
                        break;
                    }
                    case GLFW_REPEAT: {
                        KeyPressedEvent kpeRepeat{ key, true, mods };
                        data->m_Callback(kpeRepeat);
                        break;
                    }
                    default: {
                        KATE_CORE_LOGGER_WARN("Unknown Key action for key callback");
                        break;
                    }
                }
            }
        );

        glfwSetMouseButtonCallback(m_Window,
            [](GLFWwindow* window, Int32_T button, Int32_T action, Int32_T mods) {
                                       MainWindow * data{static_cast<MainWindow *>(glfwGetWindowUserPointer(window)) };

                switch (action) {
                    case GLFW_PRESS: {
                        MouseButtonPressedEvent mousePressed{ button, mods };
                        data->m_Callback(mousePressed);
                        break;
                    }
                    case GLFW_RELEASE: {
                        MouseButtonReleasedEvent mouseReleased{ button };
                        data->m_Callback(mouseReleased);
                        break;
                    }
                }
            }
        );

        glfwSetScrollCallback(m_Window,
            [](GLFWwindow* window, double xOffset, double yOffset) {
                                  MainWindow * data{static_cast<MainWindow *>(glfwGetWindowUserPointer(window)) };
                MouseScrollEvent msc{ xOffset, yOffset };
                data->m_Callback(msc);
            }
        );

        glfwSetCursorPosCallback(m_Window,
            [](GLFWwindow* window, double x, double y) {
                                     MainWindow * data{static_cast<MainWindow *>(glfwGetWindowUserPointer(window)) };
                MouseMovedEvent mme{x, y};
                data->m_Callback(mme);
            }
        );

        glfwSetCharCallback(m_Window,
            [](GLFWwindow* window, unsigned int codePoint) {
                                MainWindow * data{static_cast<MainWindow *>(glfwGetWindowUserPointer(window)) };
                KeyCharEvent kce{ codePoint };
                data->m_Callback(kce);
            }
        );
    }

    auto MainWindow::SpawnOnCenter() const -> void {
#if !defined(NDEBUG)
        Int32_T count{};
        KATE_CORE_LOGGER_INFO("Number of available monitors: {}", count);
#endif
        // See: https://www.glfw.org/docs/3.3/monitor_guide.html
        // The primary monitor is returned by glfwGetPrimaryMonitor. It is the user's
        // preferred monitor and is usually the one with global UI elements like task bar or menu bar.
        Int32_T monitorWidth{};
        Int32_T monitorHeight{};
        GLFWmonitor* primary{ glfwGetPrimaryMonitor() };
        glfwGetMonitorWorkarea(primary, nullptr, nullptr, &monitorWidth, &monitorHeight);
        glfwSetWindowPos(m_Window, monitorWidth / 10, monitorHeight / 10);
    }

    auto MainWindow::InitGLFW() -> void {
        if (!g_GLFWInitSuccess) {
            auto ret{ glfwInit() };
            KT_ASSERT(ret == GLFW_TRUE, "Failed to initialized the GLFW library");

            g_GLFWInitSuccess = true;
            glfwSetErrorCallback([](std::int32_t errCode, const char* desc) -> void {
                    KATE_CORE_LOGGER_ERROR("GLFW error code: {} Description: {}", errCode, desc);
                }
            );
        }
    }

    auto MainWindow::CreateWindowSurface(VkInstance instance, VkSurfaceKHR* surface) -> void {
        if (glfwCreateWindowSurface(instance, m_Window, nullptr, surface) != VK_SUCCESS)
            throw std::runtime_error("Failed to create Vulkan Surface");
    }
}