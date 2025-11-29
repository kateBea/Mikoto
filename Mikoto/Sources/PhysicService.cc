//
// Created by zanet on 1/26/2025.
//

#include <ranges>
#include <cstdarg>
#include <utility>

#include <Core/Profiler.hh>
#include <Logging/Logger.hh>
#include <Physics/PhysicService.hh>
#include <Scene/Component.hh>
#include <Scene/Scene.hh>

namespace Mikoto {


    PhysicService::PhysicService( const PhysicServiceCreateInfo & )
    {}

    auto PhysicService::Init() -> void {
        MKT_CORE_LOGGER_INFO("Initializing PhysicService...");

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
            MKT_CORE_LOGGER_INFO( "No phyisics simulation world for scene {}", scene->GetName() );
        }
    }

    auto PhysicService::CreatePhysicsWorld( const PhysicsWorldCreateInfo &spec ) -> PhysicsWorld* {
        const auto [it, success]{ m_Worlds.try_emplace( spec.TargetScene, PhysicsWorld::Create(spec) ) };

        if (success) {
            it->second->Init();
        }

        return it->second.get();
    }

}// namespace Mikoto
