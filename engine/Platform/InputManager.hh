/**
 * InputManager.hh
 * Created by kate on 5/30/23.
 * */

#ifndef MIKOTO_INPUT_MANAGER_HH
#define MIKOTO_INPUT_MANAGER_HH

// C++ Standard Library
#include <memory>
#include <utility>

// Project Headers
#include <Utility/Common.hh>
#include <Utility/Types.hh>
#include <Core/Assert.hh>
#include <Core/KeyCodes.hh>
#include <Core/MouseButtons.hh>
#include <Platform/Window/Window.hh>


namespace Mikoto::InputManager {
    auto Init() -> void;

    auto IsKeyPressed(Int32_T keyCode) -> bool;
    auto IsMouseKeyPressed(Int32_T button) -> bool;
    auto GetMouseX() -> double;
    auto GetMouseY() -> double;
    auto GetMousePos() -> std::pair<double, double>;

    auto PrintKey(KeyCode keycode) -> void;
    auto PrintMouse(MouseButton button) -> void;
    auto HideCursor() -> void;
    auto ShowCursor() -> void;

    auto ShutDown() -> void;

}

#endif // MIKOTO_INPUT_MANAGER_HH
