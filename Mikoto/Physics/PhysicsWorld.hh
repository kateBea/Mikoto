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

#ifndef MIKOTO_PHYSICS_PHYSICS_WORLD_HH
#define MIKOTO_PHYSICS_PHYSICS_WORLD_HH

#include <set>
#include <mutex>

#include <EASTL/vector.h>
#include <EASTL/atomic.h>
#include <EASTL/string.h>
#include <EASTL/unique_ptr.h>

#include <ankerl/unordered_dense.h>

#include <entt/entity/entity.hpp>

#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Skeleton/SkeletonPose.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/Character/Character.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Vehicle/VehicleConstraint.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Constraints/SixDOFConstraint.h>
#include <Jolt/Physics/SoftBody/SoftBodySharedSettings.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Service.hh>
#include <Core/Profiler.hh>

#include <Physics/Shape.hh>

#include <Scene/Component.hh>

// The Jolt headers don't include Jolt.h. Always include Jolt.h before including any other Jolt header.
// You can use Jolt.h in your precompiled header to speed up compilation.

namespace JPH {
    class DebugRenderer;
}

namespace mikoto::scene {
    class Scene;
}

namespace mikoto::physics {

    class RayCast;

    struct SoftBodyHit {
        entt::entity mEntity{ entt::null };
        core::u32 mParticle{};
        core::float3 mPosition{};
    };

    struct SoftBodyParticle {
        core::float3 mPosition{};
        bool mPinned{};
    };

    struct PhysicsDebugDrawSettings {
        bool mEnabled{ false };
        bool mSelectedOnly{ false };
        core::u64 mSelectedEntity{};
        bool mShapes{ true };
        bool mBoundingBoxes{ false };
        bool mBodyAxes{ false };
        bool mCenterOfMass{ false };
        bool mVelocities{ false };
        bool mConstraints{ false };
        bool mConstraintLimits{ false };
        bool mConstraintFrames{ false };
        bool mShowThroughGeometry{ false };
        float mLineWidth{ 1.0f };
    };

    struct ContactEvent {
        enum class Type {
            eEnter,
            eExit
        };

        Type mType{ Type::eEnter };
        JPH::uint64 mEntityA{};
        JPH::uint64 mEntityB{}; // Scene GUIDs, not reusable Jolt body indices.
        bool mIsTrigger{};
    };

    // Jolt invokes listeners on worker threads. Consumers drain events on the main thread.
    // Events represent subshape contacts: a triangle mesh may emit several for one entity pair.
    class ContactEventListener final : public JPH::ContactListener {
    public:
        auto OnContactValidate( const JPH::Body& a, const JPH::Body& b, JPH::RVec3Arg, const JPH::CollideShapeResult& ) -> JPH::ValidateResult override;
        auto SetDisabledPairs( std::set<std::pair<core::u64, core::u64>> pairs ) -> bool;
        auto OnContactRemoved( const JPH::SubShapeIDPair& pair ) -> void override;
        auto OnContactAdded( const JPH::Body& a, const JPH::Body& b, const JPH::ContactManifold& contact, JPH::ContactSettings& settings ) -> void override;

        auto TakeEvents() -> eastl::vector<ContactEvent>;

        auto GetDroppedEventCount() const -> core::usize;

        auto Clear() -> void;

    private:
        auto Push( const ContactEvent& event ) -> void;

    private:
        mutable std::mutex mMutex{};

        core::usize mDropped{};

        eastl::vector<ContactEvent> mEvents{};

        ankerl::unordered_dense::map<JPH::SubShapeIDPair, ContactEvent> mContacts{};
        std::set<std::pair<core::u64, core::u64>> mDisabledPairs{};
    };
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
        static constexpr core::u32 NUM_LAYERS( 2 );
    };// namespace BroadPhaseLayers

    /// Class that determines if two object layers can collide
    class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter {
    public:
        MKT_NODISCARD auto ShouldCollide( JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2 ) const -> bool override;
    };

    /// Class that determines if an object layer can collide with a broadphase layer
    class ObjectVsBroadPhaseLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter {
    public:
        MKT_NODISCARD auto ShouldCollide( JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2 ) const -> bool override;
    };

    // BroadPhaseLayerInterface implementation
    // This defines a mapping between object and broadphase layers.
    class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
    public:
        BPLayerInterfaceImpl();

        MKT_NODISCARD auto GetNumBroadPhaseLayers() const -> core::u32 override;

        MKT_NODISCARD auto GetBroadPhaseLayer( JPH::ObjectLayer inLayer ) const -> JPH::BroadPhaseLayer override;

