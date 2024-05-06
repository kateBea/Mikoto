/**
 * MainWindow.cc
 * Created by kate on 5/26/23.
 * */

// C++ Standard Library
#include <any>

// Third-Party Libraries
#include <volk.h>
#include <GLFW/glfw3.h>

// Projects headers
#include <Common/Common.hh>
#include <Common/RenderingUtils.hh>
#include <Common/Types.hh>
#include <Core/Assert.hh>
#include <Core/CoreEvents.hh>
#include <Core/EventManager.hh>
#include <Core/Logger.hh>
#include <Platform/InputManager.hh>
#include <Platform/XPWindow.hh>

namespace Mikoto {
    XPWindow::XPWindow( WindowProperties&& properties )
        :   Window{ std::move(properties) }, m_Window{ nullptr }
    {
        AllowResizing(IsResizable());
    }

    auto XPWindow::Init() -> void {
        MKT_CORE_LOGGER_INFO("Initializing new XPWindow");

        // Initialize GLFW Library
        InitGLFW();

        // Major and minor values for render backend
        UInt32_T major{};
        UInt32_T minor{};

        switch(m_Properties.GetBackend()) {
            case GraphicsAPI::OPENGL_API:
                major = MKT_OPENGL_VERSION_MAJOR;
                minor = MKT_OPENGL_VERSION_MINOR;
                m_Properties.SetTitle(fmt::format("Mikoto (OpenGL Version {}.{}.0)", major, minor));
                break;
            case GraphicsAPI::VULKAN_API:
                major = MKT_VULKAN_VERSION_MAJOR;
                minor = MKT_VULKAN_VERSION_MINOR;
                m_Properties.SetTitle(fmt::format("Mikoto (Vulkan Version {}.{})", major, minor));

                // Because GLFW was originally designed to create an OpenGL context,
                // we need to tell it to not create an OpenGL context with a subsequent call to glfwCreateWindow
                glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
                break;
        }

        // Allow resizing?
        if (IsResizable()) {
            glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        }
        else {
            glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        }

        GLFWWindowCreateSpec spec{};
        spec.Width = GetWidth();
        spec.Height = GetHeight();
        spec.Title = GetTitle();

        m_Window = CreateGLFWWindow(spec);

        MKT_ASSERT(m_Window != nullptr, "Failed to create the Window GLFW");
        MKT_CORE_LOGGER_INFO("Created GLFW Window with name '{}'", GetTitle());
        MKT_CORE_LOGGER_INFO("Created GLFW Window with dim [{}, {}]", GetWidth(), GetHeight());

        SpawnOnCenter();
        InstallCallbacks();
    }


    auto XPWindow::Shutdown() -> void {
        MKT_CORE_LOGGER_INFO("Shutting down GLFW Window with name '{}'", GetTitle());
        MKT_CORE_LOGGER_INFO("GLFW Window dimensions are [{}, {}]", GetWidth(), GetHeight());

        DestroyGLFWWindow(m_Window);
    }


