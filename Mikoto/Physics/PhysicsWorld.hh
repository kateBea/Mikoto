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

#ifndef MIKOTO_PHYSICS_WORLD_HH
#define MIKOTO_PHYSICS_WORLD_HH

#include <EASTL/atomic.h>
#include <EASTL/string.h>
#include <EASTL/unique_ptr.h>

#include <ankerl/unordered_dense.h>

// The Jolt headers don't include Jolt.h. Always include Jolt.h before including any other Jolt header.
// You can use Jolt.h in your precompiled header to speed up compilation.
#include <Jolt/Jolt.h>

// Jolt includes
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Subsystem.hh>

#include <Scene/Component.hh>

#include <Physics/PhysicSystem.hh>

namespace mikoto::scene {
    class Scene;
}

namespace mikoto::physics {

    using namespace mikoto::scene;

    // Layer that objects can be in, determines which other objects it can collide with
    // Typically you at least want to have 1 layer for moving bodies and 1 layer for static bodies, but you can have more
    // layers if you want. E.g. you could have a layer for high detail collision (which is not used by the physics simulation
    // but only if you do collision testing).
    namespace Layers {
        static constexpr JPH::ObjectLayer NON_MOVING = 0;
        static constexpr JPH::ObjectLayer MOVING = 1;
        static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
    };// namespace Layers

    // Each broad-phase layer results in a separate bounding volume tree in the broad phase. You at least want to have
    // a layer for non-moving and moving objects to avoid having to update a tree full of static objects every frame.
    // You can have a 1-on-1 mapping between object layers and broad-phase layers (like in this case) but if you have
    // many object layers you'll be creating many broad phase trees, which is not efficient. If you want to fine tune
    // your broad-phase layers define JPH_TRACK_BROADPHASE_STATS and look at the stats reported on the TTY.
    namespace BroadPhaseLayers {
        static constexpr JPH::BroadPhaseLayer NON_MOVING( 0 );
        static constexpr JPH::BroadPhaseLayer MOVING( 1 );
        static constexpr u32 NUM_LAYERS( 2 );
    };// namespace BroadPhaseLayers

    static auto GetJoltMotionType( RigidBodyComponent::BodyType bodyType ) -> JPH::EMotionType {
        switch ( bodyType ) {
            case RigidBodyComponent::BodyType::eStatic:
                return JPH::EMotionType::Static;
            case RigidBodyComponent::BodyType::eKinematic:
                return JPH::EMotionType::Kinematic;
            default:;
        }

        return JPH::EMotionType::Dynamic;
    }

    // Callback for asserts, connect this to your own assert handler if you have one
    static bool AssertFailedImpl( const char* inExpression, const char* inMessage, const char* inFile, u32 inLine ) {
        // Print to the TTY
        MKT_CORE_LOGGER_DEBUG( "AssertFailedImpl" );

        // Breakpoint
        return true;
    };

    /// Class that determines if two object layers can collide
    class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter {
    public:
        MKT_NODISCARD auto ShouldCollide( JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2 ) const -> bool override {
            switch ( inObject1 ) {
                case Layers::NON_MOVING:
                    return inObject2 == Layers::MOVING;// Non-moving only collides with moving
                case Layers::MOVING:
                    return true;// Moving collides with everything
                default:
                    JPH_ASSERT( false );
                    return false;
            }
        }
    };

    /// Class that determines if an object layer can collide with a broadphase layer
    class ObjectVsBroadPhaseLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter {
    public:
        MKT_NODISCARD auto ShouldCollide( JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2 ) const -> bool override {
            switch ( inLayer1 ) {
                case Layers::NON_MOVING:
                    return inLayer2 == BroadPhaseLayers::MOVING;
                case Layers::MOVING:
                    return true;
                default:
                    JPH_ASSERT( false );
                    return false;
            }
        }
    };

    // BroadPhaseLayerInterface implementation
    // This defines a mapping between object and broadphase layers.
    class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
    public:
        BPLayerInterfaceImpl() {
            // Create a mapping table from object to broad phase layer
            mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
            mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
        }

        MKT_NODISCARD auto GetNumBroadPhaseLayers() const -> u32 override {
            return BroadPhaseLayers::NUM_LAYERS;
        }

        MKT_NODISCARD auto GetBroadPhaseLayer( JPH::ObjectLayer inLayer ) const -> JPH::BroadPhaseLayer override {
            JPH_ASSERT( inLayer < Layers::NUM_LAYERS );
            return mObjectToBroadPhase[inLayer];
        }

#if defined( JPH_EXTERNAL_PROFILE ) || defined( JPH_PROFILE_ENABLED )
        MKT_NODISCARD auto GetBroadPhaseLayerName( JPH::BroadPhaseLayer inLayer ) const -> const char* override {
            switch ( as<JPH::BroadPhaseLayer::Type>( inLayer ) ) {
                case as<JPH::BroadPhaseLayer::Type>( BroadPhaseLayers::NON_MOVING ):
                    return "NON_MOVING";
                case as<JPH::BroadPhaseLayer::Type>( BroadPhaseLayers::MOVING ):
                    return "MOVING";
                default:
                    JPH_ASSERT( false );
                    return "INVALID";
            }
        }
#endif// JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

