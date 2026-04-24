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

#ifndef MIKOTO_EVENT_SERVICER_HH
#define MIKOTO_EVENT_SERVICER_HH

#include <ranges>
#include <type_traits>

#include <EASTL/vector.h>
#include <EASTL/utility.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/functional.h>
#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Event.hh>
#include <Core/Singleton.hh>
#include <Core/Subsystem.hh>
#include <Core/Types.hh>
#include <Math/Random.hh>

namespace mikoto::core {

    using namespace mikoto::math;
    using namespace mikoto::math::random;

    template<typename EventClassType>
    concept IsEventDerived = std::is_base_of_v<Event, EventClassType>;

    class EventHandler {
    public:
        EventHandler( const EventType type, HandlerFunc&& func )
            : m_Type{ type }, m_Category{ GetCategoryFromType( type ) }, m_Handler{ eastl::move( func ) } {
        }

        EventHandler( const EventCategory category, HandlerFunc&& func )
            : m_Category{ category }, m_Handler{ eastl::move( func ) } {
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
    * EventService::Get()->Subscribe(myListenerPtr);
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
        auto RegisterSelf() -> void;

        template<typename EventType>
        auto AddHandler(HandlerFunc&& handler) -> void {
            AddHandler( EventType::GetStaticType(), eastl::forward<HandlerFunc>( handler ));
        }

        auto AddHandler(EventType type, HandlerFunc&& handler) -> void;
        auto AddHandler(EventCategory category, HandlerFunc&& handler) -> void;

    protected:
        GlobalUniqueID m_UniqueID{};
        ankerl::unordered_dense::map<EventType, EventHandler> m_HandlersByType{};
        ankerl::unordered_dense::map<EventCategory, EventHandler> m_HandlersByCategory{};
    };

    struct EventServiceCreateInfo {

    };

    class EventSystem final : public ISubsystem, public Singleton<EventSystem> {
    public:
        explicit EventSystem( const EventServiceCreateInfo& options );

        auto Initialize() -> void override;
        auto Shutdown() -> void override;
        auto Update(float dt) -> void override;

        auto Subscribe(Subscriber* subscriber) -> void;
        auto UnSubscribe( const Subscriber* subscriber) -> void;

        MKT_NODISCARD auto IsSubscribed(Subscriber* subscriber) -> bool;

        /**
         * Queues an event to be processed later by the event service.
         * @param args arguments to be passed to the event
         * */
        template<typename EventType, typename... Args>
        auto Queue( Args&&... args ) -> void {
            QueueEvent( MakeEvent<EventType>( eastl::forward<Args>( args )... ) );
        }

        /**
         * Immediately processes an event by notifying all relevant subscribers.
         * @param args arguments to be passed to the event
         * */
        template<typename EventType, typename... Args>
        auto Emit( Args&&... args ) -> void {
            auto event{ MakeEvent<EventType>( eastl::forward<Args>( args )... ) };

            for ( auto& subscriber: mSubscribers | std::views::values ) {

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
        MKT_NODISCARD static auto MakeEvent( Args&&... args ) -> eastl::unique_ptr<Event> {
            return eastl::make_unique<EventType>( eastl::forward<Args>( args )... );
        }

        /**
         * Adds the given event to the queue of unhandled events
         * @param params event to be added
         * */
        template<typename... Args>
        auto QueueEvent( Args&&... params ) -> void {
            mEventQueue.emplace_back( eastl::forward<Args>(params)... );
        }

        /**
         * Execute event handlers.
         * */
        auto ProcessEvents() -> void;

    private:
        eastl::vector<eastl::unique_ptr<Event>> mEventQueue{};
        ankerl::unordered_dense::map<u64, Subscriber*> mSubscribers{};
    };
}

#endif// MIKOTO_EVENT_SERVICER_HH
