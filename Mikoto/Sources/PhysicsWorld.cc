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

#include <EASTL/memory.h>
#include <EASTL/atomic.h>
#include <EASTL/unique_ptr.h>

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

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Types.hh>

#include <Logging/Assert.hh>
#include <Logging/Logger.hh>

#include <Scene/Scene.hh>
#include <Scene/Component.hh>

#include <Memory/Allocator.hh>

#include <Physics/PhysicsWorld.hh>
#include <Physics/PhysicSystem.hh>

#include <Threading/ThreadUtility.hh>

namespace mikoto::physics {

    using namespace mikoto::core;
    using namespace mikoto::scene;

    PhysicsWorld::PhysicsWorld( const PhysicsWorldCreateInfo &spec )
        : mScene{ spec.mScene }, mGravity{ spec.mGravity } {}

    auto PhysicsWorld::Initialize() -> void {
        MKT_CORE_LOGGER_INFO( "Initializing PhysicsWorld..." );

        // We need a temp allocator for temporary allocations during the physics update. We're
        // pre-allocating 10 MB to avoid having to do allocations during the physics update.
        // B.t.w. 10 MB is way too much for this example but it is a typical value you can use.
        // If you don't want to pre-allocate you can also use TempAllocatorMalloc to fall back to
        // malloc / free.
        mSimulationInfo.mTempAllocator = eastl::make_unique<JPH::TempAllocatorImpl>( 10 * 1024 * 1024 );

        // This is the max amount of rigid bodies that you can add to the physics system. If you try to add more you'll get an error.
        // Note: This value is low because this is a simple test. For a real project use something in the order of 65536.
        constexpr u32 kMaxBodies{ 1024 };

        // This determines how many mutexes to allocate to protect rigid bodies from concurrent access. Set it to 0 for the default settings.
        constexpr u32 kNumBodyMutexes{ 0 };

        // This is the max amount of body pairs that can be queued at any time (the broad phase will detect overlapping
        // body pairs based on their bounding boxes and will insert them into a queue for the narrowphase). If you make this buffer
        // too small the queue will fill up and the broad phase jobs will start to do narrow phase work. This is slightly less efficient.
        // Note: This value is low because this is a simple test. For a real project use something in the order of 65536.
        constexpr u32 kMaxBodyPairs{ 1024 };

        // This is the maximum size of the contact constraint buffer. If more contacts (collisions between bodies) are detected than this
        // number then these contacts will be ignored and bodies will start interpenetrating / fall through the world.
        // Note: This value is low because this is a simple test. For a real project use something in the order of 10240.
        constexpr u32 kMaxContactConstraints{ 1024 };

        // Initialize physics system
        mSimulationInfo.mPhysicsSystem.Init(
            kMaxBodies,
            kNumBodyMutexes,
            kMaxBodyPairs,
            kMaxContactConstraints,

            // These need to be alive for as long as the physics world needs them
            mSimulationInfo.mBroadPhaseLayerInterface,
            mSimulationInfo.mObjectVsBroadPhaseLayerFilter,
            mSimulationInfo.mObjectLayerPairFilter );

        mSimulationInfo.mBodyInterface = MKT_ADDRESSOF( mSimulationInfo.mPhysicsSystem.GetBodyInterface() );

        mSimulationInfo.mPhysicsSystem.SetGravity( JPH::Vec3( mGravity.x, mGravity.y, mGravity.z ) );

#if !defined(NDEBUG)
        // A body activation listener gets notified when bodies activate and go to sleep
        // Note that this is called from a job so whatever you do here needs to be thread safe.
        // Registering one is entirely optional.
        mSimulationInfo.mPhysicsSystem.SetBodyActivationListener( MKT_ADDRESSOF( mSimulationInfo.mBodyActivationListener ) );

        // A contact listener gets notified when bodies (are about to) collide, and when they separate again.
        // Note that this is called from a job so whatever you do here needs to be thread safe.
        // Registering one is entirely optional.
        mSimulationInfo.mPhysicsSystem.SetContactListener( MKT_ADDRESSOF( mSimulationInfo.mContactListener ) );
#endif

        // Optional step: Before starting the physics simulation you can optimize the broad phase. This improves collision detection performance (it's pointless here because we only have 2 bodies).
        // You should definitely not call this every frame or when e.g. streaming in a new level section as it is an expensive operation.
        // Instead, insert all new objects in batches instead of 1 at a time to keep the broad phase efficient.
        mSimulationInfo.mPhysicsSystem.OptimizeBroadPhase();

        mIsInitialized = true;
    }

