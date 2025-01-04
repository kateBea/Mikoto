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
#include <Core/Logger.hh>
#include <Core/MouseButtons.hh>
#include <Platform/Input/InputManager.hh>
#include <STL/Utility/Types.hh>

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


    auto InputManager::IsKeyPressed( const Int32_T keyCode ) -> bool {
        bool result{ false };

        try {
            const auto window{ std::any_cast<GLFWwindow*>( s_Handle->GetNativeWindow() ) };
            const auto state{ glfwGetKey( window, keyCode ) };

            result = state == GLFW_PRESS;
        } catch ( const std::exception& exception ) {
            MKT_APP_LOGGER_ERROR( "InputManager - {}", exception.what() );
        }

        return result;
    }


    auto InputManager::IsMouseKeyPressed( const Int32_T button ) -> bool {
        bool result{ false };

        try {
            const auto window{ std::any_cast<GLFWwindow*>( s_Handle->GetNativeWindow() ) };
            const auto state{ glfwGetMouseButton( window, button ) };

            result = state == GLFW_PRESS;
        } catch ( const std::exception& exception ) {
            MKT_APP_LOGGER_ERROR( "InputManager - {}", exception.what() );
        }

        return result;
    }


    auto InputManager::GetMousePos() -> std::pair<double, double> {
        double posX{};
        double posY{};

        try {
            const auto window{ std::any_cast<GLFWwindow*>( s_Handle->GetNativeWindow() ) };
            glfwGetCursorPos( window, std::addressof( posX ), std::addressof( posY ) );
        } catch ( const std::exception& exception ) {
            MKT_APP_LOGGER_ERROR( "InputManager - {}", exception.what() );
        }

        return std::make_pair( posX, posY );
    }


    auto InputManager::GetMouseX() -> double {
        const auto [mouseX, mouseY]{ GetMousePos() };
        return mouseX;
    }


    auto InputManager::GetMouseY() -> double {
        const auto [mouseX, mouseY]{ GetMousePos() };
        return mouseY;
    }


    auto InputManager::SetCursorMode( const CursorInputMode mode ) -> void {
        try {
            const auto window{ std::any_cast<GLFWwindow*>( s_Handle->GetNativeWindow() ) };
            glfwSetInputMode( window, GLFW_CURSOR, GetMode( mode ) );
        } catch ( const std::exception& exception ) {
            MKT_APP_LOGGER_ERROR( "InputManager - {}", exception.what() );
        }
    }
#endif


    auto InputManager::Init( const Window* handle ) -> bool {
        MKT_CORE_LOGGER_INFO( "InputManager - Init" );

        s_Handle = handle;

        return true;
    }


    auto InputManager::Shutdown() noexcept -> void {
        MKT_CORE_LOGGER_INFO( "InputManager - Shutdown" );
        s_Handle = nullptr;
    }


    auto InputManager::SetFocus( const Window* newHandle ) -> void {
        if ( newHandle ) {
            s_Handle = newHandle;
            MKT_CORE_LOGGER_INFO( "InputManager - Input focus switch to '{}'", s_Handle->GetTitle() );
        } else {
            MKT_CORE_LOGGER_WARN( "InputManager - Attempted to set focus for input handling on null window handle." );
        }
    }


    auto InputManager::PrintKey( const KeyCode keycode ) -> void {
        MKT_COLOR_PRINT_FORMATTED( MKT_FMT_COLOR_LIME, "Key: {}\n", GetStringRepresentation( keycode ) );
    }


    auto InputManager::PrintMouse( const MouseButton button ) -> void {
        MKT_COLOR_PRINT_FORMATTED( MKT_FMT_COLOR_BLUE_VIOLET, "{}\n", GetStringRepresentation( button ) );
    }


    auto InputManager::PrintButton( const MouseButton button ) -> void {
        MKT_COLOR_PRINT_FORMATTED( MKT_FMT_COLOR_LIME, "Mouse button: {}\n", GetStringRepresentation( button ) );
    }
}