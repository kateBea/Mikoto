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

#ifndef MIKOTO_PLATFORM_CORE_EVENTS_HH
#define MIKOTO_PLATFORM_CORE_EVENTS_HH

#include <EASTL/span.h>
#include <EASTL/array.h>
#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/string_view.h>
#include <EASTL/fixed_vector.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Platform/Event.hh>
#include <Platform/KeyCodes.hh>
#include <Platform/MouseCodes.hh>
#include <Platform/GamepadCodes.hh>
#include <Platform/SteeringWheelCodes.hh>

namespace mikoto::platform {

    class WindowResizedEvent final : public Event {
    public:
        WindowResizedEvent(core::i32 newWidth, core::i32 newHeight);

        MKT_NODISCARD auto GetWidth() const -> core::i32;
        MKT_NODISCARD auto GetHeight() const -> core::i32;
        MKT_NODISCARD auto GetType() const -> EventType override;

        MKT_NODISCARD auto DisplayData() const -> eastl::string override;

        MKT_NODISCARD static auto GetStaticType() -> EventType;
    
    protected:
        MKT_NODISCARD auto ToString() const -> eastl::string_view override;

        core::i32 mWidth{};
        core::i32 mHeight{};
    };

    class WindowCloseEvent final : public Event {
    public:
        explicit WindowCloseEvent();

        MKT_NODISCARD auto GetType() const -> EventType override;
        MKT_NODISCARD auto DisplayData() const -> eastl::string override;
        
        MKT_NODISCARD static auto GetStaticType() -> EventType;

    protected:
        MKT_NODISCARD auto ToString() const -> eastl::string_view override;
    };

    class AppTick final : public Event {
    public:
        explicit AppTick();

        MKT_NODISCARD auto GetType() const -> EventType override;
        MKT_NODISCARD auto DisplayData() const -> eastl::string override;
        
        MKT_NODISCARD static auto GetStaticType() -> EventType;

    protected:
        MKT_NODISCARD auto ToString() const -> eastl::string_view override;
    };

    class CameraEnableRotation final : public Event {
    public:
        explicit CameraEnableRotation();

        MKT_NODISCARD auto GetType() const -> EventType override;
        MKT_NODISCARD auto DisplayData() const -> eastl::string override;
        
        MKT_NODISCARD static auto GetStaticType() -> EventType;

    protected:
        MKT_NODISCARD auto ToString() const -> eastl::string_view override;
    };

    class AppClose final : public Event {
    public:
        explicit AppClose();

        MKT_NODISCARD auto GetType() const -> EventType override;
        MKT_NODISCARD auto DisplayData() const -> eastl::string override;

        MKT_NODISCARD static auto GetStaticType() -> EventType;

    protected:
        MKT_NODISCARD auto ToString() const -> eastl::string_view override;
    };

    class AppUpdate final : public Event {
    public:
        explicit AppUpdate();

        MKT_NODISCARD auto GetType() const -> EventType override;
        MKT_NODISCARD auto DisplayData() const -> eastl::string override;
        
        MKT_NODISCARD static auto GetStaticType() -> EventType;

    protected:
        MKT_NODISCARD auto ToString() const -> eastl::string_view override;
    };

    class AppRender final : public Event {
    public:
        explicit AppRender();

        MKT_NODISCARD auto GetType() const -> EventType override;
        MKT_NODISCARD auto DisplayData() const -> eastl::string override;

        MKT_NODISCARD static auto GetStaticType() -> EventType;
    
    protected:
        MKT_NODISCARD auto ToString() const -> eastl::string_view override;
    };


    class KeyEvent : public Event {
    public:
        MKT_NODISCARD auto GetKeyCode() const -> KeyCode;

    protected:
        KeyEvent(EventType type, KeyCode keyCode);

        KeyCode mKeyCode{ Key_Unknown };
    };

    class KeyPressedEvent final : public KeyEvent {
    public:
        explicit KeyPressedEvent(const KeyCode keyCode, const bool repeated = false, ModKey modifiers = ModKey::eNone);

        MKT_NODISCARD auto GetModifiers() const -> ModKey;
        MKT_NODISCARD auto GetType() const -> EventType override;
        
        MKT_NODISCARD auto IsRepeated() const -> bool;
        MKT_NODISCARD auto IsModActive( ModKey mod ) const -> bool;

        MKT_NODISCARD auto DisplayData() const -> eastl::string override;

        MKT_NODISCARD static auto GetStaticType() -> EventType;
    private:
        MKT_NODISCARD auto ToString() const -> eastl::string_view override;

        bool mRepeated{};
        ModKey mModifiers{ ModKey::eNone };
    };

    class KeyReleasedEvent final : public KeyEvent {
    public:
        explicit KeyReleasedEvent(KeyCode code);

        MKT_NODISCARD auto GetType() const -> EventType override;
        MKT_NODISCARD auto DisplayData() const -> eastl::string override;
        
        MKT_NODISCARD static auto GetStaticType() -> EventType;
    private:
        MKT_NODISCARD auto ToString() const -> eastl::string_view override;
    };

