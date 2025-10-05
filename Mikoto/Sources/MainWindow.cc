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
#include <Core/CoreEvents.hh>
#include <Core/EventService.hh>
#include <Core/InputService.hh>
#include <Library/Utility/Types.hh>
#include <Logging/Assert.hh>
#include <Logging/Logger.hh>
#include <Platform/MainWindow.hh>

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
        UInt32 major{};
        UInt32 minor{};

        switch(m_Properties.Backend) {
            case GraphicsAPI::VULKAN_API:
                // TODO: load from config
                major = 1;
                minor = 3;
                m_Properties.Title = fmt::format("{} (Vulkan Version {}.{})", m_Properties.Title, major, minor);

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

        MKT_CORE_LOGGER_INFO("Created MainWindow '{}' [{} x {}]", GetTitle(), GetWidth(), GetHeight());

        MoveToMonitorCenter();

        InstallCallbacks();
    }

    auto MainWindow::Shutdown() -> void {
        MKT_CORE_LOGGER_INFO("Shutting MainWindow '{}'", GetTitle());
        DestroyGLFWWindow(m_Window);
    }

    auto MainWindow::InstallCallbacks() -> void {
        glfwSetWindowUserPointer(m_Window, this);

        glfwSetWindowSizeCallback(m_Window,
            [](GLFWwindow* window, int width, int height) -> void {
                const auto data{ static_cast<MainWindow*>(glfwGetWindowUserPointer(window)) };
                data->m_Properties.Width = width;
                data->m_Properties.Height = height;

                EventService::Get().Queue<WindowResizedEvent>(width, height);
            }
        );

        glfwSetWindowCloseCallback(m_Window,
            [](MKT_UNUSED_VAR GLFWwindow* window) {
                EventService::Get().Queue<WindowCloseEvent>();
            }
        );

        glfwSetKeyCallback(m_Window,
            [](MKT_UNUSED_VAR GLFWwindow* window, int key, MKT_UNUSED_VAR int scancode, int action, int mods) -> void {
                switch (action) {
                    case GLFW_PRESS: {
                        EventService::Get().Queue<KeyPressedEvent>(key, false, mods);
                        break;
                    }
                    case GLFW_RELEASE: {
                        EventService::Get().Queue<KeyReleasedEvent>(key);
                        break;
                    }
                    case GLFW_REPEAT: {
                        EventService::Get().Queue<KeyPressedEvent>(key, true, mods);
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
                        EventService::Get().Queue<MouseButtonPressedEvent>(button, mods);
                        break;
                    }
                    case GLFW_RELEASE: {
                        EventService::Get().Queue<MouseButtonReleasedEvent>(button);
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
                EventService::Get().Queue<MouseScrollEvent>(xOffset, yOffset);
            }
        );

        glfwSetCursorPosCallback(m_Window,
            [](MKT_UNUSED_VAR GLFWwindow* window, double x, double y) -> void {
                EventService::Get().Queue<MouseMovedEvent>(x, y);
            }
        );

        glfwSetCharCallback(m_Window,
            [](MKT_UNUSED_VAR GLFWwindow* window, unsigned int codePoint) -> void {
                EventService::Get().Queue<KeyCharEvent>(codePoint);
            }
        );

        glfwSetWindowCloseCallback(m_Window,
            [](MKT_UNUSED_VAR GLFWwindow* window) -> void {
                EventService::Get().Queue<WindowCloseEvent>();
            }
        );

        glfwSetWindowFocusCallback(m_Window,
            [](GLFWwindow* window, int focus) -> void {
                MainWindow* data{ static_cast<MainWindow*>(glfwGetWindowUserPointer(window)) };
                if (focus == GLFW_TRUE) {
                    InputService::Get().SetFocus( data );
                }
            });
    }

    auto MainWindow::MoveToMonitorCenter() const -> void {
        // See: https://www.glfw.org/docs/3.3/monitor_guide.html
        // The primary monitor is returned by glfwGetPrimaryMonitor. It is the user's
        // preferred monitor and is usually the one with global UI elements like task bar or menu bar.
        Int32 monitorWidth{};
        Int32 monitorHeight{};

        Int32 monitorX{};
        Int32 monitorY{};

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
            MKT_ASSERT(ret == GLFW_TRUE, "MainWindow::InitGLFW - Failed to initialized the GLFW library.");

            s_GLFWInitSuccess = true;

            glfwSetErrorCallback([](Int32 errCode, const char* desc) -> void {
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

        switch ( m_ScreenMode ) {
            case ScreenMode::WINDOW_MODE_FULLSCREEN: {
                // Get the primary monitor
                GLFWmonitor* monitor{ glfwGetPrimaryMonitor() };
                const GLFWvidmode* videoMode{ glfwGetVideoMode( monitor ) };

                m_WidthPreFullScreen = m_Properties.Width;
                m_HeightPreFullScreen = m_Properties.Height;

                glfwSetWindowMonitor( m_Window, monitor, 0, 0, videoMode->width, videoMode->height, videoMode->refreshRate );
                break;
            }
            case ScreenMode::WINDOW_MODE_WINDOWED: {
                // Get the primary monitor
                GLFWmonitor* monitor{ glfwGetPrimaryMonitor() };
                const GLFWvidmode* videoMode{ glfwGetVideoMode( monitor ) };

                m_Properties.Width = m_WidthPreFullScreen;
                m_Properties.Height = m_HeightPreFullScreen;

                glfwSetWindowMonitor( m_Window, nullptr, 0, 0, videoMode->width, videoMode->height, videoMode->refreshRate );
                glfwSetWindowSize( m_Window, m_WidthPreFullScreen, m_HeightPreFullScreen );

                MoveToMonitorCenter();

                break;
            }

            case ScreenMode::WINDOW_MODE_BORDERLESS: {
                // Get the primary monitor
                Int32 monitorWidth{};
                Int32 monitorHeight{};

                Int32 monitorX{};
                Int32 monitorY{};

                GLFWmonitor* monitor{ glfwGetPrimaryMonitor() };

                glfwGetMonitorWorkarea( monitor, std::addressof( monitorX ), std::addressof( monitorY ), std::addressof( monitorWidth ), std::addressof( monitorHeight ) );

                const GLFWvidmode* videoMode{ glfwGetVideoMode( monitor ) };

                glfwSetWindowSize( m_Window, monitorWidth, monitorHeight );

                glfwSetWindowMonitor( m_Window, monitor, 0, 0, videoMode->width, videoMode->height, videoMode->refreshRate );
                break;
            }
        }
    }

    auto MainWindow::IsKeyPressed( KeyCode keyCode ) const -> bool {
        const auto state{ glfwGetKey( m_Window, keyCode ) };

        return state == GLFW_PRESS;
    }

    auto MainWindow::IsKeyReleased( KeyCode keyCode ) const -> bool {
        const auto state{ glfwGetKey( m_Window, keyCode ) };

        return state == GLFW_RELEASE;
    }

    auto MainWindow::IsMouseKeyPressed( MouseButton button ) const -> bool {
        const auto state{ glfwGetMouseButton( m_Window, button ) };
        return state == GLFW_PRESS;
    }

    auto MainWindow::IsMouseKeyReleased( MouseButton button ) const -> bool {
        const auto state{ glfwGetMouseButton( m_Window, button ) };

        return state == GLFW_RELEASE;
    }

    auto MainWindow::GetMouseX() const -> double {
        return GetMousePos().first;
    }

    auto MainWindow::GetMouseY() const -> double {
        return GetMousePos().second;
    }

    auto MainWindow::GetMousePos() const -> std::pair<double, double> {
        double posX{};
        double posY{};
        glfwGetCursorPos( m_Window, std::addressof( posX ), std::addressof( posY ) );

        return std::make_pair( posX, posY );
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