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

#ifndef MIKOTO_PLATFORM_EVENT_HH
#define MIKOTO_PLATFORM_EVENT_HH

#include <EASTL/string.h>
#include <EASTL/functional.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Flag.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

namespace mikoto::platform {

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
        CLIPBOARD_EVENT,
        CONTENT_DROPPED_EVENT,

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

        INPUT_DEVICE_CONNECTION_EVENT,
        GAMEPAD_BUTTON_PRESSED_EVENT,
        GAMEPAD_BUTTON_RELEASED_EVENT,
        GAMEPAD_AXIS_EVENT,
        STEERING_WHEEL_BUTTON_PRESSED_EVENT,
        STEERING_WHEEL_BUTTON_RELEASED_EVENT,
        STEERING_WHEEL_AXIS_EVENT,

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
    enum EventCategory {
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
     * @brief Returns the event category from the given event. The event could be part of
     * different categories simultaneously.
     * @param type Event type whose category flags are requested.
     * @returns Event category for the given type
     */
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
            case EventType::MOUSE_SCROLLED_EVENT:
                return static_cast<EventCategory>( INPUT_EVENT_CATEGORY | MOUSE_EVENT_CATEGORY );

            case EventType::INPUT_DEVICE_CONNECTION_EVENT:
            case EventType::GAMEPAD_BUTTON_PRESSED_EVENT:
            case EventType::GAMEPAD_BUTTON_RELEASED_EVENT:
            case EventType::GAMEPAD_AXIS_EVENT:
            case EventType::STEERING_WHEEL_BUTTON_PRESSED_EVENT:
            case EventType::STEERING_WHEEL_BUTTON_RELEASED_EVENT:
            case EventType::STEERING_WHEEL_AXIS_EVENT:
                return INPUT_EVENT_CATEGORY;

            case EventType::CAMERA_ENABLE_ROTATION: return PANEL_EVENT_CATEGORY;

            default: return EMPTY_EVENT_CATEGORY;
        }
    }

    /**
     * @brief Returns the exact string representation of the given EventType enum
     * @param type Event type to name.
     * @returns EventType string representation
     */
    MKT_NODISCARD constexpr auto GetEventFormattedStr(EventType type) -> eastl::string_view {
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
            case EventType::MOUSE_SCROLLED_EVENT:
                return "MOUSE_SCROLLED_EVENT";

            // Panel events.
            // Category [PANEL_EVENT_CATEGORY]
            case EventType::CAMERA_ENABLE_ROTATION: return "CAMERA_ENABLE_ROTATION";

            case EventType::INPUT_DEVICE_CONNECTION_EVENT: return "INPUT_DEVICE_CONNECTION_EVENT";
            case EventType::GAMEPAD_BUTTON_PRESSED_EVENT: return "GAMEPAD_BUTTON_PRESSED_EVENT";
            case EventType::GAMEPAD_BUTTON_RELEASED_EVENT: return "GAMEPAD_BUTTON_RELEASED_EVENT";
            case EventType::GAMEPAD_AXIS_EVENT: return "GAMEPAD_AXIS_EVENT";
            case EventType::STEERING_WHEEL_BUTTON_PRESSED_EVENT: return "STEERING_WHEEL_BUTTON_PRESSED_EVENT";
            case EventType::STEERING_WHEEL_BUTTON_RELEASED_EVENT: return "STEERING_WHEEL_BUTTON_RELEASED_EVENT";
            case EventType::STEERING_WHEEL_AXIS_EVENT: return "STEERING_WHEEL_AXIS_EVENT";

            default: return "EVENT_TYPE_COUNT";
        }
    }

#if MIKOTO_MODULE_RENDERER
    /**
     * @brief Pumps native events and routes them to their owning windows.
     */
    auto PollWindowEvents() -> void;
#endif

    struct EventCategoryFlagsProperties {
        using Data = core::u32;
    };

    using EventCategoryFlags = core::Flags<EventCategoryFlagsProperties>;

    struct EventCategoryFlagsBits {
        static constexpr EventCategoryFlags APPLICATION{ BIT_SET( 1 ) };
        static constexpr EventCategoryFlags INPUT{ BIT_SET( 2 ) };
        static constexpr EventCategoryFlags WINDOW{ BIT_SET( 3 ) };
        static constexpr EventCategoryFlags KEY{ BIT_SET( 4 ) };
        static constexpr EventCategoryFlags MOUSE{ BIT_SET( 5 ) };
        static constexpr EventCategoryFlags MOUSE_BUTTON{ BIT_SET( 6 ) };
    };

    /**
     * Abstract base class for engine events..
     * */
    class Event {
    public:
        Event(const Event& other) = default;
        Event( const EventType type, const EventCategory categories);

        MKT_NODISCARD virtual auto GetType() const -> EventType = 0;
        MKT_NODISCARD virtual auto IsType( const EventType type ) const -> bool;
        MKT_NODISCARD virtual auto GetCategoryFlags() const -> EventCategory;;

        MKT_NODISCARD auto GetName() const -> eastl::string_view;

        MKT_NODISCARD auto IsHandled() const -> bool;
        MKT_NODISCARD auto IsInCategory( EventCategory cat ) const -> bool;

        MKT_NODISCARD virtual auto DisplayData() const -> eastl::string = 0;

        auto SetHandled( const bool value ) -> void;

        virtual ~Event() = default;
    private:
        EventType mType;
        EventCategory mCategories;

    protected:
        /**
         * Returns the event name supplied by the derived class.
         * */
        MKT_NODISCARD virtual auto ToString() const -> eastl::string_view = 0;

        /**
         * Tracks whether the event has been handled and should stop propagating.
         * */
         bool mHandled{};
    };

    /**
     * Alias for event function. The function is supposed to return true if the
     * event has been handled successfully, false otherwise.
     * */
    using HandlerFunc = eastl::function<bool(Event&)>;

}

#endif// MIKOTO_PLATFORM_EVENT_HH