#if defined( JPH_EXTERNAL_PROFILE ) || defined( JPH_PROFILE_ENABLED )
        MKT_NODISCARD auto GetBroadPhaseLayerName( JPH::BroadPhaseLayer inLayer ) const -> const char* override {
            MKT_BEGIN_PROFILER();
            switch ( core::as<JPH::BroadPhaseLayer::Type>( inLayer ) ) {
                case core::as<JPH::BroadPhaseLayer::Type>( BroadPhaseLayers::NON_MOVING ):
                    return "NON_MOVING";
                case core::as<JPH::BroadPhaseLayer::Type>( BroadPhaseLayers::MOVING ):
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
        auto OnBodyActivated( const JPH::BodyID &inBodyID, JPH::uint64 inBodyUserData ) -> void override;

        auto OnBodyDeactivated( const JPH::BodyID &inBodyID, JPH::uint64 inBodyUserData ) -> void override;
    };

    // For debug
    class DebugContactListener final : public JPH::ContactListener {
    public:
        // See: ContactListener
        auto OnContactValidate( const JPH::Body &inBody1, const JPH::Body &inBody2, JPH::RVec3Arg inBaseOffset, const JPH::CollideShapeResult &inCollisionResult ) -> JPH::ValidateResult override;

        auto OnContactAdded( const JPH::Body &inBody1, const JPH::Body &inBody2, const JPH::ContactManifold &inManifold, JPH::ContactSettings &ioSettings ) -> void  override;

        auto OnContactPersisted( const JPH::Body &inBody1, const JPH::Body &inBody2, const JPH::ContactManifold &inManifold, JPH::ContactSettings &ioSettings ) -> void  override;

        auto OnContactRemoved( const JPH::SubShapeIDPair &inSubShapePair ) -> void override;
    };

    struct PhysicsWorldCreateInfo {
        scene::Scene* mScene{ nullptr };
        core::float3 mGravity{ 0.0f, -9.81f, 0.0f };
    };

    // Every scene has its own physics  simulation world
    class PhysicsWorld final : public core::ISubsystem {
    public:
        explicit PhysicsWorld( const PhysicsWorldCreateInfo& scene );

        ~PhysicsWorld() override;

        auto Initialize() -> void override;
        auto Shutdown() -> void override;
        auto Update( float timeStep ) -> void override;

        auto DrawPhysics( JPH::DebugRenderer* renderer, const PhysicsDebugDrawSettings& settings ) -> void;

        auto AddRigidBody( scene::Entity* entity ) -> void;
        auto AddCollider( scene::Entity* entity ) -> void;

        auto RemoveRigidBody( scene::Entity* entity ) -> void;
        auto RemoveColliderBody( scene::Entity* entity ) -> void;

        // Call on the main thread after ECS edits, including while the scene is idle.
        auto Synchronize() -> void;
        auto ClearBodies() -> void;
        auto RemoveEntityBody( entt::entity entity ) -> void;

        auto GetBodyCount() const -> core::usize;
        auto GetScene() const -> scene::Scene*;
        auto GetDroppedContactEventCount() const -> core::usize;
        auto TakeContactEvents() -> eastl::vector<ContactEvent>;
        auto GetColliderError( entt::entity entity ) const -> const char*;

        // Query the current simulation surface and read its welded particles on the main thread.
        MKT_NODISCARD auto PickSoftBody( const RayCast& ray ) const -> SoftBodyHit;
        auto GetSoftBodyParticles( entt::entity entity, eastl::vector<SoftBodyParticle>& particles ) const -> void;

        // Drive one live particle until release. Targets are in world space.
        MKT_NODISCARD auto BeginSoftBodyGrab( const SoftBodyHit& hit ) -> bool;
        auto SetSoftBodyGrabTarget( const core::float3& target ) -> void;
        auto EndSoftBodyGrab() -> void;
        MKT_NODISCARD auto IsSoftBodyGrabActive() const -> bool;

        auto SetGravity( const core::float3& gravity ) -> void;

        auto SetGravityBody( GravityBody body) -> void;

        MKT_NODISCARD auto GetGravity() const -> const core::float3&;

        MKT_NODISCARD auto GetGravityBody() const -> GravityBody;

        MKT_NODISCARD static auto GetGravityFor(GravityBody body) -> core::float3;
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
            ContactEventListener mContactListener{};
            DebugBodyActivationListener mBodyActivationListener{};
        };

    private:
        // [Internal]
        auto ReportUpdateErrors( JPH::EPhysicsUpdateError errors ) -> void;
        auto PostUpdate() -> void;

        auto GenerateBodyID() -> core::u64;

        auto GetJoltBody(core::u64 id) -> JPH::Body*;

        // Capture authored settings, reconcile shapes, then synchronize live body properties.
        auto ReadColliderDescription( entt::entity entity, bool& trigger, eastl::vector<entt::entity>* ancestors = nullptr ) -> ColliderShapeDescription;
        auto ReadHeightField( entt::entity entity, ColliderShapeDescription& description ) -> void;
        auto ReadCompound( entt::entity entity, ColliderShapeDescription& description, eastl::vector<entt::entity>& ancestors ) -> void;
        std::set<entt::entity> mCompoundMembers{};
        auto DisableBody( entt::entity entity, const char* message ) -> void;
        auto SynchronizeEntity( entt::entity entity ) -> void;
        auto SynchronizeBodyProperties( entt::entity entity, const core::float3& position,
                                        const core::quat& rotation, bool trigger, bool shapeChanged ) -> void;
        auto SynchronizeVehicles() -> void;
        auto RemoveVehicle( entt::entity entity ) -> void;
        auto UpdateVehicleWheels() -> void;
        auto CreateSixDofVehicle( entt::entity entity, JPH::Body& chassis, const scene::VehicleComponent& settings ) -> void;
        auto UpdateVehicleInputs( entt::entity entity, const scene::VehicleComponent& settings ) -> void;
        auto SynchronizeBridges() -> void;
        auto RemoveBridges( entt::entity entity ) -> void;
        auto SynchronizeJoints() -> void;
        auto RemoveJoints( entt::entity body ) -> void;
        auto RemoveJoint( entt::entity entity ) -> void;
        auto FindEntityBody( core::u64 guid ) -> JPH::Body*;
        auto SynchronizeCharacters() -> void;
        auto RemoveCharacter( entt::entity entity ) -> void;
        auto UpdateCharacters( core::f32 timeStep ) -> void;
        auto PostUpdateCharacters() -> void;
        auto ApplyBuoyancy( core::f32 timeStep ) -> void;
        auto SetEntityWorldPose( entt::entity entity, const core::float3& position, const core::quat& rotation, const core::float3& scale = core::float3{ 1 } ) -> void;
        auto SynchronizeSoftBodies() -> void;
        auto RemoveSoftBody( entt::entity entity ) -> void;
        auto UpdateSoftBodyGrab( core::f32 timeStep ) -> void;
        auto PostUpdateSoftBodies() -> void;
        auto UpdateRagdollPoses() -> void;
        auto SynchronizeRagdolls() -> void;
        struct RagdollBinding {
            core::u64 mEntityID{};
            core::i32 mJoint{};
            core::float4x4 mBodyToJoint{ 1 };
        };
        struct RagdollState {
            core::u64 mRevision{}, mAnimatorID{};
            asset::AssetHandle<asset::Skeleton> mSkeleton{};
            eastl::vector<RagdollBinding> mBindings{};
            eastl::vector<core::float4x4> mAnimationPose{}, mPhysicsPose{};
            eastl::string mError{};
        };
        ankerl::unordered_dense::map<entt::entity, RagdollState> mRagdolls{};

        struct SoftBodyState {
            JPH::Body* mBody{};
            JPH::Ref<JPH::SoftBodySharedSettings> mSettings{};
            core::u64 mRevision{};
            bool mCloth{};
            asset::ModelHandle mModel{};
            renderer::MeshRenderDataHandle mSource{};
            core::Ref<renderer::MeshRenderData> mDeformed{};
            eastl::vector<core::u32> mVertexMap{};
            eastl::vector<core::float3> mPositions{};
            core::float3 mScale{ 1 }, mPosition{};
            core::quat mRotation{ 1, 0, 0, 0 };
            eastl::string mError{};
        };
        ankerl::unordered_dense::map<entt::entity, SoftBodyState> mSoftBodies{};

        struct SoftBodyGrab {
            JPH::BodyID mBody{};
            core::u32 mParticle{};
            core::f32 mInverseMass{};
            core::float3 mTarget{};
        } mSoftBodyGrab{};

        struct CharacterState {
            JPH::Ref<JPH::Character> mRigid{};
            JPH::Ref<JPH::CharacterVirtual> mVirtual{};
            core::u64 mRevision{};
            core::float3 mPosition{};
            core::quat mRotation{ 1, 0, 0, 0 };
            eastl::string mError{};
        };
        ankerl::unordered_dense::map<entt::entity, CharacterState> mCharacters{};
        JPH::CharacterVsCharacterCollisionSimple mCharacterCollisions{};

        struct JointState {
            JPH::Ref<JPH::TwoBodyConstraint> mConstraint{};
            JPH::Body* mBodyA{};
            JPH::Body* mBodyB{};
            core::u64 mRevision{};
            entt::id_type mType{};
            JPH::Ref<JPH::Constraint> mDependencyA{}, mDependencyB{};
            eastl::string mError{};
        };
        ankerl::unordered_dense::map<entt::entity, JointState> mJoints{};

        struct BridgeState {
            eastl::vector<entt::entity> mSegments{};
            eastl::vector<JPH::Ref<JPH::Constraint>> mConstraints{};
        };
        ankerl::unordered_dense::map<entt::entity, BridgeState> mBridges{};

        struct VehicleState {
            JPH::Body* mChassis{};
            JPH::Ref<JPH::VehicleConstraint> mConstraint{};
            eastl::vector<JPH::BodyID> mWheelBodies{};
            eastl::vector<JPH::Ref<JPH::SixDOFConstraint>> mWheelConstraints{};
            JPH::CollisionGroup mOriginalCollisionGroup{};
            scene::VehicleComponent mSettings{};
        };
        ankerl::unordered_dense::map<entt::entity, VehicleState> mVehicles{};

        MKT_NODISCARD static auto GetFloat4x4F( const JPH::RMat44& jphMat ) -> core::float4x4;
        MKT_NODISCARD static auto GetFloat3F( const JPH::Vec3& jphVec3 ) -> core::float3;
        MKT_NODISCARD static auto GetQuatF( const JPH::Quat& jphQuat ) -> core::quat;

        MKT_NODISCARD static auto GetFloat3F( const core::float3& vec3GLM ) -> JPH::Vec3;
        MKT_NODISCARD static auto GetQuatF( const core::float3& vec3EulerAnglesGLM ) -> JPH::Quat;
        MKT_NODISCARD static auto GetQuatF( const core::quat &q ) -> JPH::Quat;

    private:

        scene::Scene* mScene{};
        SimulationInfo mSimulationInfo{};

        GravityBody mGravityBody{ GravityBody::eEarth };
        core::float3 mGravity{ GetGravityFor( GravityBody::eEarth ) };
        JPH::EPhysicsUpdateError mReportedUpdateErrors{ JPH::EPhysicsUpdateError::None };

        std::atomic_uint64_t mBodyIdCounter{ 0 };
        ankerl::unordered_dense::map<core::u64, JPH::Body*> mBodies{};
        struct BodyState {
            core::u64 mID{};
            ColliderShapeDescription mDescription{};
            ShapeHandle mShape{};
            asset::ModelHandle mModel{}; // Keeps immutable CPU geometry alive while its mDescription description is cached.
            bool mShapeValid{};
            bool mCookingFailed{};
            bool mTrigger{};
            core::float3 mPosition{};
            core::quat mRotation{ 1, 0, 0, 0 };
            core::float3 mLinearVelocity{};
            core::float3 mAngularVelocity{};
            std::string mError{};
            core::u64 mHeightRevision{}, mLandscapeRevision{}, mHeightAssetID{};
            eastl::shared_ptr<const renderer::TerrainHeightmap> mHeightmap{};
            eastl::shared_ptr<const eastl::vector<core::f32>> mHeightSamples{};
        };
        ankerl::unordered_dense::map<entt::entity, BodyState> mEntityBodies{};

        // Drawing settings
#ifdef JPH_DEBUG_RENDERER
        bool mDrawGetTriangles{ true };            // Draw all shapes using Shape::GetTrianglesStart/Next
        bool mDrawConstraints{ true };             // If the constraints should be drawn
        bool mDrawConstraintLimits{ true };        // If the constraint limits should be drawn
        bool mDrawConstraintReferenceFrame{ true };// If the constraint reference frames should be drawn
        bool mDrawBroadPhaseBounds{ true };        // If the bounds of the broadphase should be drawn
        JPH::BodyManager::DrawSettings mBodyDrawSettings{};										// Settings for how to draw bodies from the body manager
        JPH::SkeletonPose::DrawSettings mPoseDrawSettings{};										// Settings for drawing skeletal poses
#endif // JPH_DEBUG_RENDERER
    };
}

#endif// MIKOTO_PHYSICS_PHYSICS_WORLD_HH
