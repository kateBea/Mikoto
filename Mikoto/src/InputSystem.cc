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
#include <Common/Constants.hh>
#include <Core/Logging/Logger.hh>
#include <Core/Input/MouseCodes.hh>
#include <Core/System/InputSystem.hh>
#include <Library/Utility/Types.hh>
#include <Library/String/String.hh>

namespace Mikoto {
#if defined( USE_GLFW_INPUT )
    static auto GetMode( const CursorInputMode mode ) -> Int32_T {
        switch ( mode ) {
            case CURSOR_NORMAL:
                return GLFW_CURSOR_NORMAL;
            case CURSOR_HIDDEN:
                return GLFW_CURSOR_HIDDEN;
            case CURSOR_DISABLED:
                return GLFW_CURSOR_DISABLED;
        }

        return GLFW_CURSOR_NORMAL;
    }

    auto InputSystem::IsKeyPressed( const Int32_T keyCode, const Window* handle ) const -> bool {
        bool result{ false };

        try {
            const auto window{ std::any_cast<GLFWwindow*>( handle != nullptr ? handle->GetNativeWindow() : m_Handle->GetNativeWindow() ) };
            const auto state{ glfwGetKey( window, keyCode ) };

            result = state == GLFW_PRESS;
        } catch ( const std::exception& exception ) {
            MKT_APP_LOGGER_ERROR( "InputManager - {}", exception.what() );
        }

        return result;
    }


    auto InputSystem::IsMouseKeyPressed( const Int32_T button ) const -> bool {
        bool result{ false };

        try {
            const auto window{ std::any_cast<GLFWwindow*>( m_Handle->GetNativeWindow() ) };
            const auto state{ glfwGetMouseButton( window, button ) };

            result = state == GLFW_PRESS;
        } catch ( const std::exception& exception ) {
            MKT_APP_LOGGER_ERROR( "InputManager - {}", exception.what() );
        }

        return result;
    }


    auto InputSystem::GetMousePos() const -> std::pair<double, double> {
        double posX{};
        double posY{};

        try {
            const auto window{ std::any_cast<GLFWwindow*>( m_Handle->GetNativeWindow() ) };
            glfwGetCursorPos( window, std::addressof( posX ), std::addressof( posY ) );
        } catch ( const std::exception& exception ) {
            MKT_APP_LOGGER_ERROR( "InputManager - {}", exception.what() );
        }

        return std::make_pair( posX, posY );
    }


    auto InputSystem::GetMouseX() const -> double {
        const auto [mouseX, mouseY]{ GetMousePos() };
        return mouseX;
    }


    auto InputSystem::GetMouseY() const -> double {
        const auto [mouseX, mouseY]{ GetMousePos() };
        return mouseY;
    }


    auto InputSystem::SetCursorMode( const CursorInputMode mode ) const -> void {
        try {
            const auto window{ std::any_cast<GLFWwindow*>( m_Handle->GetNativeWindow() ) };
            glfwSetInputMode( window, GLFW_CURSOR, GetMode( mode ) );
        } catch ( const std::exception& exception ) {
            MKT_APP_LOGGER_ERROR( "InputManager - {}", exception.what() );
        }
    }
#endif


    auto InputSystem::Init() -> void {

    }

    auto InputSystem::Shutdown() -> void {
        MKT_CORE_LOGGER_INFO( "InputManager - Shutdown" );
        m_Handle = nullptr;
    }

    auto InputSystem::Update() -> void {

    }

    auto InputSystem::SetFocus( const Window* newHandle ) -> void {
        if ( newHandle ) {
            m_Handle = newHandle;
            MKT_CORE_LOGGER_INFO( "InputManager - Input focus switch to '{}'", m_Handle->GetTitle() );
        } else {
            MKT_CORE_LOGGER_WARN( "InputManager - Attempted to set focus for input handling on null window handle." );
        }
    }

    auto InputSystem::PrintKey( const KeyCode keycode ) -> void {
        MKT_COLOR_PRINT_FORMATTED( MKT_FMT_COLOR_LIME, "Key: {}\n", GetStringRepresentation( keycode ) );
    }

    auto InputSystem::PrintMouse( const MouseButton button ) -> void {
        MKT_COLOR_PRINT_FORMATTED( MKT_FMT_COLOR_BLUE_VIOLET, "{}\n", GetStringRepresentation( button ) );
    }

    auto InputSystem::PrintButton( const MouseButton button ) -> void {
        MKT_COLOR_PRINT_FORMATTED( MKT_FMT_COLOR_LIME, "Mouse button: {}\n", GetStringRepresentation( button ) );
    }
}