    auto XPWindow::InstallCallbacks() -> void {
        glfwSetWindowUserPointer(m_Window, this);

        glfwSetWindowSizeCallback(m_Window,
            [](GLFWwindow* window, int width, int height) -> void {
                                       XPWindow* data{ static_cast<XPWindow*>(glfwGetWindowUserPointer(window)) };
                data->m_Properties.SetWidth(width);
                data->m_Properties.SetHeight(height);

                EventManager::Trigger<WindowResizedEvent>(width, height);
            }
        );

        glfwSetWindowCloseCallback(m_Window,
            [](MKT_UNUSED_VAR GLFWwindow* window) {
                EventManager::Trigger<WindowCloseEvent>();
            }
        );

        glfwSetKeyCallback(m_Window,
            [](MKT_UNUSED_VAR GLFWwindow* window, int key, MKT_UNUSED_VAR int scancode, int action, int mods) -> void {
                switch (action) {
                    case GLFW_PRESS: {
                        EventManager::Trigger<KeyPressedEvent>(key, false, mods);
                        break;
                    }
                    case GLFW_RELEASE: {
                        EventManager::Trigger<KeyReleasedEvent>(key);
                        break;
                    }
                    case GLFW_REPEAT: {
                        EventManager::Trigger<KeyPressedEvent>(key, true, mods);
                        break;
                    }
                    default: {
                        MKT_CORE_LOGGER_WARN("Unknown Key action for key callback");
                        break;
                    }
                }
            }
        );

        glfwSetMouseButtonCallback(m_Window,
            [](MKT_UNUSED_VAR GLFWwindow* window, int button, int action, int mods) -> void {
                switch (action) {
                    case GLFW_PRESS: {
                        EventManager::Trigger<MouseButtonPressedEvent>(button, mods);
                        break;
                    }
                    case GLFW_RELEASE: {
                        EventManager::Trigger<MouseButtonReleasedEvent>(button);
                        break;
                    }
                    default:
                        MKT_CORE_LOGGER_WARN("Unknown GLFW_ value for glfwSetMouseButtonCallback");
                        break;
                }
            }
        );

        glfwSetScrollCallback(m_Window,
            [](MKT_UNUSED_VAR GLFWwindow* window, double xOffset, double yOffset) -> void {
                EventManager::Trigger<MouseScrollEvent>(xOffset, yOffset);
            }
        );

        glfwSetCursorPosCallback(m_Window,
            [](MKT_UNUSED_VAR GLFWwindow* window, double x, double y) -> void {
                EventManager::Trigger<MouseMovedEvent>(x, y);
            }
        );

        glfwSetCharCallback(m_Window,
            [](MKT_UNUSED_VAR GLFWwindow* window, unsigned int codePoint) -> void {
                EventManager::Trigger<KeyCharEvent>(codePoint);
            }
        );

        glfwSetWindowFocusCallback(m_Window,
            [](GLFWwindow* window, int focus) -> void {
                const XPWindow* data{ static_cast<XPWindow*>(glfwGetWindowUserPointer(window)) };
                if (focus == GLFW_TRUE) {
                    InputManager::SetFocus(data);
                }
            });
    }


    auto XPWindow::SpawnOnCenter() const -> void {
#if !defined(NDEBUG)
        Int32_T count{};
        glfwGetMonitors(std::addressof(count));
        MKT_CORE_LOGGER_INFO("Number of available monitors: {}", count);
#endif
        // See: https://www.glfw.org/docs/3.3/monitor_guide.html
        // The primary monitor is returned by glfwGetPrimaryMonitor. It is the user's
        // preferred monitor and is usually the one with global UI elements like task bar or menu bar.
        Int32_T monitorWidth{};
        Int32_T monitorHeight{};
        GLFWmonitor* primary{ glfwGetPrimaryMonitor() };

        glfwGetMonitorWorkarea(primary, nullptr, nullptr, std::addressof(monitorWidth), std::addressof(monitorHeight));
        glfwSetWindowPos(m_Window, monitorWidth / 10, monitorHeight / 10);
    }


    auto XPWindow::InitGLFW() -> void {
        if (!s_GLFWInitSuccess) {
            auto ret{ glfwInit() };
            MKT_ASSERT(ret == GLFW_TRUE, "Failed to initialized the GLFW library");

            s_GLFWInitSuccess = true;
            glfwSetErrorCallback([](Int32_T errCode, const char* desc) -> void {
                    MKT_CORE_LOGGER_ERROR("GLFW error code: {} Description: {}", errCode, desc);
                }
            );
        }
    }


    auto XPWindow::ProcessEvents() -> void {
        glfwPollEvents();
    }


    auto XPWindow::DestroyGLFWWindow(GLFWwindow* window) -> void {
        // Everytime we shut down a GLFW window, we decrease the number
        // of active windows, the last GLFW window to be shutdown calls glfwTerminate()
        glfwDestroyWindow(window);
        s_WindowsCount -= 1;

        if (s_WindowsCount == 0) {
            glfwTerminate();
        }
    }


    auto XPWindow::CreateGLFWWindow(const GLFWWindowCreateSpec& spec) -> GLFWwindow* {
        GLFWwindow* window{ glfwCreateWindow(spec.Width, spec.Height, spec.Title.data(), nullptr, nullptr) };
        s_WindowsCount += 1;

        return window;
    }
}