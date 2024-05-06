/**
 * Window.cc
 * Created by kate on 1/12/24.
 * */

// C++ Standard Library
#include <memory>

// Project Headers
#include <Platform/Window.hh>
#include <Platform/XPWindow.hh>

namespace Mikoto {

    auto Window::Create( WindowProperties &&properties ) -> std::shared_ptr<Window> {
        return std::make_shared<XPWindow>(std::move(properties));
    }
}
