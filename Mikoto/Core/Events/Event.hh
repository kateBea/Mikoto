/**
 * Event.hh
 * Created by kate on 5/25/23.
 * */

#ifndef MIKOTO_EVENT_HH
#define MIKOTO_EVENT_HH

// C++ Standard Library
#include <iostream>
#include <string_view>
#include <type_traits>
#include <functional>
#include <string>

// Project Headers
#include "Common/Common.hh"
#include <Models/Enums.hh>

namespace Mikoto {

    /**
     * Defines the general interface for all types of events.
     *
     * At the moment Mikoto events are blocking, meaning when an event
     * occurs it is notified and must be handled at that moment. An alternative
     * system is Even queueing where we dispatch events to a queue and can
     * handle them when we consider appropriate
     * */
    class Event {
    public:
        /**
         * Creates a new event. The event is not handled on creation.
         * */
        Event(EventType type, EventCategory categories)
            :   m_Type{ type }, m_Categories{ categories }, m_Handled{ false } {}


        Event(const Event& other) = default;

        /**
         * Returns the type of this event. Can be used to query the type of
         * this event in scenarios where polymorphism is used
         * */
        MKT_NODISCARD virtual auto GetType() const -> EventType = 0;
        MKT_NODISCARD virtual auto GetCategoryFlags() const -> EventCategory { return m_Categories; };

        /**
         * Returns the string representation of this Event.
         * Mainly for debugging purposes
         * */
        MKT_NODISCARD auto GetNameStr() const -> std::string_view { return ToString(); };

        /**
         * Tells whether this event has been handled or not
         * @returns true if the event has been handled, false otherwise
         * */
        MKT_NODISCARD auto IsHandled() const -> bool { return m_Handled; }
        MKT_NODISCARD auto IsInCategory(EventCategory cat) const -> bool { return GetCategoryFlags() & cat; }

        /**
         * Returns a formatted string representing the data, if any,
         * that this event holds. Used for debugging purposes
         * */
        MKT_NODISCARD virtual auto DisplayData() const -> std::string = 0;

        auto SetHandled(bool value) -> void { m_Handled = value; }

        virtual ~Event() = default;
    private:
        /**
         * For easy access to Event private data
         * */
        friend class EventDispatcher;

        EventType m_Type;
        EventCategory m_Categories;
    protected:
        /**
         * This function should not be called directly by the user.
         * It is to be defined by the type of event that specializes this
         * interface
         * */
        MKT_NODISCARD virtual auto ToString() const -> std::string_view = 0;

        /**
         * In case we want to avoid propagating an Event we mark it as handled.
         * It is under protected because we want the specializations of this
         * interface to be able to directly access it, otherwise we may need
         * a sort of public interface to control this member.
         *
         * If this event has been handled this variables holds true,
         * false if this Event has not been handled yet
         * */
         bool m_Handled{};
    };

    /**
     * This concept ensures type safety determining in for the dispatcher method.
     * If the event defines the static member function GetStaticType()
     * (Ideally all events should implement it)
     * */
    template<typename EventClassType>
    concept HasStaticGetType = requires (EventClassType) { EventClassType::GetStaticType(); };

    /**
     * This a mechanism that handles the distribution and routing of events
     * to appropriate event handlers. It acts as a central hub or manager
     * for events and facilitates communication between different parts of our system.
     *
     * When it receives an Event it forwards or propagates it to the appropriate
     * handler (essentially calls the handler with the provided event). If the
     * Handler function does not want to propagate the Event, it marks it as handled,
     * that is why the handler returns a boolean that indicates this state.
     * */
    class EventDispatcher {
    public:
        /**
         * Alias for event function. The function is supposed to return true if the
         * event has been handled successfully, false otherwise.
         * @tparam T type of event, is supposed to be a class type like <code>kT::KeyPressedEvent</code>, etc
         * */
        template<typename T>
        using EventFunc_T = std::function<bool(T&)>;

        explicit EventDispatcher(Event& event)
            :   m_Event{ event } {}

        template<typename EventClassType>
            requires HasStaticGetType<EventClassType>
        MKT_NODISCARD auto Forward(EventFunc_T<EventClassType> func) -> bool {
            if (m_Event.GetType() == EventClassType::GetStaticType()) {
                m_Event.m_Handled = func(*(static_cast<EventClassType*>(&m_Event)));
                return true;
            }

            return false;
        }

    private:
        /**
         * Using a reference instead of pointer because a pointer
         * would require to construct an Event which an abstract class
         * */
        Event& m_Event;
    };

