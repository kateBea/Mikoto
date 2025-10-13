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
#include <ranges>

#include <ankerl/unordered_dense.h>
#include <Common/Singleton.hh>

namespace Mikoto {

    /**
     * EventClassType must be derived from the base Event class
     * */
    template<typename EventClassType>
    concept IsEventDerived = std::is_base_of_v<Event, EventClassType>;

    class EventHandler {
    public:
        EventHandler( const EventType type, HandlerFunc&& func )
            : m_Type{ type }, m_Category{ GetCategoryFromType( type ) }, m_Handler{ std::move( func ) } {
        }

        EventHandler( const EventCategory category, HandlerFunc&& func )
            : m_Type{ EventType::UNKNOWN_EVENT }, m_Category{ category }, m_Handler{ std::move( func ) } {
        }

        EventHandler( EventHandler&& other ) = default;
        auto operator=( EventHandler&& other ) noexcept -> EventHandler& = default;

        auto Exec( Event& event ) const -> bool {
            return m_Handler( event );
        }

        MKT_NODISCARD auto GetType() const -> EventType { return m_Type; }
        MKT_NODISCARD auto GetCategory() const -> EventCategory { return m_Category; }
        MKT_NODISCARD auto GetHandler() const -> HandlerFunc { return m_Handler; }

        /**
         * Returns true if this EventHandlerWrapper and other are the same, meaning
         * they have the same type of event and the event is from same categories.
         * @param other EventHandlerWrapper the implicit parameter is compared to
         * @returns true if this EventHandlerWrapper and other are the same, false otherwise
         * */
        auto operator==( const EventHandler& other ) const -> bool {
            return m_Type == other.m_Type && m_Category == other.m_Category;
        }

    private:
        EventType m_Type{ EventType::UNKNOWN_EVENT };
        EventCategory m_Category{ EMPTY_EVENT_CATEGORY };
        HandlerFunc m_Handler{};
    };

    /**
    * @brief Base class for all event listeners (subscribers).
    *
    * Classes that want to listen for specific types of events must inherit from this class.
    * To receive events, the subscriber must register itself with the event service (e.g., `EventService`)
    * using the `Subscribe` function. A subscriber can handle multiple event types and categories.
    * If a subscriber has registered a handler for a specific category, it can no longer register handlers for any event type of that category
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
    * EventService::GetInstance()->Subscribe(myListenerPtr);
    * @endcode
    *
    * The event system will then deliver matching events to the subscriber's registered handler.
    */
    class Subscriber {
    public:

        MKT_NODISCARD auto GetID() const -> const GlobalUniqueID&;

        auto GetHandler(EventType type) -> HandlerFunc;
        auto GetHandler(EventCategory type) -> HandlerFunc;

    protected:
        auto AddHandler(EventType type, HandlerFunc handler) -> void;
        auto AddHandler(EventCategory category, HandlerFunc handler) -> void;

    protected:
        GlobalUniqueID m_UniqueID{};
        ankerl::unordered_dense::map<EventType, EventHandler> m_HandlersByType{};
        ankerl::unordered_dense::map<EventCategory, EventHandler> m_HandlersByCategory{};
    };

    struct EventServiceCreateInfo {

    };

    class EventService final : public IService, public Singleton<EventService> {
    public:
        explicit EventService( const EventServiceCreateInfo& options );

        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto Update(float dt) -> void override;

        auto Subscribe(Subscriber* subscriber) -> void;
        auto UnSubscribe( const Subscriber* subscriber) -> void;

        /**
         * Queues an event to be processed later by the event service.
         * @param args arguments to be passed to the event
         * */
        template<typename EventType, typename... Args>
        auto Queue( Args&&... args ) -> void {
            QueueEvent( MakeEvent<EventType>( std::forward<Args>( args )... ) );
        }

        /**
         * Immediately processes an event by notifying all relevant subscribers.
         * @param args arguments to be passed to the event
         * */
        template<typename EventType, typename... Args>
        auto Emit( Args&&... args ) -> void {
            auto event{ MakeEvent<EventType>( std::forward<Args>( args )... ) };

            for ( auto& subscriber: m_Subscribers | std::views::values ) {

                // If the subscriber has a handler for the exact type of event we have, call it
                // Otherwise check whether the subscriber has a handler for the category of the event and call it
                if ( const auto handlerByType{ subscriber->GetHandler( event->GetType() ) }; handlerByType ) {
                    event->SetHandled( handlerByType( *event ) );

                } else if ( const auto handlerByCategory{ subscriber->GetHandler( event->GetCategoryFlags() ) }; handlerByCategory ) {
                    event->SetHandled( handlerByCategory( *event ) );
                }
            }
        }

    private:
        template<typename EventType, typename... Args>
            requires IsEventDerived<EventType>
        MKT_NODISCARD static auto MakeEvent( Args&&... args ) -> Unique<Event> {
            return CreateScope<EventType>( std::forward<Args>( args )... );
        }

        /**
         * Adds the given event to the queue of unhandled events
         * @param params event to be added
         * */
        template<typename... Args>
        auto QueueEvent( Args&&... params ) -> void {
            m_EventQueue.emplace_back( std::forward<Args>(params)... );
        }

        /**
         * Execute event handlers.
         * */
        auto ProcessEvents() -> void;

    private:
        std::vector<Unique<Event>> m_EventQueue{};
        ankerl::unordered_dense::map<UInt64, Subscriber*> m_Subscribers{};
    };
}// namespace Mikoto

#endif// MIKOTO_EVENT_MANAGER_HH
