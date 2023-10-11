/**
 * InputManager.cc
 * Created by kate on 6/9/23.
 * */

// C++ Standard Library
#include <any>
#include <utility>

// Third-Party Libraries
#include <GLFW/glfw3.h>

// Project Headers
#include <Utility/Common.hh>
#include <Utility/Types.hh>
#include <Core/Logger.hh>
#include <Core/Application.hh>
#include <Platform/InputManager.hh>

namespace Mikoto::InputManager {
#if defined(USE_GLFW_INPUT)
    static auto GetMode(CursorInputMode mode) -> Int32_T {
        switch (mode) {
            case CURSOR_NORMAL: return GLFW_CURSOR_NORMAL;
            case CURSOR_HIDDEN: return GLFW_CURSOR_HIDDEN;
            case CURSOR_DISABLED: return GLFW_CURSOR_DISABLED;
        }
    }

    auto IsKeyPressed(Int32_T keyCode) -> bool {
        try {
            auto window{ std::any_cast<GLFWwindow*>(s_Handle->GetNativeWindow()) };
            Int32_T state{ glfwGetKey(window, keyCode) };

            return state == GLFW_PRESS;
        }
        catch (const std::bad_any_cast& exception) {
            MKT_APP_LOGGER_ERROR("Exception thrown std::any_cast. What: {}", exception.what());
        }

        return false;
    }

    auto IsMouseKeyPressed(Int32_T button) -> bool {
        try {
            auto window{ std::any_cast<GLFWwindow*>(s_Handle->GetNativeWindow()) };
            Int32_T state{ glfwGetMouseButton(window, button) };

            return state == GLFW_PRESS;
        }
        catch (const std::bad_any_cast& exception) {
            MKT_APP_LOGGER_ERROR("Exception thrown std::any_cast. What: {}", exception.what());
        }

        return false;
    }

    auto GetMousePos() -> std::pair<double, double> {
        double posX{}, posY{};

        try {
            auto window{ std::any_cast<GLFWwindow*>(s_Handle->GetNativeWindow()) };
            glfwGetCursorPos(window, &posX, &posY);
        }
        catch (const std::bad_any_cast& exception) {
            MKT_APP_LOGGER_ERROR("Exception thrown std::any_cast. What: {}", exception.what());
        }

        return std::make_pair(posX, posY);
    }

    auto GetMouseX() -> double {
        const auto [mouseX, mouseY]{ GetMousePos() };
        return mouseX;
    }

    auto GetMouseY() -> double {
        const auto [mouseX, mouseY]{ GetMousePos() };
        return mouseY;
    }

    auto SetCursorMode(CursorInputMode mode) -> void {
        try {
            auto window{ std::any_cast<GLFWwindow*>(s_Handle->GetNativeWindow()) };
            glfwSetInputMode(window, GLFW_CURSOR, GetMode(mode));
        }
        catch (const std::bad_any_cast& exception) {
            MKT_APP_LOGGER_ERROR("Exception thrown std::any_cast. What: {}", exception.what());
        }
    }

#endif

    auto Init(const Window* handle) -> void {
        MKT_CORE_LOGGER_INFO("Mikoto Engine: Input Manager initialization");

        s_Handle = handle;
    }

    auto Shutdown() -> void {
        MKT_CORE_LOGGER_INFO("Mikoto Engine: Input Manager shut down");
    }

    auto PrintKey(KeyCode keycode) -> void {
        MKT_COLOR_PRINT_FORMATTED(MKT_FMT_COLOR_LIME, "Key: {}\n", GetStringRepresentation(keycode));
    }

    auto SetFocus(const Window* newHandle) -> void {
        if (newHandle) {
            s_Handle = newHandle;
            MKT_CORE_LOGGER_INFO("Input handle focus on window '{}'", s_Handle->GetTitle());
        }
        else {
            MKT_CORE_LOGGER_WARN("Attempted to set focus for input handling on null window handle. No operation done.");
        }
    }

    auto PrintMouse(MouseButton button) -> void {

    }

    auto ShowCursor() -> void {

    }

    auto PrintButton(MouseButton button) -> void {
        MKT_COLOR_PRINT_FORMATTED(MKT_FMT_COLOR_LIME, "Mouse button: {}\n", GetStringRepresentation(button));
    }

}