    class KeyCharEvent final : public Event {
    public:
        explicit KeyCharEvent(core::u32 charCode);

        MKT_NODISCARD auto GetType() const -> EventType override;
        MKT_NODISCARD auto GetChar() const -> core::u32;

        MKT_NODISCARD auto DisplayData() const -> eastl::string override;
        
        MKT_NODISCARD static auto GetStaticType() -> EventType;
    private:
        MKT_NODISCARD auto ToString() const -> eastl::string_view override;

        core::u32 mKeyChar{};
    };

    class ClipboardSetEvent final : public Event {
    public:
        explicit ClipboardSetEvent(eastl::span<eastl::string> contents);

        MKT_NODISCARD auto GetType() const -> EventType override;
        MKT_NODISCARD auto GetContents() const -> const eastl::fixed_vector<eastl::string, 15>&;
        
        MKT_NODISCARD auto DisplayData() const -> eastl::string override;
        
        MKT_NODISCARD static auto GetStaticType() -> EventType;

    protected:
        MKT_NODISCARD auto ToString() const -> eastl::string_view override;

    protected:
        static constexpr core::u32 kMaxContents{ 15 };
        eastl::fixed_vector<eastl::string, kMaxContents> mContents{};
    };

    class ContentDroppedEvent final : public Event {
    public:
        explicit ContentDroppedEvent(core::i32 count, const char** contents);

        MKT_NODISCARD auto GetType() const -> EventType override;

        MKT_NODISCARD auto GetContents() const -> const eastl::fixed_vector<eastl::string, 15>&;
        MKT_NODISCARD auto DisplayData() const -> eastl::string override;
        
        MKT_NODISCARD static auto GetStaticType() -> EventType;
    protected:
        MKT_NODISCARD auto ToString() const -> eastl::string_view override;

    protected:
        static constexpr core::u32 kMaxContents{ 15 };
        eastl::fixed_vector<eastl::string, kMaxContents> mContents{};
    };

    class MouseMovedEvent final : public Event {
    public:
        MouseMovedEvent(double x, double y);

        MKT_NODISCARD auto GetPositionX() const -> double;
        MKT_NODISCARD auto GetPositionY() const -> double;
        MKT_NODISCARD auto GetType() const -> EventType override;
        
        MKT_NODISCARD auto DisplayData() const -> eastl::string override;
        
        MKT_NODISCARD static auto GetStaticType() -> EventType;

    protected:
        MKT_NODISCARD auto ToString() const -> eastl::string_view override;

        core::f64 mPositionX{};
        core::f64 mPositionY{};
    };

    class MouseEvent : public Event {
    protected:
        explicit MouseEvent(EventType type);
    };

    class MouseButtonPressedEvent final : public MouseEvent {
    public:
        explicit MouseButtonPressedEvent(MouseButton button, ModKey modifiers = ModKey::eNone);

        MKT_NODISCARD auto GetMouseButton() const -> MouseButton;

        MKT_NODISCARD auto GetModifiers() const -> ModKey;
        MKT_NODISCARD auto IsModActive( ModKey mod ) const -> bool;
        MKT_NODISCARD auto GetType() const -> EventType override;
        MKT_NODISCARD auto DisplayData() const -> eastl::string override;
        
        MKT_NODISCARD static auto GetStaticType() -> EventType;
    
    protected:
        MKT_NODISCARD auto ToString() const -> eastl::string_view override;

        MouseButton mButton{ Mouse_Button_Unknown };
        ModKey mModifiers{ ModKey::eNone };
    };

    class MouseButtonReleasedEvent final : public MouseEvent {
    public:
        explicit MouseButtonReleasedEvent(MouseButton button);

        MKT_NODISCARD auto GetMouseButton() const -> MouseButton;
        MKT_NODISCARD auto GetType() const -> EventType override;
        
        MKT_NODISCARD auto DisplayData() const -> eastl::string override;
        
        MKT_NODISCARD static auto GetStaticType() -> EventType;

    protected:
        MKT_NODISCARD auto ToString() const -> eastl::string_view override;

        MouseButton mButton{ Mouse_Button_Unknown };
    };

    class MouseScrollEvent final : public MouseEvent {
    public:
        MouseScrollEvent(double xOffset, double yOffset);

        MKT_NODISCARD auto GetOffsetX() const -> double;
        MKT_NODISCARD auto GetOffsetY() const -> double;
        MKT_NODISCARD auto GetType() const -> EventType override;

        MKT_NODISCARD auto DisplayData() const -> eastl::string override;
        
        MKT_NODISCARD static auto GetStaticType() -> EventType;

    protected:
        MKT_NODISCARD auto ToString() const -> eastl::string_view override;

        core::f64 mOffsetX{};
        core::f64 mOffsetY{};
    };

    enum class InputDeviceType : core::u8 {
        eGamepad,
        eSteeringWheel
    };

    /** @brief Reports a controller connection or removal. */
    class InputDeviceConnectionEvent final : public Event {
    public:
        InputDeviceConnectionEvent( core::u32 deviceID, InputDeviceType deviceType, bool connected );

