/**
 * CoreEvents.hh
 * Created by kate on 10/8/23.
 * */

#ifndef MIKOTO_CORE_EVENTS_HH
#define MIKOTO_CORE_EVENTS_HH

// C++ Standard Library
#include <array>
#include <string_view>

// Third-Party Libraries
#include "fmt/core.h"

// Project Headers
#include "Common/Common.hh"
#include "Common/Types.hh"
#include "Event.hh"

namespace Mikoto {
    class WindowResizedEvent : public Event {
    public:
        WindowResizedEvent(Int32_T newWidth, Int32_T newHeight)
            :   Event{ GetStaticType(), GetCategoryFromType(GetStaticType()) }
            ,   m_Width{ newWidth }
            ,   m_Height{ newHeight }
        {

        }

        MKT_NODISCARD auto GetWidth() const -> Int32_T { return m_Width; }
        MKT_NODISCARD auto GetHeight() const -> Int32_T { return m_Height; }
        MKT_NODISCARD auto GetType() const -> EventType override { return GetStaticType(); }

        MKT_NODISCARD static auto GetStaticType() -> EventType { return EventType::WINDOW_RESIZE_EVENT; }

        MKT_NODISCARD auto DisplayData() const -> std::string override {
            return fmt::format("{}! New Dimensions [{},{}]", GetEventFormattedStr(GetType()).data(), m_Width, m_Height);
        }

    protected:
        MKT_NODISCARD auto ToString() const -> std::string_view override { return GetEventFormattedStr(GetType()); }

        Int32_T m_Width{};
        Int32_T m_Height{};
    };

    class WindowCloseEvent: public Event {
    public:
        explicit WindowCloseEvent()
            :   Event{ GetStaticType(), GetCategoryFromType(GetStaticType()) }
        {

        }

        MKT_NODISCARD auto GetType() const -> EventType override { return GetStaticType(); }

        MKT_NODISCARD static auto GetStaticType() -> EventType { return EventType::WINDOW_CLOSE_EVENT; }

        MKT_NODISCARD auto DisplayData() const -> std::string override {
            return fmt::format("{}!", GetEventFormattedStr(GetType()).data());
        }

    protected:
        MKT_NODISCARD auto ToString() const -> std::string_view override { return GetEventFormattedStr(GetType()); }
    };

    class AppTick : public Event {
    public:
        explicit AppTick()
            :   Event{ GetStaticType(), GetCategoryFromType(GetStaticType()) }
        {

        }

        MKT_NODISCARD auto GetType() const -> EventType override { return GetStaticType(); }

        MKT_NODISCARD static auto GetStaticType() -> EventType { return EventType::APP_TICK_EVENT; }

        MKT_NODISCARD auto DisplayData() const -> std::string override {
            return fmt::format("{}!", GetEventFormattedStr(GetType()).data());
        }

    protected:
        MKT_NODISCARD auto ToString() const -> std::string_view override { return GetEventFormattedStr(GetType()); }
    };

    class CameraEnableRotation : public Event {
    public:
        explicit CameraEnableRotation()
            :   Event{ GetStaticType(), GetCategoryFromType(GetStaticType()) }
        {

        }

        MKT_NODISCARD auto GetType() const -> EventType override { return GetStaticType(); }

        MKT_NODISCARD static auto GetStaticType() -> EventType { return EventType::CAMERA_ENABLE_ROTATION; }

        MKT_NODISCARD auto DisplayData() const -> std::string override {
            return fmt::format("{}!", GetEventFormattedStr(GetType()).data());
        }

    protected:
        MKT_NODISCARD auto ToString() const -> std::string_view override { return GetEventFormattedStr(GetType()); }
    };

    class AppClose : public Event {
    public:
        explicit AppClose()
            :   Event{ GetStaticType(), GetCategoryFromType(GetStaticType()) }
        {

        }

        MKT_NODISCARD auto GetType() const -> EventType override { return GetStaticType(); }

        MKT_NODISCARD static auto GetStaticType() -> EventType { return EventType::APP_CLOSE_EVENT; }

        MKT_NODISCARD auto DisplayData() const -> std::string override {
            return fmt::format("{}!", GetEventFormattedStr(GetType()).data());
        }

    protected:
        MKT_NODISCARD auto ToString() const -> std::string_view override { return GetEventFormattedStr(GetType()); }
    };

    class AppUpdate : public Event {
    public:
        explicit AppUpdate()
            :   Event{ GetStaticType(), GetCategoryFromType(GetStaticType()) }
        {

        }

        MKT_NODISCARD auto GetType() const -> EventType override { return GetStaticType(); }

