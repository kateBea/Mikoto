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

#ifndef MIKOTO_PLATFORM_EVENT_SYSTEM_HH
#define MIKOTO_PLATFORM_EVENT_SYSTEM_HH

#include <ranges>
#include <type_traits>

#include <EASTL/utility.h>
#include <EASTL/vector.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/functional.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Service.hh>
#include <Core/Profiler.hh>
#include <Core/Singleton.hh>

#include <Math/Random.hh>

#include <Platform/Event.hh>

namespace mikoto::platform {

    template<typename EventClassType>
    concept IsEventDerived = std::is_base_of_v<Event, EventClassType>;

    class EventHandler {
    public:
        EventHandler( EventType type, HandlerFunc&& func );

        EventHandler( EventCategory category, HandlerFunc&& func );

        EventHandler( EventHandler&& other ) noexcept = default;
        auto operator=( EventHandler&& other ) noexcept -> EventHandler& = default;

        MKT_NODISCARD auto Exec( Event& event ) const -> bool;

        MKT_NODISCARD auto GetType() const -> EventType;
        MKT_NODISCARD auto GetCategory() const -> EventCategory;
        MKT_NODISCARD auto GetHandler() const -> HandlerFunc;

        /**
         * Returns true if this EventHandlerWrapper and other are the same, meaning
         * they have the same type of event and the event is from same categories.
         * @param other EventHandlerWrapper the implicit parameter is compared to
         * @returns true if this EventHandlerWrapper and other are the same, false otherwise
         * */
        auto operator==( const EventHandler& other ) const -> bool;

    private:
        EventType mType{ EventType::UNKNOWN_EVENT };
        EventCategory mCategory{ EMPTY_EVENT_CATEGORY };
        HandlerFunc mHandler{};
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
    class Subscriber : public core::INonCopyable, public core::INonMoveable {
    public:
        Subscriber() = default;

        MKT_NODISCARD auto GetID() const -> const math::random::Guid&;

        auto GetHandler( EventType type ) -> HandlerFunc;
        auto GetHandler( EventCategory type ) -> HandlerFunc;

        virtual ~Subscriber();

    protected:
        auto RegisterSelf() -> void;

        template<typename EventType>
        auto AddHandler( HandlerFunc&& handler ) -> void {
            MKT_BEGIN_PROFILER();
            AddHandler( EventType::GetStaticType(), eastl::forward<HandlerFunc>( handler ) );
        }

        auto AddHandler( EventType type, HandlerFunc&& handler ) -> void;
        auto AddHandler( EventCategory category, HandlerFunc&& handler ) -> void;

    protected:
        math::random::Guid mGuid{};
        ankerl::unordered_dense::map<EventType, EventHandler> mHandlersByType{};
        ankerl::unordered_dense::map<EventCategory, EventHandler> mHandlersByCategory{};
    };

    struct EventServiceCreateInfo {
    };

    class EventSystem final : public core::IService, public core::Singleton<EventSystem> {
    public:
        explicit EventSystem( const EventServiceCreateInfo& options );

        auto Initialize() -> void override;
        auto Shutdown() -> void override;

        auto PollEvents() -> void;

        auto Subscribe( Subscriber* subscriber ) -> void;
        auto UnSubscribe( const Subscriber* subscriber ) -> void;

        MKT_NODISCARD auto IsSubscribed( Subscriber* subscriber ) -> bool;

        /**
         * Queues an event to be processed later by the event service.
         * @param args arguments to be passed to the event
         * */
        template<typename EventType, typename... Args>
        auto Queue( Args&&... args ) -> void {
            MKT_BEGIN_PROFILER();
            QueueEvent( MakeEvent<EventType>( eastl::forward<Args>( args )... ) );
        }

        /**
         * Immediately processes an event by notifying all relevant subscribers.
         * @param args arguments to be passed to the event
         * */
        template<typename EventType, typename... Args>
        auto Emit( Args&&... args ) -> void {
            MKT_BEGIN_PROFILER();
            auto event{ MakeEvent<EventType>( eastl::forward<Args>( args )... ) };

            Dispatch( *event );
        }

    private:
        template<typename EventType, typename... Args>
            requires IsEventDerived<EventType>
        MKT_NODISCARD static auto MakeEvent( Args&&... args ) -> eastl::unique_ptr<Event> {
            MKT_BEGIN_PROFILER();
            return eastl::make_unique<EventType>( eastl::forward<Args>( args )... );
        }

        /**
         * Adds the given event to the queue of unhandled events
         * @param params event to be added
         * */
        template<typename... Args>
        auto QueueEvent( Args&&... params ) -> void {
            MKT_BEGIN_PROFILER();

            std::lock_guard lock{ mPushEventMutex };
            mEventQueue.emplace_back( eastl::forward<Args>( params )... );
        }

    private:
        auto Dispatch( Event& event ) -> void;

    private:
        std::mutex mPushEventMutex{};

        eastl::vector<eastl::unique_ptr<Event>> mEventQueue{};
        eastl::vector<eastl::unique_ptr<Event>> mDispatchQueue{};
        ankerl::unordered_dense::map<core::u64, Subscriber*> mSubscribers{};

        bool mIsPolling{};
    };
}// namespace mikoto::platform

#endif// MIKOTO_PLATFORM_EVENT_SYSTEM_HH
