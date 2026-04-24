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

#ifndef MIKOTO_CORE_EVENTS_HH
#define MIKOTO_CORE_EVENTS_HH

#include <EASTL/array.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <fmt/core.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Event.hh>
#include <Core/String.hh>

namespace mikoto::core {
    class WindowResizedEvent final : public Event {
    public:
        WindowResizedEvent(i32 newWidth, i32 newHeight)
            :   Event{ GetStaticType(), GetCategoryFromType(GetStaticType()) }
            ,   mWidth{ newWidth }
            ,   mHeight{ newHeight }
        {

        }

        MKT_NODISCARD auto GetWidth() const -> i32 { return mWidth; }
        MKT_NODISCARD auto GetHeight() const -> i32 { return mHeight; }
        MKT_NODISCARD auto GetType() const -> EventType override { return GetStaticType(); }

        MKT_NODISCARD static auto GetStaticType() -> EventType { return EventType::WINDOW_RESIZE_EVENT; }

        MKT_NODISCARD auto DisplayData() const -> eastl::string override {
            return string::Format("{}! New Dimensions [{},{}]", GetEventFormattedStr(GetType()).data(), mWidth, mHeight);
        }

    protected:
        MKT_NODISCARD auto ToString() const -> eastl::string_view override { return GetEventFormattedStr(GetType()); }

        i32 mWidth{};
        i32 mHeight{};
    };

    class WindowCloseEvent final : public Event {
    public:
        explicit WindowCloseEvent()
            :   Event{ GetStaticType(), GetCategoryFromType(GetStaticType()) }
        {

        }

        MKT_NODISCARD auto GetType() const -> EventType override { return GetStaticType(); }

        MKT_NODISCARD static auto GetStaticType() -> EventType { return EventType::WINDOW_CLOSE_EVENT; }

        MKT_NODISCARD auto DisplayData() const -> eastl::string override {
            return string::Format("{}!", GetEventFormattedStr(GetType()).data());
        }

    protected:
        MKT_NODISCARD auto ToString() const -> eastl::string_view override { return GetEventFormattedStr(GetType()); }
    };

    class AppTick final : public Event {
    public:
        explicit AppTick()
            :   Event{ GetStaticType(), GetCategoryFromType(GetStaticType()) }
        {

        }

        MKT_NODISCARD auto GetType() const -> EventType override { return GetStaticType(); }

        MKT_NODISCARD static auto GetStaticType() -> EventType { return EventType::APP_TICK_EVENT; }

        MKT_NODISCARD auto DisplayData() const -> eastl::string override {
            return string::Format("{}!", GetEventFormattedStr(GetType()).data());
        }

    protected:
        MKT_NODISCARD auto ToString() const -> eastl::string_view override { return GetEventFormattedStr(GetType()); }
    };

    class CameraEnableRotation final : public Event {
    public:
        explicit CameraEnableRotation()
            :   Event{ GetStaticType(), GetCategoryFromType(GetStaticType()) }
        {

        }

        MKT_NODISCARD auto GetType() const -> EventType override { return GetStaticType(); }

        MKT_NODISCARD static auto GetStaticType() -> EventType { return EventType::CAMERA_ENABLE_ROTATION; }

        MKT_NODISCARD auto DisplayData() const -> eastl::string override {
            return string::Format("{}!", GetEventFormattedStr(GetType()).data());
        }

    protected:
        MKT_NODISCARD auto ToString() const -> eastl::string_view override { return GetEventFormattedStr(GetType()); }
    };

    class AppClose final : public Event {
    public:
        explicit AppClose()
            :   Event{ GetStaticType(), GetCategoryFromType(GetStaticType()) }
        {

        }

        MKT_NODISCARD auto GetType() const -> EventType override { return GetStaticType(); }

        MKT_NODISCARD static auto GetStaticType() -> EventType { return EventType::APP_CLOSE_EVENT; }

        MKT_NODISCARD auto DisplayData() const -> eastl::string override {
            return string::Format("{}!", GetEventFormattedStr(GetType()).data());
        }

    protected:
        MKT_NODISCARD auto ToString() const -> eastl::string_view override { return GetEventFormattedStr(GetType()); }
    };

    class AppUpdate final : public Event {
    public:
        explicit AppUpdate()
            :   Event{ GetStaticType(), GetCategoryFromType(GetStaticType()) }
        {

        }

        MKT_NODISCARD auto GetType() const -> EventType override { return GetStaticType(); }

        MKT_NODISCARD static auto GetStaticType() -> EventType { return EventType::APP_UPDATE_EVENT; }