        MKT_NODISCARD static auto GetStaticType() -> EventType { return EventType::APP_UPDATE_EVENT; }

        MKT_NODISCARD auto DisplayData() const -> std::string override {
            return fmt::format("{}!", GetEventFormattedStr(GetType()).data());
        }

    protected:
        MKT_NODISCARD auto ToString() const -> std::string_view override { return GetEventFormattedStr(GetType()); }
    };

    class AppRender: public Event {
    public:
        explicit AppRender()
            :   Event{ GetStaticType(), GetCategoryFromType(GetStaticType()) }
        {

        }

        MKT_NODISCARD auto GetType() const -> EventType override { return GetStaticType(); }

        MKT_NODISCARD static auto GetStaticType() -> EventType { return EventType::APP_RENDER_EVENT; }

        MKT_NODISCARD auto DisplayData() const -> std::string override {
            return fmt::format("{}", GetEventFormattedStr(GetType()).data());
        }

    protected:
        MKT_NODISCARD auto ToString() const -> std::string_view override { return GetEventFormattedStr(GetType()); }
    };


    class KeyEvent : public Event {
    public:
        MKT_NODISCARD auto GetKeyCode() const -> Int32_T { return m_KeyCode; }

    protected:
        KeyEvent(EventType type, Int32_T keyCode)
            :   Event{ type, GetCategoryFromType(type) }
            ,   m_KeyCode{ keyCode }
        {

        }

        Int32_T m_KeyCode{};
    };

    class KeyPressedEvent : public KeyEvent {
    public:
        KeyPressedEvent(Int32_T keyCode, bool repeated, Int32_T modifiers = 0)
            :   KeyEvent{ GetStaticType(), keyCode }
            ,   m_Repeated{ repeated }
            ,   m_Modifiers{ modifiers }
        {

        }

        MKT_NODISCARD auto IsRepeated() const -> bool { return m_Repeated; }
        MKT_NODISCARD auto GetModifiers() const -> bool { return m_Modifiers; }
        MKT_NODISCARD auto GetType() const -> EventType override { return GetStaticType(); }

        MKT_NODISCARD static auto GetStaticType() -> EventType { return EventType::KEY_PRESSED_EVENT; }

        MKT_NODISCARD auto DisplayData() const -> std::string override {
            return fmt::format("{}! Key {}. Repeated? {}", GetEventFormattedStr(GetType()).data(), m_KeyCode, m_Repeated ? "Yes" : "No");
        }

    private:
        MKT_NODISCARD auto ToString() const -> std::string_view override { return GetEventFormattedStr(GetType()); }

        bool m_Repeated{};
        Int32_T m_Modifiers{};
    };

    class KeyReleasedEvent : public KeyEvent {
    public:
        explicit KeyReleasedEvent(Int32_T code)
            :   KeyEvent{ GetStaticType(), code }
        {

        }

        MKT_NODISCARD auto GetType() const -> EventType override { return GetStaticType(); }

        MKT_NODISCARD static auto GetStaticType() -> EventType { return EventType::KEY_RELEASED_EVENT; }

        MKT_NODISCARD auto DisplayData() const -> std::string override {
            return fmt::format("{}! Key {}", GetEventFormattedStr(GetType()).data(), m_KeyCode);
        }
    private:
        MKT_NODISCARD auto ToString() const -> std::string_view override { return GetEventFormattedStr(GetType()); }
    };

    class KeyCharEvent : public Event {
    public:
        explicit KeyCharEvent(UInt32_T charCode)
            :   Event{ GetStaticType(), GetCategoryFromType(GetStaticType()) }
            ,   m_KeyChar{ charCode }
        {

        }

        MKT_NODISCARD auto GetType() const -> EventType override { return GetStaticType(); }
        MKT_NODISCARD auto GetChar() const -> UInt32_T { return m_KeyChar; }

        MKT_NODISCARD static auto GetStaticType() -> EventType { return EventType::KEY_CHAR_EVENT; }

        MKT_NODISCARD auto DisplayData() const -> std::string override {
            return fmt::format("{}! Key {}", GetEventFormattedStr(GetType()).data(), m_KeyChar);
        }
    private:
        MKT_NODISCARD auto ToString() const -> std::string_view override { return GetEventFormattedStr(GetType()); }

        UInt32_T m_KeyChar{};
    };

    class MouseMovedEvent : public Event {
    public:
        MouseMovedEvent(double x, double y)
            :   Event{ GetStaticType(), GetCategoryFromType(GetStaticType()) }
            ,   m_PositionX{ x }
            ,   m_PositionY{ y }
        {

        }

