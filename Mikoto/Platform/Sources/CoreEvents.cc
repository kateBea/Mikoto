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

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Platform/Event.hh>
#include <Platform/Utility.hh>
#include <Platform/CoreEvents.hh>

namespace mikoto::platform {
    using namespace mikoto::core;

    WindowResizedEvent::WindowResizedEvent( core::i32 newWidth, core::i32 newHeight )
        : Event{ GetStaticType(), GetCategoryFromType( GetStaticType() ) }, mWidth{ newWidth }, mHeight{ newHeight } {
    }


    auto WindowResizedEvent::GetWidth() const -> core::i32 {
        MKT_BEGIN_PROFILER();
        return mWidth;
    }


    auto WindowResizedEvent::GetHeight() const -> core::i32 {
        MKT_BEGIN_PROFILER();
        return mHeight;
    }


    auto WindowResizedEvent::GetType() const -> EventType {
        MKT_BEGIN_PROFILER();
        return GetStaticType();
    }


    auto WindowResizedEvent::GetStaticType() -> EventType {
        MKT_BEGIN_PROFILER();
        return EventType::WINDOW_RESIZE_EVENT;
    }


    auto WindowResizedEvent::DisplayData() const -> eastl::string {
        MKT_BEGIN_PROFILER();
        return string::Format( "{}! New Dimensions [{},{}]", GetEventFormattedStr( GetType() ).data(), mWidth, mHeight );
    }


    auto WindowResizedEvent::ToString() const -> eastl::string_view {
        MKT_BEGIN_PROFILER();
        return GetEventFormattedStr( GetType() );
    }


    WindowCloseEvent::WindowCloseEvent()
        : Event{ GetStaticType(), GetCategoryFromType( GetStaticType() ) } {
    }


    auto WindowCloseEvent::GetType() const -> EventType {
        MKT_BEGIN_PROFILER();
        return GetStaticType();
    }


    auto WindowCloseEvent::GetStaticType() -> EventType {
        MKT_BEGIN_PROFILER();
        return EventType::WINDOW_CLOSE_EVENT;
    }


    auto WindowCloseEvent::DisplayData() const -> eastl::string {
        MKT_BEGIN_PROFILER();
        return string::Format( "{}!", GetEventFormattedStr( GetType() ).data() );
    }


    auto WindowCloseEvent::ToString() const -> eastl::string_view {
        MKT_BEGIN_PROFILER();
        return GetEventFormattedStr( GetType() );
    }


    AppTick::AppTick()
        : Event{ GetStaticType(), GetCategoryFromType( GetStaticType() ) } {
    }


    auto AppTick::GetType() const -> EventType {
        MKT_BEGIN_PROFILER();
        return GetStaticType();
    }


    auto AppTick::GetStaticType() -> EventType {
        MKT_BEGIN_PROFILER();
        return EventType::APP_TICK_EVENT;
    }


    auto AppTick::DisplayData() const -> eastl::string {
        MKT_BEGIN_PROFILER();
        return string::Format( "{}!", GetEventFormattedStr( GetType() ).data() );
    }


    auto AppTick::ToString() const -> eastl::string_view {
        MKT_BEGIN_PROFILER();
        return GetEventFormattedStr( GetType() );
    }


    CameraEnableRotation::CameraEnableRotation()
        : Event{ GetStaticType(), GetCategoryFromType( GetStaticType() ) } {
    }


    auto CameraEnableRotation::GetType() const -> EventType {
        MKT_BEGIN_PROFILER();
        return GetStaticType();
    }


    auto CameraEnableRotation::GetStaticType() -> EventType {
        MKT_BEGIN_PROFILER();
        return EventType::CAMERA_ENABLE_ROTATION;
    }


    auto CameraEnableRotation::DisplayData() const -> eastl::string {
        MKT_BEGIN_PROFILER();
        return string::Format( "{}!", GetEventFormattedStr( GetType() ).data() );
    }