        MKT_NODISCARD auto DisplayData() const -> eastl::string override {
            return string::Format("{}!", GetEventFormattedStr(GetType()).data());
        }

    protected:
        MKT_NODISCARD auto ToString() const -> eastl::string_view override { return GetEventFormattedStr(GetType()); }
    };

    class AppRender final : public Event {
    public:
        explicit AppRender()
            :   Event{ GetStaticType(), GetCategoryFromType(GetStaticType()) }
        {

        }

        MKT_NODISCARD auto GetType() const -> EventType override { return GetStaticType(); }

        MKT_NODISCARD static auto GetStaticType() -> EventType { return EventType::APP_RENDER_EVENT; }

        MKT_NODISCARD auto DisplayData() const -> eastl::string override {
            return string::Format("{}", GetEventFormattedStr(GetType()).data());
        }

    protected:
        MKT_NODISCARD auto ToString() const -> eastl::string_view override { return GetEventFormattedStr(GetType()); }
    };


    class KeyEvent : public Event {
    public:
        MKT_NODISCARD auto GetKeyCode() const -> i32 { return mKeyCode; }

    protected:
        KeyEvent(EventType type, i32 keyCode)
            :   Event{ type, GetCategoryFromType(type) }
            ,   mKeyCode{ keyCode }
        {

        }

        i32 mKeyCode{};
    };

    class KeyPressedEvent final : public KeyEvent {
    public:
        explicit KeyPressedEvent(const i32 keyCode, const bool repeated = false, i32 modifiers = 0)
            :   KeyEvent{ GetStaticType(), keyCode }
            ,   mRepeated{ repeated }
            ,   mModifiers{ modifiers }
        {

        }

        MKT_NODISCARD auto IsRepeated() const -> bool { return mRepeated; }
        MKT_NODISCARD auto GetModifiers() const -> bool { return mModifiers; }
        MKT_NODISCARD auto GetType() const -> EventType override { return GetStaticType(); }

        MKT_NODISCARD static auto GetStaticType() -> EventType { return EventType::KEY_PRESSED_EVENT; }

        MKT_NODISCARD auto DisplayData() const -> eastl::string override {
            return string::Format("{}! Key {}. Repeated? {}", GetEventFormattedStr(GetType()).data(), mKeyCode, mRepeated ? "Yes" : "No");
        }

    private:
        MKT_NODISCARD auto ToString() const -> eastl::string_view override { return GetEventFormattedStr(GetType()); }

        bool mRepeated{};
        i32 mModifiers{};
    };

    class KeyReleasedEvent final : public KeyEvent {
    public:
        explicit KeyReleasedEvent(i32 code)
            :   KeyEvent{ GetStaticType(), code }
        {

        }

        MKT_NODISCARD auto GetType() const -> EventType override { return GetStaticType(); }

        MKT_NODISCARD static auto GetStaticType() -> EventType { return EventType::KEY_RELEASED_EVENT; }

        MKT_NODISCARD auto DisplayData() const -> eastl::string override {
            return string::Format("{}! Key {}", GetEventFormattedStr(GetType()).data(), mKeyCode);
        }
    private:
        MKT_NODISCARD auto ToString() const -> eastl::string_view override { return GetEventFormattedStr(GetType()); }
    };

    class KeyCharEvent final : public Event {
    public:
        explicit KeyCharEvent(u32 charCode)
            :   Event{ GetStaticType(), GetCategoryFromType(GetStaticType()) }
            ,   mKeyChar{ charCode }
        {

        }

        MKT_NODISCARD auto GetType() const -> EventType override { return GetStaticType(); }
        MKT_NODISCARD auto GetChar() const -> u32 { return mKeyChar; }

        MKT_NODISCARD static auto GetStaticType() -> EventType { return EventType::KEY_CHAR_EVENT; }

        MKT_NODISCARD auto DisplayData() const -> eastl::string override {
            return string::Format("{}! Key {}", GetEventFormattedStr(GetType()).data(), mKeyChar);
        }
    private:
        MKT_NODISCARD auto ToString() const -> eastl::string_view override { return GetEventFormattedStr(GetType()); }

        u32 mKeyChar{};
    };

    class MouseMovedEvent final : public Event {
    public:
        MouseMovedEvent(double x, double y)
            :   Event{ GetStaticType(), GetCategoryFromType(GetStaticType()) }
            ,   mPositionX{ x }
            ,   mPositionY{ y }
        {

        }

