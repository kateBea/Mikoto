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
#include <Core/Logging/Assert.hh>
#include <Core/Events/CoreEvents.hh>
#include <Core/Logging/Logger.hh>
#include <Platform/Window/MainWindow.hh>
#include <Renderer/Vulkan/VulkanHelpers.hh>
#include <Library/Utility/Types.hh>
#include <Core/System/EventSystem.hh>
#include <Core/System/InputSystem.hh>
#include <Core/Engine.hh>

namespace Mikoto {
    MainWindow::MainWindow(const WindowProperties& properties )
        :   Window{ properties }
    {
        AllowResizing(IsResizable());
    }

    auto MainWindow::Init() -> void {
        MKT_CORE_LOGGER_INFO("MainWindow::Init - Initializing new GLFW Window.");

        // Initialize GLFW Library
        InitGLFW();

        // Major and minor values for render backend
        UInt32_T major{};
        UInt32_T minor{};

        switch(m_Properties.Backend) {
            case GraphicsAPI::VULKAN_API:
                major = MKT_VULKAN_VERSION_MAJOR;
                minor = MKT_VULKAN_VERSION_MINOR;
                m_Properties.Title = fmt::format("Mikoto (Vulkan Version {}.{})", major, minor);

                // Because GLFW was originally designed to create an OpenGL context,
                // we need to tell it to not create an OpenGL context with a later call to glfwCreateWindow
                glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
                break;
            default:;
        }

        // Allow resizing?
        if (IsResizable()) {
            glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        }
        else {
            glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        }

        MainWindowCreateSpec spec{
            .Width{ GetWidth() },
            .Height{ GetHeight() },
            .Title{ GetTitle() },
        };

        m_Window = Create(spec);

        MKT_ASSERT(m_Window != nullptr, "XPWindow::Init - Failed to create the Window GLFW");
        MKT_CORE_LOGGER_INFO("Created GLFW Window with name '{}' and Dimensions [{}, {}]", GetTitle(), GetWidth(), GetHeight());

        MoveToMonitorCenter();

        InstallCallbacks();
    }


    auto MainWindow::Shutdown() -> void {
        MKT_CORE_LOGGER_INFO("Shutting down GLFW Window with name '{}'", GetTitle());
        MKT_CORE_LOGGER_INFO("GLFW Window dimensions are [{}, {}]", GetWidth(), GetHeight());

        DestroyGLFWWindow(m_Window);
    }