    auto CameraEnableRotation::ToString() const -> eastl::string_view {
        MKT_BEGIN_PROFILER();
        return GetEventFormattedStr( GetType() );
    }


    AppClose::AppClose()
        : Event{ GetStaticType(), GetCategoryFromType( GetStaticType() ) } {
    }


    auto AppClose::GetType() const -> EventType {
        MKT_BEGIN_PROFILER();
        return GetStaticType();
    }


    auto AppClose::GetStaticType() -> EventType {
        MKT_BEGIN_PROFILER();
        return EventType::APP_CLOSE_EVENT;
    }


    auto AppClose::DisplayData() const -> eastl::string {
        MKT_BEGIN_PROFILER();
        return string::Format( "{}!", GetEventFormattedStr( GetType() ).data() );
    }


    auto AppClose::ToString() const -> eastl::string_view {
        MKT_BEGIN_PROFILER();
        return GetEventFormattedStr( GetType() );
    }


    AppUpdate::AppUpdate()
        : Event{ GetStaticType(), GetCategoryFromType( GetStaticType() ) } {
    }


    auto AppUpdate::GetType() const -> EventType {
        MKT_BEGIN_PROFILER();
        return GetStaticType();
    }


    auto AppUpdate::GetStaticType() -> EventType {
        MKT_BEGIN_PROFILER();
        return EventType::APP_UPDATE_EVENT;
    }


    auto AppUpdate::DisplayData() const -> eastl::string {
        MKT_BEGIN_PROFILER();
        return string::Format( "{}!", GetEventFormattedStr( GetType() ).data() );
    }


    auto AppUpdate::ToString() const -> eastl::string_view {
        MKT_BEGIN_PROFILER();
        return GetEventFormattedStr( GetType() );
    }


    AppRender::AppRender()
        : Event{ GetStaticType(), GetCategoryFromType( GetStaticType() ) } {
    }


    auto AppRender::GetType() const -> EventType {
        MKT_BEGIN_PROFILER();
        return GetStaticType();
    }


    auto AppRender::GetStaticType() -> EventType {
        MKT_BEGIN_PROFILER();
        return EventType::APP_RENDER_EVENT;
    }


    auto AppRender::DisplayData() const -> eastl::string {
        MKT_BEGIN_PROFILER();
        return string::Format( "{}", GetEventFormattedStr( GetType() ).data() );
    }


    auto AppRender::ToString() const -> eastl::string_view {
        MKT_BEGIN_PROFILER();
        return GetEventFormattedStr( GetType() );
    }


    auto KeyEvent::GetKeyCode() const -> KeyCode {
        MKT_BEGIN_PROFILER();
        return mKeyCode;
    }


    KeyEvent::KeyEvent( EventType type, KeyCode keyCode )
        : Event{ type, GetCategoryFromType( type ) }, mKeyCode{ keyCode } {
    }


    KeyPressedEvent::KeyPressedEvent( const KeyCode keyCode, const bool repeated, ModKey modifiers )
        : KeyEvent{ GetStaticType(), keyCode }, mRepeated{ repeated }, mModifiers{ modifiers } {
    }


    auto KeyPressedEvent::IsRepeated() const -> bool {
        MKT_BEGIN_PROFILER();
        return mRepeated;
    }


    auto KeyPressedEvent::GetModifiers() const -> ModKey {
        MKT_BEGIN_PROFILER();
        return mModifiers;
    }


    auto KeyPressedEvent::IsModActive( ModKey mod ) const -> bool {
        MKT_BEGIN_PROFILER();
        return ( mModifiers & mod ) != ModKey::eNone;
    }


    auto KeyPressedEvent::GetType() const -> EventType {
        MKT_BEGIN_PROFILER();
        return GetStaticType();
    }


    auto KeyPressedEvent::GetStaticType() -> EventType {
        MKT_BEGIN_PROFILER();
        return EventType::KEY_PRESSED_EVENT;
    }


