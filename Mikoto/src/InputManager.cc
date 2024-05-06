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
#include <Common/Types.hh>
#include <Common/Common.hh>
#include <Core/Logger.hh>
#include <Core/MouseButtons.hh>
#include <Platform/InputManager.hh>

namespace Mikoto {
#if defined(USE_GLFW_INPUT)
    static auto GetMode(CursorInputMode mode) -> Int32_T {
        switch (mode) {
            case CURSOR_NORMAL: return GLFW_CURSOR_NORMAL;
            case CURSOR_HIDDEN: return GLFW_CURSOR_HIDDEN;
            case CURSOR_DISABLED: return GLFW_CURSOR_DISABLED;
        }
    }


    auto InputManager::IsKeyPressed(Int32_T keyCode) -> bool {
        bool result{ false };

        try {
            auto window{ std::any_cast<GLFWwindow*>(s_Handle->GetNativeWindow()) };
            Int32_T state{ glfwGetKey(window, keyCode) };

            result = state == GLFW_PRESS;
        } catch (const std::bad_any_cast& exception) {
            MKT_APP_LOGGER_ERROR("bad_any_cast. What: {}", exception.what());
        }

        return result;
    }


    auto InputManager::IsMouseKeyPressed(Int32_T button) -> bool {
        bool result{ false };

        try {
            auto window{ std::any_cast<GLFWwindow*>(s_Handle->GetNativeWindow()) };
            Int32_T state{ glfwGetMouseButton(window, button) };

            result = state == GLFW_PRESS;
        } catch (const std::bad_any_cast& exception) {
            MKT_APP_LOGGER_ERROR("bad_any_cast. What: {}", exception.what());
        }

        return result;
    }


    auto InputManager::GetMousePos() -> std::pair<double, double> {
        double posX{};
        double posY{};

        try {
            auto window{ std::any_cast<GLFWwindow*>(s_Handle->GetNativeWindow()) };
            glfwGetCursorPos(window, std::addressof(posX), std::addressof(posY));
        }
        catch (const std::bad_any_cast& exception) {
            MKT_APP_LOGGER_ERROR("bad_any_cast. What: {}", exception.what());
        }

        return std::make_pair(posX, posY);
    }


    auto InputManager::GetMouseX() -> double {
        const auto [mouseX, mouseY]{ GetMousePos() };
        return mouseX;
    }


    auto InputManager::GetMouseY() -> double {
        const auto [mouseX, mouseY]{ GetMousePos() };
        return mouseY;
    }


    auto InputManager::SetCursorMode(CursorInputMode mode) -> void {
        try {
            auto window{ std::any_cast<GLFWwindow*>(s_Handle->GetNativeWindow()) };
            glfwSetInputMode(window, GLFW_CURSOR, GetMode(mode));
        }
        catch (const std::bad_any_cast& exception) {
            MKT_APP_LOGGER_ERROR("bad_any_cast. What: {}", exception.what());
        }
    }
#endif


    auto InputManager::Init(const Window* handle) -> void {
        MKT_CORE_LOGGER_INFO("Mikoto Engine: Input Manager initialization");

        s_Handle = handle;
    }


    auto InputManager::Shutdown() -> void {
        MKT_CORE_LOGGER_INFO("Mikoto Engine: Input Manager shut down");
    }


    auto InputManager::SetFocus(const Window* newHandle) -> void {
        if (newHandle) {
            s_Handle = newHandle;
            MKT_CORE_LOGGER_INFO("Input handle focus on window '{}'", s_Handle->GetTitle());
        }
        else {
            MKT_CORE_LOGGER_WARN("Attempted to set focus for input handling on null window handle. No operation done.");
        }
    }


    auto InputManager::PrintKey(KeyCode keycode) -> void {
        MKT_COLOR_PRINT_FORMATTED(MKT_FMT_COLOR_LIME, "Key: {}\n", GetStringRepresentation(keycode));
    }


    auto InputManager::PrintMouse(MouseButton button) -> void {
        MKT_COLOR_PRINT_FORMATTED(MKT_FMT_COLOR_BLUE_VIOLET, "{}\n", GetStringRepresentation(button));
    }


    auto InputManager::PrintButton(MouseButton button) -> void {
        MKT_COLOR_PRINT_FORMATTED(MKT_FMT_COLOR_LIME, "Mouse button: {}\n", GetStringRepresentation(button));
    }
}