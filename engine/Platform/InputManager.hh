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
#include "Window.hh"
#include <Core/Assert.hh>
#include <Core/KeyCodes.hh>
#include <Core/MouseButtons.hh>
#include <Utility/Common.hh>
#include <Utility/Types.hh>


namespace Mikoto::InputManager {
    enum CursorInputMode {
        CURSOR_NORMAL = 0,
        CURSOR_HIDDEN = 1,
        CURSOR_DISABLED = 3,
    };

    inline const Window* s_Handle{ nullptr };

    auto Init(const Window* handle) -> void;
    auto Shutdown() -> void;

    auto IsKeyPressed(Int32_T keyCode) -> bool;
    auto IsMouseKeyPressed(Int32_T button) -> bool;
    auto GetMouseX() -> double;
    auto GetMouseY() -> double;
    auto GetMousePos() -> std::pair<double, double>;

    auto PrintKey(KeyCode keycode) -> void;
    auto PrintMouse(MouseButton button) -> void;
    auto SetCursorMode(CursorInputMode mode) -> void;
    auto ShowCursor() -> void;

    /**
     * When the user clicks on another window we want to handle
     * input from that newly focused window. Input is handled
     * for a single window at once
     * */
    auto SetFocus(const Window* newHandle) -> void;

}

#endif // MIKOTO_INPUT_MANAGER_HH
