/**
 * EventService.hh
 * Created by kate on 10/7/23.
 * */

#ifndef MIKOTO_EVENT_MANAGER_HH
#define MIKOTO_EVENT_MANAGER_HH

#include <Common/Common.hh>
#include <Common/Service.hh>
#include <Core/Event.hh>
#include <Library/Random/Random.hh>
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

    /**
    * @brief Base class for all event listeners (subscribers).
    *
    * Classes that want to listen for specific types of events must inherit from this class.
    * To receive events, the subscriber must register itself with the event service (e.g., `EventService`)
    * using the `Subscribe` function.
    *
    * Example usage:
    *
    * @code
    * class MyListener : public Subscriber {
    * public:
    *
    * };
    *
    * // Somewhere during initialization:
    * EventService::GetInstance()->Subscribe<MyEvent>(myListenerPtr);
    * @endcode
    *
    * The event system will then deliver matching events to the subscriber's registered handler.
    */
    class Subscriber {
    public:

        auto GetID() const -> const GlobalUniqueID& { return m_UniqueID; }

        auto GetHandler(EventType type) -> const EventHandler_T&;
        auto GetHandler(EventCategory type) -> const EventHandler_T&;

    protected:
        auto AddHandler(EventType type, EventHandler_T handler) -> void;

    protected:
        GlobalUniqueID m_UniqueID{};
        ankerl::unordered_dense::map<EventType, EventHandlerWrapper> m_Handlers{};
    };

    struct EventServiceCreateInfo {
    };

    class EventService final : public IService<EventService> {
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
        explicit EventService( const EventServiceCreateInfo& options );

        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto Update() -> void;

        auto Subscribe(Subscriber* subscriber, EventType eventType) -> void;
        auto Subscribe(Subscriber* subscriber, EventCategory eventCategory) -> void;

        auto UnSubscribe(Subscriber* subscriber, EventType eventType) -> void;
        auto UnSubscribe(Subscriber* subscriber, EventCategory eventCategory) -> void;

        /**
         * Can be executed by a publisher to notify a type of event has happened.
         * @param args arguments to be passed to the event
         * */
        template<typename EventType, typename... Args>
        auto Trigger( Args&&... args ) -> void {
            QueueEvent( MakeEvent<EventType>( std::forward<Args>( args )... ) );
        }

    private:
        template<typename EventType, typename... Args>
            requires IsEventDerived<EventType>
        MKT_NODISCARD static auto MakeEvent( Args&&... args ) -> Scope_T<Event> {
            return CreateScope<EventType>( std::forward<Args>( args )... );
        }

        /**
         * Adds the given event to the queue of unhandled events
         * @param params event to be added
         * */
        auto QueueEvent( auto&&... params ) -> void {
            m_EventQueue.emplace_back( std::forward<decltype(params)>(params)... );
        }

        /**
         * Execute event handlers.
         * */
        auto ProcessEvents() -> void;

    private:
        std::vector<Subscriber> m_EventSubscribers{};
        std::vector<Scope_T<Event>> m_EventQueue{};
    };
}// namespace Mikoto

#endif// MIKOTO_EVENT_MANAGER_HH
