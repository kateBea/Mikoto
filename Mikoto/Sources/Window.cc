/**
 * Window.cc
 * Created by kate on 1/12/24.
 * */

// C++ Standard Library
#include <memory>

// Project Headers
#include <Platform/Window.hh>
#include <Platform/MainWindow.hh>

namespace Mikoto {

    auto Window::Create( const WindowProperties &properties ) -> Scope_T<Window> {
        return CreateScope<MainWindow>( properties );
    }
}
