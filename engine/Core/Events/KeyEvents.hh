/**
 * KeyEvents.hh
 * Created by kate on 5/25/23.
 * */

#ifndef MIKOTO_KEY_EVENTS_HH
#define MIKOTO_KEY_EVENTS_HH

// C++ Standard Library
#include <cstdint>
#include <string_view>
#include <iomanip>
#include <cstdio>

// Third-Party Libraries
#include <fmt/format.h>

// Project Headers
#include <Core/Events/Event.hh>
#include <Utility/Common.hh>

namespace Mikoto {
    /**
     * Common interface for Key Events
     * */
    class KeyEvent : public Event {
    public:
        MKT_NODISCARD auto GetKeyCode() const -> Int32_T { return m_KeyCode; }

    protected:
        /**
         * Only specializations may create KeyEvents
         * */
        KeyEvent(EventType type, Int32_T keyCode)
            :   Event{ type, static_cast<EventCategory>(INPUT_EVENT_CATEGORY | KEY_EVENT_CATEGORY) }, m_KeyCode{ keyCode }
        {}

        Int32_T m_KeyCode{};
    };

    class KeyPressedEvent : public KeyEvent {
    public:
        KeyPressedEvent(Int32_T keyCode, bool repeated, Int32_T modifiers = 0)
            :   KeyEvent{ EventType::KEY_PRESSED_EVENT, keyCode }, m_Repeated{ repeated }, m_Modifiers{ modifiers }
        {}

        MKT_NODISCARD auto IsRepeated() const -> bool { return m_Repeated; }
        MKT_NODISCARD auto GetModifiers() const -> bool { return m_Modifiers; }
        MKT_NODISCARD auto GetType() const -> EventType override { return GetStaticType(); }

        /**
         * Useful if we need to query the type of this event.
         * See <code>kT::EventDispatcher</code> for usage
         * */
        MKT_NODISCARD static auto GetStaticType() -> EventType { return EventType::KEY_PRESSED_EVENT; }

        MKT_NODISCARD auto DisplayData() const -> std::string override {
            return fmt::format("{} KEY: {} REPEATED: {}", GetEventFormattedStr(GetType()).data(), m_KeyCode, m_Repeated ? "TRUE" : "FALSE");
        }

    private:
        MKT_NODISCARD auto ToString() const -> std::string_view override { return GetEventFormattedStr(GetType()); }

        bool m_Repeated{};
        Int32_T m_Modifiers{};
    };

    class KeyReleasedEvent : public KeyEvent {
    public:
        explicit KeyReleasedEvent(Int32_T code)
            :   KeyEvent{ EventType::KEY_RELEASED_EVENT, code }
        {}

        MKT_NODISCARD auto GetType() const -> EventType override { return GetStaticType(); }

        /**
         * Useful if we need to query the type of this event.
         * See <code>kT::EventDispatcher</code> for usage
         * */
        MKT_NODISCARD static auto GetStaticType() -> EventType { return EventType::KEY_RELEASED_EVENT; }

        MKT_NODISCARD auto DisplayData() const -> std::string override {
            return fmt::format("{} KEY: {}", GetEventFormattedStr(GetType()).data(), m_KeyCode);
        }
    private:
        MKT_NODISCARD auto ToString() const -> std::string_view override { return GetEventFormattedStr(GetType()); }
    };

    class KeyCharEvent : public Event {
    public:
        explicit KeyCharEvent(UInt32_T charCode)
            :   Event{ EventType::KEY_CHAR_EVENT, static_cast<EventCategory>(INPUT_EVENT_CATEGORY | KEY_EVENT_CATEGORY) }
            ,   m_KeyChar{ charCode }
        {}

        MKT_NODISCARD auto GetType() const -> EventType override { return GetStaticType(); }
        MKT_NODISCARD auto GetChar() const -> UInt32_T { return m_KeyChar; }

        /**
         * Useful if we need to query the type of this event.
         * See <code>kT::EventDispatcher</code> for usage
         * */
        MKT_NODISCARD static auto GetStaticType() -> EventType { return EventType::KEY_CHAR_EVENT; }

        MKT_NODISCARD auto DisplayData() const -> std::string override {
            return fmt::format("{} KEY: {}", GetEventFormattedStr(GetType()).data(), m_KeyChar);
        }
    private:
        MKT_NODISCARD auto ToString() const -> std::string_view override { return GetEventFormattedStr(GetType()); }

        UInt32_T m_KeyChar{};
    };

}

#endif // MIKOTO_KEY_EVENTS_HH
