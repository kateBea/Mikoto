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
#include <cstdarg>
#include <utility>

// The Jolt headers don't include Jolt.h. Always include Jolt.h before including any other Jolt header.
// You can use Jolt.h in your precompiled header to speed up compilation.
#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <Core/Profiler.hh>
#include <Logging/Logger.hh>

#include <Scene/Scene.hh>
#include <Scene/Component.hh>

#include <Physics/PhysicService.hh>

namespace Mikoto {

    PhysicService::PhysicService( const PhysicServiceCreateInfo & )
    {}

    auto PhysicService::Init() -> void {
        MKT_CORE_LOGGER_INFO("Initializing PhysicService...");

        // If you want your code to compile using single or double precision write
        // 0.0_r to get a Real value that compiles to double or float depending
        // if JPH_DOUBLE_PRECISION is set or not.
        using namespace JPH::literals;

        // Register allocation hook. In this example we'll just let Jolt use malloc / free
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

        m_IsInitialized = true;
    }

    auto PhysicService::Shutdown() -> void {
        if (!m_IsInitialized) {
            return;
        }

        // The Log comes after so we know the service was
        // initialized before attempting to shut it down
        MKT_CORE_LOGGER_INFO( "Shutting down PhysicService..." );

        m_ActiveWorld = nullptr;

        for ( auto &world: m_Worlds | std::views::values ) {
            world->Shutdown();
        }

        m_Worlds.clear();

        JPH::UnregisterTypes();

        delete JPH::Factory::sInstance;

        m_IsInitialized = false;
    }

    auto PhysicService::Update( float dt ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (m_ActiveWorld) {
            m_ActiveWorld->Update( dt );
        }
    }

    auto PhysicService::SetSimulationTarget( Scene *scene ) -> void {
        const auto it{ m_Worlds.find( scene  ) };

        if (it != m_Worlds.end()) {
            m_ActiveWorld = it->second.get();
        } else {
            MKT_CORE_LOGGER_INFO( "No physics simulation world for scene {}", scene->GetName() );
        }
    }

    auto PhysicService::CreatePhysicsWorld( const PhysicsWorldCreateInfo &spec ) -> PhysicsWorld* {
        const auto [it, success]{ m_Worlds.try_emplace( spec.TargetScene, PhysicsWorld::Create(spec) ) };

        if (success) {
            it->second->Init();
        }

        return it->second.get();
    }
}