    /**
     * Returns the event category from the given event. The event could be part of
     * different categories simultaneously.
     * @returns Event category for the given type
     * */
    MKT_NODISCARD constexpr auto GetCategoryFromType( const EventType type) -> EventCategory {
        switch (type) {
            case EventType::EMPTY_EVENT: return EMPTY_EVENT_CATEGORY;

            case EventType::WINDOW_RESIZE_EVENT:
            case EventType::WINDOW_CLOSE_EVENT:
            case EventType::WINDOW_MOVED_EVENT: return WINDOW_EVENT_CATEGORY;

            case EventType::APP_CLOSE_EVENT:
            case EventType::APP_RENDER_EVENT:
            case EventType::APP_UPDATE_EVENT:
            case EventType::APP_TICK_EVENT: return APPLICATION_EVENT_CATEGORY;

            case EventType::KEY_PRESSED_EVENT:
            case EventType::KEY_RELEASED_EVENT:
            case EventType::KEY_CHAR_EVENT: return static_cast<EventCategory>( KEY_EVENT_CATEGORY | INPUT_EVENT_CATEGORY );

            case EventType::MOUSE_BUTTON_PRESSED_EVENT:
            case EventType::MOUSE_BUTTON_RELEASED_EVENT: return static_cast<EventCategory>( INPUT_EVENT_CATEGORY | MOUSE_EVENT_CATEGORY | MOUSE_BUTTON_EVENT_CATEGORY );

            case EventType::MOUSE_MOVED_EVENT:
            case EventType::MOUSE_SCROLLED_EVENT: return static_cast<EventCategory>( INPUT_EVENT_CATEGORY | MOUSE_BUTTON_EVENT_CATEGORY );

            case EventType::CAMERA_ENABLE_ROTATION: return PANEL_EVENT_CATEGORY;

            default: return EMPTY_EVENT_CATEGORY;
        }
    }


    /**
     * Returns the exact string representation of the given EventType enum
     * @returns EventType string representation
     * */
    MKT_NODISCARD constexpr auto GetEventFormattedStr(EventType type) -> std::string_view {
        switch(type) {
            case EventType::EMPTY_EVENT: return "EMPTY_EVENT";

            // Window events.
            // Category [WINDOW_EVENT_CATEGORY]
            case EventType::WINDOW_RESIZE_EVENT: return "WINDOW_RESIZE_EVENT";
            case EventType::WINDOW_CLOSE_EVENT: return "WINDOW_CLOSE_EVENT";
            case EventType::WINDOW_MOVED_EVENT: return "WINDOW_MOVED_EVENT";

            // Application events.
            // Category [APPLICATION_EVENT_CATEGORY]
            case EventType::APP_RENDER_EVENT: return "APP_RENDER_EVENT";
            case EventType::APP_UPDATE_EVENT: return "APP_UPDATE_EVENT";
            case EventType::APP_TICK_EVENT: return "APP_TICK_EVENT";
            case EventType::APP_CLOSE_EVENT: return "APP_CLOSE_EVENT";

            // Key Events.
            // Category [KEYBOARD_EVENT_CATEGORY]
            case EventType::KEY_PRESSED_EVENT: return "KEY_PRESSED_EVENT";
            case EventType::KEY_RELEASED_EVENT: return "KEY_RELEASED_EVENT";
            case EventType::KEY_CHAR_EVENT: return "KEY_CHAR_EVENT";

            // Mouse button events.
            // Category [MOUSE_EVENT_CATEGORY]
            case EventType::MOUSE_BUTTON_PRESSED_EVENT: return "MOUSE_BUTTON_PRESSED_EVENT";
            case EventType::MOUSE_BUTTON_RELEASED_EVENT: return "MOUSE_BUTTON_RELEASED_EVENT";

            // Mouse events.
            // Category [MOUSE_BUTTON_EVENT_CATEGORY]
            case EventType::MOUSE_MOVED_EVENT: return "MOUSE_MOVED_EVENT";
            case EventType::MOUSE_SCROLLED_EVENT: return "MOUSE_SCROLLED_EVENT";


            // Panel events.
            // Category [PANEL_EVENT_CATEGORY]
            case EventType::CAMERA_ENABLE_ROTATION: return "CAMERA_ENABLE_ROTATION";

            default: return "EVENT_TYPE_COUNT";
        }
    }


    /**
     * Helper to print an Event to console
     * */
    inline auto operator<<(std::ostream& out, const Event& e) -> std::ostream& {
        return out << "Type: " << e.GetNameStr();
    }
}

#endif // MIKOTO_EVENT_HH
