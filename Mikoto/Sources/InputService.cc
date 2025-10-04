/**
 * InputManager.cc
 * Created by kate on 6/9/23.
 * */

// C++ Standard Library
#include <any>
#include <memory>
#include <utility>

// Third-Party Libraries
#include <GLFW/glfw3.h>

// Project Headers
#include <Common/Common.hh>
#include <Core/InputService.hh>
#include <Core/MouseCodes.hh>
#include <Library/String/String.hh>
#include <Library/Utility/Types.hh>
#include <Logging/Logger.hh>

namespace Mikoto {

#if defined( USE_GLFW_INPUT )
    static auto GetMode( const CursorInputMode mode ) -> Int32 {
        switch ( mode ) {
            case CursorInputMode::CURSOR_NORMAL:
                return GLFW_CURSOR_NORMAL;
            case CursorInputMode::CURSOR_HIDDEN:
                return GLFW_CURSOR_HIDDEN;
            case CursorInputMode::CURSOR_DISABLED:
                return GLFW_CURSOR_DISABLED;
        }

        return GLFW_CURSOR_NORMAL;
    }

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

            result = state == GLFW_RELEASE;
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

            result = state == GLFW_PRESS;
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

    auto InputService::GetMouseX() const -> double {
        const auto [mouseX, mouseY]{ GetMousePos() };
        return mouseX;
    }

    auto InputService::GetMouseY() const -> double {
        const auto [mouseX, mouseY]{ GetMousePos() };
        return mouseY;
    }

    auto InputService::SetCursorMode( const CursorInputMode mode ) const -> void {
        try {
            const auto window{ std::any_cast<GLFWwindow*>( m_Handle->GetNativeWindow() ) };
            glfwSetInputMode( window, GLFW_CURSOR, GetMode( mode ) );
        } catch ( const std::exception& exception ) {
            MKT_CORE_LOGGER_ERROR( "InputManager - {}", exception.what() );
        }
    }
#endif

    InputService::InputService( const InputServiceCreateInfo& options )
        : m_Handle{ options.MainWindow } {

        if ( m_Handle == nullptr ) {
            MKT_CORE_LOGGER_WARN( "InputSystem::InputSystem - Handle for input system is null." );
        }
    }

    auto InputService::Init() -> void {
        MKT_CORE_LOGGER_INFO("Initializing InputManager...");

        m_IsInitialized = true;
    }

    auto InputService::Shutdown() -> void {
        if (!m_IsInitialized) {
            return;
        }

        // The Log comes after so we know the service was
        // initialized before attempting to shut it down
        MKT_CORE_LOGGER_INFO( "Shutting down InputService..." );

        m_Handle = nullptr;
    }

    auto InputService::Update( float dt ) -> void {
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
}// namespace Mikoto