    auto KeyPressedEvent::DisplayData() const -> eastl::string {
        MKT_BEGIN_PROFILER();
        return string::Format( "{}! Key {}. Repeated? {}", GetEventFormattedStr( GetType() ).data(), core::as<core::i32>( mKeyCode ), mRepeated ? "Yes" : "No" );
    }


    auto KeyPressedEvent::ToString() const -> eastl::string_view {
        MKT_BEGIN_PROFILER();
        return GetEventFormattedStr( GetType() );
    }


    KeyReleasedEvent::KeyReleasedEvent( KeyCode code )
        : KeyEvent{ GetStaticType(), code } {
    }


    auto KeyReleasedEvent::GetType() const -> EventType {
        MKT_BEGIN_PROFILER();
        return GetStaticType();
    }


    auto KeyReleasedEvent::GetStaticType() -> EventType {
        MKT_BEGIN_PROFILER();
        return EventType::KEY_RELEASED_EVENT;
    }


    auto KeyReleasedEvent::DisplayData() const -> eastl::string {
        MKT_BEGIN_PROFILER();
        return string::Format( "{}! Key {}", GetEventFormattedStr( GetType() ).data(), core::as<core::i32>( mKeyCode ) );
    }


    auto KeyReleasedEvent::ToString() const -> eastl::string_view {
        MKT_BEGIN_PROFILER();
        return GetEventFormattedStr( GetType() );
    }


    KeyCharEvent::KeyCharEvent( core::u32 charCode )
        : Event{ GetStaticType(), GetCategoryFromType( GetStaticType() ) }, mKeyChar{ charCode } {
    }


    auto KeyCharEvent::GetType() const -> EventType {
        MKT_BEGIN_PROFILER();
        return GetStaticType();
    }


    auto KeyCharEvent::GetChar() const -> core::u32 {
        MKT_BEGIN_PROFILER();
        return mKeyChar;
    }


    auto KeyCharEvent::GetStaticType() -> EventType {
        MKT_BEGIN_PROFILER();
        return EventType::KEY_CHAR_EVENT;
    }


    auto KeyCharEvent::DisplayData() const -> eastl::string {
        MKT_BEGIN_PROFILER();
        return string::Format( "{}! Key {}", GetEventFormattedStr( GetType() ).data(), mKeyChar );
    }


    auto KeyCharEvent::ToString() const -> eastl::string_view {
        MKT_BEGIN_PROFILER();
        return GetEventFormattedStr( GetType() );
    }


    ClipboardSetEvent::ClipboardSetEvent( eastl::span<eastl::string> contents )
        : Event{ GetStaticType(), GetCategoryFromType( GetStaticType() ) },
          mContents{ contents.begin(), contents.end() } {
    }


    auto ClipboardSetEvent::GetType() const -> EventType {
        MKT_BEGIN_PROFILER();
        return GetStaticType();
    }


    auto ClipboardSetEvent::GetContents() const -> const eastl::fixed_vector<eastl::string, 15>& {
        MKT_BEGIN_PROFILER();
        return mContents;
    }


    auto ClipboardSetEvent::GetStaticType() -> EventType {
        MKT_BEGIN_PROFILER();
        return EventType::CLIPBOARD_EVENT;
    }


    auto ClipboardSetEvent::DisplayData() const -> eastl::string {
        MKT_BEGIN_PROFILER();
        return "NOT_IMPLEMENTED";
    }


    auto ClipboardSetEvent::ToString() const -> eastl::string_view {
        MKT_BEGIN_PROFILER();
        return GetEventFormattedStr( GetType() );
    }


    ContentDroppedEvent::ContentDroppedEvent( core::i32 count, const char** contents )
        : Event{ GetStaticType(), GetCategoryFromType( GetStaticType() ) } {
        MKT_BEGIN_PROFILER();
        mContents.reserve( count );

        for ( core::i32 i{}; i < count; ++i ) {
            mContents.emplace_back( contents[i] );
        }
    }


