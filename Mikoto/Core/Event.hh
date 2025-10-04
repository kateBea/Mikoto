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
#include <Common/Common.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {

    /**
     * This concept ensures type safety determining in for the dispatcher method.
     * If the event defines the static member function GetStaticType()
     * (Ideally all events should implement it)
     * */
    template<typename EventClassType>
    concept HasStaticGetType = requires (EventClassType) { EventClassType::GetStaticType(); };

    /**
     * Simply specifies the type of an Event
     * */
    enum class EventType {
        UNKNOWN_EVENT,

        // Window events.
        // Category [WINDOW_EVENT_CATEGORY]
        WINDOW_RESIZE_EVENT,
        WINDOW_CLOSE_EVENT,
        WINDOW_MOVED_EVENT,

        // Application events.
        // Category [APPLICATION_EVENT_CATEGORY]
        APP_RENDER_EVENT,
        APP_UPDATE_EVENT,
        APP_TICK_EVENT,
        APP_CLOSE_EVENT,

        // Key Events.
        // Category [KEYBOARD_EVENT_CATEGORY]
        KEY_PRESSED_EVENT,
        KEY_RELEASED_EVENT,
        KEY_CHAR_EVENT,

        // Mouse button events.
        // Category [MOUSE_EVENT_CATEGORY]
        MOUSE_BUTTON_PRESSED_EVENT,
        MOUSE_BUTTON_RELEASED_EVENT,

        // Mouse events.
        // Category [MOUSE_BUTTON_EVENT_CATEGORY]
        MOUSE_MOVED_EVENT,
        MOUSE_SCROLLED_EVENT,

        // Panels Events
        // Category [PANEL_EVENT_CATEGORY]
        CAMERA_ENABLE_ROTATION,

        EVENT_TYPE_COUNT,
    };

    /**
     * Specifies the group of an event. This is defined
     * if and event handler may need to filter certain events,
     * this is an easy way to group all of our events into one set.
     * Bear in mind an event can be of different categories at the same time,
     * for this purpose we may want to query the category flags later by using a
     * bit wise OR which is supported by integers, for simplicity sake, this structure
     * is not declared as an enum class
     * */
    enum EventCategory : UInt32 {
        EMPTY_EVENT_CATEGORY = BIT_SET( 0 ),

        APPLICATION_EVENT_CATEGORY = BIT_SET( 1 ),
        INPUT_EVENT_CATEGORY = BIT_SET( 2 ),
        WINDOW_EVENT_CATEGORY = BIT_SET( 3 ),
        KEY_EVENT_CATEGORY = BIT_SET( 4 ),
        MOUSE_EVENT_CATEGORY = BIT_SET( 5 ),
        MOUSE_BUTTON_EVENT_CATEGORY = BIT_SET( 6 ),
        PANEL_EVENT_CATEGORY = BIT_SET( 8 ),

        EVENT_CATEGORY_COUNT = BIT_SET( 9 ),
    };

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
        Event( const EventType type, const EventCategory categories)
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
        MKT_NODISCARD auto GetName() const -> std::string_view { return ToString(); }

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

        auto SetHandled( const bool value) -> void { m_Handled = value; }

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
     * Alias for event function. The function is supposed to return true if the
     * event has been handled successfully, false otherwise.
     * */
    using HandlerFunc = std::function<bool(Event&)>;

    /**
     * Returns the event category from the given event. The event could be part of
     * different categories simultaneously.
     * @returns Event category for the given type
     * */
    MKT_NODISCARD constexpr auto GetCategoryFromType( const EventType type) -> EventCategory {
        switch (type) {
            case EventType::UNKNOWN_EVENT: return EMPTY_EVENT_CATEGORY;

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
            case EventType::UNKNOWN_EVENT: return "EMPTY_EVENT";

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
}

#endif // MIKOTO_EVENT_HH