    auto PhysicsWorld::Shutdown() -> void {
        if ( !mIsInitialized ) {
            return;
        }

        MKT_CORE_LOGGER_INFO( "Shutting down PhysicsWorld..." );

        for ( auto &val: mBodies | std::views::values ) {
            mSimulationInfo.mBodyInterface->RemoveBody(val->GetID());
            mSimulationInfo.mBodyInterface->DestroyBody(val->GetID());
        }

        mSimulationInfo.mTempAllocator.reset();


        mIsInitialized = false;
    }

    auto PhysicsWorld::Update( float dt ) -> void {
        MKT_ASSERT( mSimulationInfo.mBodyInterface, "BodyInterface is NULL forgot to call PhysicsWorld::Init()" );
        MKT_ASSERT( mSimulationInfo.mTempAllocator, "TempAllocator is NULL forgot to call PhysicsWorld::Init()" );

        PreUpdate();

        constexpr i32 collisionSteps{ 1 };
        mSimulationInfo.mPhysicsSystem.Update( dt, collisionSteps, mSimulationInfo.mTempAllocator.get(), PhysicSystem::Get()->GetJoltJobSystem() );

        PostUpdate();
    }

    auto PhysicsWorld::PreUpdate() -> void {
        // Here we line up ECS and Jolt
        auto &registry{ mScene->GetRegistry() };
        for ( const auto& [entity, rb, tr]: registry.view<RigidBodyComponent, TransformComponent>().each() ) {
            if ( !rb.IsValidBodyID() ) {
                continue;
            }

            JPH::Body* body{ GetJoltBody( rb.GetBodyID() ) };
            UpdateBodyProperties( body, tr, rb );

            // Jolt puts bodies to sleep to save resources
            if (!rb.IsBodyType( RigidBodyComponent::BodyType::eStatic ) && !mSimulationInfo.mBodyInterface->IsActive( body->GetID() ) ) {
                mSimulationInfo.mBodyInterface->ActivateBody( body->GetID() );
            }
        }
    }

    auto PhysicsWorld::UpdateBodyProperties(JPH::Body* body, TransformComponent& tr, RigidBodyComponent& rb ) const -> void {
        // should only be called by the BodyInterface since it also requires updating the broadphase
        mSimulationInfo.mBodyInterface->SetPositionAndRotation(
            body->GetID(), GetFloat3F( tr.GetTranslation() ),
            GetQuatF( tr.GetRotation() ), JPH::EActivation::Activate );

        const auto motionType{ GetJoltMotionType( rb.GetBodyType( ) ) };

        // Line up motion types if it has been updated
        if (motionType != body->GetMotionType()) {
            if (motionType != JPH::EMotionType::Static || !body->IsActive()) {
                mSimulationInfo.mBodyInterface->DeactivateBody( body->GetID() );
            }

            body->SetMotionType( motionType );
        }

        if ( rb.IsBodyType( RigidBodyComponent::BodyType::eKinematic ) ) {
            body->SetAngularVelocity( GetFloat3F( rb.GetAngularVelocity() ) );
            body->SetLinearVelocity( GetFloat3F( rb.GetLinearVelocity() ) );
        }

        // Mass and friction
        if (rb.IsBodyType(RigidBodyComponent::BodyType::eDynamic) && (1.0f / rb.GetMass()) != body->GetMotionProperties()->GetInverseMass()) {
            JPH::MassProperties	massPropertiesOverride{};
            massPropertiesOverride.mMass = rb.GetMass(); // Jolt uses kg, see docs
            body->GetMotionProperties()->SetMassProperties( body->GetMotionProperties()->GetAllowedDOFs(), massPropertiesOverride );
        }

        // Degrees of freedom dynamic objects (which axis the object is allowed to rotate, translate


        // Restitution
        body->SetRestitution( rb.GetRestitution() );
    }