    auto ContentDroppedEvent::GetType() const -> EventType {
        MKT_BEGIN_PROFILER();
        return GetStaticType();
    }


    auto ContentDroppedEvent::GetContents() const -> const eastl::fixed_vector<eastl::string, 15>& {
        MKT_BEGIN_PROFILER();
        return mContents;
    }


    auto ContentDroppedEvent::GetStaticType() -> EventType {
        MKT_BEGIN_PROFILER();
        return EventType::CONTENT_DROPPED_EVENT;
    }


    auto ContentDroppedEvent::DisplayData() const -> eastl::string {
        MKT_BEGIN_PROFILER();
        return "NOT_IMPLEMENTED";
    }


    auto ContentDroppedEvent::ToString() const -> eastl::string_view {
        MKT_BEGIN_PROFILER();
        return GetEventFormattedStr( GetType() );
    }


    MouseMovedEvent::MouseMovedEvent( double x, double y )
        : Event{ GetStaticType(), GetCategoryFromType( GetStaticType() ) }, mPositionX{ x }, mPositionY{ y } {
    }


    auto MouseMovedEvent::GetPositionX() const -> double {
        MKT_BEGIN_PROFILER();
        return mPositionX;
    }


    auto MouseMovedEvent::GetPositionY() const -> double {
        MKT_BEGIN_PROFILER();
        return mPositionY;
    }


    auto MouseMovedEvent::GetType() const -> EventType {
        MKT_BEGIN_PROFILER();
        return GetStaticType();
    }


    auto MouseMovedEvent::GetStaticType() -> EventType {
        MKT_BEGIN_PROFILER();
        return EventType::MOUSE_MOVED_EVENT;
    }


    auto MouseMovedEvent::DisplayData() const -> eastl::string {
        MKT_BEGIN_PROFILER();
        return string::Format( "{}! Position [{},{}]", GetEventFormattedStr( GetType() ).data(), mPositionX, mPositionY );
    }


    auto MouseMovedEvent::ToString() const -> eastl::string_view {
        MKT_BEGIN_PROFILER();
        return GetEventFormattedStr( GetType() );
    }


    MouseEvent::MouseEvent( EventType type )
        : Event{ type, GetCategoryFromType( type ) } {
    }


    MouseButtonPressedEvent::MouseButtonPressedEvent( MouseButton button, ModKey modifiers )
        : MouseEvent{ GetStaticType() }, mButton{ button }, mModifiers{ modifiers } {
    }


    auto MouseButtonPressedEvent::GetMouseButton() const -> MouseButton {
        MKT_BEGIN_PROFILER();
        return mButton;
    }


    auto MouseButtonPressedEvent::GetModifiers() const -> ModKey {
        MKT_BEGIN_PROFILER();
        return mModifiers;
    }


    auto MouseButtonPressedEvent::IsModActive( ModKey mod ) const -> bool {
        MKT_BEGIN_PROFILER();
        return ( mModifiers & mod ) != ModKey::eNone;
    }


    auto MouseButtonPressedEvent::GetType() const -> EventType {
        MKT_BEGIN_PROFILER();
        return GetStaticType();
    }


    auto MouseButtonPressedEvent::GetStaticType() -> EventType {
        MKT_BEGIN_PROFILER();
        return EventType::MOUSE_BUTTON_PRESSED_EVENT;
    }


    auto MouseButtonPressedEvent::DisplayData() const -> eastl::string {
        MKT_BEGIN_PROFILER();
        return string::Format( "{}! Button {}", GetEventFormattedStr( GetType() ).data(), GetStringRepresentation( mButton ).data() );
    }


    auto MouseButtonPressedEvent::ToString() const -> eastl::string_view {
        MKT_BEGIN_PROFILER();
        return GetEventFormattedStr( GetType() );
    }


