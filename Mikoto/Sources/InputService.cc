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
#include <memory>
#include <utility>

#include <GLFW/glfw3.h>

#include <Common/Common.hh>
#include <Core/InputService.hh>
#include <Core/MouseCodes.hh>
#include <Core/Profiler.hh>
#include <Library/Utility/Types.hh>
#include <Logging/Logger.hh>

namespace Mikoto {

#if defined( USE_GLFW_INPUT )

    auto InputService::IsKeyPressed( const KeyCode keyCode ) const -> bool {
        bool result{ false };

        try {
            const auto window{ std::any_cast<GLFWwindow*>( m_Handle->GetNativeWindow() ) };
            const auto state{ glfwGetKey( window, keyCode ) };

            result = state == GLFW_PRESS;
        } catch ( const std::exception& exception ) {
            MKT_CORE_LOGGER_ERROR( "InputManager - {}", exception.what() );
        }

        return result;
    }

    auto InputService::IsKeyReleased( const KeyCode keyCode ) const -> bool {
        bool result{ false };

        try {
            const auto window{ std::any_cast<GLFWwindow*>( m_Handle->GetNativeWindow() ) };
            const auto state{ glfwGetKey( window, keyCode ) };

            result = state == GLFW_RELEASE;
        } catch ( const std::exception& exception ) {
            MKT_CORE_LOGGER_ERROR( "InputManager - {}", exception.what() );
        }

        return result;
    }

    auto InputService::IsMouseKeyPressed( const MouseButton button ) const -> bool {
        bool result{ false };

        try {
            const auto window{ std::any_cast<GLFWwindow*>( m_Handle->GetNativeWindow() ) };
            const auto state{ glfwGetMouseButton( window, button ) };

            result = state == GLFW_PRESS;
        } catch ( const std::exception& exception ) {
            MKT_CORE_LOGGER_ERROR( "InputManager - {}", exception.what() );
        }

        return result;
    }

    auto InputService::IsMouseKeyReleased( const MouseButton button ) const -> bool {
        bool result{ false };

        try {
            const auto window{ std::any_cast<GLFWwindow*>( m_Handle->GetNativeWindow() ) };
            const auto state{ glfwGetMouseButton( window, button ) };

            result = state == GLFW_RELEASE;
        } catch ( const std::exception& exception ) {
            MKT_CORE_LOGGER_ERROR( "InputManager - {}", exception.what() );
        }

        return result;
    }

    auto InputService::GetMousePos() const -> std::pair<double, double> {
        double posX{};
        double posY{};

        try {
            const auto window{ std::any_cast<GLFWwindow*>( m_Handle->GetNativeWindow() ) };
            glfwGetCursorPos( window, std::addressof( posX ), std::addressof( posY ) );
        } catch ( const std::exception& exception ) {
            MKT_CORE_LOGGER_ERROR( "InputManager - {}", exception.what() );
        }

        return std::make_pair( posX, posY );
    }

    auto InputService::GetClipBoardContents() const -> std::string {
        // glfwSetClipboardString
        return "";
    }

    auto InputService::GetDroppedPaths( Window *window ) const -> std::vector<std::string> {
        // It will return the last files dropped on the target window
        // glfwSetDropCallback
        return {};
    }

    auto InputService::GetMouseX() const -> double {
        const auto [mouseX, mouseY]{ GetMousePos() };
        return mouseX;
    }

    auto InputService::GetMouseY() const -> double {
        const auto [mouseX, mouseY]{ GetMousePos() };
        return mouseY;
    }

#endif

    InputService::InputService( const InputServiceCreateInfo& options )
        : m_Handle{ options.MainWindow } {
        MKT_ASSERT( m_Handle, "Handle for InputService cannot be null." );
    }

    auto InputService::Init() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_CORE_LOGGER_INFO("Initializing InputService...");

        m_IsInitialized = true;
    }

    auto InputService::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (!m_IsInitialized) {
            return;
        }

        // The Log comes after so we know the service was
        // initialized before attempting to shut it down
        MKT_CORE_LOGGER_INFO( "Shutting down InputService..." );

        m_Handle = nullptr;
    }

    auto InputService::Update( float ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( m_Handle != nullptr ) {
            m_Handle->ProcessEvents();
        }
    }

    auto InputService::SetFocus( Window* newHandle ) -> void {
        if ( newHandle ) {
            m_Handle = newHandle;
            MKT_CORE_LOGGER_INFO( "InputManager - Input focus switch to '{}'", m_Handle->GetTitle() );
        } else {
            MKT_CORE_LOGGER_WARN( "InputManager - Attempted to set focus for input handling on null window handle." );
        }
    }
}