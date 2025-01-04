/**
 * Window.cc
 * Created by kate on 1/12/24.
 * */

// C++ Standard Library
#include <memory>

// Project Headers
#include <Platform/Window/Window.hh>
#include <Platform/Window/XPWindow.hh>

#include "Models/WindowProperties.hh"

namespace Mikoto {

    auto Window::Create( WindowProperties &&properties ) -> std::shared_ptr<Window> {
        return std::make_shared<XPWindow>(std::move(properties));
    }
}