    MouseButtonReleasedEvent::MouseButtonReleasedEvent( MouseButton button )
        : MouseEvent{ GetStaticType() }, mButton{ button } {
    }


    auto MouseButtonReleasedEvent::GetMouseButton() const -> MouseButton {
        MKT_BEGIN_PROFILER();
        return mButton;
    }


    auto MouseButtonReleasedEvent::GetType() const -> EventType {
        MKT_BEGIN_PROFILER();
        return GetStaticType();
    }


    auto MouseButtonReleasedEvent::GetStaticType() -> EventType {
        MKT_BEGIN_PROFILER();
        return EventType::MOUSE_BUTTON_RELEASED_EVENT;
    }


    auto MouseButtonReleasedEvent::DisplayData() const -> eastl::string {
        MKT_BEGIN_PROFILER();
        constexpr static eastl::array<eastl::string_view, 3> NAME{ "LEFT_CLICK", "RIGHT_CLICK", "SCROLL_WHEEL_CLICK" };
        return string::Format( "{}! Button {}", GetEventFormattedStr( GetType() ).data(), GetStringRepresentation( mButton ).data() );
    }


    auto MouseButtonReleasedEvent::ToString() const -> eastl::string_view {
        MKT_BEGIN_PROFILER();
        return GetEventFormattedStr( GetType() );
    }


    MouseScrollEvent::MouseScrollEvent( double xOffset, double yOffset )
        : MouseEvent{ GetStaticType() }, mOffsetX{ xOffset }, mOffsetY{ yOffset } {
    }


    auto MouseScrollEvent::GetOffsetX() const -> double {
        MKT_BEGIN_PROFILER();
        return mOffsetX;
    }


    auto MouseScrollEvent::GetOffsetY() const -> double {
        MKT_BEGIN_PROFILER();
        return mOffsetY;
    }


    auto MouseScrollEvent::GetType() const -> EventType {
        MKT_BEGIN_PROFILER();
        return GetStaticType();
    }


    auto MouseScrollEvent::GetStaticType() -> EventType {
        MKT_BEGIN_PROFILER();
        return EventType::MOUSE_SCROLLED_EVENT;
    }


    auto MouseScrollEvent::DisplayData() const -> eastl::string {
        MKT_BEGIN_PROFILER();
        return string::Format( "{}! Offsets [{},{}]", GetEventFormattedStr( GetType() ).data(), mOffsetX, mOffsetY );
    }


    auto MouseScrollEvent::ToString() const -> eastl::string_view {
        MKT_BEGIN_PROFILER();
        return GetEventFormattedStr( GetType() );
    }

    InputDeviceConnectionEvent::InputDeviceConnectionEvent( core::u32 deviceID, InputDeviceType deviceType, bool connected )
        : Event{ GetStaticType(), GetCategoryFromType( GetStaticType() ) },
          mDeviceID{ deviceID },
          mDeviceType{ deviceType },
          mConnected{ connected } {
    }

    auto InputDeviceConnectionEvent::DisplayData() const -> eastl::string {
        return string::Format( "{} {} {}", mDeviceType == InputDeviceType::eGamepad ? "Gamepad" : "Steering wheel",
            mDeviceID, mConnected ? "connected" : "disconnected" );
    }


    GamepadButtonEvent::GamepadButtonEvent( EventType type, core::u32 deviceID, GamepadButton button )
        : Event{ type, GetCategoryFromType( type ) }, mDeviceID{ deviceID }, mButton{ button } {
    }


    auto GamepadButtonEvent::GetDeviceID() const -> core::u32 {
        MKT_BEGIN_PROFILER();
        return mDeviceID;
    }


    auto GamepadButtonEvent::GetGamepadButton() const -> GamepadButton {
        MKT_BEGIN_PROFILER();
        return mButton;
    }