    auto MainWindow::InstallCallbacks() -> void {
        glfwSetWindowUserPointer(m_Window, this);

        glfwSetWindowSizeCallback(m_Window,
            [](GLFWwindow* window, int width, int height) -> void {
                const auto data{ static_cast<MainWindow*>(glfwGetWindowUserPointer(window)) };
                data->m_Properties.Width = width;
                data->m_Properties.Height = height;

                auto& eventSystem{ Engine::GetSystem<EventSystem>() };
                eventSystem.Trigger<WindowResizedEvent>(width, height);
            }
        );

        glfwSetWindowCloseCallback(m_Window,
            [](MKT_UNUSED_VAR GLFWwindow* window) {
                auto& eventSystem{ Engine::GetSystem<EventSystem>() };
                eventSystem.Trigger<WindowCloseEvent>();
            }
        );

        glfwSetKeyCallback(m_Window,
            [](MKT_UNUSED_VAR GLFWwindow* window, int key, MKT_UNUSED_VAR int scancode, int action, int mods) -> void {
                auto& eventSystem{ Engine::GetSystem<EventSystem>() };
                switch (action) {
                    case GLFW_PRESS: {
                        eventSystem.Trigger<KeyPressedEvent>(key, false, mods);
                        break;
                    }
                    case GLFW_RELEASE: {
                        eventSystem.Trigger<KeyReleasedEvent>(key);
                        break;
                    }
                    case GLFW_REPEAT: {
                        eventSystem.Trigger<KeyPressedEvent>(key, true, mods);
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
                auto& eventSystem{ Engine::GetSystem<EventSystem>() };
                switch (action) {
                    case GLFW_PRESS: {
                        eventSystem.Trigger<MouseButtonPressedEvent>(button, mods);
                        break;
                    }
                    case GLFW_RELEASE: {
                        eventSystem.Trigger<MouseButtonReleasedEvent>(button);
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
                auto& eventSystem{ Engine::GetSystem<EventSystem>() };
                eventSystem.Trigger<MouseScrollEvent>(xOffset, yOffset);
            }
        );

        glfwSetCursorPosCallback(m_Window,
            [](MKT_UNUSED_VAR GLFWwindow* window, double x, double y) -> void {
                auto& eventSystem{ Engine::GetSystem<EventSystem>() };
                eventSystem.Trigger<MouseMovedEvent>(x, y);
            }
        );

        glfwSetCharCallback(m_Window,
            [](MKT_UNUSED_VAR GLFWwindow* window, unsigned int codePoint) -> void {
                auto& eventSystem{ Engine::GetSystem<EventSystem>() };
                eventSystem.Trigger<KeyCharEvent>(codePoint);
            }
        );

        glfwSetWindowFocusCallback(m_Window,
            [](GLFWwindow* window, int focus) -> void {
                auto& inputSystem{ Engine::GetSystem<InputSystem>() };
                MainWindow* data{ static_cast<MainWindow*>(glfwGetWindowUserPointer(window)) };
                if (focus == GLFW_TRUE) {
                    inputSystem.SetFocus(data);
                }
            });
    }


    auto MainWindow::MoveToMonitorCenter() const -> void {
        // See: https://www.glfw.org/docs/3.3/monitor_guide.html
        // The primary monitor is returned by glfwGetPrimaryMonitor. It is the user's
        // preferred monitor and is usually the one with global UI elements like task bar or menu bar.
        Int32_T monitorWidth{};
        Int32_T monitorHeight{};

        Int32_T monitorX{};
        Int32_T monitorY{};

        GLFWmonitor* primary{ glfwGetWindowMonitor( m_Window ) };

        if (primary == nullptr) {
            primary = glfwGetPrimaryMonitor();
        }

        glfwGetMonitorWorkarea(primary, std::addressof( monitorX  ), std::addressof( monitorY  ), std::addressof(monitorWidth), std::addressof(monitorHeight));
        glfwSetWindowPos(m_Window, monitorWidth / 2 - m_Properties.Width / 2, monitorHeight / 2 - m_Properties.Height / 2);
    }


    auto MainWindow::InitGLFW() -> void {
        if (!s_GLFWInitSuccess) {
            const auto ret{ glfwInit() };
            MKT_ASSERT(ret == GLFW_TRUE, "MainWindow::InitGLFW - Failed to initialized the GLFW library");

            s_GLFWInitSuccess = true;

            glfwSetErrorCallback([](Int32_T errCode, const char* desc) -> void {
                    MKT_CORE_LOGGER_ERROR("GLFW error code: {} Description: {}", errCode, desc);
                }
            );
        }
    }


    auto MainWindow::ProcessEvents() -> void {
        glfwPollEvents();
    }

    auto MainWindow::SetScreenMode( const ScreenMode mode ) -> void {
        m_ScreenMode = mode;

        switch (m_ScreenMode) {
            case FULLSCREEN: {
                // Get the primary monitor
                GLFWmonitor* monitor{ glfwGetPrimaryMonitor() };
                const GLFWvidmode* videoMode{ glfwGetVideoMode(monitor) };

                m_WidthPreFullScreen = m_Properties.Width;
                m_HeightPreFullScreen = m_Properties.Height;

                glfwSetWindowMonitor(m_Window, monitor, 0, 0, videoMode->width, videoMode->height, videoMode->refreshRate);
                break;
            }
            case WINDOWED: {
                // Get the primary monitor
                GLFWmonitor* monitor{ glfwGetPrimaryMonitor() };
                const GLFWvidmode* videoMode{ glfwGetVideoMode(monitor) };

                m_Properties.Width = m_WidthPreFullScreen;
                m_Properties.Height = m_HeightPreFullScreen;

                glfwSetWindowMonitor(m_Window, nullptr, 0, 0, videoMode->width, videoMode->height, videoMode->refreshRate);
                glfwSetWindowSize( m_Window,m_WidthPreFullScreen, m_HeightPreFullScreen );

                MoveToMonitorCenter();

                break;
            }

            case BORDERLESS: {
                // Get the primary monitor
                Int32_T monitorWidth{};
                Int32_T monitorHeight{};

                Int32_T monitorX{};
                Int32_T monitorY{};

                GLFWmonitor* monitor{ glfwGetPrimaryMonitor() };

                glfwGetMonitorWorkarea(monitor, std::addressof( monitorX  ), std::addressof( monitorY  ), std::addressof(monitorWidth), std::addressof(monitorHeight));

                const GLFWvidmode* videoMode{ glfwGetVideoMode(monitor) };

                glfwSetWindowSize( m_Window, monitorWidth, monitorHeight );

                glfwSetWindowMonitor(m_Window, monitor, 0, 0, videoMode->width, videoMode->height, videoMode->refreshRate);
                break;
            }
        }
    }


    auto MainWindow::DestroyGLFWWindow(GLFWwindow* window) -> void {
        // Everytime we shut down a GLFW window, we decrease the number
        // of active windows, the last GLFW window to be shutdown calls glfwTerminate()
        glfwDestroyWindow(window);
        s_WindowsCount -= 1;

        if (s_WindowsCount == 0) {
            glfwTerminate();
        }
    }


    auto MainWindow::Create(const MainWindowCreateSpec& spec) -> GLFWwindow* {
        // All windows are created in non-fullscreen mode because the monitor we pass is null, see docs for glfwCreateWindow
        GLFWwindow* window{ glfwCreateWindow(spec.Width, spec.Height, spec.Title.data(), nullptr, nullptr) };
        s_WindowsCount += 1;
        return window;
    }
}