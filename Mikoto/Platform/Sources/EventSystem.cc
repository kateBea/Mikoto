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

#include <EASTL/utility.h>
#include <EASTL/vector.h>
#include <EASTL/iterator.h>
#include <EASTL/algorithm.h>
#include <EASTL/functional.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/fixed_vector.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Service.hh>
#include <Core/Profiler.hh>
#include <Core/Singleton.hh>

#include <Logging/Logger.hh>
#include <Logging/Assert.hh>

#include <Math/Random.hh>

#include <Platform/Event.hh>
#include <Platform/EventSystem.hh>

namespace mikoto::platform {
    
    using namespace mikoto::core;

    Subscriber::~Subscriber() {
        MKT_BEGIN_PROFILER();

        if ( EventSystem::HasInstance() ) {
            EventSystem::Get()->UnSubscribe( this );
        }
    }

    auto Subscriber::GetID() const -> const math::random::Guid& {
        MKT_BEGIN_PROFILER();
        return mGuid;
    }

    auto Subscriber::GetHandler( const EventType type ) -> HandlerFunc {
        MKT_BEGIN_PROFILER();

        if ( const auto it{ mHandlersByType.find( type ) }; it != mHandlersByType.end() ) {
            return it->second.GetHandler();
        }

        return {};
    }

    auto Subscriber::GetHandler( const EventCategory type ) -> HandlerFunc {
        MKT_BEGIN_PROFILER();

        if ( const auto it{ mHandlersByCategory.find( type ) }; it != mHandlersByCategory.end() ) {
            return it->second.GetHandler();
        }

        return {};
    }

    auto Subscriber::RegisterSelf() -> void {
        MKT_BEGIN_PROFILER();

        if ( !EventSystem::Get()->IsSubscribed( this ) ) {
            EventSystem::Get()->Subscribe( this );
        }
    }

    auto Subscriber::AddHandler( EventType type, HandlerFunc&& handler ) -> void {
        MKT_BEGIN_PROFILER();

        MKT_ASSERT( EventSystem::HasInstance(), "Event system is null" );
        MKT_ASSERT( EventSystem::Get()->IsInitialized(), "Event system is not initialized" );

        RegisterSelf();

        const auto [it, success]{
            mHandlersByType.try_emplace( type, EventHandler{ type, eastl::move( handler ) } )
        };

        if ( !success ) {
            MKT_CORE_LOGGER_WARN( "Handler for type {} already exists", static_cast<core::u32>( type ) );
        }
    }

    auto Subscriber::AddHandler( const EventCategory category, HandlerFunc&& handler ) -> void {
        MKT_BEGIN_PROFILER();

        MKT_ASSERT( EventSystem::HasInstance(), "Event system is null" );
        MKT_ASSERT( EventSystem::Get()->IsInitialized(), "Event system is not initialized" );

        RegisterSelf();

        const auto [it, success]{
            mHandlersByCategory.try_emplace( category, EventHandler{ category, eastl::move( handler ) } )
        };

        if ( !success ) {
            MKT_CORE_LOGGER_WARN( "Handler for category {} already exists", static_cast<core::u32>( category ) );
        }
    }

    EventSystem::EventSystem( const EventServiceCreateInfo& ) {
    }


    auto EventSystem::Initialize() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_CORE_LOGGER_INFO( "Initializing EventService..." );