        MKT_NODISCARD auto GetPositionX() const -> double { return mPositionX; }
        MKT_NODISCARD auto GetPositionY() const -> double { return mPositionY; }
        MKT_NODISCARD auto GetType() const -> EventType override { return GetStaticType(); }


        MKT_NODISCARD static auto GetStaticType() -> EventType { return EventType::MOUSE_MOVED_EVENT; }

        MKT_NODISCARD auto DisplayData() const -> eastl::string override {
            return string::Format("{}! Position [{},{}]", GetEventFormattedStr(GetType()).data(), mPositionX, mPositionY);
        }
    protected:
        MKT_NODISCARD auto ToString() const -> eastl::string_view override { return GetEventFormattedStr(GetType()); }

        f64 mPositionX{};
        f64 mPositionY{};
    };

    class MouseEvent : public Event {
    protected:
        explicit MouseEvent(EventType type)
            :   Event{ type, GetCategoryFromType(type) }
        {

        }
    };

    class MouseButtonPressedEvent final : public MouseEvent {
    public:
        explicit MouseButtonPressedEvent(i32 button, i32 modifiers = 0)
            :   MouseEvent{ GetStaticType() }
            ,   mButton{ button }
            ,   mModifiers{ modifiers }
        {

        }

        MKT_NODISCARD auto GetMouseButton() const -> i32 { return mButton; }
        MKT_NODISCARD auto GetModifiers() const -> i32 { return mModifiers; }
        MKT_NODISCARD auto GetType() const -> EventType override { return GetStaticType(); }

        MKT_NODISCARD static auto GetStaticType() -> EventType { return EventType::MOUSE_BUTTON_PRESSED_EVENT; }

        MKT_NODISCARD auto DisplayData() const -> eastl::string override {
            constexpr static eastl::array<eastl::string_view, 3> NAME{ "LEFT_CLICK", "RIGHT_CLICK", "SCROLL_WHEEL_CLICK" };
            // we are just testing with a mouse with three buttons for now
            return string::Format("{}! Button {}", GetEventFormattedStr(GetType()).data(), NAME[mButton]);
        }
    protected:
        MKT_NODISCARD auto ToString() const -> eastl::string_view override { return GetEventFormattedStr(GetType()); }

        i32 mButton{};
        i32 mModifiers{};
    };

    class MouseButtonReleasedEvent final : public MouseEvent {
    public:
        explicit MouseButtonReleasedEvent(i32 button)
            :   MouseEvent{ GetStaticType() }
            ,   mButton{ button }
        {

        }

        MKT_NODISCARD auto GetMouseButton() const -> i32 { return mButton; }
        MKT_NODISCARD auto GetType() const -> EventType override { return GetStaticType(); }

        MKT_NODISCARD static auto GetStaticType() -> EventType { return EventType::MOUSE_BUTTON_RELEASED_EVENT; }
        MKT_NODISCARD auto DisplayData() const -> eastl::string override {
            constexpr static eastl::array<eastl::string_view, 3> NAME{ "LEFT_CLICK", "RIGHT_CLICK", "SCROLL_WHEEL_CLICK" };
            return string::Format("{}! Button {}", GetEventFormattedStr(GetType()).data(), NAME[mButton]);
        }

    protected:
        MKT_NODISCARD auto ToString() const -> eastl::string_view override { return GetEventFormattedStr(GetType()); }

        i32 mButton{};
    };

    class MouseScrollEvent final : public MouseEvent {
    public:
        MouseScrollEvent(double xOffset, double yOffset)
            :   MouseEvent{ GetStaticType() }
            ,   mOffsetX{ xOffset }
            ,   mOffsetY{ yOffset }
        {}

        MKT_NODISCARD auto GetOffsetX() const -> double { return mOffsetX; }
        MKT_NODISCARD auto GetOffsetY() const -> double { return mOffsetY; }
        MKT_NODISCARD auto GetType() const -> EventType override { return GetStaticType(); }

        MKT_NODISCARD static auto GetStaticType() -> EventType { return EventType::MOUSE_SCROLLED_EVENT; }

        MKT_NODISCARD auto DisplayData() const -> eastl::string override {
            return string::Format("{}! Offsets [{},{}]", GetEventFormattedStr(GetType()).data(), mOffsetX, mOffsetY);
        }

    protected:
        MKT_NODISCARD auto ToString() const -> eastl::string_view override { return GetEventFormattedStr(GetType()); }

        f64 mOffsetX{};
        f64 mOffsetY{};
    };
}

#endif // MIKOTO_CORE_EVENTS_HH
