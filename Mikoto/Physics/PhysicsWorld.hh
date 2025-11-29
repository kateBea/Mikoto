//
// Created by kate on 10/22/25.
//

#ifndef MIKOTO_PHYSICS_BASE_HH
#define MIKOTO_PHYSICS_BASE_HH

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
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <Scene/Component.hh>

namespace Mikoto {

    class Scene;
    class Entity;

    // Layer that objects can be in, determines which other objects it can collide with
    // Typically you at least want to have 1 layer for moving bodies and 1 layer for static bodies, but you can have more
    // layers if you want. E.g. you could have a layer for high detail collision (which is not used by the physics simulation
    // but only if you do collision testing).
    namespace Layers {
        static constexpr JPH::ObjectLayer NON_MOVING = 0;
        static constexpr JPH::ObjectLayer MOVING = 1;
        static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
    };// namespace Layers

    // Each broadphase layer results in a separate bounding volume tree in the broad phase. You at least want to have
    // a layer for non-moving and moving objects to avoid having to update a tree full of static objects every frame.
    // You can have a 1-on-1 mapping between object layers and broadphase layers (like in this case) but if you have
    // many object layers you'll be creating many broad phase trees, which is not efficient. If you want to fine tune
    // your broadphase layers define JPH_TRACK_BROADPHASE_STATS and look at the stats reported on the TTY.
    namespace BroadPhaseLayers {
        static constexpr JPH::BroadPhaseLayer NON_MOVING( 0 );
        static constexpr JPH::BroadPhaseLayer MOVING( 1 );
        static constexpr UInt32 NUM_LAYERS( 2 );
    };// namespace BroadPhaseLayers

    struct PhysicsWorldCreateInfo {
        Scene* TargetScene{ nullptr };
        Vec3F Gravity{ 0.0f, -9.81f, 0.0f };
    };

    // Callback for traces, connect this to your own trace function if you have one
    static auto TraceImpl( const char* inFMT, ... ) -> void {
        // Format the message
        va_list list;
        va_start( list, inFMT );
        char buffer[1024];
        vsnprintf( buffer, sizeof( buffer ), inFMT, list );
        va_end( list );

        MKT_CORE_LOGGER_DEBUG( "Jolt Trace {}", std::string( buffer ) );
    }

    static auto ConvertToJoltMotionType( RigidBodyComponent::BodyType bodyType ) -> JPH::EMotionType {
        switch ( bodyType ) {
            case RigidBodyComponent::BodyType::STATIC:
                return JPH::EMotionType::Static;
            case RigidBodyComponent::BodyType::KINEMATIC:
                return JPH::EMotionType::Kinematic;
            default:;
        }

        return JPH::EMotionType::Dynamic;
    }

    // Callback for asserts, connect this to your own assert handler if you have one
    static bool AssertFailedImpl( const char* inExpression, const char* inMessage, const char* inFile, UInt32 inLine ) {
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
                    return inObject2 == Layers::MOVING;// Non moving only collides with moving
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

        MKT_NODISCARD auto GetNumBroadPhaseLayers() const -> UInt32 override { return BroadPhaseLayers::NUM_LAYERS; }

        MKT_NODISCARD auto GetBroadPhaseLayer( JPH::ObjectLayer inLayer ) const -> JPH::BroadPhaseLayer override {
            JPH_ASSERT( inLayer < Layers::NUM_LAYERS );
            return mObjectToBroadPhase[inLayer];
        }

#if defined( JPH_EXTERNAL_PROFILE ) || defined( JPH_PROFILE_ENABLED )
        MKT_NODISCARD auto GetBroadPhaseLayerName( JPH::BroadPhaseLayer inLayer ) const -> const char* override {
            switch ( static_cast<JPH::BroadPhaseLayer::Type>( inLayer ) ) {
                case static_cast<JPH::BroadPhaseLayer::Type>( BroadPhaseLayers::NON_MOVING ):
                    return "NON_MOVING";
                case static_cast<JPH::BroadPhaseLayer::Type>( BroadPhaseLayers::MOVING ):
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

    enum class GravityBody {
        EARTH,
        MOON,
        MARS,
        JUPITER,
    };

    // Controls physics API specifics
    class PhysicsWorld final : public IService, public Singleton<PhysicsWorld> {
    public:
        explicit PhysicsWorld( const PhysicsWorldCreateInfo& scene );

        ~PhysicsWorld() override = default;

        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto Update( float timeStep ) -> void override;

        auto SetSimulationScene( Scene* scene ) -> void;

        auto OnRigidBodyRemoved( Entity& entity ) -> void;
        auto OnRigidBodyAdded( Entity& entity) -> void;

        auto OnRigidBodyRemoved( RigidBodyComponent& rigidBody ) -> void;
        auto OnRigidBodyAdded( TransformComponent& transformComponent, RigidBodyComponent& rigidBodyComponent ) -> void;

        MKT_NODISCARD static auto GetGravityFor(GravityBody body) -> Vec3F;
        MKT_NODISCARD static auto Create( const PhysicsWorldCreateInfo& spec ) -> Unique<PhysicsWorld>;

    private:
        struct Impl {
            JPH::PhysicsSystem PhysicsSystem{};
            JPH::BodyInterface* BodyInterface{ nullptr };

            Unique<JPH::TempAllocatorImpl> TempAllocator{};
            Unique<JPH::JobSystemThreadPool> JobSystem{};

            // Create mapping table from object layer to broadphase layer
            // Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
            // Also have a look at BroadPhaseLayerInterfaceTable or BroadPhaseLayerInterfaceMask for a simpler interface.
            BPLayerInterfaceImpl BroadPhaseLayerInterface{};

            // Create class that filters object vs broadphase layers
            // Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
            // Also have a look at ObjectVsBroadPhaseLayerFilterTable or ObjectVsBroadPhaseLayerFilterMask for a simpler interface.
            ObjectVsBroadPhaseLayerFilterImpl ObjectVsBroadPhaseLayerFilter{};

            // Create class that filters object vs object layers
            // Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
            // Also have a look at ObjectLayerPairFilterTable or ObjectLayerPairFilterMask for a simpler interface.
            ObjectLayerPairFilterImpl ObjectLayerPairFilter{};

            // FOR DEBUG
            DebugContactListener ContactListener{};
            DebugBodyActivationListener BodyActivationListener{};
        };


    private:
        auto PreUpdate() -> void;
        auto PostUpdate() -> void;

        auto GenerateBodyID() -> UInt64;

        auto GetJoltBody(UInt64 id) -> JPH::Body*;

        MKT_NODISCARD static auto ToMat4F( const JPH::RMat44& jphMat ) -> glm::mat4;
        MKT_NODISCARD static auto ToVec3F( const JPH::Vec3& jphVec3 ) -> glm::vec3;
        MKT_NODISCARD static auto ToQuatF( const JPH::Quat& jphQuat ) -> glm::quat;

        MKT_NODISCARD static auto ToVec3( const glm::vec3& vec3GLM ) -> JPH::Vec3;
        MKT_NODISCARD static auto ToQuat( const glm::vec3& vec3EulerAnglesGLM ) -> JPH::Quat;

    private:
        inline static Unique<Impl> m_Impl{};

        Scene* m_Scene{ nullptr };
        Vec3F m_Gravity{ GetGravityFor( GravityBody::EARTH ) };

        UInt64 m_BodyIdCounter{ 0 };
        ankerl::unordered_dense::map<UInt64, JPH::Body*> m_Bodies{};
    };
}// namespace Mikoto

#endif
