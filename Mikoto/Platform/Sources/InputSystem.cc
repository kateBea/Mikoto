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

#include <EASTL/utility.h>
#include <EASTL/unique_ptr.h>

#include <SDL3/SDL.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Logging/Logger.hh>

#include <Platform/Window.hh>
#include <Platform/CoreEvents.hh>
#include <Platform/EventSystem.hh>
#include <Platform/InputSystem.hh>

namespace mikoto::platform {
        
    using namespace mikoto::core;

    MKT_NODISCARD static auto NormalizeControllerAxis( core::i16 value ) -> core::f32 {
        const core::f32 range{ value < 0 ? -core::f32{ SDL_JOYSTICK_AXIS_MIN } : core::f32{ SDL_JOYSTICK_AXIS_MAX } };
        return core::as<core::f32>( value ) / range;
    }

    InputSystem::InputSystem( const InputServiceCreateInfo& options )
        : mEventWindow{ options.mWindow },
          mHandle{ options.mWindow } {
    }

    auto InputSystem::Initialize() -> void {
        MKT_BEGIN_PROFILER();

        if ( mIsInitialized ) {
            return;
        }

        // SDL's gamepad subsystem also starts joystick support for wheels.
        // https://wiki.libsdl.org/SDL3/SDL_Init
        mControllerBackendInitialized = SDL_InitSubSystem( SDL_INIT_GAMEPAD );
        if ( !mControllerBackendInitialized ) {
            MKT_CORE_LOGGER_WARN( "Could not initialize controller input: {}", SDL_GetError() );
        }

        mIsInitialized = true;
    }

    auto InputSystem::Shutdown() -> void {
        MKT_BEGIN_PROFILER();

        for ( const auto& [id, gamepad]: mGamepads ) {
            SDL_CloseGamepad( gamepad );
        }

        for ( const auto& [id, wheel]: mSteeringWheels ) {
            SDL_CloseJoystick( wheel );
        }

        mGamepads.clear();
        mSteeringWheels.clear();

        if ( mControllerBackendInitialized ) {
            SDL_QuitSubSystem( SDL_INIT_GAMEPAD );
            mControllerBackendInitialized = false;
        }

        mHandle = nullptr;
        mMouseScroll = {};
        mIsInitialized = false;
    }

    auto InputSystem::Update( core::f32 ) -> void {
        MKT_BEGIN_PROFILER();
        // Poll even without focus: focus-gained events must be able to restore input.
        mMouseScroll = {};

        if ( mIsInitialized && mEventWindow ) {
            mEventWindow->ProcessEvents();
        }
    }

    auto InputSystem::SetFocus( platform::IWindow* window ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mHandle != window ) {
            mMouseScroll = {};
        }

