//
// Created by kate on 10/22/25.
//

#include <memory>
#include <atomic>

#include <entt/entt.hpp>

// The Jolt headers don't include Jolt.h. Always include Jolt.h before including any other Jolt header.
// You can use Jolt.h in your precompiled header to speed up compilation.
#include <Jolt/Jolt.h>

// Jolt includes
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <Library/String/String.hh>
#include <Logging/Assert.hh>
#include <Logging/Logger.hh>
#include <Physics/PhysicsWorld.hh>
#include <Scene/Component.hh>
#include <Scene/Scene.hh>
#include <Threading/ThreadUtility.hh>

namespace Mikoto {

    PhysicsWorld::PhysicsWorld( const PhysicsWorldCreateInfo &spec )
        : m_Scene{ spec.TargetScene }, m_Gravity{ spec.Gravity } {}

    auto PhysicsWorld::Init() -> void {
        MKT_CORE_LOGGER_INFO( "Initializing PhysicsWorld..." );

        // We need a temp allocator for temporary allocations during the physics update. We're
        // pre-allocating 10 MB to avoid having to do allocations during the physics update.
        // B.t.w. 10 MB is way too much for this example but it is a typical value you can use.
        // If you don't want to pre-allocate you can also use TempAllocatorMalloc to fall back to
        // malloc / free.
        m_SimulationInfo.TempAllocator = CreateScope<JPH::TempAllocatorImpl>( 10 * 1024 * 1024 );

        // We need a job system that will execute physics jobs on multiple threads. Typically
        // you would implement the JobSystem interface yourself and let Jolt Physics run on top
        // of your own job scheduler. JobSystemThreadPool is an example implementation.
        m_SimulationInfo.JobSystem = CreateScope<JPH::JobSystemThreadPool>(
                JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, ThreadUtils::InferConcurrentThreads() - 1 );

        // This is the max amount of rigid bodies that you can add to the physics system. If you try to add more you'll get an error.
        // Note: This value is low because this is a simple test. For a real project use something in the order of 65536.
        constexpr UInt32 cMaxBodies{ 1024 };

        // This determines how many mutexes to allocate to protect rigid bodies from concurrent access. Set it to 0 for the default settings.
        constexpr UInt32 cNumBodyMutexes{ 0 };

        // This is the max amount of body pairs that can be queued at any time (the broad phase will detect overlapping
        // body pairs based on their bounding boxes and will insert them into a queue for the narrowphase). If you make this buffer
        // too small the queue will fill up and the broad phase jobs will start to do narrow phase work. This is slightly less efficient.
        // Note: This value is low because this is a simple test. For a real project use something in the order of 65536.
        constexpr UInt32 cMaxBodyPairs{ 1024 };

        // This is the maximum size of the contact constraint buffer. If more contacts (collisions between bodies) are detected than this
        // number then these contacts will be ignored and bodies will start interpenetrating / fall through the world.
        // Note: This value is low because this is a simple test. For a real project use something in the order of 10240.
        constexpr UInt32 cMaxContactConstraints{ 1024 };

        // Initialize physics system
        m_SimulationInfo.PhysicsSystem.Init(
                cMaxBodies,
                cNumBodyMutexes,
                cMaxBodyPairs,
                cMaxContactConstraints,

                // These need to be alive for as long as the physics system needs it
                m_SimulationInfo.BroadPhaseLayerInterface,
                m_SimulationInfo.ObjectVsBroadPhaseLayerFilter,
                m_SimulationInfo.ObjectLayerPairFilter );

        m_SimulationInfo.BodyInterface = std::addressof( m_SimulationInfo.PhysicsSystem.GetBodyInterface() );

        m_SimulationInfo.PhysicsSystem.SetGravity( JPH::Vec3( m_Gravity.x, m_Gravity.y, m_Gravity.z ) );

#if false
        // A body activation listener gets notified when bodies activate and go to sleep
        // Note that this is called from a job so whatever you do here needs to be thread safe.
        // Registering one is entirely optional.
        m_SimulationInfo.PhysicsSystem.SetBodyActivationListener( std::addressof( m_SimulationInfo.BodyActivationListener ) );

        // A contact listener gets notified when bodies (are about to) collide, and when they separate again.
        // Note that this is called from a job so whatever you do here needs to be thread safe.
        // Registering one is entirely optional.
        m_SimulationInfo.PhysicsSystem.SetContactListener( std::addressof( m_SimulationInfo.ContactListener ) );
#endif

        // Optional step: Before starting the physics simulation you can optimize the broad phase. This improves collision detection performance (it's pointless here because we only have 2 bodies).
        // You should definitely not call this every frame or when e.g. streaming in a new level section as it is an expensive operation.
        // Instead insert all new objects in batches instead of 1 at a time to keep the broad phase efficient.
        m_SimulationInfo.PhysicsSystem.OptimizeBroadPhase();

        m_IsInitialized = true;
    }