    auto PhysicsWorld::PostUpdate() -> void {
        auto& registry{ mScene->GetRegistry() };

        const auto& lockInterface{ mSimulationInfo.mPhysicsSystem.GetBodyLockInterface() };
        for ( auto [entity, rb, tr]: registry.view<RigidBodyComponent, TransformComponent>().each() ) {
            if ( !rb.IsValidBodyID() ) {
                continue;
            }

            const JPH::Body* body{ GetJoltBody( rb.GetBodyID() ) };

            const JPH::RMat44 transform{ body->GetCenterOfMassTransform() };

            const JPH::Vec3 position{ transform.GetTranslation() };
            const JPH::Quat rotation{ transform.GetRotation().GetQuaternion() };

            tr.SetTranslation( GetFloat3F( position ) );
            tr.SetRotation( GetQuatF( rotation ) );
        }
    }

    auto PhysicsWorld::GenerateBodyID() -> u64 {
        return mBodyIdCounter.fetch_add(1, std::memory_order_relaxed);
    }

    auto PhysicsWorld::GetJoltBody( const u64 id ) -> JPH::Body * {
        const auto it{ mBodies.find(id) };
        return it == mBodies.end() ? nullptr : it->second;
    }

    auto PhysicsWorld::RemoveRigidBody( Entity* entity ) -> void {
        if (!entity) {
            return;
        }

        RigidBodyComponent& rb{ entity->GetComponent<RigidBodyComponent>() };

        const JPH::Body* body{ GetJoltBody( rb.GetBodyID() ) };
        mSimulationInfo.mBodyInterface->RemoveBody( body->GetID() );
        mSimulationInfo.mBodyInterface->DestroyBody( body->GetID() );

        mBodies.erase( rb.GetBodyID() );

        rb.RemoveBodyID();
    }

    auto PhysicsWorld::AddRigidBody( Entity* entity ) -> void {
        if (!entity) {
            return;
        }

        TransformComponent& tc{ entity->GetComponent<TransformComponent>() };
        RigidBodyComponent& rb{ entity->GetComponent<RigidBodyComponent>() };

        // The size expect the half extent, dimensions defined from the center outwards
        float3 halfExtent{ tc.GetScale() / 2.0f };

        JPH::Vec3 size{ GetFloat3F( halfExtent ) };
        JPH::Vec3 position{ tc.GetTranslation().x, tc.GetTranslation().y, tc.GetTranslation().z };

        // Simple shape for now (box)
        const JPH::BoxShapeSettings shapeSettings{ size };
        auto shape{ shapeSettings.Create().Get() };

        JPH::BodyCreationSettings settings{
            shape,
            position,
            JPH::Quat::sIdentity(),
            GetJoltMotionType( rb.GetBodyType() ),
            Layers::MOVING
        };

        settings.mFriction = rb.GetFriction();
        settings.mMassPropertiesOverride.mMass = rb.GetMass();
        settings.mRestitution = rb.GetRestitution();

        // When this body is created as static, this setting tells the system to create
        // a MotionProperties object so that the object can be switched to kinematic or dynamic
        settings.mAllowDynamicOrKinematic = true;

        // Motion quality, or how well it detects collisions when it has a high velocity
        settings.mMotionQuality = JPH::EMotionQuality::LinearCast;

        JPH::Body *body{ mSimulationInfo.mBodyInterface->CreateBody( settings ) };
        mSimulationInfo.mBodyInterface->AddBody( body->GetID(), JPH::EActivation::Activate );

        const auto [it, success] {
            mBodies.try_emplace( GenerateBodyID(), body ) };
        if ( success ) {
            rb.SetBodyID( it->first );
        }
    }

