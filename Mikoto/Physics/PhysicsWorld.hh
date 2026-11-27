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

#include <ankerl/unordered_dense.h>

#include <EASTL/atomic.h>
#include <EASTL/string.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/vector.h>

#include <entt/entity/entity.hpp>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Character/Character.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Constraints/SixDOFConstraint.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/SoftBody/SoftBodyMotionProperties.h>
#include <Jolt/Physics/SoftBody/SoftBodySharedSettings.h>
#include <Jolt/Physics/Vehicle/VehicleConstraint.h>

#include <Core/Core.hh>
#include <Core/Service.hh>
#include <Core/Types.hh>

#include <Physics/ContactEvents.hh>
#include <Physics/RayCast.hh>
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

    struct PhysicsWorldCreateInfo {
        scene::Scene *mScene{ nullptr };
        core::float3 mGravity{ 0.0f, -9.81f, 0.0f };
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

    // Owns the simulation and reconciles its bodies with one scene.
    class PhysicsWorld final : public core::ISubsystem {
    public:
        explicit PhysicsWorld( const PhysicsWorldCreateInfo &scene );

        ~PhysicsWorld() override;

        auto Initialize() -> void override;
        auto Shutdown() -> void override;
        auto Update( float timeStep ) -> void override;

        // Reconcile ECS edits on the main thread, including while the scene is idle.
        auto Synchronize() -> void;
        auto AddRigidBody( scene::Entity *entity ) -> void;
        auto AddCollider( scene::Entity *entity ) -> void;
        auto RemoveRigidBody( scene::Entity *entity ) -> void;
        auto RemoveColliderBody( scene::Entity *entity ) -> void;
        auto RemoveEntityBody( entt::entity entity ) -> void;
        auto ClearBodies() -> void;

        auto SetGravity( const core::float3 &gravity ) -> void;
        auto SetGravityBody( GravityBody body ) -> void;

        // Drive one live particle until release. Targets are in world space.
        MKT_NODISCARD auto BeginSoftBodyGrab( const SoftBodyHit &hit ) -> bool;
        auto SetSoftBodyGrabTarget( const core::float3 &target ) -> void;
        auto EndSoftBodyGrab() -> void;

        auto DrawPhysics( JPH::DebugRenderer *renderer, const PhysicsDebugDrawSettings &settings ) -> void;
        auto RestartContactNotifications() -> void;
        /** Returns pending collision/trigger notifications and clears their queue. */
        MKT_NODISCARD auto GetContactEvents() -> eastl::vector<ContactEvent>;

        MKT_NODISCARD auto GetScene() const -> scene::Scene *;
        MKT_NODISCARD auto GetGravity() const -> const core::float3 &;
        MKT_NODISCARD auto GetGravityBody() const -> GravityBody;
        MKT_NODISCARD auto GetBodyCount() const -> core::usize;
        MKT_NODISCARD auto GetDroppedContactEventCount() const -> core::usize;
        MKT_NODISCARD auto GetColliderError( entt::entity entity ) const -> const char *;

        // Read positions from the current simulation surface on the main thread.
        MKT_NODISCARD auto IsSoftBodyGrabActive() const -> bool;
        MKT_NODISCARD auto CastRay( const RayCast &ray, bool staticOnly = false ) const -> RayCastHit;
        MKT_NODISCARD auto PickSoftBody( const RayCast &ray ) const -> SoftBodyHit;
        auto ReadSoftBodyParticles( entt::entity entity, eastl::vector<SoftBodyParticle> &particles ) const -> void;

        MKT_NODISCARD static auto GetGravityFor( GravityBody body ) -> core::float3;
        MKT_NODISCARD static auto Create( const PhysicsWorldCreateInfo &spec ) -> eastl::unique_ptr<PhysicsWorld>;

    private:
        struct SimulationInfo;
        auto ReportUpdateErrors( JPH::EPhysicsUpdateError errors ) -> void;
        auto PostUpdate() -> void;

        auto GenerateBodyID() -> core::u64;
        auto SetEntityWorldPose( entt::entity entity, const core::float3 &position, const core::quat &rotation, const core::float3 &scale = core::float3{ 1 } ) -> void;

        MKT_NODISCARD auto GetJoltBody( core::u64 id ) -> JPH::Body *;

        // Capture authored settings, reconcile shapes, then synchronize live body properties.
        auto ReadColliderDescription( entt::entity entity, bool &trigger, eastl::vector<entt::entity> *ancestors = nullptr ) -> ColliderShapeDescription;
        auto ReadHeightField( entt::entity entity, ColliderShapeDescription &description ) -> void;
        auto ReadCompound( entt::entity entity, ColliderShapeDescription &description, eastl::vector<entt::entity> &ancestors ) -> void;
        auto DisableBody( entt::entity entity, const char *message ) -> void;
        auto SynchronizeEntity( entt::entity entity ) -> void;
        auto SynchronizeBodyProperties( entt::entity entity, const core::float3 &position,
            const core::quat &rotation, bool trigger, bool shapeChanged ) -> void;
        auto SynchronizeVehicles() -> void;
        auto RemoveVehicle( entt::entity entity ) -> void;
        auto UpdateVehicleWheels() -> void;
        auto CreateSixDofVehicle( entt::entity entity, JPH::Body &chassis, const scene::VehicleComponent &settings ) -> void;
        auto UpdateVehicleInputs( entt::entity entity, const scene::VehicleComponent &settings ) -> void;
        auto SynchronizeBridges() -> void;
        auto RemoveBridges( entt::entity entity ) -> void;
        auto SynchronizeJoints() -> void;
        auto RemoveJoints( entt::entity body ) -> void;
        auto RemoveJoint( entt::entity entity ) -> void;
        MKT_NODISCARD auto FindEntityBody( core::u64 guid ) -> JPH::Body *;
        auto SynchronizeCharacters() -> void;
        auto RemoveCharacter( entt::entity entity ) -> void;
        auto UpdateCharacters( core::f32 timeStep ) -> void;
        auto PostUpdateCharacters() -> void;
        auto ApplyBuoyancy( core::f32 timeStep ) -> void;
        auto SynchronizeSoftBodies() -> void;
        auto RemoveSoftBody( entt::entity entity ) -> void;
        auto UpdateSoftBodyGrab( core::f32 timeStep ) -> void;
        auto PostUpdateSoftBodies() -> void;
        auto ApplyClothAerodynamics( core::f32 dt ) -> void;
        static auto ApplyClothWind( JPH::SoftBodyMotionProperties &motion, eastl::vector<core::float4> &forces,
            const scene::WindField &field, const core::float4x4 &localToWorld, const core::float3 &authoredWind,
            core::f32 drag, core::f32 lift, core::f32 gust, core::f32 dt ) -> void;
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
            JPH::Body *mBody{};
            JPH::Ref<JPH::SoftBodySharedSettings> mSettings{};
            core::u64 mRevision{};
            bool mCloth{};
            asset::ModelHandle mModel{};
            renderer::MeshRenderDataHandle mSource{};
            core::Ref<renderer::MeshRenderData> mDeformed{};
            eastl::vector<core::u32> mVertexMap{};
            eastl::vector<core::float3> mPositions{};
            eastl::vector<core::float4> mWindForces{};
            core::float3 mScale{ 1 }, mPosition{};
            core::quat mRotation{ 1, 0, 0, 0 };
            eastl::string mError{};
        };
        ankerl::unordered_dense::map<entt::entity, SoftBodyState> mSoftBodies{};
        scene::WindField mClothWindField{};

        struct SoftBodyGrab {
            JPH::BodyID mBody{};
            core::u32 mParticle{};
            core::f32 mInverseMass{};
            core::float3 mTarget{};
        } mSoftBodyGrab{};

        struct CharacterState {
            JPH::Ref<JPH::Character> mRigid{};
            JPH::Ref<JPH::CharacterVirtual> mVirtual{};
            eastl::vector<ContactEvent> mContacts{};
            core::u64 mRevision{};
            core::float3 mPosition{};
            core::quat mRotation{ 1, 0, 0, 0 };
            eastl::string mError{};
        };
        ankerl::unordered_dense::map<entt::entity, CharacterState> mCharacters{};
        JPH::CharacterVsCharacterCollisionSimple mCharacterCollisions{};

        struct JointState {
            JPH::Ref<JPH::TwoBodyConstraint> mConstraint{};
            JPH::Body *mBodyA{};
            JPH::Body *mBodyB{};
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
            JPH::Body *mChassis{};
            JPH::Ref<JPH::VehicleConstraint> mConstraint{};
            eastl::vector<JPH::BodyID> mWheelBodies{};
            eastl::vector<JPH::Ref<JPH::SixDOFConstraint>> mWheelConstraints{};
            JPH::CollisionGroup mOriginalCollisionGroup{};
            scene::VehicleComponent mSettings{};
        };
        ankerl::unordered_dense::map<entt::entity, VehicleState> mVehicles{};

        MKT_NODISCARD static auto GetFloat4x4F( const JPH::RMat44 &jphMat ) -> core::float4x4;
        MKT_NODISCARD static auto GetFloat3F( const JPH::Vec3 &jphVec3 ) -> core::float3;
        MKT_NODISCARD static auto GetQuatF( const JPH::Quat &jphQuat ) -> core::quat;

        MKT_NODISCARD static auto GetFloat3F( const core::float3 &vec3GLM ) -> JPH::Vec3;
        MKT_NODISCARD static auto GetQuatF( const core::float3 &vec3EulerAnglesGLM ) -> JPH::Quat;
        MKT_NODISCARD static auto GetQuatF( const core::quat &q ) -> JPH::Quat;

    private:
        scene::Scene *mScene{};
        eastl::unique_ptr<SimulationInfo> mSimulationInfo{};

        GravityBody mGravityBody{ GravityBody::eEarth };
        core::float3 mGravity{ GetGravityFor( GravityBody::eEarth ) };
        JPH::EPhysicsUpdateError mReportedUpdateErrors{ JPH::EPhysicsUpdateError::None };

        eastl::atomic<core::u64> mBodyIdCounter{ 0 };
        ankerl::unordered_dense::map<core::u64, JPH::Body *> mBodies{};
        struct BodyState {
            core::u64 mID{};
            ColliderShapeDescription mDescription{};
            ShapeHandle mShape{};
            asset::ModelHandle mModel{};// Keeps immutable CPU geometry alive while its mDescription description is cached.
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
        std::set<entt::entity> mCompoundMembers{};
    };
}// namespace mikoto::physics

#endif// MIKOTO_PHYSICS_PHYSICS_WORLD_HH