    auto PhysicsWorld::Shutdown() -> void {
        if ( !m_IsInitialized ) {
            return;
        }

        // The Log comes after so we know the service was
        // initialized before attempting to shut it down
        MKT_CORE_LOGGER_INFO( "Shutting down PhysicsWorld..." );

        for ( auto &val: m_Bodies | std::views::values ) {
            m_SimulationInfo.BodyInterface->RemoveBody(val->GetID());
            m_SimulationInfo.BodyInterface->DestroyBody(val->GetID());
        }

        m_SimulationInfo.JobSystem.reset();
        m_SimulationInfo.TempAllocator.reset();


        m_IsInitialized = false;
    }

    auto PhysicsWorld::Update( float dt ) -> void {
        MKT_ASSERT( m_SimulationInfo.BodyInterface, "BodyInterface is NULL forgot to call PhysicsWorld::Init()" );
        MKT_ASSERT( m_SimulationInfo.JobSystem, "JobSystem is NULL forgot to call PhysicsWorld::Init()" );
        MKT_ASSERT( m_SimulationInfo.TempAllocator, "TempAllocator is NULL forgot to call PhysicsWorld::Init()" );

        PreUpdate();

        constexpr int collisionSteps{ 1 };
        m_SimulationInfo.PhysicsSystem.Update( dt, collisionSteps, m_SimulationInfo.TempAllocator.get(), m_SimulationInfo.JobSystem.get() );

        PostUpdate();
    }

    auto PhysicsWorld::PreUpdate() -> void {
        // Here we line up ECS and Jolt
        auto &registry{ m_Scene->GetRegistry() };

        for ( auto [entity, rb, tr]: registry.view<RigidBodyComponent, TransformComponent>().each() ) {
            if ( !rb.IsValidBodyID() ) {
                continue;
            }

            const auto body{ GetJoltBody( rb.GetBodyID() ) };

            m_SimulationInfo.BodyInterface->SetPositionAndRotation( body->GetID(), ToVec3( tr.GetTranslation() ), ToQuat( tr.GetRotation() ), JPH::EActivation::Activate );

            UpdateBodyProperties( body, rb );

            // Jolt puts bodies to sleep to save resources
            if (!rb.IsBodyType( RigidBodyComponent::BodyType::STATIC ) && !m_SimulationInfo.BodyInterface->IsActive( body->GetID() ) ) {
                m_SimulationInfo.BodyInterface->ActivateBody( body->GetID() );
            }

        }
    }

