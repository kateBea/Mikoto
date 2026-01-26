//    Copyright 2026 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <any>

#include <volk.h>
#include <GLFW/glfw3.h>

#include <Common/Common.hh>
#include <Core/CoreEvents.hh>
#include <Core/EventService.hh>
#include <Core/InputService.hh>
#include <Core/Profiler.hh>
#include <Library/Utility/Types.hh>
#include <Logging/Assert.hh>
#include <Logging/Logger.hh>
#include <Platform/MainWindow.hh>

#include <Renderer/Vulkan/VulkanHelpers.hh>

namespace Mikoto {
    MainWindow::MainWindow(const WindowProperties& properties )
        :   Window{ properties }
    {
        AllowResizing(IsResizable());
    }

    auto MainWindow::Init() -> void {
        MKT_BEGIN_PROFILER_NAMED();
        MKT_CORE_LOGGER_INFO("MainWindow::Init - Initializing new GLFW Window.");

        SetCustomTitle();
        SetBasicHints();

        CreateNativeHandle();

        SetCursorMode( GetCursorMode() );
        SetCursorType( GetCursorType() );

        MoveToMonitorCenter();

        InstallCallbacks();

        MKT_CORE_LOGGER_INFO("Created MainWindow '{}' [{} x {}]", GetTitle(), GetWidth(), GetHeight());
    }

    auto MainWindow::Shutdown() -> void {
        MKT_CORE_LOGGER_INFO("Shutting MainWindow '{}'", GetTitle());

        glfwDestroyWindow(m_Window);
    }

    auto MainWindow::SetBasicHints() -> void {
        // Because GLFW was originally designed to create an OpenGL context,
        // we need to tell it to not create an OpenGL context with a later call to glfwCreateWindow
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, IsResizable() ? GLFW_TRUE : GLFW_FALSE);

