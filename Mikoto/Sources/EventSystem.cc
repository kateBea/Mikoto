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

#include <ranges>

#include <EASTL/algorithm.h>
#include <EASTL/functional.h>
#include <EASTL/iterator.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/utility.h>
#include <EASTL/vector.h>
#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Event.hh>
#include <Core/Profiler.hh>
#include <Core/Singleton.hh>
#include <Core/Subsystem.hh>
#include <Core/EventSystem.hh>

#include <Math/Random.hh>

#include <Logging/Logger.hh>

namespace mikoto::core {

    auto Subscriber::GetID() const -> const Guid& { return mGuid; }

    auto Subscriber::GetHandler( const EventType type ) -> HandlerFunc {

        if ( const auto it{ mHandlersByType.find( type ) }; it != mHandlersByType.end() ) {
            return it->second.GetHandler();
        }

        return {};
    }

    auto Subscriber::GetHandler( const EventCategory type ) -> HandlerFunc {
        if ( const auto it{ mHandlersByCategory.find( type ) }; it != mHandlersByCategory.end() ) {
            return it->second.GetHandler();
        }

        return {};
    }

    auto Subscriber::RegisterSelf() -> void {
        if (!EventSystem::Get()->IsSubscribed(this)) {
            EventSystem::Get()->Subscribe( this );
        }
    }

    auto Subscriber::AddHandler( EventType type, HandlerFunc&& handler ) -> void {
        MKT_ASSERT( EventSystem::GetPtr() != nullptr, "Event system is null" );
        MKT_ASSERT( EventSystem::Get()->IsInitialized(), "Event system is not initialized" );

        RegisterSelf();

        const auto [it, success] {
            mHandlersByType.try_emplace( type, EventHandler{ type, std::move(handler) } )
        };

        if ( !success ) {
            MKT_CORE_LOGGER_WARN( "Handler for type {} already exists", static_cast<u32>(type) );
        }
    }

    auto Subscriber::AddHandler( const EventCategory category, HandlerFunc&& handler ) -> void {
        MKT_ASSERT( EventSystem::GetPtr() != nullptr, "Event system is null" );
        MKT_ASSERT( EventSystem::Get()->IsInitialized(), "Event system is not initialized" );

        RegisterSelf();

        const auto [it, success] {
            mHandlersByCategory.try_emplace( category, EventHandler{ category, std::move(handler) } )
        };

        if ( !success ) {
            MKT_CORE_LOGGER_WARN( "Handler for category {} already exists", static_cast<u32>(category) );
        }
    }

    EventSystem::EventSystem(const EventServiceCreateInfo& ) {}

    auto EventSystem::ProcessEvents() -> void {
        for (const auto& event : mEventQueue ) {
            for ( auto& subscriber: mSubscribers | std::views::values ) {

                // If the subscriber has a handler for the exact type of event we have, call it
                // Otherwise check whether the subscriber has a handler for the category of the event and call it
                if ( const auto handlerByType{ subscriber->GetHandler( event->GetType() ) } ) {
                    event->SetHandled( handlerByType( *event ) );
                } else if ( const auto handlerByCategory{ subscriber->GetHandler( event->GetCategoryFlags() ) } ) {
                    event->SetHandled( handlerByCategory( *event ) );
                }
            }
        }

        mEventQueue.clear();
    }

    auto EventSystem::Initialize() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_CORE_LOGGER_INFO("Initializing EventService...");

        mIsInitialized = true;
    }

    auto EventSystem::Update(float dt) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        ProcessEvents();
    }

    auto EventSystem::Subscribe( Subscriber *subscriber ) -> void {
        if (!subscriber) {
            MKT_CORE_LOGGER_WARN("Trying to subscribe a null subscriber");
            return;
        }

        const auto it{ mSubscribers.try_emplace( as<u64>(subscriber->GetID()), subscriber )  };
    }

    auto EventSystem::UnSubscribe( const Subscriber *subscriber ) -> void {
        if (!subscriber) {
            MKT_CORE_LOGGER_WARN("Trying to unsubscribe a null subscriber");
            return;
        }

        mSubscribers.erase( as<u64>(subscriber->GetID()) );
    }

    auto EventSystem::IsSubscribed( Subscriber* subscriber ) -> bool {
        MKT_ASSERT( subscriber, "Subscriber pointer is null." );
        return mSubscribers.contains( as<u64>(subscriber->GetID()) );
    }

    auto EventSystem::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (!mIsInitialized) {
            return;
        }

        // The Log comes after so we know the service was
        // initialized before attempting to shut it down
        MKT_CORE_LOGGER_INFO( "Shutting down EventService..." );

        ProcessEvents();

        mEventQueue.clear();
        mSubscribers.clear();
    }
}