    auto PhysicsWorld::UpdateBodyProperties(JPH::Body* body, RigidBodyComponent& rb ) const -> void {
        const auto motionType{ ConvertToJoltMotionType( rb.GetBodyType( ) ) };

        // Line up motion types if it has been updated
        if (motionType != body->GetMotionType()) {
            m_SimulationInfo.BodyInterface->SetMotionType( body->GetID(), motionType, JPH::EActivation::Activate );
        }

        if (rb.IsBodyType( RigidBodyComponent::BodyType::KINEMATIC )) {
            m_SimulationInfo.BodyInterface->SetAngularVelocity( body->GetID(), ToVec3( rb.GetAngularVelocity() ) );
            m_SimulationInfo.BodyInterface->SetLinearVelocity( body->GetID(), ToVec3( rb.GetLinearVelocity() ) );
        }

        // Mass and friction
        if (rb.IsBodyType(RigidBodyComponent::BodyType::DYNAMIC) && (1.0f / rb.GetMass()) != body->GetMotionProperties()->GetInverseMass()) {
            JPH::MassProperties	massPropertiesOverride{};
            massPropertiesOverride.mMass = rb.GetMass(); // Jolt uses kg, see docs
            body->GetMotionProperties()->SetMassProperties( body->GetMotionProperties()->GetAllowedDOFs(), massPropertiesOverride );
        }

        // Degrees of freedom dynamic objects (which axis the object is allowed to rotate, translate


        // Restitution
        body->SetRestitution( rb.GetRestitution() );
    }

    auto PhysicsWorld::PostUpdate() -> void {
        auto& registry{ m_Scene->GetRegistry() };

        for ( auto [entity, rb, tr]: registry.view<RigidBodyComponent, TransformComponent>().each() ) {
            if ( !rb.IsValidBodyID() )
                continue;

            const auto body{ GetJoltBody( rb.GetBodyID() ) };
            const JPH::RMat44 transform{ m_SimulationInfo.BodyInterface->GetCenterOfMassTransform( body->GetID() ) };

            const JPH::Vec3 position{ transform.GetTranslation() };
            const JPH::Quat rotation{ transform.GetRotation().GetQuaternion() };

            tr.SetTranslation( ToVec3F( position ) );
            tr.SetRotation( ToQuatF( rotation ) );
        }
    }

    auto PhysicsWorld::GenerateBodyID() -> UInt64 {
        return m_BodyIdCounter.fetch_add(1, std::memory_order_relaxed);
    }

    auto PhysicsWorld::GetJoltBody( const UInt64 id ) -> JPH::Body * {
        const auto it{ m_Bodies.find(id) };
        return it == m_Bodies.end() ? nullptr : it->second;
    }

    auto PhysicsWorld::OnRigidBodyRemoved( Entity &entity ) -> void {

        if ( !entity.HasComponent<RigidBodyComponent>() ) {
            return;
        }

        RigidBodyComponent &rb{ entity.GetComponent<RigidBodyComponent>() };

        OnRigidBodyRemoved( rb );
    }

    auto PhysicsWorld::OnRigidBodyAdded( Entity &entity ) -> void {
        if ( !entity.HasComponent<RigidBodyComponent>() ) {
            return;
        }

        if ( !m_SimulationInfo.BodyInterface ) {
            return;
        }

        auto &transform{ entity.GetComponent<TransformComponent>() };
        auto &rigiBody{ entity.GetComponent<RigidBodyComponent>() };

        OnRigidBodyAdded( transform, rigiBody );
    }

    auto PhysicsWorld::OnRigidBodyRemoved( RigidBodyComponent &rigidBody ) -> void {
        if ( !rigidBody.IsValidBodyID() ) {
            return;
        }

        const auto body{ GetJoltBody( rigidBody.GetBodyID() ) };
        m_SimulationInfo.BodyInterface->RemoveBody( body->GetID() );
        m_SimulationInfo.BodyInterface->DestroyBody( body->GetID() );

        m_Bodies.erase( rigidBody.GetBodyID() );

        rigidBody.RemoveBodyID();
    }

