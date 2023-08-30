/**
 * AppEvents.hh
 * Created by kate on 5/26/23.
 * */

#ifndef MIKOTO_APP_EVENTS_HH
#define MIKOTO_APP_EVENTS_HH

// C++ Standard Library
#include <string_view>

// Third-Party Libraries
#include <fmt/core.h>

// Project Headers
#include <Core/Events/Event.hh>
#include <Utility/Common.hh>

namespace Mikoto {
    class WindowResizedEvent : public Event {
    public:
        WindowResizedEvent(Int32_T newWidth, Int32_T newHeight)
            :   Event{ EventType::WINDOW_RESIZE_EVENT, APPLICATION_EVENT_CATEGORY }, m_Width{ newWidth }, m_Height{ newHeight } {}

        MKT_NODISCARD auto GetWidth() const -> Int32_T { return m_Width; }
        MKT_NODISCARD auto GetHeight() const -> Int32_T { return m_Height; }
        MKT_NODISCARD auto GetType() const -> EventType override { return GetStaticType(); }

        /**
         * Useful if we need to query the type of this event.
         * See <code>kT::EventDispatcher</code> for usage
         * */
        MKT_NODISCARD static auto GetStaticType() -> EventType { return EventType::WINDOW_RESIZE_EVENT; }

        MKT_NODISCARD auto DisplayData() const -> std::string override {
            return fmt::format("{} NEW DIMENSIONS: [{}, {}]", GetEventFormattedStr(GetType()).data(), m_Width, m_Height);
        }

    protected:
        MKT_NODISCARD auto ToString() const -> std::string_view override { return GetEventFormattedStr(GetType()); }

        Int32_T m_Width{};
        Int32_T m_Height{};
    };

    class WindowCloseEvent: public Event {
    public:
        WindowCloseEvent() : Event{ EventType::WINDOW_CLOSE_EVENT, APPLICATION_EVENT_CATEGORY } {}

        MKT_NODISCARD auto GetType() const -> EventType override { return GetStaticType(); }

        /**
         * Useful if we need to query the type of this event.
         * See <code>kT::EventDispatcher</code> for usage
         * */
        MKT_NODISCARD static auto GetStaticType() -> EventType { return EventType::WINDOW_CLOSE_EVENT; }

        MKT_NODISCARD auto DisplayData() const -> std::string override {
            return fmt::format("{}", GetEventFormattedStr(GetType()).data());
        }
    protected:
        MKT_NODISCARD auto ToString() const -> std::string_view override { return GetEventFormattedStr(GetType()); }
    };

    class AppTick : public Event {
    public:
        AppTick() : Event{ EventType::APP_TICK_EVENT, APPLICATION_EVENT_CATEGORY } {}

        MKT_NODISCARD auto GetType() const -> EventType override { return getStaticType(); }

        /**
         * Useful if we need to query the type of this event.
         * See <code>kT::EventDispatcher</code> for usage
         * */
        MKT_NODISCARD static auto getStaticType() -> EventType { return EventType::APP_TICK_EVENT; }

        MKT_NODISCARD auto DisplayData() const -> std::string override {
            return fmt::format("{}", GetEventFormattedStr(GetType()).data());
        }

    protected:
        MKT_NODISCARD auto ToString() const -> std::string_view override { return GetEventFormattedStr(GetType()); }
    };

    class AppUpdate : public Event {
    public:
        AppUpdate() : Event{ EventType::APP_UPDATE_EVENT, APPLICATION_EVENT_CATEGORY } {}

        MKT_NODISCARD auto GetType() const -> EventType override { return GetStaticType(); }

        /**
         * Useful if we need to query the type of this event.
         * See <code>kT::EventDispatcher</code> for usage
         * */
        MKT_NODISCARD static auto GetStaticType() -> EventType { return EventType::APP_UPDATE_EVENT; }

        MKT_NODISCARD auto DisplayData() const -> std::string override {
            return fmt::format("{}", GetEventFormattedStr(GetType()).data());
        }
    protected:
        MKT_NODISCARD auto ToString() const -> std::string_view override { return GetEventFormattedStr(GetType()); }
    };

    class AppRender: public Event {
    public:
        AppRender() : Event{ EventType::APP_RENDER_EVENT, APPLICATION_EVENT_CATEGORY } {}

        MKT_NODISCARD auto GetType() const -> EventType override { return GetStaticType(); }

        /**
         * Useful if we need to query the type of this event.
         * See <code>kT::EventDispatcher</code> for usage
         * */
        MKT_NODISCARD static auto GetStaticType() -> EventType { return EventType::APP_RENDER_EVENT; }

        MKT_NODISCARD auto DisplayData() const -> std::string override {
            return fmt::format("{}", GetEventFormattedStr(GetType()).data());
        }
    protected:
        MKT_NODISCARD auto ToString() const -> std::string_view override { return GetEventFormattedStr(GetType()); }
    };


}

#endif // MIKOTO_APP_EVENTS_HH
