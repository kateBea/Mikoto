//
// Created by kate on 6/9/23.
//

#include <any>
#include <utility>

// Third-Party Libraries
#include <GLFW/glfw3.h>

// Project Headers
#include <Utility/Common.hh>

#include <Core/Logger.hh>
#include <Core/Application.hh>

#include <Platform/InputManager.hh>

namespace Mikoto::InputManager {
#if defined(USE_GLFW_INPUT)
    auto IsKeyPressed(Int32_T keyCode) -> bool {
        GLFWwindow* window{ nullptr };

        try {
            // We expect the native window for Linux Window to be a GLFWwindow*
            window = std::any_cast<GLFWwindow*>(Application::Get().GetMainWindow().GetNativeWindow());
            Int32_T state{ glfwGetKey(window, keyCode) };

            return state == GLFW_PRESS;
        }
        catch (const std::bad_any_cast& exception) {
            KATE_APP_LOGGER_ERROR("Exception thrown std::any_cast. What: {}", exception.what());
        }

        return false;
    }

    auto IsMouseKeyPressed(Int32_T button) -> bool {
        GLFWwindow* window{ nullptr };

        try {
            // We expect the native window for Linux Window to be a GLFWwindow*
            window = std::any_cast<GLFWwindow*>(Application::Get().GetMainWindow().GetNativeWindow());
            Int32_T state{ glfwGetMouseButton(window, button) };

            return state == GLFW_PRESS;
        }
        catch (const std::bad_any_cast& exception) {
            KATE_APP_LOGGER_ERROR("Exception thrown std::any_cast. What: {}", exception.what());
        }

        return false;
    }

    auto GetMousePos() -> std::pair<double, double> {
        double posX{};
        double posY{};
        GLFWwindow* window{ nullptr };

        try {
            // We expect the native window for Linux Window to be a GLFWwindow*
            window = std::any_cast<GLFWwindow*>(Application::Get().GetMainWindow().GetNativeWindow());
            glfwGetCursorPos(window, &posX, &posY);
        }
        catch (const std::bad_any_cast& exception) {
            KATE_APP_LOGGER_ERROR("Exception thrown std::any_cast. What: {}", exception.what());
        }

        return std::make_pair(posX, posY);
    }

    auto GetMouseX() -> double {
        auto [mouseX, mouseY]{ GetMousePos() };
        return mouseX;
    }

    auto GetMouseY() -> double {
        auto [mouseX, mouseY]{ GetMousePos() };
        return mouseY;
    }
#endif

    auto Init() -> void {
        KATE_CORE_LOGGER_INFO("Mikoto Engine: Input Manager initialization");

    }

    auto ShutDown() -> void {
        KATE_CORE_LOGGER_INFO("Mikoto Engine: Input Manager shut down");
    }

    auto PrintKey(KeyCode keycode) -> void {
        KT_COLOR_PRINT_FORMATTED(KT_FMT_COLOR_LIME, "Key: {}\n", GetStringRepresentation(keycode));
    }

    auto PrintButton(MouseButton button) -> void {
        KT_COLOR_PRINT_FORMATTED(KT_FMT_COLOR_LIME, "Mouse button: {}\n", GetStringRepresentation(button));
    }

}