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
//

#include <ranges>
#include <cstdarg>
#include <utility>
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
#include <Core/String.hh>
#include <Core/Profiler.hh>

#include <Threading/ThreadUtility.hh>

#include <Scene/Scene.hh>

#include <Logging/Logger.hh>

#include <Physics/PhysicSystem.hh>
#include <Physics/PhysicsWorld.hh>
#include <Renderer/Core/PhysicsDebugRenderer.hh>

// TODO: Command context needed because incomplete class
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/RenderSystem.hh>
#include <Renderer/Core/CommandContext.hh>

namespace mikoto::physics {

    using namespace mikoto::scene;

    // Callback for traces, connect this to your own trace function if you have one
    static auto TraceImpl( const char* inFMT, ... ) -> void {
        // Format the message
        std::va_list list{};
        va_start( list, inFMT );

        eastl::fixed_string<char, 1024> buffer{};
        vsnprintf( buffer.data(), buffer.max_size(), inFMT, list );
        va_end( list );

        MKT_CORE_LOGGER_DEBUG( "Jolt Trace {}", eastl::string( buffer ).c_str() );
    }

    PhysicSystem::PhysicSystem( const PhysicServiceCreateInfo & )
    {}

    auto PhysicSystem::Initialize() -> void {
        MKT_CORE_LOGGER_INFO("Initializing PhysicService...");

        // If you want your code to compile using single or double precision write
        // 0.0_r to get a Real value that compiles to double or float depending on
        // whether JPH_DOUBLE_PRECISION is set or not.
        using namespace JPH::literals;

        // Register allocation hook. In this example we'll just let Jolt use malloc / free,
        // but you can override these if you want (see Memory.h).
        // This needs to be done before any other Jolt function is called.
        JPH::RegisterDefaultAllocator();

        // Install trace and assert callbacks
        JPH::Trace = TraceImpl;
        JPH_IF_ENABLE_ASSERTS( JPH::AssertFailed = AssertFailedImpl; )

        // Create a factory, this class is responsible for creating instances of classes
        // based on their name or hash and is mainly used for deserialization of saved data.
        // It is not directly used in this example but still required.
        JPH::Factory::sInstance = new JPH::Factory();

        // Register all physics types with the factory and install their collision handlers
        // with the CollisionDispatch class. If you have your own custom shape types you probably
        // need to register their handlers with the CollisionDispatch before calling this function.
        // If you implement your own default material (PhysicsMaterial::sDefault) make sure to
        // initialize it before this function or else this function will create one for you.
        JPH::RegisterTypes();

        // We need a job system that will execute physics jobs on multiple threads. Typically
        // you would implement the JobSystem interface yourself and let Jolt Physics run on top
        // of your own job scheduler. JobSystemThreadPool is an example implementation.
        mJobSystem = eastl::make_unique<JPH::JobSystemThreadPool>( JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, threading::GetThreadConcurrency() );

        // Can only create one instance of DebugRenderer

#if false
        // Physics debug renderer
        auto physicsRendererDesc{ PhysicsDebugRendererCreateInfo{}
            .SetName( "PhysicsDebugRenderer" )
            .SetShaderBasePath( "Resources/Shaders/slang" )
            .SetDevice( RenderSystem::Get()->GetGpuDevice() ) };
        mPhysicsDebugRenderer = PhysicsDebugRenderer::Create( physicsRendererDesc );

        if (mPhysicsDebugRenderer) {
            mPhysicsDebugRenderer->Init();
        }
#else

        // Physics debug renderer simple
        auto physicsRendererSimpleDesc{ PhysicsDebugRendererSimpleCreateInfo{}
            .SetName( "PhysicsDebugRendererSimple" )
            .SetShaderBasePath( "Resources/Shaders/slang" )
            .SetDevice( RenderSystem::Get()->GetGpuDevice() ) };
        mPhysicsDebugRendererSimple = PhysicsDebugRendererSimple::Create( physicsRendererSimpleDesc );

        if (mPhysicsDebugRendererSimple) {
            mPhysicsDebugRendererSimple->Init();
        }
#endif

        mIsInitialized = true;
    }

    auto PhysicSystem::Shutdown() -> void {
        if (!mIsInitialized) {
            return;
        }

        mActiveWorld = nullptr;

        // The Log comes after so we know the service was
        // initialized before attempting to shut it down
        MKT_CORE_LOGGER_INFO( "Shutting down PhysicService..." );

        for ( auto &world: mWorlds | std::views::values ) {
            world->Shutdown();
        }

        mWorlds.clear();

        mJobSystem.reset();

        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;

        mIsInitialized = false;
    }

    auto PhysicSystem::Update( float dt ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (mActiveWorld) {
            mActiveWorld->Update( dt );

            // Physics world is drawn using debug lines, when not paused
            // Draw state prior to step so that debug lines are created from the same state
            // (the constraints are solved on the current state and then the world is stepped)
            if (mPhysicsDebugRenderer) {
                mActiveWorld->DrawPhysics( mPhysicsDebugRenderer.get() );
            }

            if (mPhysicsDebugRendererSimple) {
                mActiveWorld->DrawPhysics( mPhysicsDebugRendererSimple.get() );
            }
        }
    }

    auto PhysicSystem::SetSimulationTarget( Scene *scene ) -> void {
        const auto it{ mWorlds.find( scene  ) };

        if (it != mWorlds.end()) {
            mActiveWorld = it->second.get();
        } else {
            MKT_ASSERT( false, string::Format( "No physics simulation world for scene {}", scene->GetName() ).c_str() );
        }
    }

    auto PhysicSystem::GetDebugRenderer() const -> renderer::PhysicsDebugRenderer * {
        return mPhysicsDebugRenderer.get();
    }

    auto PhysicSystem::GetDebugRendererSimple() const -> renderer::PhysicsDebugRendererSimple * {
        return mPhysicsDebugRendererSimple.get();
    }

    auto PhysicSystem::GetJoltJobSystem() -> JPH::JobSystemThreadPool * {
        return mJobSystem.get();
    }

    auto PhysicSystem::CreatePhysicsWorld( const PhysicsWorldCreateInfo &spec ) -> PhysicsWorld* {
        const auto [it, success] {
            mWorlds.try_emplace( spec.mScene, PhysicsWorld::Create(spec) )
        };

        if (success) {
            it->second->Initialize();
        }

        return it->second.get();
    }
}// namespace mikoto::physics
