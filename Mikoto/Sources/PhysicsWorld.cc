//
// Created by kate on 10/22/25.
//

#include <entt/entt.hpp>
#include <memory>

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
        MKT_CORE_LOGGER_INFO( "Initializing PhysicsBase..." );

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

        m_Impl = CreateScope<Impl>();

        // We need a temp allocator for temporary allocations during the physics update. We're
        // pre-allocating 10 MB to avoid having to do allocations during the physics update.
        // B.t.w. 10 MB is way too much for this example but it is a typical value you can use.
        // If you don't want to pre-allocate you can also use TempAllocatorMalloc to fall back to
        // malloc / free.
        m_Impl->TempAllocator = CreateScope<JPH::TempAllocatorImpl>( 10 * 1024 * 1024 );

        // We need a job system that will execute physics jobs on multiple threads. Typically
        // you would implement the JobSystem interface yourself and let Jolt Physics run on top
        // of your own job scheduler. JobSystemThreadPool is an example implementation.
        m_Impl->JobSystem = CreateScope<JPH::JobSystemThreadPool>(
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
        m_Impl->PhysicsSystem.Init(
                cMaxBodies,
                cNumBodyMutexes,
                cMaxBodyPairs,
                cMaxContactConstraints,

                // These need to be alive for as long as the physics system needs it
                m_Impl->BroadPhaseLayerInterface,
                m_Impl->ObjectVsBroadPhaseLayerFilter,
                m_Impl->ObjectLayerPairFilter );

        m_Impl->BodyInterface = std::addressof( m_Impl->PhysicsSystem.GetBodyInterface() );

        m_Impl->PhysicsSystem.SetGravity( JPH::Vec3( m_Gravity.x, m_Gravity.y, m_Gravity.z ) );

#if false
        // A body activation listener gets notified when bodies activate and go to sleep
        // Note that this is called from a job so whatever you do here needs to be thread safe.
        // Registering one is entirely optional.
        m_Impl->PhysicsSystem.SetBodyActivationListener( std::addressof( m_Impl->BodyActivationListener ) );

        // A contact listener gets notified when bodies (are about to) collide, and when they separate again.
        // Note that this is called from a job so whatever you do here needs to be thread safe.
        // Registering one is entirely optional.
        m_Impl->PhysicsSystem.SetContactListener( std::addressof( m_Impl->ContactListener ) );
#endif

        m_IsInitialized = true;
    }

    auto PhysicsWorld::Shutdown() -> void {
        if ( !m_IsInitialized ) {
            return;
        }

        // The Log comes after so we know the service was
        // initialized before attempting to shut it down
        MKT_CORE_LOGGER_INFO( "Shutting down PhysicsBase..." );

        m_Impl.reset();
        JPH::UnregisterTypes();

        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;

        m_IsInitialized = false;
    }

    auto PhysicsWorld::Update( float dt ) -> void {
        MKT_ASSERT( m_Impl, "m_Implementation is NULL forgot to call PhysicsWorld::Init()" );
        MKT_ASSERT( m_Impl->BodyInterface, "BodyInterface is NULL forgot to call PhysicsWorld::Init()" );
        MKT_ASSERT( m_Impl->JobSystem, "JobSystem is NULL forgot to call PhysicsWorld::Init()" );
        MKT_ASSERT( m_Impl->TempAllocator, "TempAllocator is NULL forgot to call PhysicsWorld::Init()" );

        PreUpdate();

        constexpr int collisionSteps{ 1 };
        m_Impl->PhysicsSystem.Update( dt, collisionSteps, m_Impl->TempAllocator.get(), m_Impl->JobSystem.get() );

        PostUpdate();
    }

    auto PhysicsWorld::SetSimulationScene( Scene *scene ) -> void {
        if ( scene ) {
            // This needs to be handled properly
            m_Scene = scene;
        }
    }

    auto PhysicsWorld::PreUpdate() -> void {
        // Here we line up ECS and Jolt
        auto &registry{ m_Scene->GetRegistry() };

        for ( auto [entity, rb, tr]: registry.view<RigidBodyComponent, TransformComponent>().each() ) {
            if ( !rb.IsValidBodyID() ) {
                continue;
            }

            const auto body{ GetJoltBody( rb.GetBodyID() ) };

            // Update body type as of ECS
            m_Impl->BodyInterface->SetMotionType( body->GetID(), ConvertToJoltMotionType( rb.GetBodyType() ), JPH::EActivation::Activate );

            // Dynamic bodies not affected by user code
            if ( rb.IsDynamic() ) {
                continue;
            }

            m_Impl->BodyInterface->SetPositionAndRotation( body->GetID(), ToVec3( tr.GetTranslation() ), ToQuat( tr.GetRotation() ), JPH::EActivation::Activate );

            m_Impl->BodyInterface->SetAngularVelocity( body->GetID(), ToVec3( rb.GetAngularVelocity() ) );
            m_Impl->BodyInterface->SetLinearVelocity( body->GetID(), ToVec3( rb.GetLinearVelocity() ) );

            if (m_Impl->BodyInterface->IsActive( body->GetID() ) ) {
                // Jolt puts bodies to sleep to save resources
                m_Impl->BodyInterface->ActivateBody( body->GetID() );
            }
        }
    }

    auto PhysicsWorld::PostUpdate() -> void {
        auto &registry{ m_Scene->GetRegistry() };

        for ( auto [entity, rb, tr]: registry.view<RigidBodyComponent, TransformComponent>().each() ) {
            if ( !rb.IsValidBodyID() || !rb.IsDynamic() )
                continue;

            const auto body{ GetJoltBody( rb.GetBodyID() ) };
            const JPH::RMat44 transform{ m_Impl->BodyInterface->GetCenterOfMassTransform( body->GetID() ) };

            const JPH::Vec3 position{ transform.GetTranslation() };
            const JPH::Quat rotation{ transform.GetRotation().GetQuaternion() };

            tr.SetTranslation( ToVec3F( position ) );
            tr.SetRotation( ToQuatF( rotation ) );
        }
    }

    auto PhysicsWorld::GenerateBodyID() -> UInt64 {
        return m_BodyIdCounter++;
    }

    auto PhysicsWorld::GetJoltBody( UInt64 id ) -> JPH::Body * {
        return m_Bodies.at( id );
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

        if ( !m_Impl || !m_Impl->BodyInterface ) {
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
        m_Impl->BodyInterface->RemoveBody( body->GetID() );
        m_Impl->BodyInterface->DestroyBody( body->GetID() );

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

        JPH::Body *body{ m_Impl->BodyInterface->CreateBody( settings ) };
        m_Impl->BodyInterface->AddBody( body->GetID(), JPH::EActivation::Activate );

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

    auto PhysicsWorld::ToMat4F( const JPH::RMat44 &jphMat ) -> glm::mat4 {
        // JPH::RMat44 stores a 4x4 matrix in row-major RMat44::mMatrix (check your JPH version)
        // We'll read elements by row/column to construct glm::mat4 (glm is column-major by default).
        // Convert row-major -> column-major expected by glm::mat4
        glm::mat4 out{};

        // copy each column (Vec4 -> glm::vec4)
        constexpr Int32 rowCount{ 4 };
        for ( Int32 c{}; c < rowCount; ++c ) {
            JPH::Vec4 col = jphMat.GetColumn4( c );// Vec4: (x,y,z,w)
            out[c][0] = col.GetX();                // glm stores as out[column][row]
            out[c][1] = col.GetY();
            out[c][2] = col.GetZ();
            out[c][3] = col.GetW();
        }
        return out;
    }

    auto PhysicsWorld::ToVec3F( const JPH::Vec3 &jphVec3 ) -> glm::vec3 {
        return { jphVec3.GetX(), jphVec3.GetY(), jphVec3.GetZ() };
    }

    auto PhysicsWorld::ToQuatF( const JPH::Quat &jphQuat ) -> glm::quat {
        // JPH::Quat stores (x,y,z,w) typically; glm::quat constructor accepts (w, x, y, z)
        return { ( jphQuat.GetW() ),
                 ( jphQuat.GetX() ),
                 ( jphQuat.GetY() ),
                 ( jphQuat.GetZ() ) };
    }

    auto PhysicsWorld::ToVec3( const glm::vec3 &vec3GLM ) -> JPH::Vec3 {
        return { vec3GLM.x, vec3GLM.y, vec3GLM.z };
    }

    auto PhysicsWorld::ToQuat( const glm::vec3 &vec3EulerAnglesGLM ) -> JPH::Quat {
        // Convert Euler (pitch, yaw, roll) GLM's order to quaternion explicitly
        // Requires GLM_RADIANS
        glm::quat gq{ glm::quat( vec3EulerAnglesGLM ) };
        // glm::quat is (w, x, y, z) layout for access

        return { ( gq.x ),
                 ( gq.y ),
                 ( gq.z ),
                 ( gq.w ) };
    }

}// namespace Mikoto