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

#include <Jolt/Jolt.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Logging/Assert.hh>
#include <Logging/Logger.hh>

#include <Physics/PhysicsWorld.hh>
#include <Physics/PhysicSystem.hh>

#include <Renderer/Core/RenderSystem.hh>
#include <Renderer/Core/PhysicsDebugRenderer.hh>

#include <Scene/Scene.hh>

namespace mikoto::physics {

    using namespace mikoto::scene;

    PhysicSystem::PhysicSystem( const PhysicServiceCreateInfo & ) {
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
            mPhysicsBackend.Initialize();
            InitializeDebugRenderer();
            mIsInitialized = true;
        } catch ( ... ) {
            mPhysicsDebugRendererSimple.reset();
            mPhysicsBackend.Shutdown();
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
        mPhysicsBackend.Shutdown();

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

    auto PhysicSystem::GetPhysicsBackend() const -> const PhysicsBackend& {
        return mPhysicsBackend;
    }

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
