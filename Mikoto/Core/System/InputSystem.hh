/**
 * InputSystem.hh
 * Created by kate on 5/30/23.
 * */

#ifndef MIKOTO_INPUT_MANAGER_HH
#define MIKOTO_INPUT_MANAGER_HH

// C++ Standard Library
#include <memory>
#include <utility>

// Project Headers
#include <Common/Common.hh>
#include <Core/Engine.hh>
#include <Core/Input/KeyCodes.hh>
#include <Core/Input/MouseCodes.hh>
#include <Core/Logging/Logger.hh>
#include <Library/Utility/Types.hh>
#include <Platform/Window/Window.hh>

namespace Mikoto {

    /**
     * @brief Manages input functionality for the currently focused window.
     * */
    class InputSystem : public IEngineSystem {
    public:
        explicit InputSystem() = default;

        explicit InputSystem( const EngineConfig& options )
            : m_Handle{ options.TargetWindow } {

            if ( m_Handle == nullptr ) {
                MKT_CORE_LOGGER_INFO( "InputSystem::InputSystem - Handle for input system is null." );
            }
        }

        ~InputSystem() override = default;

        /**
         * @brief Initializes the input manager with a given window handle.
         * @param handle Pointer to the Window object to handle input for.
         * */
        auto Init() -> void override;


        /**
         * @brief Shuts down the input manager.
         * */
        auto Shutdown() -> void override;

        auto Update() -> void override;


        /**
         * @brief Checks if a key is currently pressed.
         * @param keyCode The code of the key to check.
         * @param handle Pointer to the Window object to check input for.
         * @returns True if the key is pressed, false otherwise.
         * */
        MKT_NODISCARD auto IsKeyPressed( Int32_T keyCode, const Window* handle = nullptr ) const -> bool;


        /**
         * @brief Checks if a mouse button is currently pressed.
         * @param button The code of the mouse button to check.
         * @returns True if the mouse button is pressed, false otherwise.
         * */
        MKT_NODISCARD auto IsMouseKeyPressed( Int32_T button ) const -> bool;


        /**
         * @brief Retrieves the X-coordinate of the mouse position.
         * @returns The X-coordinate of the mouse position.
         * */
        MKT_NODISCARD auto GetMouseX() const -> double;


        /**
         * @brief Retrieves the Y-coordinate of the mouse position.
         * @returns The Y-coordinate of the mouse position.
         * */
        MKT_NODISCARD auto GetMouseY() const -> double;


        /**
         * @brief Retrieves the current mouse position as a pair of (X, Y) coordinates.
         * @returns The current mouse position as a pair of (X, Y) coordinates.
         * */
        MKT_NODISCARD auto GetMousePos() const -> std::pair<double, double>;

        /**
         * @brief Sets the cursor input mode.
         * For debugging purposes.
         * @param mode The mode to set for the cursor.
         * */
        MKT_UNUSED_FUNC auto SetCursorMode( CursorInputMode mode ) const -> void;

        /**
         * @brief Sets focus to a newly focused window for input handling.
         * Input is handled for a single window at a time.
         * @param newHandle Pointer to the newly focused Window object.
         * */
        auto SetFocus( Window* newHandle ) -> void;


        /**
         * @brief Prints the mouse button.
         * @param button The mouse button to print.
         * */
        static auto PrintMouse( MouseButton button ) -> void;

        /**
         * @brief Prints the key code of a keyboard key. For debugging purposes.
         * @param keycode The key code to print.
         * */
        MKT_UNUSED_FUNC static auto PrintKey( KeyCode keycode ) -> void;


        /**
         * @brief Prints the mouse button. For debugging purposes.
         * @param button The mouse button to print.
         * */
        MKT_UNUSED_FUNC static auto PrintButton( MouseButton button ) -> void;


    private:
        /**< Pointer to the current window being handled. */
        Window* m_Handle{ nullptr };
    };
}// namespace Mikoto

#endif// MIKOTO_INPUT_MANAGER_HH