        mHandle = window;
    }

    auto InputSystem::HasFocus() const -> bool {
        return mHandle != nullptr;
    }

    auto InputSystem::ClearFocus( const platform::IWindow* window ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mHandle == window ) {
            mHandle = nullptr;
            mMouseScroll = {};
        }
    }

    auto InputSystem::GetMouseScrollDelta() const -> eastl::pair<core::f32, core::f32> {
        return mHandle ? mMouseScroll : eastl::pair<core::f32, core::f32>{};
    }

    auto InputSystem::AddMouseScroll( const platform::IWindow* window, core::f32 x, core::f32 y ) -> void {
        if ( window && window == mHandle ) {
            mMouseScroll.first += x;
            mMouseScroll.second += y;
        }
    }

    auto InputSystem::IsKeyPressed( KeyCode key ) const -> bool {
        MKT_BEGIN_PROFILER();
        return mHandle && mHandle->IsKeyPressed( key );
    }

    auto InputSystem::IsKeyReleased( KeyCode key ) const -> bool {
        MKT_BEGIN_PROFILER();
        return mHandle && mHandle->IsKeyReleased( key );
    }

    auto InputSystem::IsMouseKeyPressed( MouseButton button ) const -> bool {
        MKT_BEGIN_PROFILER();
        return mHandle && mHandle->IsMouseKeyPressed( button );
    }

    auto InputSystem::IsMouseKeyReleased( MouseButton button ) const -> bool {
        MKT_BEGIN_PROFILER();
        return mHandle && mHandle->IsMouseKeyReleased( button );
    }

    auto InputSystem::GetMousePos() const -> eastl::pair<core::f64, core::f64> {
        MKT_BEGIN_PROFILER();
        return mHandle ? mHandle->GetMousePos() : eastl::pair<core::f64, core::f64>{};
    }

    auto InputSystem::GetMouseX() const -> core::f64 {
        MKT_BEGIN_PROFILER();
        return GetMousePos().first;
    }

    auto InputSystem::GetMouseY() const -> core::f64 {
        MKT_BEGIN_PROFILER();
        return GetMousePos().second;
    }

    auto InputSystem::IsGamepadConnected( core::u32 deviceID ) const -> bool {
        return mGamepads.contains( deviceID );
    }

    auto InputSystem::IsGamepadButtonPressed( core::u32 deviceID, GamepadButton button ) const -> bool {
        const auto gamepad{ mGamepads.find( deviceID ) };
        return gamepad != mGamepads.end() && button >= GamepadButton::eSouth && button < GamepadButton::eCount &&
            SDL_GetGamepadButton( gamepad->second, static_cast<SDL_GamepadButton>( button ) );
    }

    auto InputSystem::GetGamepadAxis( core::u32 deviceID, GamepadAxis axis ) const -> core::f32 {
        const auto gamepad{ mGamepads.find( deviceID ) };
        if ( gamepad == mGamepads.end() || axis < GamepadAxis::eLeftX || axis >= GamepadAxis::eCount ) {
            return 0.0f;
        }

        return NormalizeControllerAxis( SDL_GetGamepadAxis( gamepad->second, static_cast<SDL_GamepadAxis>( axis ) ) );
    }

    auto InputSystem::IsSteeringWheelConnected( core::u32 deviceID ) const -> bool {
        return mSteeringWheels.contains( deviceID );
    }

    auto InputSystem::IsSteeringWheelButtonPressed( core::u32 deviceID, SteeringWheelButton button ) const -> bool {
        const auto wheel{ mSteeringWheels.find( deviceID ) };
        const auto index{ core::as<core::i32>( button ) };
        return wheel != mSteeringWheels.end() && index >= 0 && index < SDL_GetNumJoystickButtons( wheel->second ) &&
            SDL_GetJoystickButton( wheel->second, index );
    }

    auto InputSystem::GetSteeringWheelAxis( core::u32 deviceID, SteeringWheelAxis axis ) const -> core::f32 {
        const auto wheel{ mSteeringWheels.find( deviceID ) };
        const auto index{ core::as<core::i32>( axis ) };
        if ( wheel == mSteeringWheels.end() || index < 0 || index >= SDL_GetNumJoystickAxes( wheel->second ) ) {
            return 0.0f;
        }

        return NormalizeControllerAxis( SDL_GetJoystickAxis( wheel->second, index ) );
    }

    auto InputSystem::ProcessDeviceEvent( const SDL_Event& event ) -> void {
        auto* events{ EventSystem::GetPtr() };
        if ( !mControllerBackendInitialized || !events || !events->IsInitialized() ) {
            return;
        }

        // Mapped gamepads also emit joystick events. Use the joystick path for wheels.
        // https://wiki.libsdl.org/SDL3/SDL_GamepadDeviceEvent
        switch ( event.type ) {
            case SDL_EVENT_GAMEPAD_ADDED: {
                const auto id{ event.gdevice.which };
                if ( !IsGamepadConnected( id ) && SDL_GetJoystickTypeForID( id ) != SDL_JOYSTICK_TYPE_WHEEL ) {
                    eastl::unique_ptr<SDL_Gamepad, decltype( &SDL_CloseGamepad )> gamepad{ SDL_OpenGamepad( id ), &SDL_CloseGamepad };
                    if ( gamepad ) {
                        mGamepads.emplace( id, gamepad.get() );
                        gamepad.release();
                        events->Queue<InputDeviceConnectionEvent>( id, InputDeviceType::eGamepad, true );
                    }
                }
                break;
            }
            case SDL_EVENT_GAMEPAD_REMOVED: {
                const auto id{ event.gdevice.which };
                if ( const auto gamepad{ mGamepads.find( id ) }; gamepad != mGamepads.end() ) {
                    SDL_CloseGamepad( gamepad->second );
                    mGamepads.erase( gamepad );
                    events->Queue<InputDeviceConnectionEvent>( id, InputDeviceType::eGamepad, false );
                }
                break;
            }
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            case SDL_EVENT_GAMEPAD_BUTTON_UP: {
                const auto button{ static_cast<GamepadButton>( event.gbutton.button ) };
                if ( IsGamepadConnected( event.gbutton.which ) && button >= GamepadButton::eSouth && button < GamepadButton::eCount ) {
                    if ( event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN ) {
                        events->Queue<GamepadButtonPressedEvent>( event.gbutton.which, button );
                    }
                    else {
                        events->Queue<GamepadButtonReleasedEvent>( event.gbutton.which, button );
                    }
                }
                break;
            }
            case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
                const auto axis{ static_cast<GamepadAxis>( event.gaxis.axis ) };
                if ( IsGamepadConnected( event.gaxis.which ) && axis >= GamepadAxis::eLeftX && axis < GamepadAxis::eCount ) {
                    events->Queue<GamepadAxisEvent>( event.gaxis.which, axis, NormalizeControllerAxis( event.gaxis.value ) );
                }
                break;
            }
            case SDL_EVENT_JOYSTICK_ADDED: {
                const auto id{ event.jdevice.which };
                if ( !IsSteeringWheelConnected( id ) && SDL_GetJoystickTypeForID( id ) == SDL_JOYSTICK_TYPE_WHEEL ) {
                    eastl::unique_ptr<SDL_Joystick, decltype( &SDL_CloseJoystick )> wheel{ SDL_OpenJoystick( id ), &SDL_CloseJoystick };
                    if ( wheel ) {
                        mSteeringWheels.emplace( id, wheel.get() );
                        wheel.release();
                        events->Queue<InputDeviceConnectionEvent>( id, InputDeviceType::eSteeringWheel, true );
                    }
                }
                break;
            }
            case SDL_EVENT_JOYSTICK_REMOVED: {
                const auto id{ event.jdevice.which };
                if ( const auto wheel{ mSteeringWheels.find( id ) }; wheel != mSteeringWheels.end() ) {
                    SDL_CloseJoystick( wheel->second );
                    mSteeringWheels.erase( wheel );
                    events->Queue<InputDeviceConnectionEvent>( id, InputDeviceType::eSteeringWheel, false );
                }
                break;
            }
            case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
            case SDL_EVENT_JOYSTICK_BUTTON_UP:
                if ( IsSteeringWheelConnected( event.jbutton.which ) ) {
                    const auto button{ static_cast<SteeringWheelButton>( event.jbutton.button ) };
                    if ( event.type == SDL_EVENT_JOYSTICK_BUTTON_DOWN ) {
                        events->Queue<SteeringWheelButtonPressedEvent>( event.jbutton.which, button );
                    }
                    else {
                        events->Queue<SteeringWheelButtonReleasedEvent>( event.jbutton.which, button );
                    }
                }
                break;
            case SDL_EVENT_JOYSTICK_AXIS_MOTION:
                if ( IsSteeringWheelConnected( event.jaxis.which ) ) {
                    events->Queue<SteeringWheelAxisEvent>( event.jaxis.which, static_cast<SteeringWheelAxis>( event.jaxis.axis ), NormalizeControllerAxis( event.jaxis.value ) );
                }
                break;
            default:
                break;
        }
    }

    auto InputSystem::GetClipBoardContents() const -> eastl::string {
        MKT_BEGIN_PROFILER();
        return mHandle ? mHandle->GetClipBoardContents() : eastl::string{};
    }
}// namespace mikoto::platform