    auto PhysicsWorld::AddCollider( Entity *entity ) -> void {
        if (!entity) {
            return;
        }

        ColliderComponent& colliderComponent{ entity->GetComponent<ColliderComponent>() };
        physics::ColliderType colliderType{ colliderComponent.GetColliderType() };
    }

    auto PhysicsWorld::RemoveColliderBody( Entity *entity ) -> void {
        if (!entity) {
            return;
        }
    }

    auto PhysicsWorld::SetGravity( const float3 &gravity ) -> void {
        mGravity = gravity;
        mSimulationInfo.mPhysicsSystem.SetGravity( JPH::Vec3( mGravity.x, mGravity.y, mGravity.z ) );
    }

    auto PhysicsWorld::SetGravityBody( GravityBody body ) -> void {
        mGravityBody = body;
        SetGravity( GetGravityFor( body ) );
    }

    auto PhysicsWorld::GetGravityBody() const -> GravityBody {
        return mGravityBody;
    }

    auto PhysicsWorld::GetGravityFor( const GravityBody body) -> float3 {
        // https://en.wikipedia.org/wiki/List_of_gravitationally_rounded_objects_of_the_Solar_System
        switch (body) {
            case GravityBody::eEarth:   return { 0.0f, -9.80665f, 0.0f };
            case GravityBody::eMoon:    return { 0.0f, -1.62f,    0.0f };
            case GravityBody::eMars:    return { 0.0f, -3.721f,   0.0f };
            case GravityBody::eJupiter: return { 0.0f, -24.79f,   0.0f };

            default: return { 0.0f, -9.80665f, 0.0f }; // fallback: Earth
        }
    }

    auto PhysicsWorld::Create( const PhysicsWorldCreateInfo &spec ) -> eastl::unique_ptr<PhysicsWorld> {
        return eastl::make_unique<PhysicsWorld>( spec );
    }

    auto PhysicsWorld::GetFloat4x4F( const JPH::RMat44 &m ) -> float4x4 {
        float4x4 out{};

        for (i32 c{}; c < 4; ++c) {
            // RMat44 stores row-major internally,
            // Jolt provides columns.
            JPH::Vec4 col{ m.GetColumn4(c) };
            out[c][0] = col.GetX();
            out[c][1] = col.GetY();
            out[c][2] = col.GetZ();
            out[c][3] = col.GetW();
        }

        return out;
    }

    auto PhysicsWorld::GetFloat3F( const JPH::Vec3 &v ) -> float3 {
        return float3{ v.GetX(), v.GetY(), v.GetZ() };
    }

    auto PhysicsWorld::GetQuatF( const JPH::Quat &q ) -> quat {
        return quat{ q.GetW(), q.GetX(), q.GetY(), q.GetZ() };
    }

    auto PhysicsWorld::GetFloat3F( const float3 &v ) -> JPH::Vec3 {
        return JPH::Vec3{ v.x, v.y, v.z };
    }

    auto PhysicsWorld::GetQuatF( const float3 &vec3EulerAnglesGLM ) -> JPH::Quat {
        quat gq{ quat{vec3EulerAnglesGLM} }; // requires GLM_FORCE_RADIANS
        return GetQuatF(gq);
    }

    auto PhysicsWorld::GetQuatF( const quat &q ) -> JPH::Quat {
        // glm stores (w, x, y, z)
        return JPH::Quat{ q.x, q.y, q.z, q.w };
    }

}// namespace Mikoto