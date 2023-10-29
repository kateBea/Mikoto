/**
 * EventManager.hh
 * Created by kate on 10/7/23.
 * */

#ifndef MIKOTO_EVENT_MANAGER_HH
#define MIKOTO_EVENT_MANAGER_HH

#include <set>
#include <vector>
#include <functional>
#include <string_view>
#include <unordered_map>

#include "Event.hh"

namespace Mikoto::EventManager {
    /**
     * This concept ensures type safety determining in for the dispatcher method.
     * If the event defines the static member function GetStaticType()
     * (Ideally all events should implement it)
     * */
    template<typename EventClassType>
    concept HasStaticGetType = requires (EventClassType) { EventClassType::GetStaticType(); };

    /**
     * EventClassType must be derived from the base Event class
     * */
    template<typename EventClassType>
    concept IsEventDerived = std::is_base_of_v<Event, EventClassType>;

    template<typename EventType, typename... Args>
        requires IsEventDerived<EventType>
    MKT_NODISCARD inline auto MakeEvent(Args&&... args) -> std::unique_ptr<Event> {
        return std::make_unique<EventType>(std::forward<Args>(args)...);
    }

    /**
     * Alias for event function. The function is supposed to return true if the
     * event has been handled successfully, false otherwise.
     * */
    using EventHandler_T = std::function<bool(Event&)>;

    class EventHandlerWrapper {
    public:
        EventHandlerWrapper(EventType type, EventHandler_T&& func)
            :   m_Handler{ std::move(func) }
            ,   m_Type{ type }
            ,   m_Category{ GetCategoryFromType(type) }
        {

        }

        EventHandlerWrapper(EventHandlerWrapper&& other) = default;
        auto operator=(EventHandlerWrapper&& other) noexcept -> EventHandlerWrapper& = default;

        auto Exec(Event& event) -> bool {
            return m_Handler(event);
        }

        MKT_NODISCARD auto GetType() const -> EventType { return m_Type; }
        MKT_NODISCARD auto GetCategory() const -> EventCategory { return m_Category; }
        MKT_NODISCARD auto GetHandler() const -> EventHandler_T { return m_Handler; }

        /**
         * Returns true if this EventHandlerWrapper and other are the same, meaning
         * they have same type of event and the event is from same categories.
         * @param other EventHandlerWrapper the implicit parameter is compared to
         * @returns true if this EventHandlerWrapper and other are the same, false otherwise
         * */
        auto operator==(const EventHandlerWrapper& other) -> bool {
            return m_Type == other.m_Type && m_Category == other.m_Category;
        }

    private:
        EventType m_Type{};
        EventCategory m_Category{};
        EventHandler_T m_Handler{};
    };

    // List of events handlers
    using Handlers_T = std::vector<EventHandlerWrapper>;

    // Holds all the event subscribers with the corresponding event handler for each type of event.
    // Subscribers are differentiated by their universally unique identifier (uuid for short).
    // When a subscriber wants to receive some type of event, it is added to this map, and when that
    // event has been triggered, the event handler will be run.
    using Subscribers_T = std::unordered_map<UInt64_T, std::vector<EventHandlerWrapper>>;

    // Represents an event queue
    using EventQueue_T = std::vector<std::unique_ptr<Event>>;

    /**
     * Returns the queue of pending events
     * @returns queue of pending events
     * */
    inline auto GetEventQueue() -> EventQueue_T& {
        static std::vector<std::unique_ptr<Event>> eventQueue{};
        return eventQueue;
    }

    /**
     * Returns the set of event subscribers
     * @returns event subscribers
     * */
    inline auto GetSubscribers() -> Subscribers_T& {
        static Subscribers_T subscribers{};
        return subscribers;
    }

    /**
     * Subscribes an object to be notified when a type of event has happened.
     * @param subId identifier for the subscriber object
     * @param handler event handler from the subscriber
     * */
    inline auto Subscribe(UInt64_T subId, EventType type, EventHandler_T&& handler) -> void {
        auto& subscribers{ GetSubscribers() };

        if (!subscribers.contains(subId)) {
            // subscriber does not exists
            auto result{ subscribers.emplace(subId, Handlers_T{}) };
            result.first->second.emplace_back(type, std::move(handler));

        }
        else {
            // subscriber does exists
            subscribers[subId].emplace_back(type, std::move(handler));

        }
    }

    /**
     * Unsubscribes the object with the given id from the event type specified.
     * When that type of event is triggered, the specified handler will no longer be run.
     * @param subId subscriber unique identifier
     * @param type type of event to unsubscribe from
     * */
    auto Unsubscribe(UInt64_T subId, EventType type) -> void;

    /**
     * Unsubscribes the object with the given id from the event category.
     * When any type of event of the given category is triggered, the specified
     * handler will no longer be run.
     * @param subId subscriber unique identifier
     * @param category event category to unsubscribe from
     * */
    auto Unsubscribe(UInt64_T subId, EventCategory category) -> void;

    /**
     * Adds the given event to the queue of unhandled events
     * @param event event to be added
     * */
    inline auto QueueEvent(std::unique_ptr<Event>&& event) -> void {
        GetEventQueue().push_back(std::move(event));
    }

    /**
     * Can be executed by a publisher to notify a type of event has happened.
     * @param event event to be triggered
     * */
    template<typename EventType, typename... Args>
    inline auto Trigger(Args&&... args) -> void {
        QueueEvent(MakeEvent<EventType>(std::forward<Args>(args)...));
    }

    /**
     * Execute event handlers
     * */
    auto ProcessEvents() -> void;

    /**
     * Cleanup
     * */
    auto Shutdown() -> void;
}

#endif // MIKOTO_EVENT_MANAGER_HH
