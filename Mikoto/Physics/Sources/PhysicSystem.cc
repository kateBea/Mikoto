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

#include <utility>
#include <ranges>
#include <cstdarg>

#include <EASTL/fixed_string.h>

// The Jolt headers don't include Jolt.h. Always include Jolt.h before including any other Jolt header.
// You can use Jolt.h in your precompiled header to speed up compilation.
#include <Jolt/Jolt.h>

// Jolt includes
#include <Jolt/Core/Factory.h>
#include <Jolt/RegisterTypes.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Logging/Assert.hh>
#include <Logging/Logger.hh>

#include <Threading/Utility.hh>

#include <Physics/Utility.hh>
#include <Physics/PhysicsWorld.hh>
#include <Physics/PhysicSystem.hh>

#include <Renderer/Core/RenderSystem.hh>
#include <Renderer/Core/PhysicsDebugRenderer.hh>

#include <Scene/Scene.hh>

namespace mikoto::physics {

    using namespace mikoto::scene;

    // Route Jolt diagnostics through the engine logger.
    static auto TraceImpl( const char *inFMT, ... ) -> void {
        MKT_BEGIN_PROFILER();
        // Format the message
        std::va_list list{};
        va_start( list, inFMT );

        char buffer[1024]{};
        vsnprintf( buffer, sizeof( buffer ), inFMT, list );
        va_end( list );

        MKT_CORE_LOGGER_DEBUG( "Jolt Trace {}", buffer );
    }

    PhysicSystem::PhysicSystem( const PhysicServiceCreateInfo & ) {
    }

    auto PhysicSystem::InitializeRuntime() -> void {
        // Jolt's allocator and type registry are process-wide; worlds are created afterwards.
        JPH::RegisterDefaultAllocator();
        JPH::Trace = TraceImpl;
        JPH_IF_ENABLE_ASSERTS( JPH::AssertFailed = AssertFailedImpl; )
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();
        mJobSystem = eastl::make_unique<JPH::JobSystemThreadPool>( JPH::cMaxPhysicsJobs,
            JPH::cMaxPhysicsBarriers, threading::GetThreadConcurrency() );
    }

    auto PhysicSystem::InitializeDebugRenderer() -> void {
#if defined( JPH_DEBUG_RENDERER )
        if ( !renderer::RenderSystem::HasInstance() ) {
            return;
        }

        auto physicsRendererSimpleDesc{ renderer::PhysicsDebugRendererSimpleCreateInfo{}
            .SetName( "renderer::PhysicsDebugRendererSimple" )
            .SetShaderBasePath( "Resources/Shaders/slang" )
            .SetDevice( renderer::RenderSystem::Get()->GetGpuDevice() ) };
        mPhysicsDebugRendererSimple = renderer::PhysicsDebugRendererSimple::Create( physicsRendererSimpleDesc );

        if ( mPhysicsDebugRendererSimple ) {
            mPhysicsDebugRendererSimple->Initialize();
        }
#endif
    }

    auto PhysicSystem::Initialize() -> void {
        MKT_BEGIN_PROFILER();

        if ( mIsInitialized ) {
            return;
        }

        MKT_CORE_LOGGER_INFO( "Initializing PhysicSystem..." );

        try {
            InitializeRuntime();
            InitializeDebugRenderer();
#if defined( MIKOTO_ENABLE_PHYSX )
            mPhysXBackend.Initialize();
#endif
            mIsInitialized = true;
        } catch ( ... ) {
            mPhysicsDebugRendererSimple.reset();
            mJobSystem.reset();

            if ( JPH::Factory::sInstance ) {
                JPH::UnregisterTypes();
                delete JPH::Factory::sInstance;
                JPH::Factory::sInstance = nullptr;
            }

            throw;
        }
    }

    auto PhysicSystem::Shutdown() -> void {
        MKT_BEGIN_PROFILER();

        if ( !mIsInitialized ) {
            return;
        }

        mActiveWorld = nullptr;

        if ( mPhysicsDebugRenderer ) {
            mPhysicsDebugRenderer->Shutdown();
            mPhysicsDebugRenderer.reset();
        }

        if ( mPhysicsDebugRendererSimple ) {
            mPhysicsDebugRendererSimple->Shutdown();
            mPhysicsDebugRendererSimple.reset();
        }

        // The Log comes after so we know the service was
        // initialized before attempting to shut it down
        MKT_CORE_LOGGER_INFO( "Shutting down PhysicService..." );

        for ( auto &world: mWorlds | std::views::values ) {
            world->Shutdown();
            world->GetScene()->mPhysicsWorld = nullptr;
        }

        mWorlds.clear();

#if defined( MIKOTO_ENABLE_PHYSX )
        mPhysXBackend.Shutdown();
#endif

        mJobSystem.reset();

        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;

        mIsInitialized = false;
    }

    auto PhysicSystem::Update( float dt ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( mActiveWorld ) {
            if ( mActiveWorld->GetScene()->IsPlaying() || mActiveWorld->GetScene()->IsSimulating() ) {
                mActiveWorld->Update( dt );
            }
            else {
                mActiveWorld->Synchronize();
            }

            // The viewport gathers debug geometry after scene synchronization, including in edit mode.
        }
    }

    auto PhysicSystem::SetSimulationTarget( scene::Scene *scene ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !scene ) {
            mActiveWorld = nullptr;
            return;
        }

        const auto it{ mWorlds.find( scene ) };

        if ( it != mWorlds.end() ) {
            mActiveWorld = it->second.get();
        }
        else {
            MKT_ASSERT( false, string::Format( "No physics simulation world for scene {}", scene->GetName() ).c_str() );
        }
    }

    auto PhysicSystem::GetDebugRenderer() const -> renderer::PhysicsDebugRenderer * {
        MKT_BEGIN_PROFILER();
        return mPhysicsDebugRenderer.get();
    }

    auto PhysicSystem::RemovePhysicsWorld( scene::Scene *scene ) -> void {
        MKT_BEGIN_PROFILER();
        const auto it = mWorlds.find( scene );

        if ( it == mWorlds.end() ) {
            return;
        }

        if ( mActiveWorld == it->second.get() ) {
            mActiveWorld = nullptr;
        }

        it->second->Shutdown();
        scene->mPhysicsWorld = nullptr;
        mWorlds.erase( it );
    }

    auto PhysicSystem::GetDebugRendererSimple() const -> renderer::PhysicsDebugRendererSimple * {
        MKT_BEGIN_PROFILER();
        return mPhysicsDebugRendererSimple.get();
    }

    auto PhysicSystem::GetJoltJobSystem() -> JPH::JobSystemThreadPool * {
        MKT_BEGIN_PROFILER();
        return mJobSystem.get();
    }

#if defined( MIKOTO_ENABLE_PHYSX )
    auto PhysicSystem::GetPhysXBackend() -> PhysXBackend& {
        return mPhysXBackend;
    }
#endif

    auto PhysicSystem::CreatePhysicsWorld( const PhysicsWorldCreateInfo &spec ) -> PhysicsWorld * {
        MKT_BEGIN_PROFILER();
        const auto [it, success]{
            mWorlds.try_emplace( spec.mScene, PhysicsWorld::Create( spec ) )
        };

        if ( success ) {
            it->second->Initialize();
        }

        return it->second.get();
    }
}// namespace mikoto::physics