    private:
        JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
    };

    // For debug
    class DebugBodyActivationListener final : public JPH::BodyActivationListener {
    public:
        auto OnBodyActivated( const JPH::BodyID &inBodyID, JPH::uint64 inBodyUserData ) -> void override {
            MKT_CORE_LOGGER_DEBUG( "A body got activated" );
        }

        auto OnBodyDeactivated( const JPH::BodyID &inBodyID, JPH::uint64 inBodyUserData ) -> void override {
            MKT_CORE_LOGGER_DEBUG( "A body got activated" );
        }
    };

    // For debug
    class DebugContactListener final : public JPH::ContactListener {
    public:
        // See: ContactListener
        auto OnContactValidate( const JPH::Body &inBody1, const JPH::Body &inBody2, JPH::RVec3Arg inBaseOffset, const JPH::CollideShapeResult &inCollisionResult ) -> JPH::ValidateResult override {
            MKT_CORE_LOGGER_DEBUG( "Contact validate callback" );

            // Allows you to ignore a contact before it is created (using layers to not make objects collide is cheaper!)
            return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
        }

        auto OnContactAdded( const JPH::Body &inBody1, const JPH::Body &inBody2, const JPH::ContactManifold &inManifold, JPH::ContactSettings &ioSettings ) -> void  override {
            MKT_CORE_LOGGER_DEBUG( "A contact was added" );
        }

        auto OnContactPersisted( const JPH::Body &inBody1, const JPH::Body &inBody2, const JPH::ContactManifold &inManifold, JPH::ContactSettings &ioSettings ) -> void  override {
            MKT_CORE_LOGGER_DEBUG( "A contact was persisted" );
        }

        auto OnContactRemoved( const JPH::SubShapeIDPair &inSubShapePair ) -> void override {
            MKT_CORE_LOGGER_DEBUG( "A contact was removed" );
        }
    };

    // Every scene has its own physics  simulation world
    class PhysicsWorld final : public core::ISubsystem {
    public:
        explicit PhysicsWorld( const PhysicsWorldCreateInfo& scene );

        ~PhysicsWorld() override = default;

        auto Initialize() -> void override;
        auto Shutdown() -> void override;
        auto Update( float timeStep ) -> void override;

        auto AddRigidBody( Entity* entity ) -> void;
        auto AddCollider( Entity* entity ) -> void; // Can have collider and no rigid body

        auto RemoveRigidBody( Entity* entity ) -> void;
        auto RemoveColliderBody( Entity* entity ) -> void;

        auto SetGravity( const float3& gravity ) -> void;
        auto SetGravityBody( GravityBody body) -> void;

        MKT_NODISCARD auto GetGravityBody() const -> GravityBody;

        MKT_NODISCARD static auto GetGravityFor(GravityBody body) -> float3;
        MKT_NODISCARD static auto Create( const PhysicsWorldCreateInfo& spec ) -> eastl::unique_ptr<PhysicsWorld>;

    private:
        struct SimulationInfo {
            JPH::PhysicsSystem mPhysicsSystem{};
            JPH::BodyInterface* mBodyInterface{ nullptr };

            eastl::unique_ptr<JPH::TempAllocatorImpl> mTempAllocator{};

            // Create mapping table from object layer to broadphase layer
            // Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
            // Also have a look at BroadPhaseLayerInterfaceTable or BroadPhaseLayerInterfaceMask for a simpler interface.
            BPLayerInterfaceImpl mBroadPhaseLayerInterface{};

            // Create class that filters object vs broadphase layers
            // Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
            // Also have a look at ObjectVsBroadPhaseLayerFilterTable or ObjectVsBroadPhaseLayerFilterMask for a simpler interface.
            ObjectVsBroadPhaseLayerFilterImpl mObjectVsBroadPhaseLayerFilter{};

            // Create class that filters object vs object layers
            // Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
            // Also have a look at ObjectLayerPairFilterTable or ObjectLayerPairFilterMask for a simpler interface.
            ObjectLayerPairFilterImpl mObjectLayerPairFilter{};

            // FOR DEBUG
            DebugContactListener mContactListener{};
            DebugBodyActivationListener mBodyActivationListener{};
        };


    private:
        // [Internal]
        auto PreUpdate() -> void;
        auto PostUpdate() -> void;

        auto GenerateBodyID() -> u64;

        auto GetJoltBody(u64 id) -> JPH::Body*;

        auto UpdateBodyProperties(JPH::BodyID id, TransformComponent& tr, RigidBodyComponent& rbComponent ) const -> void;

        MKT_NODISCARD static auto GetFloat4x4F( const JPH::RMat44& jphMat ) -> float4x4;
        MKT_NODISCARD static auto GetFloat3F( const JPH::Vec3& jphVec3 ) -> float3;
        MKT_NODISCARD static auto GetQuatF( const JPH::Quat& jphQuat ) -> quat;

        MKT_NODISCARD static auto GetFloat3F( const float3& vec3GLM ) -> JPH::Vec3;
        MKT_NODISCARD static auto GetQuatF( const float3& vec3EulerAnglesGLM ) -> JPH::Quat;
        MKT_NODISCARD static auto GetQuatF( const quat &q ) -> JPH::Quat;

    private:

        Scene* mScene{};
        SimulationInfo mSimulationInfo{};

        GravityBody mGravityBody{ GravityBody::eEarth };
        float3 mGravity{ GetGravityFor( GravityBody::eEarth ) };

        std::atomic_uint64_t mBodyIdCounter{ 0 };
        ankerl::unordered_dense::map<u64, JPH::Body*> mBodies{};
    };
}

#endif // MIKOTO_PHYSICS_WORLD_HH
