//
// Created by zanet on 1/26/2025.
//

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

        if (m_PhysicsBase != nullptr) {
            m_PhysicsBase->Shutdown();
        }
        m_PhysicsBase.reset();

        m_IsInitialized = false;
    }

    auto PhysicService::Update( float dt ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (m_PhysicsBase) {
            m_PhysicsBase->Update( dt );
        }
    }

    auto PhysicService::SetSimulationScene( Scene *scene ) -> void {
        if (scene) {
            if (m_PhysicsBase == nullptr) {
                m_PhysicsBase = CreateScope<PhysicsBase>( scene );
                m_PhysicsBase->Init();
            }

            m_PhysicsBase->SetGravity( EARTH_GRAVITY );
        }
    }

    auto PhysicService::OnRigidBodyRemoved( RigidBodyComponent &rb ) -> void {
        if (m_PhysicsBase) {
            m_PhysicsBase->OnRigidBodyRemoved( rb );
        }
    }

    auto PhysicService::OnRigidBodyAdded( Entity &entity, RigidBodyComponent &rb ) -> void {
        if ( m_PhysicsBase ) {
            m_PhysicsBase->OnRigidBodyAdded( entity, rb );
        }
    }
    auto PhysicService::OnRigidBodyAdded( TransformComponent &tr, RigidBodyComponent &rb ) -> void {
        if ( m_PhysicsBase ) {
            m_PhysicsBase->OnRigidBodyAdded( tr, rb );
        }
    }

}// namespace Mikoto
