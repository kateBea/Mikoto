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

#include <GLFW/glfw3.h>
#include <volk.h>

#include <Core/Core.hh>
#include <Core/CoreEvents.hh>
#include <Core/EventSystem.hh>
#include <Core/Exception.hh>
#include <Core/InputSystem.hh>
#include <Core/Profiler.hh>
#include <Core/String.hh>
#include <Core/Types.hh>
#include <Logging/Assert.hh>
#include <Logging/Logger.hh>
#include <Platform/MainWindow.hh>

namespace mikoto::platform {

    using namespace mikoto::core;

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

        glfwDestroyWindow(mWindow);
    }

    auto MainWindow::SetBasicHints() -> void {
        // Because GLFW was originally designed to create an OpenGL context,
        // we need to tell it to not create an OpenGL context with a later call to glfwCreateWindow
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, IsResizable() ? GLFW_TRUE : GLFW_FALSE);

        if (mScreenMode != ScreenMode::eFullScreen) {
            glfwWindowHint(GLFW_MAXIMIZED, GLFW_FALSE);
        }
    }

    auto MainWindow::InstallCallbacks() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        glfwSetWindowUserPointer(mWindow, this);

        glfwSetWindowSizeCallback(mWindow,
            [](GLFWwindow* window, int width, int height) -> void {
                const auto data{ static_cast<MainWindow*>(glfwGetWindowUserPointer(window)) };
                data->mWidth = width;
                data->mHeight = height;

                EventSystem::Get()->Queue<WindowResizedEvent>(width, height);
            }
        );

        glfwSetWindowCloseCallback(mWindow,
            []( GLFWwindow* ) {
                EventSystem::Get()->Queue<WindowCloseEvent>();
            }
        );

        glfwSetKeyCallback(mWindow,
            []( GLFWwindow* , int key,  int , int action, int mods) -> void {
                switch (action) {
                    case GLFW_PRESS: {
                        EventSystem::Get()->Queue<KeyPressedEvent>(key, false, mods);
                        break;
                    }
                    case GLFW_RELEASE: {
                        EventSystem::Get()->Queue<KeyReleasedEvent>(key);
                        break;
                    }
                    case GLFW_REPEAT: {
                        EventSystem::Get()->Queue<KeyPressedEvent>(key, true, mods);
                        break;
                    }
                    default: {
                        MKT_CORE_LOGGER_WARN("Unknown Key action for key callback");
                        break;
                    }
                }
            }
        );

        glfwSetMouseButtonCallback(mWindow,
            []( GLFWwindow* , int button, int action, int mods) -> void {
                switch (action) {
                    case GLFW_PRESS: {
                        EventSystem::Get()->Queue<MouseButtonPressedEvent>(button, mods);
                        break;
                    }
                    case GLFW_RELEASE: {
                        EventSystem::Get()->Queue<MouseButtonReleasedEvent>(button);
                        break;
                    }
                    default:
                        MKT_CORE_LOGGER_WARN("Unknown GLFW_ value for glfwSetMouseButtonCallback");
                        break;
                }
            }
        );

        glfwSetScrollCallback(mWindow,
            []( GLFWwindow* , double xOffset, double yOffset) -> void {
                EventSystem::Get()->Queue<MouseScrollEvent>(xOffset, yOffset);
            }
        );

        glfwSetCursorPosCallback(mWindow,
            [](GLFWwindow* , double x, double y) -> void {
                EventSystem::Get()->Queue<MouseMovedEvent>(x, y);
            }
        );

        glfwSetCharCallback(mWindow,
            [](GLFWwindow* , unsigned int codePoint) -> void {
                EventSystem::Get()->Queue<KeyCharEvent>(codePoint);
            }
        );

        glfwSetWindowCloseCallback(mWindow,
            [](GLFWwindow* ) -> void {
                EventSystem::Get()->Queue<WindowCloseEvent>();
            }
        );

        glfwSetWindowFocusCallback(mWindow,
            [](GLFWwindow* window, int focus) -> void {
                MainWindow* data{ static_cast<MainWindow*>(glfwGetWindowUserPointer(window)) };
                if (focus == GLFW_TRUE) {
                    InputSystem::Get()->SetFocus( data );
                }
            });

        glfwSetDropCallback(mWindow,
            [](GLFWwindow *, int pathCount, const char** paths) -> void {
                EventSystem::Get()->Queue<ContentDroppedEvent>( as<i32>( pathCount ), paths);
            });
    }

    auto MainWindow::SetCustomTitle() -> void {
        switch(mBackend) {
            case GraphicsAPI::eVulkan:
                mTitle = string::Format("{} (Vulkan Version {}.{})", mTitle, 1, 3); // 1 and 3 are major and minor versions, take them from the vulkan context
                break;
            case GraphicsAPI::eD3D12:
                mTitle = string::Format("{} DirectX 12", mTitle);
                break;
            case GraphicsAPI::eD3D11:
                mTitle = string::Format("{} DirectX 11", mTitle);
                break;
            default:;
        }
    }

    auto MainWindow::MoveToMonitorCenter() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // See: https://www.glfw.org/docs/3.3/monitor_guide.html
        // The primary monitor is returned by glfwGetPrimaryMonitor. It is the user's
        // preferred monitor and is usually the one with global UI elements like task bar or menu bar.
        i32 monitorWidth{};
        i32 monitorHeight{};

        i32 monitorX{};
        i32 monitorY{};

        GLFWmonitor* primary{ glfwGetWindowMonitor( mWindow ) };

        if (primary == nullptr) {
            primary = glfwGetPrimaryMonitor();
        }

        glfwGetMonitorWorkarea(primary, std::addressof( monitorX  ), std::addressof( monitorY  ), std::addressof(monitorWidth), std::addressof(monitorHeight));
        glfwSetWindowPos(mWindow, monitorWidth / 2 - mWidth / 2, monitorHeight / 2 - mHeight / 2);
    }

    auto MainWindow::CreateNativeHandle() -> void {
        try {
            mWindow = glfwCreateWindow(GetWidth(), GetHeight(), GetTitle().data(), nullptr, nullptr);
            if (mWindow == nullptr) {
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
        mScreenMode = mode;

        switch ( mScreenMode ) {
            case ScreenMode::eFullScreen: {
                // Get the primary monitor
                GLFWmonitor* monitor{ glfwGetPrimaryMonitor() };
                const GLFWvidmode* videoMode{ glfwGetVideoMode( monitor ) };

                mWidthPreFullScreen = mWidth;
                mHeightPreFullScreen = mHeight;

                glfwSetWindowMonitor( mWindow, monitor, 0, 0, videoMode->width, videoMode->height, videoMode->refreshRate );
                break;
            }
            case ScreenMode::eWindowed: {
                // Get the primary monitor
                GLFWmonitor* monitor{ glfwGetPrimaryMonitor() };
                const GLFWvidmode* videoMode{ glfwGetVideoMode( monitor ) };

                mWidth = mWidthPreFullScreen;
                mHeight = mHeightPreFullScreen;

                glfwSetWindowMonitor( mWindow, nullptr, 0, 0, videoMode->width, videoMode->height, videoMode->refreshRate );
                glfwSetWindowSize( mWindow, mWidthPreFullScreen, mHeightPreFullScreen );

                MoveToMonitorCenter();

                break;
            }

            case ScreenMode::eBorderless: {
                // Get the primary monitor
                i32 monitorWidth{};
                i32 monitorHeight{};

                i32 monitorX{};
                i32 monitorY{};

                GLFWmonitor* monitor{ glfwGetPrimaryMonitor() };

                glfwGetMonitorWorkarea( monitor, std::addressof( monitorX ), std::addressof( monitorY ), std::addressof( monitorWidth ), std::addressof( monitorHeight ) );

                const GLFWvidmode* videoMode{ glfwGetVideoMode( monitor ) };

                glfwSetWindowSize( mWindow, monitorWidth, monitorHeight );
                glfwSetWindowMonitor( mWindow, monitor, 0, 0, videoMode->width, videoMode->height, videoMode->refreshRate );
                break;
            }
        }
    }

    auto MainWindow::SetCursorMode( CursorMode mode ) -> void {
        MKT_ASSERT( mWindow != nullptr, "Trying to set cursor mode on NULL Window" );

        switch (mode) {
            case CursorMode::eNormal:
                glfwSetInputMode( mWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL );
                break;
            case CursorMode::eHidden:
                glfwSetInputMode( mWindow, GLFW_CURSOR, GLFW_CURSOR_HIDDEN );
                break;
            case CursorMode::eDisabled:
                glfwSetInputMode( mWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED );
                break;
        }

        mCursorMode = mode;
    }

    auto MainWindow::SetCursorType( CursorType type ) -> void {
        MKT_ASSERT( mWindow != nullptr, "Trying to set cursor type on NULL Window" );

        GLFWcursor *cursor{ nullptr };

        switch (type) {
            case CursorType::eArrow:
                cursor = glfwCreateStandardCursor( GLFW_ARROW_CURSOR );
                break;
            case CursorType::eHand:
                cursor = glfwCreateStandardCursor( GLFW_HAND_CURSOR );
                break;
            case CursorType::eText:
                cursor = glfwCreateStandardCursor( GLFW_IBEAM_CURSOR );
                break;
            case CursorType::eResizeVertical:
                cursor = glfwCreateStandardCursor( GLFW_VRESIZE_CURSOR );
                break;
            case CursorType::eResizeHorizontal:
                cursor = glfwCreateStandardCursor( GLFW_HRESIZE_CURSOR );
                break;
            case CursorType::eCrossHair:
                cursor = glfwCreateStandardCursor( GLFW_CROSSHAIR_CURSOR );
                break;
            default:
                cursor = glfwCreateStandardCursor( GLFW_ARROW_CURSOR );
                break;
        }

        glfwSetCursor( mWindow, cursor );

        mCursorType = type;
    }

    auto MainWindow::ResetCursorType() -> void {
        GLFWcursor *cursor{ glfwCreateStandardCursor( GLFW_ARROW_CURSOR ) };
        glfwSetCursor( mWindow, cursor );

        mCursorType = CursorType::eArrow;
    }

    auto MainWindow::IsKeyPressed( KeyCode keyCode ) const -> bool {
        const auto state{ glfwGetKey( mWindow, keyCode ) };
        return state == GLFW_PRESS;
    }

    auto MainWindow::IsKeyReleased( KeyCode keyCode ) const -> bool {
        const auto state{ glfwGetKey( mWindow, keyCode ) };
        return state == GLFW_RELEASE;
    }

    auto MainWindow::IsMouseKeyPressed( MouseButton button ) const -> bool {
        const auto state{ glfwGetMouseButton( mWindow, button ) };
        return state == GLFW_PRESS;
    }

    auto MainWindow::IsMouseKeyReleased( MouseButton button ) const -> bool {
        const auto state{ glfwGetMouseButton( mWindow, button ) };
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
        glfwGetCursorPos( mWindow, std::addressof( posX ), std::addressof( posY ) );

        return std::make_pair( posX, posY );
    }

    auto MainWindow::ShouldClose() const -> bool {
        return glfwWindowShouldClose( mWindow );
    }

    auto MainWindow::GetNativeWindow() const -> eastl::any {
        return mWindow;
    }
}// namespace mikoto::platform