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
#include <Common/Types.hh>
#include <Common/Common.hh>
#include <Core/Assert.hh>
#include <Core/KeyCodes.hh>
#include <Core/MouseButtons.hh>
#include <Platform/Window.hh>

namespace Mikoto {
    /**
     * @brief Enum defining cursor input modes.
     * */
    enum CursorInputMode {
        CURSOR_NORMAL   = 0, /**< The default cursor mode. */
        CURSOR_HIDDEN   = 1, /**< The hidden cursor mode. */
        CURSOR_DISABLED = 2, /**< The disabled cursor mode. */
    };

    /**
     * @brief Manages input functionality for the currently focused window.
     * */
    class InputManager {
    public:
        /**
         * @brief Initializes the input manager with a given window handle.
         * @param handle Pointer to the Window object to handle input for.
         * */
        static auto Init(const Window* handle) -> void;


        /**
         * @brief Shuts down the input manager.
         * */
        static auto Shutdown() -> void;


        /**
         * @brief Checks if a key is currently pressed.
         * @param keyCode The code of the key to check.
         * @returns True if the key is pressed, false otherwise.
         * */
        MKT_NODISCARD static auto IsKeyPressed(Int32_T keyCode) -> bool;


        /**
         * @brief Checks if a mouse button is currently pressed.
         * @param button The code of the mouse button to check.
         * @returns True if the mouse button is pressed, false otherwise.
         * */
        MKT_NODISCARD static auto IsMouseKeyPressed(Int32_T button) -> bool;


        /**
         * @brief Retrieves the X-coordinate of the mouse position.
         * @returns The X-coordinate of the mouse position.
         * */
        MKT_NODISCARD static auto GetMouseX() -> double;


        /**
         * @brief Retrieves the Y-coordinate of the mouse position.
         * @returns The Y-coordinate of the mouse position.
         * */
        MKT_NODISCARD static auto GetMouseY() -> double;


        /**
         * @brief Retrieves the current mouse position as a pair of (X, Y) coordinates.
         * @returns The current mouse position as a pair of (X, Y) coordinates.
         * */
        MKT_NODISCARD static auto GetMousePos() -> std::pair<double, double>;


        /**
         * @brief Prints the mouse button.
         * @param button The mouse button to print.
         * */
        static auto PrintMouse(MouseButton button) -> void;

        /**
         * @brief Prints the key code of a keyboard key. For debugging purposes.
         * @param keycode The key code to print.
         * */
        MKT_UNUSED_FUNC static auto PrintKey(KeyCode keycode) -> void;


        /**
         * @brief Prints the mouse button. For debugging purposes.
         * @param button The mouse button to print.
         * */
        MKT_UNUSED_FUNC static auto PrintButton(MouseButton button) -> void;


        /**
         * @brief Sets the cursor input mode.
         * For debugging purposes.
         * @param mode The mode to set for the cursor.
         * */
        MKT_UNUSED_FUNC static auto SetCursorMode(CursorInputMode mode) -> void;


        /**
         * @brief Sets focus to a newly focused window for input handling.
         * Input is handled for a single window at a time.
         * @param newHandle Pointer to the newly focused Window object.
         * */
        static auto SetFocus(const Window* newHandle) -> void;

    private:
        static inline const Window* s_Handle{ nullptr }; /**< Pointer to the current window being handled. */

    };
}

#endif // MIKOTO_INPUT_MANAGER_HH