//    Copyright 2026 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MIKOTO_PLATFORM_INPUT_SYSTEM_HH
#define MIKOTO_PLATFORM_INPUT_SYSTEM_HH

#include <EASTL/utility.h>
#include <EASTL/string.h>
#include <EASTL/hash_map.h>

#include <Core/Core.hh>
#include <Core/Service.hh>
#include <Core/Singleton.hh>

#include <Platform/Window.hh>
#include <Platform/KeyCodes.hh>
#include <Platform/MouseCodes.hh>
#include <Platform/GamepadCodes.hh>
#include <Platform/SteeringWheelCodes.hh>

struct SDL_Gamepad;
struct SDL_Joystick;
union SDL_Event;

namespace mikoto::platform {

    struct InputServiceCreateInfo {
        platform::IWindow* mWindow{ nullptr };
    };

    class InputSystem final : public core::ISubsystem, public core::Singleton<InputSystem> {
    public:
        explicit InputSystem( const InputServiceCreateInfo& options );

        auto Initialize() -> void override;
        auto Shutdown() -> void override;
        auto Update( core::f32 timeStep ) -> void override;

        MKT_NODISCARD auto IsKeyPressed( KeyCode keyCode ) const -> bool;
        MKT_NODISCARD auto IsKeyReleased( KeyCode keyCode ) const -> bool;

        MKT_NODISCARD auto IsMouseKeyPressed( MouseButton button ) const -> bool;
        MKT_NODISCARD auto IsMouseKeyReleased( MouseButton button ) const -> bool;

        MKT_NODISCARD auto GetMouseX() const -> core::f64;
        MKT_NODISCARD auto GetMouseY() const -> core::f64;
        MKT_NODISCARD auto GetMousePos() const -> eastl::pair<core::f64, core::f64>;
        /// Wheel movement accumulated during this frame, shared by all readers.
        MKT_NODISCARD auto GetMouseScrollDelta() const -> eastl::pair<core::f32, core::f32>;
        auto AddMouseScroll( const platform::IWindow* window, core::f32 x, core::f32 y ) -> void;

        /** @brief Tests whether the gamepad with this connection ID is available. */
        MKT_NODISCARD auto IsGamepadConnected( core::u32 deviceID ) const -> bool;
        MKT_NODISCARD auto IsGamepadButtonPressed( core::u32 deviceID, GamepadButton button ) const -> bool;

        /** @returns Stick position in [-1, 1] or trigger pressure in [0, 1]. */
        MKT_NODISCARD auto GetGamepadAxis( core::u32 deviceID, GamepadAxis axis ) const -> core::f32;

        /** @brief Tests whether the steering wheel with this connection ID is available. */
        MKT_NODISCARD auto IsSteeringWheelConnected( core::u32 deviceID ) const -> bool;
        MKT_NODISCARD auto IsSteeringWheelButtonPressed( core::u32 deviceID, SteeringWheelButton button ) const -> bool;

        /** @returns The raw device axis normalized to [-1, 1]. */
        MKT_NODISCARD auto GetSteeringWheelAxis( core::u32 deviceID, SteeringWheelAxis axis ) const -> core::f32;

        MKT_NODISCARD auto GetClipBoardContents() const -> eastl::string;
        MKT_NODISCARD auto HasFocus() const -> bool;

        auto SetFocus( platform::IWindow* newHandle ) -> void;
        auto ClearFocus( const platform::IWindow* window ) -> void;

        ~InputSystem() override = default;

    private:
        friend auto PollWindowEvents() -> void;

        auto ProcessDeviceEvent( const SDL_Event& event ) -> void;

        platform::IWindow* mEventWindow{};
        platform::IWindow* mHandle{ nullptr };
        eastl::pair<core::f32, core::f32> mMouseScroll{};

        eastl::hash_map<core::u32, SDL_Gamepad*> mGamepads{};
        eastl::hash_map<core::u32, SDL_Joystick*> mSteeringWheels{};
        bool mControllerBackendInitialized{};
    };
}// namespace mikoto::platform

#endif// MIKOTO_PLATFORM_INPUT_SYSTEM_HH
