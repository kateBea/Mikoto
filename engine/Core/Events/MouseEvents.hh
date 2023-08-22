/**
 * MouseEvents.hh
 * Created by kate on 5/25/23.
 * */

#ifndef KATE_ENGINE_MOUSE_EVENTS_HH
#define KATE_ENGINE_MOUSE_EVENTS_HH

// C++ Standard Library
#include <string_view>
#include <array>

// Third-Party Libraries
#include <fmt/format.h>

// Project Headers
#include <Utility/Common.hh>
#include <Core/Events/Event.hh>


namespace Mikoto {
    class MouseMovedEvent : public Event {
    public:
        MouseMovedEvent(double x, double y)
            :   Event{ EventType::MOUSE_MOVED_EVENT, static_cast<EventCategory>(INPUT_EVENT_CATEGORY | MOUSE_EVENT_CATEGORY) }
            ,   m_PositionX{ x }
            ,   m_PositionY{ y }
        {}

        MKT_NODISCARD auto GetPositionX() const -> double { return m_PositionX; }
        MKT_NODISCARD auto GetPositionY() const -> double { return m_PositionY; }
        MKT_NODISCARD auto GetType() const -> EventType override { return GetStaticType(); }

        /**
         * Useful if we need to query the type of this event.
         * See <code>kT::EventDispatcher</code> for usage
         * */
        MKT_NODISCARD static auto GetStaticType() -> EventType { return EventType::MOUSE_MOVED_EVENT; }

        MKT_NODISCARD auto DisplayData() const -> std::string override {
            return fmt::format("{} POS [{}, {}]", GetEventFormattedStr(GetType()).data(), m_PositionX, m_PositionY);
        }
    protected:
        MKT_NODISCARD auto ToString() const -> std::string_view override { return GetEventFormattedStr(GetType()); }

        double m_PositionX{};
        double m_PositionY{};
    };

    class MouseEvent : public Event {
    protected:
        explicit MouseEvent(EventType type, EventCategory categories = EMPTY_EVENT_CATEGORY)
            :   Event{ type, static_cast<EventCategory>(MOUSE_EVENT_CATEGORY | INPUT_EVENT_CATEGORY | categories) } {}
    };

    class MouseButtonPressedEvent : public MouseEvent {
    public:
        explicit MouseButtonPressedEvent(Int32_T button, Int32_T modifiers = 0)
            :   MouseEvent{ EventType::MOUSE_BUTTON_PRESSED_EVENT, MOUSE_BUTTON_EVENT_CATEGORY}, m_Button{ button }, m_Modifiers{ modifiers }
        {}

        MKT_NODISCARD auto GetMouseButton() const -> Int32_T { return m_Button; }
        MKT_NODISCARD auto GetModifiers() const -> Int32_T { return m_Modifiers; }
        MKT_NODISCARD auto GetType() const -> EventType override { return GetStaticType(); }

        /**
         * Useful if we need to query the type of this event.
         * See <code>kT::EventDispatcher</code> for usage
         * */
        MKT_NODISCARD static auto GetStaticType() -> EventType { return EventType::MOUSE_BUTTON_PRESSED_EVENT; }

        MKT_NODISCARD auto DisplayData() const -> std::string override {
            constexpr static std::array<std::string_view, 3> NAME{ "LEFT_CLICK", "RIGHT_CLICK", "SCROLL_WHEEL_CLICK" };
            // we are just testing with a mouse with three buttons for now
            return fmt::format("{} BUTTON {}", GetEventFormattedStr(GetType()).data(), NAME[m_Button]);
        }
    protected:
        MKT_NODISCARD auto ToString() const -> std::string_view override { return GetEventFormattedStr(GetType()); }

        Int32_T m_Button{};
        Int32_T m_Modifiers{};
    };

    class MouseButtonReleasedEvent : public MouseEvent {
    public:
        explicit MouseButtonReleasedEvent(Int32_T button)
            :    MouseEvent{ EventType::MOUSE_BUTTON_RELEASED_EVENT, MOUSE_BUTTON_EVENT_CATEGORY }, m_Button{ button }
        {}

        MKT_NODISCARD auto GetMouseButton() const -> Int32_T { return m_Button; }
        MKT_NODISCARD auto GetType() const -> EventType override { return GetStaticType(); }

        /**
         * Useful if we need to query the type of this event.
         * See <code>kT::EventDispatcher</code> for usage
         * */
        MKT_NODISCARD static auto GetStaticType() -> EventType { return EventType::MOUSE_BUTTON_RELEASED_EVENT; }
        MKT_NODISCARD auto DisplayData() const -> std::string override {
            constexpr static std::array<std::string_view, 3> NAME{ "LEFT_CLICK", "RIGHT_CLICK", "SCROLL_WHEEL_CLICK" };
            return fmt::format("{} BUTTON {}", GetEventFormattedStr(GetType()).data(), NAME[m_Button]);
        }
    protected:
        MKT_NODISCARD auto ToString() const -> std::string_view override { return GetEventFormattedStr(GetType()); }

        Int32_T m_Button{};
    };

    class MouseScrollEvent : public MouseEvent {
    public:
        MouseScrollEvent(double xOffset, double yOffset)
            :   MouseEvent{ EventType::MOUSE_SCROLLED_EVENT }, m_OffsetX{ xOffset }, m_OffsetY{ yOffset }
        {}

        MKT_NODISCARD auto GetOffsetX() const -> double { return m_OffsetX; }
        MKT_NODISCARD auto GetOffsetY() const -> double { return m_OffsetY; }
        MKT_NODISCARD auto GetType() const -> EventType override { return GetStaticType(); }

        /**
         * Useful if we need to query the type of this event.
         * See <code>kT::EventDispatcher</code> for usage
         * */
        MKT_NODISCARD static auto GetStaticType() -> EventType { return EventType::MOUSE_SCROLLED_EVENT; }

        MKT_NODISCARD auto DisplayData() const -> std::string override {
            return fmt::format("{} OFFSETS [{}, {}]", GetEventFormattedStr(GetType()).data(), m_OffsetX, m_OffsetY);
        }
    protected:
        MKT_NODISCARD auto ToString() const -> std::string_view override { return GetEventFormattedStr(GetType()); }

        double m_OffsetX{};
        double m_OffsetY{};
    };

}

#endif // KATE_ENGINE_MOUSE_EVENTS_HH