        MKT_NODISCARD auto GetPositionX() const -> double { return m_PositionX; }
        MKT_NODISCARD auto GetPositionY() const -> double { return m_PositionY; }
        MKT_NODISCARD auto GetType() const -> EventType override { return GetStaticType(); }


        MKT_NODISCARD static auto GetStaticType() -> EventType { return EventType::MOUSE_MOVED_EVENT; }

        MKT_NODISCARD auto DisplayData() const -> std::string override {
            return fmt::format("{}! Position [{},{}]", GetEventFormattedStr(GetType()).data(), m_PositionX, m_PositionY);
        }
    protected:
        MKT_NODISCARD auto ToString() const -> std::string_view override { return GetEventFormattedStr(GetType()); }

        double m_PositionX{};
        double m_PositionY{};
    };

    class MouseEvent : public Event {
    protected:
        explicit MouseEvent(EventType type)
            :   Event{ type, GetCategoryFromType(type) }
        {

        }
    };

    class MouseButtonPressedEvent : public MouseEvent {
    public:
        explicit MouseButtonPressedEvent(Int32_T button, Int32_T modifiers = 0)
            :   MouseEvent{ GetStaticType() }
            ,   m_Button{ button }
            ,   m_Modifiers{ modifiers }
        {

        }

        MKT_NODISCARD auto GetMouseButton() const -> Int32_T { return m_Button; }
        MKT_NODISCARD auto GetModifiers() const -> Int32_T { return m_Modifiers; }
        MKT_NODISCARD auto GetType() const -> EventType override { return GetStaticType(); }

        MKT_NODISCARD static auto GetStaticType() -> EventType { return EventType::MOUSE_BUTTON_PRESSED_EVENT; }

        MKT_NODISCARD auto DisplayData() const -> std::string override {
            constexpr static std::array<std::string_view, 3> NAME{ "LEFT_CLICK", "RIGHT_CLICK", "SCROLL_WHEEL_CLICK" };
            // we are just testing with a mouse with three buttons for now
            return fmt::format("{}! Button {}", GetEventFormattedStr(GetType()).data(), NAME[m_Button]);
        }
    protected:
        MKT_NODISCARD auto ToString() const -> std::string_view override { return GetEventFormattedStr(GetType()); }

        Int32_T m_Button{};
        Int32_T m_Modifiers{};
    };

    class MouseButtonReleasedEvent : public MouseEvent {
    public:
        explicit MouseButtonReleasedEvent(Int32_T button)
            :   MouseEvent{ GetStaticType() }
            ,   m_Button{ button }
        {

        }

        MKT_NODISCARD auto GetMouseButton() const -> Int32_T { return m_Button; }
        MKT_NODISCARD auto GetType() const -> EventType override { return GetStaticType(); }

        MKT_NODISCARD static auto GetStaticType() -> EventType { return EventType::MOUSE_BUTTON_RELEASED_EVENT; }
        MKT_NODISCARD auto DisplayData() const -> std::string override {
            constexpr static std::array<std::string_view, 3> NAME{ "LEFT_CLICK", "RIGHT_CLICK", "SCROLL_WHEEL_CLICK" };
            return fmt::format("{}! Button {}", GetEventFormattedStr(GetType()).data(), NAME[m_Button]);
        }

    protected:
        MKT_NODISCARD auto ToString() const -> std::string_view override { return GetEventFormattedStr(GetType()); }

        Int32_T m_Button{};
    };

    class MouseScrollEvent : public MouseEvent {
    public:
        MouseScrollEvent(double xOffset, double yOffset)
            :   MouseEvent{ GetStaticType() }
            ,   m_OffsetX{ xOffset }
            ,   m_OffsetY{ yOffset }
        {}

        MKT_NODISCARD auto GetOffsetX() const -> double { return m_OffsetX; }
        MKT_NODISCARD auto GetOffsetY() const -> double { return m_OffsetY; }
        MKT_NODISCARD auto GetType() const -> EventType override { return GetStaticType(); }

        MKT_NODISCARD static auto GetStaticType() -> EventType { return EventType::MOUSE_SCROLLED_EVENT; }

        MKT_NODISCARD auto DisplayData() const -> std::string override {
            return fmt::format("{}! Offsets [{},{}]", GetEventFormattedStr(GetType()).data(), m_OffsetX, m_OffsetY);
        }

    protected:
        MKT_NODISCARD auto ToString() const -> std::string_view override { return GetEventFormattedStr(GetType()); }

        double m_OffsetX{};
        double m_OffsetY{};
    };
}

#endif // MIKOTO_CORE_EVENTS_HH