        mIsInitialized = true;
    }

    auto EventSystem::Dispatch( Event& event ) -> void {
        MKT_BEGIN_PROFILER();

        // Snapshot IDs, not pointers: callbacks may destroy listeners or rehash
        // the subscriber table. Re-look-up each ID immediately before invocation.
        eastl::fixed_vector<core::u64, 32> recipients{};
        recipients.reserve( mSubscribers.size() );

        for ( const auto& [id, subscriber]: mSubscribers ) {
            recipients.push_back( id );
        }

        for ( const core::u64 id: recipients ) {
            const auto it{ mSubscribers.find( id ) };

            if ( it == mSubscribers.end() ) {
                continue;
            }

            auto handler{ it->second->GetHandler( event.GetType() ) };

            if ( !handler ) {
                handler = it->second->GetHandler( event.GetCategoryFlags() );
            }

            if ( handler ) {
                // Keep broadcast behavior. A later unhandled result must not
                // erase an earlier handled result. Copying the callback protects
                // its lifetime when it unregisters/destroys its owning listener.
                const bool handled{ handler( event ) };
                event.SetHandled( event.IsHandled() || handled );
            }
        }
    }

    auto EventSystem::PollEvents() -> void {
        MKT_BEGIN_PROFILER();

        if ( mIsPolling || !mIsInitialized ) {
            return;
        }

        mIsPolling = true;

        struct PollGuard {
            eastl::reference_wrapper<bool> mFlag;
            eastl::vector<eastl::unique_ptr<Event>>& mQueue;

            ~PollGuard() {
                MKT_BEGIN_PROFILER();
                mQueue.clear();
                mFlag.get() = false;
            }
        } guard{ mIsPolling, mDispatchQueue };

        // Callbacks queue into the next batch. This avoids iterator invalidation
        // and prevents an event-producing callback from starving the frame.
        {
            std::lock_guard lock{ mPushEventMutex };
            mDispatchQueue.swap( mEventQueue );
        }

        for ( const auto& event: mDispatchQueue ) {
            Dispatch( *event );
        }
    }

    auto EventSystem::Subscribe( Subscriber* subscriber ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !subscriber ) {
            MKT_CORE_LOGGER_WARN( "Subscriber is null." );
            return;
        }

        const auto it{ mSubscribers.try_emplace( core::as<core::u64>( subscriber->GetID() ), subscriber ) };
    }

    auto EventSystem::UnSubscribe( const Subscriber* subscriber ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !subscriber ) {
            MKT_CORE_LOGGER_WARN( "Trying to unsubscribe a null subscriber" );
            return;
        }

        mSubscribers.erase( core::as<core::u64>( subscriber->GetID() ) );
    }

    auto EventSystem::IsSubscribed( Subscriber* subscriber ) -> bool {
        MKT_BEGIN_PROFILER();

        MKT_ASSERT( subscriber, "Subscriber pointer is null." );
        
        return mSubscribers.contains( core::as<core::u64>( subscriber->GetID() ) );
    }

    auto EventSystem::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( !mIsInitialized ) {
            return;
        }

        // The Log comes after so we know the service was
        // initialized before attempting to shut it down
        MKT_CORE_LOGGER_INFO( "Shutting down EventService..." );

        // If we had any pending
        PollEvents();

        mEventQueue.clear();
        mSubscribers.clear();
        mIsInitialized = false;
    }

    EventHandler::EventHandler( const EventType type, HandlerFunc&& func )
        : mType{ type }, mCategory{ GetCategoryFromType( type ) }, mHandler{ eastl::move( func ) } {
    }


    EventHandler::EventHandler( const EventCategory category, HandlerFunc&& func )
        : mCategory{ category }, mHandler{ eastl::move( func ) } {
    }


    auto EventHandler::Exec( Event& event ) const -> bool {
        MKT_BEGIN_PROFILER();
        return mHandler( event );
    }


    auto EventHandler::GetType() const -> EventType {
        MKT_BEGIN_PROFILER();
        return mType;
    }


    auto EventHandler::GetCategory() const -> EventCategory {
        MKT_BEGIN_PROFILER();
        return mCategory;
    }


    auto EventHandler::GetHandler() const -> HandlerFunc {
        MKT_BEGIN_PROFILER();
        return mHandler;
    }


    auto EventHandler::operator==( const EventHandler& other ) const -> bool {
        MKT_BEGIN_PROFILER();
        return mType == other.mType && mCategory == other.mCategory;
    }
}