        if (m_ScreenMode != ScreenMode::WINDOW_MODE_FULLSCREEN) {
            glfwWindowHint(GLFW_MAXIMIZED, GLFW_FALSE);
        }
    }

    auto MainWindow::InstallCallbacks() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        glfwSetWindowUserPointer(m_Window, this);

        glfwSetWindowSizeCallback(m_Window,
            [](GLFWwindow* window, int width, int height) -> void {
                const auto data{ static_cast<MainWindow*>(glfwGetWindowUserPointer(window)) };
                data->m_Width = width;
                data->m_Height = height;

                EventService::Get()->Queue<WindowResizedEvent>(width, height);
            }
        );

        glfwSetWindowCloseCallback(m_Window,
            []( GLFWwindow* ) {
                EventService::Get()->Queue<WindowCloseEvent>();
            }
        );

        glfwSetKeyCallback(m_Window,
            []( GLFWwindow* , int key,  int , int action, int mods) -> void {
                switch (action) {
                    case GLFW_PRESS: {
                        EventService::Get()->Queue<KeyPressedEvent>(key, false, mods);
                        break;
                    }
                    case GLFW_RELEASE: {
                        EventService::Get()->Queue<KeyReleasedEvent>(key);
                        break;
                    }
                    case GLFW_REPEAT: {
                        EventService::Get()->Queue<KeyPressedEvent>(key, true, mods);
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
            []( GLFWwindow* , int button, int action, int mods) -> void {
                switch (action) {
                    case GLFW_PRESS: {
                        EventService::Get()->Queue<MouseButtonPressedEvent>(button, mods);
                        break;
                    }
                    case GLFW_RELEASE: {
                        EventService::Get()->Queue<MouseButtonReleasedEvent>(button);
                        break;
                    }
                    default:
                        MKT_CORE_LOGGER_WARN("Unknown GLFW_ value for glfwSetMouseButtonCallback");
                        break;
                }
            }
        );

        glfwSetScrollCallback(m_Window,
            []( GLFWwindow* , double xOffset, double yOffset) -> void {
                EventService::Get()->Queue<MouseScrollEvent>(xOffset, yOffset);
            }
        );

        glfwSetCursorPosCallback(m_Window,
            [](GLFWwindow* , double x, double y) -> void {
                EventService::Get()->Queue<MouseMovedEvent>(x, y);
            }
        );

        glfwSetCharCallback(m_Window,
            [](GLFWwindow* , unsigned int codePoint) -> void {
                EventService::Get()->Queue<KeyCharEvent>(codePoint);
            }
        );

        glfwSetWindowCloseCallback(m_Window,
            [](GLFWwindow* ) -> void {
                EventService::Get()->Queue<WindowCloseEvent>();
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

    auto MainWindow::SetCustomTitle() -> void {
        switch(m_Backend) {
            case GraphicsAPI::VULKAN_API:
                m_Title = fmt::format("{} (Vulkan Version {}.{})", m_Title, MKT_VULKAN_VERSION_MAJOR, MKT_VULKAN_VERSION_MINOR);
                break;
            default:;
        }
    }

    auto MainWindow::MoveToMonitorCenter() const -> void {
        MKT_BEGIN_PROFILER_NAMED();

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
        glfwSetWindowPos(m_Window, monitorWidth / 2 - m_Width / 2, monitorHeight / 2 - m_Height / 2);
    }

    auto MainWindow::CreateNativeHandle() -> void {
        try {
            m_Window = glfwCreateWindow(GetWidth(), GetHeight(), GetTitle().data(), nullptr, nullptr);
            if (m_Window == nullptr) {
                MKT_THROW_RUNTIME_ERROR( "GLFWindow handle is NULL" );
            }
        } catch( std::exception& e ) {
            const auto result{  glfwGetError( nullptr ) };
            MKT_CORE_LOGGER_ERROR("Error creating native window. Last error code {}", e.what(), result);
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

                m_WidthPreFullScreen = m_Width;
                m_HeightPreFullScreen = m_Height;

                glfwSetWindowMonitor( m_Window, monitor, 0, 0, videoMode->width, videoMode->height, videoMode->refreshRate );
                break;
            }
            case ScreenMode::WINDOW_MODE_WINDOWED: {
                // Get the primary monitor
                GLFWmonitor* monitor{ glfwGetPrimaryMonitor() };
                const GLFWvidmode* videoMode{ glfwGetVideoMode( monitor ) };

                m_Width = m_WidthPreFullScreen;
                m_Height = m_HeightPreFullScreen;

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

    auto MainWindow::SetCursorMode( CursorMode mode ) -> void {
        MKT_ASSERT( m_Window != nullptr, "Trying to set cursor mode on NULL Window" );

        switch (mode) {
            case CursorMode::NORMAL:
                glfwSetInputMode( m_Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL );
                break;
            case CursorMode::HIDDEN:
                glfwSetInputMode( m_Window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN );
                break;
            case CursorMode::DISABLED:
                glfwSetInputMode( m_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED );
                break;
        }

        m_CursorMode = mode;
    }

    auto MainWindow::SetCursorType( CursorType type ) -> void {
        MKT_ASSERT( m_Window != nullptr, "Trying to set cursor type on NULL Window" );

        GLFWcursor *cursor{ nullptr };

        switch (type) {
            case CursorType::ARROW:
                cursor = glfwCreateStandardCursor( GLFW_ARROW_CURSOR );
                break;
            case CursorType::HAND:
                cursor = glfwCreateStandardCursor( GLFW_HAND_CURSOR );
                break;
            case CursorType::TEXT:
                cursor = glfwCreateStandardCursor( GLFW_IBEAM_CURSOR );
                break;
            case CursorType::RESIZE_VERTICAL:
                cursor = glfwCreateStandardCursor( GLFW_VRESIZE_CURSOR );
                break;
            case CursorType::RESIZE_HORIZONTAL:
                cursor = glfwCreateStandardCursor( GLFW_HRESIZE_CURSOR );
                break;
            case CursorType::CROSSHAIR:
                cursor = glfwCreateStandardCursor( GLFW_CROSSHAIR_CURSOR );
                break;
            default:
                cursor = glfwCreateStandardCursor( GLFW_ARROW_CURSOR );
                break;
        }

        glfwSetCursor( m_Window, cursor );

        m_CursorType = type;
    }

    auto MainWindow::ResetCursorType() -> void {
        GLFWcursor *cursor{ glfwCreateStandardCursor( GLFW_ARROW_CURSOR ) };
        glfwSetCursor( m_Window, cursor );

        m_CursorType = CursorType::ARROW;
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

    auto MainWindow::ShouldClose() const -> bool {
        return glfwWindowShouldClose( m_Window );
    }
}