        MKT_NODISCARD auto GetDeviceID() const -> core::u32 { return mDeviceID; }
        MKT_NODISCARD auto GetDeviceType() const -> InputDeviceType { return mDeviceType; }
        MKT_NODISCARD auto IsConnected() const -> bool { return mConnected; }

        MKT_NODISCARD static constexpr auto GetStaticType() -> EventType { return EventType::INPUT_DEVICE_CONNECTION_EVENT; }
        MKT_NODISCARD auto GetType() const -> EventType override { return GetStaticType(); }
        MKT_NODISCARD auto DisplayData() const -> eastl::string override;

    private:
        MKT_NODISCARD auto ToString() const -> eastl::string_view override { return GetEventFormattedStr( GetStaticType() ); }

        core::u32 mDeviceID{};
        InputDeviceType mDeviceType{};
        bool mConnected{};
    };

    class GamepadButtonEvent : public Event {
    public:
        MKT_NODISCARD auto GetDeviceID() const -> core::u32;
        MKT_NODISCARD auto GetGamepadButton() const -> GamepadButton;
        MKT_NODISCARD auto DisplayData() const -> eastl::string override;

    protected:
        GamepadButtonEvent( EventType type, core::u32 deviceID, GamepadButton button );

    private:
        MKT_NODISCARD auto ToString() const -> eastl::string_view override;

        core::u32 mDeviceID{};
        GamepadButton mButton{ GamepadButton::eUnknown };
    };

    class GamepadButtonPressedEvent final : public GamepadButtonEvent {
    public:
        GamepadButtonPressedEvent( core::u32 deviceID, GamepadButton button );

        MKT_NODISCARD auto GetType() const -> EventType override;
        MKT_NODISCARD static auto GetStaticType() -> EventType;
    };

    class GamepadButtonReleasedEvent final : public GamepadButtonEvent {
    public:
        GamepadButtonReleasedEvent( core::u32 deviceID, GamepadButton button );

        MKT_NODISCARD auto GetType() const -> EventType override;
        MKT_NODISCARD static auto GetStaticType() -> EventType;
    };

    class SteeringWheelButtonEvent : public Event {
    public:
        MKT_NODISCARD auto GetDeviceID() const -> core::u32;
        MKT_NODISCARD auto GetSteeringWheelButton() const -> SteeringWheelButton;
        MKT_NODISCARD auto DisplayData() const -> eastl::string override;

    protected:
        SteeringWheelButtonEvent( EventType type, core::u32 deviceID, SteeringWheelButton button );

    private:
        MKT_NODISCARD auto ToString() const -> eastl::string_view override;

        core::u32 mDeviceID{};
        SteeringWheelButton mButton{ SteeringWheelButton::eUnknown };
    };

    class SteeringWheelButtonPressedEvent final : public SteeringWheelButtonEvent {
    public:
        SteeringWheelButtonPressedEvent( core::u32 deviceID, SteeringWheelButton button );

        MKT_NODISCARD auto GetType() const -> EventType override;
        MKT_NODISCARD static auto GetStaticType() -> EventType;
    };

    class SteeringWheelButtonReleasedEvent final : public SteeringWheelButtonEvent {
    public:
        SteeringWheelButtonReleasedEvent( core::u32 deviceID, SteeringWheelButton button );

        MKT_NODISCARD auto GetType() const -> EventType override;
        MKT_NODISCARD static auto GetStaticType() -> EventType;
    };

    /** @brief Reports the new value of a controller axis. */
    template<EventType Type, typename Control, typename Value>
    class InputControlEvent final : public Event {
    public:
        InputControlEvent( core::u32 deviceID, Control control, Value value )
            : Event{ Type, GetCategoryFromType( Type ) }, mDeviceID{ deviceID }, mControl{ control }, mValue{ value } {}

        MKT_NODISCARD auto GetDeviceID() const -> core::u32 { return mDeviceID; }
        MKT_NODISCARD auto GetControl() const -> Control { return mControl; }
        MKT_NODISCARD auto GetValue() const -> Value { return mValue; }

        MKT_NODISCARD static constexpr auto GetStaticType() -> EventType { return Type; }
        MKT_NODISCARD auto GetType() const -> EventType override { return Type; }
        MKT_NODISCARD auto DisplayData() const -> eastl::string override {
            return string::Format( "{}! Device {} control {} value {}", ToString().data(), mDeviceID, core::as<core::i32>( mControl ), mValue );
        }

    private:
        MKT_NODISCARD auto ToString() const -> eastl::string_view override { return GetEventFormattedStr( Type ); }

        core::u32 mDeviceID{};
        Control mControl{};
        Value mValue{};
    };

    using GamepadAxisEvent = InputControlEvent<EventType::GAMEPAD_AXIS_EVENT, GamepadAxis, core::f32>;
    using SteeringWheelAxisEvent = InputControlEvent<EventType::STEERING_WHEEL_AXIS_EVENT, SteeringWheelAxis, core::f32>;
}

#endif// MIKOTO_PLATFORM_CORE_EVENTS_HH