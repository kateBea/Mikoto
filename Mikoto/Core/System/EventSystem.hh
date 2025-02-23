/**
 * EventSystem.hh
 * Created by kate on 10/7/23.
 * */

#ifndef MIKOTO_EVENT_MANAGER_HH
#define MIKOTO_EVENT_MANAGER_HH

#include <Common/Common.hh>
#include <Core/Engine.hh>
#include <Core/Events/Event.hh>
#include <Library/Utility/Types.hh>
#include <functional>
#include <set>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace Mikoto {
    /**
     * EventClassType must be derived from the base Event class
     * */
    template<typename EventClassType>
    concept IsEventDerived = std::is_base_of_v<Event, EventClassType>;

    class EventHandlerWrapper {
    public:
        EventHandlerWrapper( EventType type, EventHandler_T&& func )
            : m_Type{ type }, m_Category{ GetCategoryFromType( type ) }, m_Handler{ std::move( func ) } {
        }

        EventHandlerWrapper( EventHandlerWrapper&& other ) = default;
        auto operator=( EventHandlerWrapper&& other ) noexcept -> EventHandlerWrapper& = default;

        auto Exec( Event& event ) const -> bool {
            return m_Handler( event );
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
        auto operator==( const EventHandlerWrapper& other ) const -> bool {
            return m_Type == other.m_Type && m_Category == other.m_Category;
        }

    private:
        EventType m_Type{};
        EventCategory m_Category{};
        EventHandler_T m_Handler{};
    };

    class EventSystem final : public IEngineSystem {
    public:

        // Holds all the event subscribers with the corresponding event handler for each type of event.
        // Subscribers are differentiated by their universally unique identifier (uuid for short).
        // When a subscriber wants to receive some type of event, it is added to this map, and when that
        // event has been triggered, the event handler will be run.
        using Subscribers_T = std::unordered_map<UInt64_T, std::vector<EventHandlerWrapper>>;

        // Represents an event queue
        using EventQueue_T = std::vector<Scope_T<Event>>;

        // List of events handlers
        using Handlers_T = std::vector<EventHandlerWrapper>;

    public:
        explicit EventSystem(const EngineConfig& options);

        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto Update() -> void override;

        template<typename EventType, typename... Args>
            requires IsEventDerived<EventType>
        MKT_NODISCARD auto MakeEvent( Args&&... args ) -> Scope_T<Event> {
            return CreateScope<EventType>( std::forward<Args>( args )... );
        }

        /**
         * Returns the queue of pending events
         * @returns queue of pending events
         * */
        auto GetEventQueue() -> EventQueue_T& {
            return m_EventQueue;
        }

        /**
         * Returns the set of event subscribers
         * @returns event subscribers
         * */
        auto GetSubscribers() -> Subscribers_T& {
            return m_Subscribers;
        }

        /**
         * Subscribes an object to be notified when a type of event has happened.
         * @param subId identifier for the subscriber object
         * @param type type of event to subscribe to
         * @param handler event handler from the subscriber
         * */
        auto Subscribe( const UInt64_T subId, EventType type, EventHandler_T&& handler ) -> void {
            if ( !m_Subscribers.contains( subId ) ) {
                auto [fst, snd]{ m_Subscribers.try_emplace( subId, Handlers_T{} ) };
                fst->second.emplace_back( type, std::move( handler ) );

            } else {
                m_Subscribers[subId].emplace_back( type, std::move( handler ) );
            }
        }

        /**
         * Unsubscribes the object with the given id from the event type specified.
         * When that type of event is triggered, the specified handler will no longer be run.
         * @param subId subscriber unique identifier
         * @param type type of event to unsubscribe from
         * */
        auto Unsubscribe( UInt64_T subId, EventType type ) -> void;

        /**
         * Unsubscribes the object with the given id from the event category.
         * When any type of event of the given category is triggered, the specified
         * handler will no longer be run.
         * @param subId subscriber unique identifier
         * @param category event category to unsubscribe from
         * */
        auto Unsubscribe( UInt64_T subId, EventCategory category ) -> void;

        /**
         * Adds the given event to the queue of unhandled events
         * @param event event to be added
         * */
        auto QueueEvent( Scope_T<Event>&& event ) -> void {
            m_EventQueue.emplace_back( std::move( event ) );
        }

        /**
         * Can be executed by a publisher to notify a type of event has happened.
         * @param args arguments to be passed to the event
         * */
        template<typename EventType, typename... Args>
        auto Trigger( Args&&... args ) -> void {
            QueueEvent( MakeEvent<EventType>( std::forward<Args>( args )... ) );
        }

        /**
         * Execute event handlers.
         * */
        auto ProcessEvents() -> void;

    private:
        Subscribers_T m_Subscribers{};
        EventQueue_T m_EventQueue{};

    };
}// namespace Mikoto

#endif// MIKOTO_EVENT_MANAGER_HH
