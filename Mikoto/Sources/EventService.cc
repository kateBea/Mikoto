/**
 * EventManager.cc
 * Created by kate on 10/7/23.
 * */

// C++ Standard Library
#include <utility>
#include <algorithm>
#include <iterator>
#include <ranges>

// Project Headers
#include <Core/EventService.hh>
#include <Core/Profiler.hh>
#include <Library/Utility/Types.hh>
#include <Logging/Logger.hh>

namespace Mikoto {

    auto Subscriber::GetID() const -> const GlobalUniqueID& { return m_UniqueID; }

    auto Subscriber::GetHandler( const EventType type ) -> HandlerFunc {

        if ( const auto it{ m_HandlersByType.find( type ) }; it != m_HandlersByType.end() ) {
            return it->second.GetHandler();
        }

        return {};
    }

    auto Subscriber::GetHandler( const EventCategory type ) -> HandlerFunc {
        if ( const auto it{ m_HandlersByCategory.find( type ) }; it != m_HandlersByCategory.end() ) {
            return it->second.GetHandler();
        }

        return {};
    }

    auto Subscriber::AddHandler( EventType type, HandlerFunc handler ) -> void {
        const auto [it, success] {
            m_HandlersByType.try_emplace( type, EventHandler{ type, std::move(handler) } )
        };

        if ( !success ) {
            MKT_CORE_LOGGER_WARN( "Handler for type {} already exists", static_cast<UInt32>(type) );
        }
    }

    auto Subscriber::AddHandler( const EventCategory category, HandlerFunc handler ) -> void {
        const auto [it, success] {
            m_HandlersByCategory.try_emplace( category, EventHandler{ category, std::move(handler) } )
        };

        if ( !success ) {
            MKT_CORE_LOGGER_WARN( "Handler for category {} already exists", static_cast<UInt32>(category) );
        }
    }

    EventService::EventService(const EventServiceCreateInfo& options) {}

    auto EventService::ProcessEvents() -> void {
        for (const auto& event : m_EventQueue ) {
            for ( auto& subscriber: m_Subscribers | std::views::values ) {

                // If the subscriber has a handler for the exact type of event we have, call it
                // Otherwise check whether the subscriber has a handler for the category of the event and call it
                if ( const auto handler{ subscriber->GetHandler( event->GetType() ) }; handler ) {
                    event->SetHandled( handler( *event ) );
                } else if ( const auto handler{ subscriber->GetHandler( event->GetCategoryFlags() ) }; handler ) {
                    event->SetHandled( handler( *event ) );
                }
            }
        }

        m_EventQueue.clear();
    }

    auto EventService::Init() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_CORE_LOGGER_INFO("Initializing EventService...");

        m_IsInitialized = true;
    }

    auto EventService::Update(float dt) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Process pending events if any
        ProcessEvents();
    }

    auto EventService::Subscribe( Subscriber *subscriber ) -> void {
        if (!subscriber) {
            MKT_CORE_LOGGER_WARN("Trying to subscribe a null subscriber");
            return;
        }

        const auto it{ m_Subscribers.try_emplace( static_cast<UInt64>(subscriber->GetID()), subscriber )  };
    }

    auto EventService::UnSubscribe( const Subscriber *subscriber ) -> void {
        if (!subscriber) {
            MKT_CORE_LOGGER_WARN("Trying to unsubscribe a null subscriber");
            return;
        }

        m_Subscribers.erase( static_cast<UInt64>(subscriber->GetID()) );
    }

    auto EventService::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (!m_IsInitialized) {
            return;
        }

        // The Log comes after so we know the service was
        // initialized before attempting to shut it down
        MKT_CORE_LOGGER_INFO( "Shutting down EventService..." );

        ProcessEvents();

        m_EventQueue.clear();
        m_Subscribers.clear();
    }
}