    auto GamepadButtonEvent::DisplayData() const -> eastl::string {
        MKT_BEGIN_PROFILER();
        return string::Format( "{}! Device {} button {}", ToString().data(), mDeviceID, core::as<core::i32>( mButton ) );
    }


    auto GamepadButtonEvent::ToString() const -> eastl::string_view {
        MKT_BEGIN_PROFILER();
        return GetEventFormattedStr( GetType() );
    }


    GamepadButtonPressedEvent::GamepadButtonPressedEvent( core::u32 deviceID, GamepadButton button )
        : GamepadButtonEvent{ GetStaticType(), deviceID, button } {
    }


    auto GamepadButtonPressedEvent::GetType() const -> EventType {
        MKT_BEGIN_PROFILER();
        return GetStaticType();
    }


    auto GamepadButtonPressedEvent::GetStaticType() -> EventType {
        MKT_BEGIN_PROFILER();
        return EventType::GAMEPAD_BUTTON_PRESSED_EVENT;
    }


    GamepadButtonReleasedEvent::GamepadButtonReleasedEvent( core::u32 deviceID, GamepadButton button )
        : GamepadButtonEvent{ GetStaticType(), deviceID, button } {
    }


    auto GamepadButtonReleasedEvent::GetType() const -> EventType {
        MKT_BEGIN_PROFILER();
        return GetStaticType();
    }


    auto GamepadButtonReleasedEvent::GetStaticType() -> EventType {
        MKT_BEGIN_PROFILER();
        return EventType::GAMEPAD_BUTTON_RELEASED_EVENT;
    }


    SteeringWheelButtonEvent::SteeringWheelButtonEvent( EventType type, core::u32 deviceID, SteeringWheelButton button )
        : Event{ type, GetCategoryFromType( type ) }, mDeviceID{ deviceID }, mButton{ button } {
    }


    auto SteeringWheelButtonEvent::GetDeviceID() const -> core::u32 {
        MKT_BEGIN_PROFILER();
        return mDeviceID;
    }


    auto SteeringWheelButtonEvent::GetSteeringWheelButton() const -> SteeringWheelButton {
        MKT_BEGIN_PROFILER();
        return mButton;
    }


    auto SteeringWheelButtonEvent::DisplayData() const -> eastl::string {
        MKT_BEGIN_PROFILER();
        return string::Format( "{}! Device {} button {}", ToString().data(), mDeviceID, core::as<core::i32>( mButton ) );
    }


    auto SteeringWheelButtonEvent::ToString() const -> eastl::string_view {
        MKT_BEGIN_PROFILER();
        return GetEventFormattedStr( GetType() );
    }


    SteeringWheelButtonPressedEvent::SteeringWheelButtonPressedEvent( core::u32 deviceID, SteeringWheelButton button )
        : SteeringWheelButtonEvent{ GetStaticType(), deviceID, button } {
    }


    auto SteeringWheelButtonPressedEvent::GetType() const -> EventType {
        MKT_BEGIN_PROFILER();
        return GetStaticType();
    }


    auto SteeringWheelButtonPressedEvent::GetStaticType() -> EventType {
        MKT_BEGIN_PROFILER();
        return EventType::STEERING_WHEEL_BUTTON_PRESSED_EVENT;
    }


    SteeringWheelButtonReleasedEvent::SteeringWheelButtonReleasedEvent( core::u32 deviceID, SteeringWheelButton button )
        : SteeringWheelButtonEvent{ GetStaticType(), deviceID, button } {
    }


    auto SteeringWheelButtonReleasedEvent::GetType() const -> EventType {
        MKT_BEGIN_PROFILER();
        return GetStaticType();
    }


    auto SteeringWheelButtonReleasedEvent::GetStaticType() -> EventType {
        MKT_BEGIN_PROFILER();
        return EventType::STEERING_WHEEL_BUTTON_RELEASED_EVENT;
    }
}