    auto PhysicsWorld::OnRigidBodyAdded( TransformComponent &transformComponent, RigidBodyComponent &rigidBodyComponent ) -> void {
        // Convert transform to Jolt space
        JPH::Vec3 size{ ToVec3( transformComponent.GetScale() ) };
        JPH::Vec3 position{ transformComponent.GetTranslation().x, transformComponent.GetTranslation().y, transformComponent.GetTranslation().z };

        // Simple shape for now (box)
        const JPH::BoxShapeSettings shapeSettings{ size };
        auto shape{ shapeSettings.Create().Get() };

        JPH::BodyCreationSettings settings{
            shape,
            position,
            JPH::Quat::sIdentity(),
            ConvertToJoltMotionType( rigidBodyComponent.GetBodyType() ),
            Layers::MOVING
        };

        settings.mFriction = rigidBodyComponent.GetFriction();
        settings.mMassPropertiesOverride.mMass = rigidBodyComponent.GetMass();
        settings.mRestitution = rigidBodyComponent.GetRestitution();

        // When this body is created as static, this setting tells the system to create
        // a MotionProperties object so that the object can be switched to kinematic or dynamic
        settings.mAllowDynamicOrKinematic = true;

        // Motion quality, or how well it detects collisions when it has a high velocity
        settings.mMotionQuality = JPH::EMotionQuality::LinearCast;

        JPH::Body *body{ m_SimulationInfo.BodyInterface->CreateBody( settings ) };
        m_SimulationInfo.BodyInterface->AddBody( body->GetID(), JPH::EActivation::Activate );

        const auto [it, success]{ m_Bodies.try_emplace( GenerateBodyID(), body ) };

        if ( success ) {
            rigidBodyComponent.SetBodyID( it->first );
        }
    }

    auto PhysicsWorld::GetGravityFor( const GravityBody body) -> Vec3F {
        // https://en.wikipedia.org/wiki/List_of_gravitationally_rounded_objects_of_the_Solar_System
        switch (body) {
            case GravityBody::EARTH:   return { 0.0f, -9.80665f, 0.0f };
            case GravityBody::MOON:    return { 0.0f, -1.62f,    0.0f };
            case GravityBody::MARS:    return { 0.0f, -3.721f,   0.0f };
            case GravityBody::JUPITER: return { 0.0f, -24.79f,   0.0f };

            default: return { 0.0f, -9.80665f, 0.0f }; // fallback: Earth
        }
    }

    auto PhysicsWorld::Create( const PhysicsWorldCreateInfo &spec ) -> Unique<PhysicsWorld> {
        return CreateScope<PhysicsWorld>( spec );
    }

    auto PhysicsWorld::ToMat4F( const JPH::RMat44 &m ) -> glm::mat4 {
        glm::mat4 out{};

        for (int c = 0; c < 4; ++c)
        {
            JPH::Vec4 col = m.GetColumn4(c);  // RMat44 stores row-major internally, Jolt provides columns.
            out[c][0] = col.GetX();
            out[c][1] = col.GetY();
            out[c][2] = col.GetZ();
            out[c][3] = col.GetW();
        }

        return out;
    }

    auto PhysicsWorld::ToVec3F( const JPH::Vec3 &v ) -> glm::vec3 {
        return glm::vec3{ v.GetX(), v.GetY(), v.GetZ() };
    }

    auto PhysicsWorld::ToQuatF( const JPH::Quat &q ) -> glm::quat {
        return glm::quat{ q.GetW(), q.GetX(), q.GetY(), q.GetZ() };
    }

    auto PhysicsWorld::ToVec3( const glm::vec3 &v ) -> JPH::Vec3 {
        return JPH::Vec3{ v.x, v.y, v.z };
    }

    auto PhysicsWorld::ToQuat( const glm::vec3 &vec3EulerAnglesGLM ) -> JPH::Quat {
        glm::quat gq{ glm::quat{vec3EulerAnglesGLM} }; // requires GLM_FORCE_RADIANS
        return ToQuat(gq);
    }
    auto PhysicsWorld::ToQuat( const glm::quat &q ) -> JPH::Quat {
        // glm stores (w, x, y, z)
        return JPH::Quat{ q.x, q.y, q.z, q.w };
    }

}// namespace Mikoto