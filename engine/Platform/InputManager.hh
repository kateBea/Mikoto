/**
 * InputManager.hh
 * Created by kate on 5/30/23.
 * */

#ifndef KATE_ENGINE_INPUT_MANAGER_HH
#define KATE_ENGINE_INPUT_MANAGER_HH

// C++ Standard Library
#include <memory>
#include <utility>

// Project Headers
#include <Utility/Singleton.hh>
#include <Utility/Common.hh>

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

    auto ShutDown() -> void;

}


#endif// KATE_ENGINE_INPUT_MANAGER_HH
