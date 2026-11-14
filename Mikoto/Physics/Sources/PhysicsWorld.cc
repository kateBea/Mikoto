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

#include <cmath>
#include <algorithm>

#include <EASTL/sort.h>
#include <EASTL/atomic.h>
#include <EASTL/memory.h>
#include <EASTL/algorithm.h>
#include <EASTL/unique_ptr.h>

#include <glm/glm.hpp>
#include <glm/gtx/compatibility.hpp>

#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/GroupFilterTable.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Vehicle/MotorcycleController.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Vehicle/VehicleCollisionTester.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Vehicle/WheeledVehicleController.h>
#include <Jolt/Physics/Vehicle/TrackedVehicleController.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Logging/Assert.hh>
#include <Logging/Logger.hh>

#include <Memory/Allocator.hh>

#include <Threading/Utility.hh>

#include <Physics/RayCast.hh>
#include <Physics/Utility.hh>
#include <Physics/PhysicSystem.hh>
#include <Physics/PhysicsWorld.hh>

#include <Renderer/Core/PhysicsDebugRenderer.hh>

#include <Scene/Scene.hh>
#include <Scene/Utility.hh>
#include <Scene/Component.hh>

// The Jolt headers don't include Jolt.h. Always include Jolt.h before including any other Jolt header.
// You can use Jolt.h in your precompiled header to speed up compilation.

// Jolt includes

#include <map>
#include <tuple>

#include <glm/gtx/matrix_decompose.hpp>

#include <Jolt/Physics/Constraints/ConeConstraint.h>
#include <Jolt/Physics/Constraints/PathConstraint.h>
#include <Jolt/Physics/Constraints/GearConstraint.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/SliderConstraint.h>
#include <Jolt/Physics/Constraints/PulleyConstraint.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Constraints/SwingTwistConstraint.h>
#include <Jolt/Physics/SoftBody/SoftBodyShape.h>
#include <Jolt/Physics/SoftBody/SoftBodyCreationSettings.h>
#include <Jolt/Physics/SoftBody/SoftBodyMotionProperties.h>
#include <Jolt/Physics/Constraints/RackAndPinionConstraint.h>
#include <Jolt/Physics/Constraints/PathConstraintPathHermite.h>

#include <Core/Exception.hh>

#include <Assets/ImageProcessing.hh>

#include <Animation/Animator.hh>
#include <Animation/AnimationSystem.hh>

#include <Renderer/Core/MeshRenderData.hh>
#include <Renderer/Core/TerrainGeneration.hh>

namespace mikoto::physics {

    using namespace mikoto::core;
    using namespace mikoto::scene;

    static auto Vec( const core::float3& v ) -> JPH::Vec3 {
        return { v.x, v.y, v.z };
    }

    static auto Quat( const core::quat& q ) -> JPH::Quat {
        if ( glm::dot( q, q ) < .0001f ) {
            MKT_THROW_RUNTIME_ERROR( "Joint rotation must be nonzero." );
        }

        return JPH::Quat{ q.x, q.y, q.z, q.w }.Normalized();
    }

    static auto Spring( const JointSpringSettings& source ) -> JPH::SpringSettings {
        if ( source.mFrequency < 0 || source.mDamping < 0 ) {
            MKT_THROW_RUNTIME_ERROR( "Joint spring frequency and damping must be nonnegative." );
        }

        return { JPH::ESpringMode::FrequencyAndDamping, source.mFrequency, source.mDamping };
    }

    static auto Motor( const JointMotorSettings& source, bool angular = false ) -> JPH::MotorSettings {
        JPH::MotorSettings motor{};
        motor.mSpringSettings = Spring( source.mSpring );

        if ( source.mMaxForceOrTorque < 0 ) {
            MKT_THROW_RUNTIME_ERROR( "Joint motor limit must be nonnegative." );
        }

        if ( angular ) {
            motor.SetTorqueLimit( source.mMaxForceOrTorque );
        }
        else {
            motor.SetForceLimit( source.mMaxForceOrTorque );
        }

        return motor;
    }

    static auto MotorState( JointMotorMode mode ) -> JPH::EMotorState {
        switch ( mode ) {
            case JointMotorMode::eOff:
                return JPH::EMotorState::Off;
            case JointMotorMode::eVelocity:
                return JPH::EMotorState::Velocity;
            case JointMotorMode::ePosition:
                return JPH::EMotorState::Position;
        }

        MKT_THROW_RUNTIME_ERROR( "Invalid joint motor mode." );
    }

    static auto CheckLimits( float minimum, float maximum ) -> void {
        if ( minimum > maximum ) {
            MKT_THROW_RUNTIME_ERROR( "Joint minimum must not exceed maximum." );
        }
    }

    class SoftBodyPickFilter final : public JPH::BodyFilter {
    public:
        auto ShouldCollideLocked( const JPH::Body& body ) const -> bool override {
            return !body.IsSensor();
        }
    };

    struct JointFrame {
        JPH::RVec3 mPoint;
        JPH::Quat mRotation;
        auto X() const -> JPH::Vec3 {
            return mRotation.RotateAxisX();
        }

        auto Y() const -> JPH::Vec3 {
            return mRotation.RotateAxisY();
        }
    };

    static auto Frame( const JPH::Body& body, const core::float3& anchor, const core::quat& rotation ) -> JointFrame {
        if ( &body == &JPH::Body::sFixedToWorld ) {
            return { Vec( anchor ), Quat( rotation ) };
        }

        return { body.GetWorldTransform() * Vec( anchor ), body.GetRotation() * Quat( rotation ) };
    }

    template<typename T>
    static auto CreateJoint( const T& joint, JPH::Body& a, JPH::Body& b, JPH::Constraint* dependencyA, JPH::Constraint* dependencyB ) -> JPH::Ref<JPH::TwoBodyConstraint> {
        // The settings/drive calls below follow the corresponding Samples/Tests/Constraints/*Test.cpp.
        // https://github.com/jrouwe/JoltPhysics/tree/e77f175595e64cb44218cc9d9d56fc365ad0e36a/Samples/Tests/Constraints
        const auto fa{ Frame( a, joint.GetAnchorA(), joint.GetRotationA() ) };
        const auto fb{ Frame( b, joint.GetAnchorB(), joint.GetRotationB() ) };

        if constexpr ( std::is_same_v<T, DistanceJointComponent> ) {
            CheckLimits( joint.GetMinDistance(), joint.GetMaxDistance() );

            if ( joint.GetMinDistance() < 0 ) {
                MKT_THROW_RUNTIME_ERROR( "Distance must be nonnegative." );
            }

            JPH::DistanceConstraintSettings s{};
            s.mPoint1 = fa.mPoint;
            s.mPoint2 = fb.mPoint;
            s.mMinDistance = joint.GetMinDistance();
            s.mMaxDistance = joint.GetMaxDistance();
            s.mLimitsSpringSettings = Spring( joint.GetSpring() );

            return s.Create( a, b );
        }
        else if constexpr ( std::is_same_v<T, PointJointComponent> ) {
            JPH::PointConstraintSettings s{};
            s.mPoint1 = fa.mPoint;
            s.mPoint2 = fb.mPoint;

            return s.Create( a, b );
        }
        else if constexpr ( std::is_same_v<T, FixedJointComponent> ) {
            JPH::FixedConstraintSettings s{};
            s.mPoint1 = fa.mPoint;
            s.mPoint2 = fb.mPoint;
            s.mAxisX1 = fa.X();
            s.mAxisY1 = fa.Y();
            s.mAxisX2 = fb.X();
            s.mAxisY2 = fb.Y();

            return s.Create( a, b );
        }
        else if constexpr ( std::is_same_v<T, HingeJointComponent> || std::is_same_v<T, SliderJointComponent> ) {
            constexpr bool hinge{ std::is_same_v<T, HingeJointComponent> };
            using Settings = std::conditional_t<hinge, JPH::HingeConstraintSettings, JPH::SliderConstraintSettings>;
            using Constraint = std::conditional_t<hinge, JPH::HingeConstraint, JPH::SliderConstraint>;
            Settings s{};
            s.mPoint1 = fa.mPoint;
            s.mPoint2 = fb.mPoint;
            s.mNormalAxis1 = fa.Y();
            s.mNormalAxis2 = fb.Y();

            if constexpr ( hinge ) {
                s.mHingeAxis1 = fa.X();
                s.mHingeAxis2 = fb.X();
                s.mLimitsMin = joint.GetMinAngle();
                s.mLimitsMax = joint.GetMaxAngle();

                if ( s.mLimitsMin < -JPH::JPH_PI || s.mLimitsMax > JPH::JPH_PI ) {
                    MKT_THROW_RUNTIME_ERROR( "Hinge limits must lie in [-pi, pi]." );
                }
            }
            else {
                s.mAutoDetectPoint = false;
                s.mSliderAxis1 = fa.X();
                s.mSliderAxis2 = fb.X();
                s.mLimitsMin = joint.GetMinDistance();
                s.mLimitsMax = joint.GetMaxDistance();

                if ( s.mLimitsMin > 0 || s.mLimitsMax < 0 ) {
                    MKT_THROW_RUNTIME_ERROR( "Slider limits must include zero." );
                }
            }

            CheckLimits( s.mLimitsMin, s.mLimitsMax );
            s.mLimitsSpringSettings = Spring( joint.GetLimitSpring() );
            s.mMotorSettings = Motor( joint.GetMotor(), hinge );

            // Build the constraint before applying the runtime drive mode and targets.
            JPH::Ref<Constraint> c{ static_cast<Constraint*>( s.Create( a, b ) ) };
            c->SetMotorState( MotorState( joint.GetMotor().mMode ) );

            if constexpr ( hinge ) {
                c->SetTargetAngle( joint.GetMotor().mTargetPosition );
                c->SetTargetAngularVelocity( joint.GetMotor().mTargetVelocity );
            }
            else {
                c->SetTargetPosition( joint.GetMotor().mTargetPosition );
                c->SetTargetVelocity( joint.GetMotor().mTargetVelocity );
            }

            return c.GetPtr();
        }
        else if constexpr ( std::is_same_v<T, ConeJointComponent> ) {
            if ( joint.GetHalfConeAngle() < 0 || joint.GetHalfConeAngle() > JPH::JPH_PI ) {
                MKT_THROW_RUNTIME_ERROR( "Cone angle must lie in [0, pi]." );
            }

            JPH::ConeConstraintSettings s{};
            s.mPoint1 = fa.mPoint;
            s.mPoint2 = fb.mPoint;
            s.mTwistAxis1 = fa.X();
            s.mTwistAxis2 = fb.X();
            s.mHalfConeAngle = joint.GetHalfConeAngle();

            return s.Create( a, b );
        }
        else if constexpr ( std::is_same_v<T, SwingTwistJointComponent> ) {
            JPH::SwingTwistConstraintSettings s{};
            s.mPosition1 = fa.mPoint;
            s.mPosition2 = fb.mPoint;
            s.mTwistAxis1 = fa.X();
            s.mTwistAxis2 = fb.X();
            s.mPlaneAxis1 = fa.Y();
            s.mPlaneAxis2 = fb.Y();
            s.mNormalHalfConeAngle = joint.GetNormalHalfConeAngle();
            s.mPlaneHalfConeAngle = joint.GetPlaneHalfConeAngle();
            s.mTwistMinAngle = joint.GetMinTwistAngle();
            s.mTwistMaxAngle = joint.GetMaxTwistAngle();
            CheckLimits( s.mTwistMinAngle, s.mTwistMaxAngle );

            if ( s.mNormalHalfConeAngle < 0 || s.mNormalHalfConeAngle > JPH::JPH_PI || s.mPlaneHalfConeAngle < 0 || s.mPlaneHalfConeAngle > JPH::JPH_PI || s.mTwistMinAngle < -JPH::JPH_PI || s.mTwistMaxAngle > JPH::JPH_PI ) {
                MKT_THROW_RUNTIME_ERROR( "Invalid swing/twist limits." );
            }

            const auto& motors{ joint.GetMotors() };

            if ( motors[1].mMode != motors[2].mMode ) {
                MKT_THROW_RUNTIME_ERROR( "Jolt uses one shared mode for both swing motors." );
            }

            s.mTwistMotorSettings = Motor( motors[0], true );
            s.mSwingMotorSettings = Motor( motors[1], true );

            JPH::Ref<JPH::SwingTwistConstraint> c{ static_cast<JPH::SwingTwistConstraint*>( s.Create( a, b ) ) };
            c->SetTwistMotorState( MotorState( motors[0].mMode ) );
            c->SetSwingMotorState( MotorState( motors[1].mMode ) );
            c->SetTargetAngularVelocityCS( JPH::Vec3{ motors[0].mTargetVelocity, motors[1].mTargetVelocity, motors[2].mTargetVelocity } );
            c->SetTargetOrientationCS( JPH::Quat::sEulerAngles( JPH::Vec3{ motors[0].mTargetPosition, motors[1].mTargetPosition, motors[2].mTargetPosition } ) );

            return c.GetPtr();
        }
        else if constexpr ( std::is_same_v<T, SixDofJointComponent> ) {
            JPH::SixDOFConstraintSettings s{};
            s.mPosition1 = fa.mPoint;
            s.mPosition2 = fb.mPoint;
            s.mAxisX1 = fa.X();
            s.mAxisY1 = fa.Y();
            s.mAxisX2 = fb.X();
            s.mAxisY2 = fb.Y();
            const auto& motors{ joint.GetMotors() };
            JPH::Vec3 position{}, rotation{}, velocity{}, angular{};

            // Each translation/rotation axis has its own limits and drive settings.
            for ( int i{}; i < 6; ++i ) {
                const auto axis{ static_cast<JPH::SixDOFConstraintSettings::EAxis>( i ) };
                const auto motion{ i < 3 ? joint.GetTranslationMotion()[i] : joint.GetRotationMotion()[i - 3] };

                if ( motion == JointAxisMotion::eFixed ) {
                    s.MakeFixedAxis( axis );
                }
                else if ( motion == JointAxisMotion::eFree ) {
                    s.MakeFreeAxis( axis );
                }
                else {
                    const float minimum{ i < 3 ? joint.GetMinTranslation()[i] : joint.GetMinRotation()[i - 3] };
                    const float maximum{ i < 3 ? joint.GetMaxTranslation()[i] : joint.GetMaxRotation()[i - 3] };
                    CheckLimits( minimum, maximum );

                    if ( minimum > 0 || maximum < 0 || ( i >= 3 && ( minimum < -JPH::JPH_PI || maximum > JPH::JPH_PI ) ) ) {
                        MKT_THROW_RUNTIME_ERROR( "6DOF limits must include zero; rotation limits must lie in [-pi, pi]." );
                    }

                    s.SetLimitedAxis( axis, minimum, maximum );
                }

                s.mMotorSettings[i] = Motor( motors[i], i >= 3 );
                ( i < 3 ? position : rotation ).SetComponent( i % 3, motors[i].mTargetPosition );
                ( i < 3 ? velocity : angular ).SetComponent( i % 3, motors[i].mTargetVelocity );
            }

            JPH::Ref<JPH::SixDOFConstraint> c{ static_cast<JPH::SixDOFConstraint*>( s.Create( a, b ) ) };

            for ( int i{}; i < 6; ++i ) {
                c->SetMotorState( static_cast<JPH::SixDOFConstraint::EAxis>( i ), MotorState( motors[i].mMode ) );
            }

            c->SetTargetPositionCS( position );
            c->SetTargetOrientationCS( JPH::Quat::sEulerAngles( rotation ) );
            c->SetTargetVelocityCS( velocity );
            c->SetTargetAngularVelocityCS( angular );

            return c.GetPtr();
        }
        else if constexpr ( std::is_same_v<T, PathJointComponent> ) {
            if ( joint.GetControlPoints().size() < 2 || !b.IsDynamic() ) {
                MKT_THROW_RUNTIME_ERROR( "A path needs at least two points and a dynamic body B." );
            }

            JPH::Ref<JPH::PathConstraintPathHermite> path{ new JPH::PathConstraintPathHermite{} };
            path->SetIsLooping( joint.IsLooping() );

            for ( const auto& point: joint.GetControlPoints() ) {
                if ( glm::length2( point.mTangent ) < .0001f || glm::length2( glm::cross( point.mTangent, point.mNormal ) ) < .0001f ) {
                    MKT_THROW_RUNTIME_ERROR( "Path tangent and normal must form a valid frame." );
                }

                path->AddPoint( Vec( point.mPosition ), Vec( point.mTangent ), Vec( point.mNormal ).Normalized() );
            }

            if ( joint.GetInitialFraction() < 0 || joint.GetInitialFraction() > path->GetPathMaxFraction() ) {
                MKT_THROW_RUNTIME_ERROR( "Path fraction is outside the spline." );
            }

            JPH::PathConstraintSettings s{};
            s.mPath = path;
            s.mPathPosition = Vec( joint.GetAnchorA() );
            s.mPathRotation = Quat( joint.GetRotationA() );
            s.mPathFraction = joint.GetInitialFraction();
            s.mMaxFrictionForce = joint.GetMaxFrictionForce();
            s.mPositionMotorSettings = Motor( joint.GetMotor() );

            JPH::Ref<JPH::PathConstraint> c{ static_cast<JPH::PathConstraint*>( s.Create( a, b ) ) };
            c->SetPositionMotorState( MotorState( joint.GetMotor().mMode ) );
            c->SetTargetVelocity( joint.GetMotor().mTargetVelocity );
            c->SetTargetPathFraction( joint.GetMotor().mTargetPosition );

            return c.GetPtr();
        }
        else if constexpr ( std::is_same_v<T, PulleyJointComponent> ) {
            CheckLimits( joint.GetMinLength(), joint.GetMaxLength() );

            if ( joint.GetRatio() <= 0 ) {
                MKT_THROW_RUNTIME_ERROR( "Pulley ratio must be positive." );
            }

            JPH::PulleyConstraintSettings s{};
            s.mBodyPoint1 = fa.mPoint;
            s.mBodyPoint2 = fb.mPoint;
            s.mFixedPoint1 = Vec( joint.GetFixedPointA() );
            s.mFixedPoint2 = Vec( joint.GetFixedPointB() );
            s.mRatio = joint.GetRatio();
            s.mMinLength = joint.GetMinLength();
            s.mMaxLength = joint.GetMaxLength();

            return s.Create( a, b );
        }
        else if constexpr ( std::is_same_v<T, GearJointComponent> ) {
            if ( !dependencyA || !dependencyB || dependencyA->GetSubType() != JPH::EConstraintSubType::Hinge || dependencyB->GetSubType() != JPH::EConstraintSubType::Hinge || joint.GetRatio() == 0 ) {
                MKT_THROW_RUNTIME_ERROR( "Gear requires two live hinges and a nonzero ratio." );
            }

            JPH::GearConstraintSettings s{};
            s.mHingeAxis1 = fa.X();
            s.mHingeAxis2 = fb.X();
            s.mRatio = joint.GetRatio();

            JPH::Ref<JPH::GearConstraint> c{ static_cast<JPH::GearConstraint*>( s.Create( a, b ) ) };
            c->SetConstraints( dependencyA, dependencyB );

            return c.GetPtr();
        }
        else if constexpr ( std::is_same_v<T, RackAndPinionJointComponent> ) {
            if ( !dependencyA || !dependencyB || dependencyA->GetSubType() != JPH::EConstraintSubType::Hinge || dependencyB->GetSubType() != JPH::EConstraintSubType::Slider || joint.GetRatio() == 0 ) {
                MKT_THROW_RUNTIME_ERROR( "Rack and pinion requires a live hinge, slider and a nonzero ratio." );
            }

            JPH::RackAndPinionConstraintSettings s{};
            s.mHingeAxis = fa.X();
            s.mSliderAxis = fb.X();
            s.mRatio = joint.GetRatio();

            JPH::Ref<JPH::RackAndPinionConstraint> c{ static_cast<JPH::RackAndPinionConstraint*>( s.Create( a, b ) ) };
            c->SetConstraints( dependencyA, dependencyB );

            return c.GetPtr();
        }
    }


    PhysicsWorld::PhysicsWorld( const PhysicsWorldCreateInfo& spec )
        : mScene{ spec.mScene }, mGravity{ spec.mGravity } {
    }

    PhysicsWorld::~PhysicsWorld() {
        // Constraints retain raw chassis pointers; detach them while the world
        // and body interface still exist, even if explicit shutdown was omitted.
        Shutdown();
    }

    auto PhysicsWorld::Initialize() -> void {
        MKT_BEGIN_PROFILER();

        if ( mIsInitialized ) {
            return;
        }

        MKT_CORE_LOGGER_INFO( "Initializing PhysicsWorld..." );

        // Reserve scratch space once per world; solver steps reuse it.
        mSimulationInfo.mTempAllocator = eastl::make_unique<JPH::TempAllocatorImpl>( 10 * 1024 * 1024 );

        constexpr core::u32 kMaxBodies{ 1024 };
        constexpr core::u32 kNumBodyMutexes{ 0 };// Autodetect
        // A pile has several contacts per body, and even more nearby candidate pairs.
        // Exhausting either cache makes Jolt discard collisions during the step.
        constexpr core::u32 kMaxBodyPairs{ 16 * kMaxBodies };
        constexpr core::u32 kMaxContactConstraints{ 8 * kMaxBodies };

        // Broadphase filters are members because Jolt borrows them for the world's lifetime.
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

#if !defined( NDEBUG )
        // A body activation listener gets notified when bodies activate and go to sleep
        // Note that this is called from a job so whatever you do here needs to be thread safe.
        // Registering one is entirely optional.
        mSimulationInfo.mPhysicsSystem.SetBodyActivationListener( MKT_ADDRESSOF( mSimulationInfo.mBodyActivationListener ) );
#endif

        // A contact listener gets notified when bodies (are about to) collide, and when they separate again.
        // Note that this is called from a job so whatever you do here needs to be thread safe.
        // Registering one is entirely optional.
        mSimulationInfo.mPhysicsSystem.SetContactListener( MKT_ADDRESSOF( mSimulationInfo.mContactListener ) );

        mSimulationInfo.mPhysicsSystem.OptimizeBroadPhase();

        mIsInitialized = true;
    }

    auto PhysicsWorld::Shutdown() -> void {
        MKT_BEGIN_PROFILER();

        if ( !mIsInitialized ) {
            return;
        }

        MKT_CORE_LOGGER_INFO( "Shutting down PhysicsWorld..." );

        ClearBodies();
        mSimulationInfo.mBodyInterface = nullptr;

        mSimulationInfo.mTempAllocator.reset();

        mIsInitialized = false;
    }

    auto PhysicsWorld::Update( float dt ) -> void {
        MKT_BEGIN_PROFILER();
        MKT_ASSERT( mSimulationInfo.mBodyInterface, "BodyInterface is NULL forgot to call PhysicsWorld::Init()" );
        MKT_ASSERT( mSimulationInfo.mTempAllocator, "TempAllocator is NULL forgot to call PhysicsWorld::Init()" );

        if ( dt <= 0.0f ) {
            return;
        }

        Synchronize();

        // Keep the solver stable during short frame hitches without allowing one bad
        // frame to monopolise the game thread.
        constexpr core::i32 kMaxCollisionSteps{ 4 };
        constexpr core::f32 kCollisionStep{ 1.0f / 60.0f };

        // Bound the elapsed time too: capping only the step count still permits huge solver steps.
        const core::f32 simulationTime{ eastl::min( dt, kMaxCollisionSteps * kCollisionStep ) };
        const core::i32 collisionSteps{ eastl::clamp( static_cast<core::i32>( glm::ceil( simulationTime / kCollisionStep ) ), 1, kMaxCollisionSteps ) };
        UpdateCharacters( simulationTime );
        ApplyBuoyancy( simulationTime );
        UpdateSoftBodyGrab( simulationTime );
        const auto errors{ mSimulationInfo.mPhysicsSystem.Update( simulationTime, collisionSteps,
            mSimulationInfo.mTempAllocator.get(), PhysicSystem::Get()->GetJoltJobSystem() ) };
        ReportUpdateErrors( errors );

        PostUpdate();
        UpdateVehicleWheels();
        PostUpdateCharacters();
        PostUpdateSoftBodies();
        UpdateRagdollPoses();
    }

    auto PhysicsWorld::DrawPhysics( JPH::DebugRenderer* renderer, const PhysicsDebugDrawSettings& settings ) -> void {
        MKT_BEGIN_PROFILER();
#ifdef JPH_DEBUG_RENDERER
        if ( !mIsInitialized || !renderer || !settings.mEnabled ) {
            return;
        }

        JPH::BodyManager::DrawSettings drawSettings{};

        // Draw shapes below with editor colors; keep Jolt's colors for axes,
        // velocities and constraints.
        drawSettings.mDrawShape = false;
        drawSettings.mDrawShapeWireframe = true;
        drawSettings.mDrawBoundingBox = settings.mBoundingBoxes;
        drawSettings.mDrawWorldTransform = settings.mBodyAxes;
        drawSettings.mDrawCenterOfMassTransform = settings.mCenterOfMass;
        drawSettings.mDrawVelocity = settings.mVelocities;

        struct SelectionFilter final : JPH::BodyDrawFilter {
            core::u64 selected{};
            bool ShouldDraw( const JPH::Body& body ) const override {
                return selected != 0 && body.GetUserData() == selected;
            }
        } filter;
        filter.selected = settings.mSelectedEntity;
        mSimulationInfo.mPhysicsSystem.DrawBodies( drawSettings, renderer, settings.mSelectedOnly ? &filter : nullptr );

        if ( settings.mShapes ) {
            for ( const auto& [id, body]: mBodies ) {
                if ( !body->IsInBroadPhase() || ( settings.mSelectedOnly && !filter.ShouldDraw( *body ) ) ) {
                    continue;
                }

                const JPH::Color color{ body->IsStatic() || body->IsActive() ? JPH::Color{ 144, 238, 144 } : JPH::Color::sRed };
                body->GetShape()->Draw( renderer, body->GetCenterOfMassTransform(), JPH::Vec3::sReplicate( 1 ), color, false, true );
            }
        }

        if ( settings.mConstraints ) {
            mSimulationInfo.mPhysicsSystem.DrawConstraints( renderer );
        }

        if ( settings.mConstraintLimits ) {
            mSimulationInfo.mPhysicsSystem.DrawConstraintLimits( renderer );
        }

        if ( settings.mConstraintFrames ) {
            mSimulationInfo.mPhysicsSystem.DrawConstraintReferenceFrame( renderer );
        }

        // if (mDrawBroadPhaseBounds)
        //     mSimulationInfo.mPhysicsSystem.DrawWireBox(mSimulationInfo.mPhysicsSystem.GetBroadPhaseQuery().GetBounds(), JPH::Color::sGreen);
#endif// JPH_DEBUG_RENDERER
    }

    auto PhysicsWorld::ReportUpdateErrors( JPH::EPhysicsUpdateError errors ) -> void {
        using Error = JPH::EPhysicsUpdateError;

        // Report each exhausted buffer once per world reset, not on every failing frame.
        const auto report = [&]( Error flag, const char* message ) {
            if ( ( errors & flag ) != Error::None && ( mReportedUpdateErrors & flag ) == Error::None ) {
                MKT_CORE_LOGGER_ERROR( "Physics collisions dropped: {}", message );
            }
        };
        report( Error::BodyPairCacheFull, "body-pair cache exhausted; increase kMaxBodyPairs." );
        report( Error::ManifoldCacheFull, "manifold cache exhausted; increase kMaxContactConstraints." );
        report( Error::ContactConstraintsFull, "contact constraint buffer exhausted; increase kMaxContactConstraints." );
        mReportedUpdateErrors |= errors;
    }

    auto PhysicsWorld::Synchronize() -> void {
        MKT_BEGIN_PROFILER();

        if ( !mIsInitialized || !mScene ) {
            return;
        }

        auto& registry{ mScene->GetRegistry() };
        mScene->UpdateWorldTransformations();

        // Cooking a compound also caches its child shapes; keep references stable during that traversal.
        mEntityBodies.reserve( mEntityBodies.size() + registry.view<scene::TransformComponent>().size() );
        mCompoundMembers.clear();

        for ( auto [owner, compound]: registry.view<scene::CompoundColliderComponent>().each() ) {
            for ( auto id: compound.GetColliderEntities() ) {
                if ( auto* child{ mScene->FindByID( id ) }; child && child->Get() != owner ) {
                    mCompoundMembers.insert( child->Get() );
                }
            }
        }

        eastl::vector<entt::entity> removed{};

        for ( const auto& [entity, state]: mEntityBodies ) {
            if ( !registry.valid( entity ) || !registry.all_of<scene::TransformComponent>( entity ) ||
                 !scene::HasCollider( registry, entity ) || registry.any_of<scene::CharacterControllerComponent, scene::SoftBodyComponent, scene::ClothComponent>( entity ) || ( mCompoundMembers.contains( entity ) && state.mID != 0 ) ) {
                removed.push_back( entity );
            }
        }

        for ( auto entity: removed ) {
            RemoveEntityBody( entity );
        }

        for ( auto entity: registry.view<scene::TransformComponent>() ) {
            if ( scene::HasCollider( registry, entity ) && !registry.any_of<scene::CharacterControllerComponent, scene::SoftBodyComponent, scene::ClothComponent>( entity ) && !mCompoundMembers.contains( entity ) ) {
                SynchronizeEntity( entity );
            }
        }

        SynchronizeVehicles();
        SynchronizeBridges();
        SynchronizeJoints();
        SynchronizeCharacters();
        SynchronizeSoftBodies();
        SynchronizeRagdolls();
    }

    auto PhysicsWorld::ReadColliderDescription( entt::entity entity, bool& trigger, eastl::vector<entt::entity>* ancestry ) -> ColliderShapeDescription {
        auto& registry{ mScene->GetRegistry() };
        auto& state{ mEntityBodies.at( entity ) };
        ColliderShapeDescription desc{};

        if ( const auto* vehicle{ registry.try_get<scene::VehicleComponent>( entity ) } ) {
            desc.mCenterOfMassOffset.y = vehicle->mCenterOfMassOffsetY;
        }

        if ( auto* collider{ registry.try_get<scene::CompoundColliderComponent>( entity ) } ) {
            desc.mKind = ColliderShapeKind::eCompound;
            desc.mOffset = collider->GetOffset();
            desc.mMutable = collider->GetMode() == scene::CompoundColliderMode::eMutable;
            trigger = collider->IsTrigger();
            eastl::vector<entt::entity> ancestors{};
            ReadCompound( entity, desc, ancestry ? *ancestry : ancestors );
        }
        else if ( auto* collider{ registry.try_get<scene::HeightFieldColliderComponent>( entity ) } ) {
            desc.mKind = ColliderShapeKind::eHeightField;
            trigger = collider->IsTrigger();
            ReadHeightField( entity, desc );
        }
        else if ( auto* collider{ registry.try_get<scene::PlaneColliderComponent>( entity ) } ) {
            desc.mKind = ColliderShapeKind::ePlane;
            desc.mDimensions = collider->GetNormal();
            desc.mPlaneDistance = collider->GetDistance();
            desc.mOffset = collider->GetOffset();
            trigger = collider->IsTrigger();
        }
        else if ( auto* collider{ registry.try_get<scene::CylinderColliderComponent>( entity ) } ) {
            desc.mKind = ColliderShapeKind::eCylinder;
            desc.mDimensions = { collider->GetRadius(), collider->GetHalfHeight(), 0 };
            desc.mOffset = collider->GetOffset();
            trigger = collider->IsTrigger();
        }
        else if ( auto* collider{ registry.try_get<scene::TaperedCapsuleColliderComponent>( entity ) } ) {
            desc.mKind = ColliderShapeKind::eTaperedCapsule;
            desc.mDimensions = { collider->GetTopRadius(), collider->GetHalfHeight(), collider->GetBottomRadius() };
            desc.mOffset = collider->GetOffset();
            trigger = collider->IsTrigger();
        }
        else if ( auto* collider{ registry.try_get<scene::TaperedCylinderColliderComponent>( entity ) } ) {
            desc.mKind = ColliderShapeKind::eTaperedCylinder;
            desc.mDimensions = { collider->GetTopRadius(), collider->GetHalfHeight(), collider->GetBottomRadius() };
            desc.mOffset = collider->GetOffset();
            trigger = collider->IsTrigger();
        }
        else if ( auto* collider = registry.try_get<scene::MeshColliderComponent>( entity ) ) {
            desc.mKind = ColliderShapeKind::eMesh;
            desc.mOffset = collider->GetOffset();
            desc.mIsConvex = collider->IsConvex();

            trigger = collider->IsTrigger();

            if ( auto* mesh = registry.try_get<scene::MeshComponent>( entity ); mesh && mesh->GetMesh() ) {
                state.mModel = mesh->GetModel();
                desc.mVertices = mesh->GetMesh()->GetCollisionVertices();
                desc.mIndices = mesh->GetMesh()->GetCollisionIndices();
            }
        }
        else if ( auto* collider{ registry.try_get<scene::CapsuleColliderComponent>( entity ) } ) {
            desc.mKind = ColliderShapeKind::eCapsule;
            desc.mDimensions = { collider->GetRadius(), collider->GetHalfHeight(), 0 };
            desc.mOffset = collider->GetOffset();
            trigger = collider->IsTrigger();
        }
        else if ( auto* collider{ registry.try_get<scene::SphereColliderComponent>( entity ) } ) {
            desc.mKind = ColliderShapeKind::eSphere;
            desc.mDimensions.x = collider->GetRadius();
            desc.mOffset = collider->GetOffset();
            trigger = collider->IsTrigger();
        }
        else if ( auto* collider{ registry.try_get<scene::BoxColliderComponent>( entity ) } ) {
            desc.mDimensions = collider->GetHalfExtents();
            desc.mOffset = collider->GetOffset();
            trigger = collider->IsTrigger();
        }

        return desc;
    }

    auto PhysicsWorld::DisableBody( entt::entity entity, const char* message ) -> void {
        auto& state{ mEntityBodies.at( entity ) };
        auto* body{ GetJoltBody( state.mID ) };
        auto* bodyInterface{ mSimulationInfo.mBodyInterface };
        auto* rb{ mScene->GetRegistry().try_get<scene::RigidBodyComponent>( entity ) };
        const bool changed = state.mError != message;

        if ( body ) {
            RemoveJoints( entity );
            RemoveBridges( entity );
            RemoveVehicle( entity );
            bodyInterface->RemoveBody( body->GetID() );
            bodyInterface->DestroyBody( body->GetID() );
            mBodies.erase( state.mID );
            body = nullptr;
        }

        state.mID = 0;
        state.mShapeValid = false;
        state.mShape.Reset();
        state.mError = message;

        if ( rb ) {
            rb->RemoveBodyID();
        }

        if ( changed ) {
            MKT_CORE_LOGGER_WARN( "Collider disabled: {}", state.mError.c_str() );
        }
    }

    auto PhysicsWorld::SynchronizeEntity( entt::entity entity ) -> void {
        MKT_BEGIN_PROFILER();
        auto& registry{ mScene->GetRegistry() };
        mEntityBodies.reserve( mEntityBodies.size() + registry.view<scene::TransformComponent>().size() );
        auto& tr{ registry.get<scene::TransformComponent>( entity ) };
        auto* rb{ registry.try_get<scene::RigidBodyComponent>( entity ) };
        auto& state{ mEntityBodies[entity] };
        auto* body{ GetJoltBody( state.mID ) };
        auto* bodyInterface{ mSimulationInfo.mBodyInterface };

        ColliderShapeDescription desc{};
        bool trigger{};

        try {
            desc = ReadColliderDescription( entity, trigger );
        } catch ( const std::exception& error ) {
            DisableBody( entity, error.what() );
            return;
        }

        core::float3 position{}, skew{};
        core::quat rotation{};
        core::float4 perspective{};
        const bool validTransform{ glm::decompose( tr.GetWorldTransform(), desc.mScale, rotation, position, skew, perspective ) };

        const auto motion{ rb ? GetJoltMotionType( rb->GetBodyType() ) : JPH::EMotionType::Static };
        const bool triangleMesh{ ( desc.mKind == ColliderShapeKind::eMesh && !desc.mIsConvex ) || desc.mKind == ColliderShapeKind::eHeightField || desc.mKind == ColliderShapeKind::ePlane };

        if ( !validTransform || glm::length2( skew ) > 0.000001f ) {
            DisableBody( entity, "Physics requires a transform without shear. Use uniform scale on rotated parents." );
            return;
        }

        if ( triangleMesh && motion != JPH::EMotionType::Static ) {
            DisableBody( entity, "Triangle mesh, plane and heightfield colliders require a static body." );
            return;
        }

        rotation = glm::normalize( rotation );
        const auto activation = motion == JPH::EMotionType::Static ? JPH::EActivation::DontActivate : JPH::EActivation::Activate;

        // Sensors must be in the moving broadphase layer to overlap kinematic bodies, even when static.
        const auto layer{ motion == JPH::EMotionType::Static && !trigger ? Layers::NON_MOVING : Layers::MOVING };
        const bool shapeChanged{ !state.mShapeValid || !( state.mDescription == desc ) };

        if ( shapeChanged ) {
            RemoveJoints( entity );
            RemoveBridges( entity );
            RemoveVehicle( entity );

            if ( state.mCookingFailed && state.mDescription == desc ) {
                return;
            }

            eastl::string cookingError{};
            auto cooked{ Shape::Create( desc, cookingError ) };

            if ( !cooked ) {
                state.mDescription = desc;
                DisableBody( entity, cookingError.c_str() );
                state.mCookingFailed = true;
                return;
            }

            state.mCookingFailed = false;

            if ( body ) {
                // Static triangle meshes have no motion properties. Recreate only when crossing that boundary.
                const bool wasTriangle{ body->GetShape()->MustBeStatic() };

                if ( triangleMesh != wasTriangle ) {
                    RemoveBridges( entity );
                    RemoveVehicle( entity );

                    bodyInterface->RemoveBody( body->GetID() );
                    bodyInterface->DestroyBody( body->GetID() );

                    mBodies.erase( state.mID );
                    body = nullptr;
                }
                else {
                    bodyInterface->SetShape( body->GetID(), cooked->GetNativeShape(), !triangleMesh, activation );
                }
            }

            if ( !body ) {
                JPH::BodyCreationSettings settings{ cooked->GetNativeShape(), GetFloat3F( position ), GetQuatF( rotation ), motion, layer };
                settings.mIsSensor = trigger;
                settings.mCollideKinematicVsNonDynamic = true;
                settings.mAllowDynamicOrKinematic = !triangleMesh;
                settings.mMotionQuality = JPH::EMotionQuality::LinearCast;

                if ( rb ) {
                    settings.mFriction = rb->GetFriction();
                    settings.mRestitution = rb->GetRestitution();
                    settings.mGravityFactor = rb->UseGravity() ? 1.0f : 0.0f;
                    settings.mLinearVelocity = GetFloat3F( rb->GetLinearVelocity() );
                    settings.mAngularVelocity = GetFloat3F( rb->GetAngularVelocity() );

                    if ( !triangleMesh ) {
                        settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
                        settings.mMassPropertiesOverride.mMass = rb->GetMass();
                    }
                }

                if ( auto* tag{ registry.try_get<scene::TagComponent>( entity ) } ) {
                    settings.mUserData = tag->GetGuid();
                }

                body = bodyInterface->CreateBody( settings );

                if ( !body ) {
                    DisableBody( entity, "Physics body capacity exceeded." );
                    return;
                }

                state.mID = GenerateBodyID();

                mBodies.emplace( state.mID, body );

                bodyInterface->AddBody( body->GetID(), activation );
                state.mPosition = position;
                state.mRotation = rotation;
                state.mLinearVelocity = rb ? rb->GetLinearVelocity() : core::float3{};
                state.mAngularVelocity = rb ? rb->GetAngularVelocity() : core::float3{};
            }

            state.mDescription = desc;
            state.mShape = eastl::move( cooked );
            state.mShapeValid = true;
        }

        SynchronizeBodyProperties( entity, position, rotation, trigger, shapeChanged );
    }

    auto PhysicsWorld::SynchronizeBodyProperties( entt::entity entity, const core::float3& position,
        const core::quat& rotation, bool trigger, bool shapeChanged ) -> void {
        auto& state{ mEntityBodies.at( entity ) };
        auto* body{ GetJoltBody( state.mID ) };
        auto* rb{ mScene->GetRegistry().try_get<scene::RigidBodyComponent>( entity ) };
        auto* bodyInterface{ mSimulationInfo.mBodyInterface };
        const auto motion{ rb ? GetJoltMotionType( rb->GetBodyType() ) : JPH::EMotionType::Static };
        const auto activation{ motion == JPH::EMotionType::Static ? JPH::EActivation::DontActivate : JPH::EActivation::Activate };
        const auto layer{ motion == JPH::EMotionType::Static && !trigger ? Layers::NON_MOVING : Layers::MOVING };
        state.mError.clear();
        const bool motionChanged{ body->GetMotionType() != motion };
        bool propertiesEdited{};

        if ( motionChanged ) {
            RemoveBridges( entity );
            RemoveVehicle( entity );
            bodyInterface->SetMotionType( body->GetID(), motion, activation );
        }

        if ( body->GetObjectLayer() != layer ) {
            bodyInterface->SetObjectLayer( body->GetID(), layer );
        }

        if ( body->IsSensor() != trigger ) {
            RemoveBridges( entity );
            RemoveVehicle( entity );
            body->SetIsSensor( trigger );// Main thread only; no simulation jobs are running here.
            bodyInterface->InvalidateContactCache( body->GetID() );
            propertiesEdited = true;
        }

        const bool poseEdited = glm::length2( position - state.mPosition ) > 0.00000001f ||
                                glm::abs( glm::dot( rotation, state.mRotation ) ) < 0.999999f;

        if ( motion == JPH::EMotionType::Static || poseEdited || motionChanged ) {
            bodyInterface->SetPositionAndRotationWhenChanged( body->GetID(), GetFloat3F( position ), GetQuatF( rotation ), activation );
        }

        if ( rb ) {
            rb->SetBodyID( state.mID );

            if ( motion != JPH::EMotionType::Static ) {
                if ( rb->GetLinearVelocity() != state.mLinearVelocity || motionChanged ) {
                    bodyInterface->SetLinearVelocity( body->GetID(), GetFloat3F( rb->GetLinearVelocity() ) );
                }

                if ( rb->GetAngularVelocity() != state.mAngularVelocity || motionChanged ) {
                    bodyInterface->SetAngularVelocity( body->GetID(), GetFloat3F( rb->GetAngularVelocity() ) );
                }

                const core::f32 gravityFactor{ rb->UseGravity() ? 1.0f : 0.0f };

                if ( body->GetMotionProperties()->GetGravityFactor() != gravityFactor ) {
                    bodyInterface->SetGravityFactor( body->GetID(), gravityFactor );
                    propertiesEdited = true;
                }
            }

            if ( motion == JPH::EMotionType::Dynamic && ( shapeChanged || motionChanged ||
                                                            glm::abs( body->GetMotionProperties()->GetInverseMass() - 1.0f / rb->GetMass() ) > 0.0000001f ) ) {
                auto massProperties = body->GetShape()->GetMassProperties();
                massProperties.ScaleToMass( rb->GetMass() );
                body->GetMotionProperties()->SetMassProperties( body->GetMotionProperties()->GetAllowedDOFs(), massProperties );
                propertiesEdited = true;
            }

            propertiesEdited |= body->GetFriction() != rb->GetFriction() || body->GetRestitution() != rb->GetRestitution();
            bodyInterface->SetFriction( body->GetID(), rb->GetFriction() );
            bodyInterface->SetRestitution( body->GetID(), rb->GetRestitution() );
            state.mLinearVelocity = rb->GetLinearVelocity();
            state.mAngularVelocity = rb->GetAngularVelocity();
        }

        // Let settled bodies sleep; edits that change their response must wake them.
        if ( propertiesEdited && motion != JPH::EMotionType::Static ) {
            bodyInterface->ActivateBody( body->GetID() );
        }

        state.mPosition = position;
        state.mRotation = rotation;
        state.mTrigger = trigger;
    }

    auto PhysicsWorld::PostUpdate() -> void {
        MKT_BEGIN_PROFILER();
        auto& registry = mScene->GetRegistry();

        // Set all simulated world poses before converting children back to local space.
        ankerl::unordered_dense::map<entt::entity, core::float4x4> poses{};

        for ( auto [entity, rb, tr]: registry.view<scene::RigidBodyComponent, scene::TransformComponent>().each() ) {
            if ( !rb.IsValidBodyID() || rb.IsBodyType( scene::RigidBodyComponent::BodyType::eStatic ) ) {
                continue;
            }

            auto* body = GetJoltBody( rb.GetBodyID() );

            if ( !body ) {
                continue;
            }

            auto& state = mEntityBodies.at( entity );
            state.mPosition = GetFloat3F( body->GetPosition() );// Entity origin, never center of mass.
            state.mRotation = GetQuatF( body->GetRotation() );
            poses.emplace( entity, glm::translate( core::float4x4{ 1 }, state.mPosition ) * glm::mat4_cast( state.mRotation ) * glm::scale( core::float4x4{ 1 }, state.mDescription.mScale ) );
            state.mLinearVelocity = GetFloat3F( body->GetLinearVelocity() );
            state.mAngularVelocity = GetFloat3F( body->GetAngularVelocity() );
            rb.SetLinearVelocity( state.mLinearVelocity );
            rb.SetAngularVelocity( state.mAngularVelocity );
        }

        // Resolve the new world transform of intermediate (nonphysical) parents too.
        auto worldPose = [&]( auto&& self, scene::Entity* entity ) -> core::float4x4 {
            if ( auto it = poses.find( entity->Get() ); it != poses.end() ) {
                return it->second;
            }

            auto local = entity->GetComponent<scene::TransformComponent>().GetTransform();
            const auto& relation = entity->GetComponent<scene::RelationComponent>();

            if ( relation.HasParent() ) {
                if ( auto* parent = mScene->FindByID( *relation.GetParent() ) ) {
                    return self( self, parent ) * local;
                }
            }

            return local;
        };

        for ( const auto& [entity, pose]: poses ) {
            auto local = pose;

            if ( auto* relation = registry.try_get<scene::RelationComponent>( entity ); relation && relation->HasParent() ) {
                if ( auto* parent = mScene->FindByID( *relation->GetParent() ) ) {
                    local = glm::inverse( worldPose( worldPose, parent ) ) * pose;
                }
            }

            registry.get<scene::TransformComponent>( entity ).SetTransform( local );
        }

        mScene->UpdateWorldTransformations();
    }

    auto PhysicsWorld::GenerateBodyID() -> core::u64 {
        MKT_BEGIN_PROFILER();

        return mBodyIdCounter.fetch_add( 1, std::memory_order_relaxed ) + 1;
    }

    auto PhysicsWorld::GetJoltBody( core::u64 id ) -> JPH::Body* {
        MKT_BEGIN_PROFILER();
        const auto it = mBodies.find( id );

        return it == mBodies.end() ? nullptr : it->second;
    }

    auto PhysicsWorld::RemoveEntityBody( entt::entity entity ) -> void {
        RemoveJoints( entity );
        RemoveBridges( entity );
        RemoveVehicle( entity );
        MKT_BEGIN_PROFILER();
        const auto it = mEntityBodies.find( entity );

        if ( it != mEntityBodies.end() ) {
            if ( auto* body = GetJoltBody( it->second.mID ) ) {
                mSimulationInfo.mBodyInterface->RemoveBody( body->GetID() );
                mSimulationInfo.mBodyInterface->DestroyBody( body->GetID() );
                mBodies.erase( it->second.mID );
            }

            mEntityBodies.erase( it );
        }

        auto& registry = mScene->GetRegistry();

        if ( registry.valid( entity ) ) {
            if ( auto* rb = registry.try_get<scene::RigidBodyComponent>( entity ) ) {
                rb->RemoveBodyID();
            }
        }
    }

    auto PhysicsWorld::ClearBodies() -> void {
        MKT_BEGIN_PROFILER();

        while ( !mJoints.empty() ) {
            RemoveJoint( mJoints.begin()->first );
        }

        while ( !mCharacters.empty() ) {
            RemoveCharacter( mCharacters.begin()->first );
        }

        while ( !mSoftBodies.empty() ) {
            RemoveSoftBody( mSoftBodies.begin()->first );
        }

        mRagdolls.clear();

        while ( !mEntityBodies.empty() ) {
            RemoveEntityBody( mEntityBodies.begin()->first );
        }

        mSimulationInfo.mContactListener.Clear();
        mReportedUpdateErrors = JPH::EPhysicsUpdateError::None;
    }

    auto PhysicsWorld::GetColliderError( entt::entity entity ) const -> const char* {
        MKT_BEGIN_PROFILER();

        if ( const auto found{ mJoints.find( entity ) }; found != mJoints.end() && !found->second.mError.empty() ) {
            return found->second.mError.c_str();
        }

        if ( const auto found{ mCharacters.find( entity ) }; found != mCharacters.end() && !found->second.mError.empty() ) {
            return found->second.mError.c_str();
        }

        if ( const auto found{ mSoftBodies.find( entity ) }; found != mSoftBodies.end() && !found->second.mError.empty() ) {
            return found->second.mError.c_str();
        }

        if ( const auto found{ mRagdolls.find( entity ) }; found != mRagdolls.end() && !found->second.mError.empty() ) {
            return found->second.mError.c_str();
        }

        const auto it = mEntityBodies.find( entity );

        return it == mEntityBodies.end() ? "" : it->second.mError.c_str();
    }

    auto PhysicsWorld::RemoveRigidBody( scene::Entity* entity ) -> void {
        MKT_BEGIN_PROFILER();

        if ( entity ) {
            RemoveEntityBody( entity->Get() );
        }
    }

    auto PhysicsWorld::RemoveColliderBody( scene::Entity* entity ) -> void {
        MKT_BEGIN_PROFILER();
        RemoveRigidBody( entity );
    }

    auto PhysicsWorld::AddRigidBody( scene::Entity* entity ) -> void {
        MKT_BEGIN_PROFILER();
        AddCollider( entity );
    }

    auto PhysicsWorld::AddCollider( scene::Entity* entity ) -> void {
        MKT_BEGIN_PROFILER();

        // Creation callbacks run before a newly created entity is registered in the scene.
        // Synchronize after all ECS mutations have finished, never recurse into a signal.
        if ( entity ) {
            if ( auto it = mEntityBodies.find( entity->Get() ); it != mEntityBodies.end() ) {
                it->second.mShapeValid = false;
                it->second.mCookingFailed = false;
            }
        }
    }

    auto PhysicsWorld::SetGravity( const core::float3& gravity ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mGravity == gravity ) {
            return;
        }

        mGravity = gravity;
        mSimulationInfo.mPhysicsSystem.SetGravity( JPH::Vec3( mGravity.x, mGravity.y, mGravity.z ) );

        for ( const auto& [id, body]: mBodies ) {
            if ( body->IsDynamic() ) {
                mSimulationInfo.mBodyInterface->ActivateBody( body->GetID() );
            }
        }
    }

    auto PhysicsWorld::SetGravityBody( GravityBody body ) -> void {
        MKT_BEGIN_PROFILER();
        mGravityBody = body;
        SetGravity( GetGravityFor( body ) );
    }

    auto PhysicsWorld::GetGravityBody() const -> GravityBody {
        MKT_BEGIN_PROFILER();

        return mGravityBody;
    }

    auto PhysicsWorld::GetGravityFor( const GravityBody body ) -> core::float3 {
        MKT_BEGIN_PROFILER();

        // https://en.wikipedia.org/wiki/List_of_gravitationally_rounded_objects_of_the_Solar_System
        switch ( body ) {
            case GravityBody::eEarth:
                return { 0.0f, -9.80665f, 0.0f };
            case GravityBody::eMoon:
                return { 0.0f, -1.62f, 0.0f };
            case GravityBody::eMars:
                return { 0.0f, -3.721f, 0.0f };
            case GravityBody::eJupiter:
                return { 0.0f, -24.79f, 0.0f };

            default:
                return { 0.0f, -9.80665f, 0.0f };// fallback: Earth
        }
    }

    auto PhysicsWorld::Create( const PhysicsWorldCreateInfo& spec ) -> eastl::unique_ptr<PhysicsWorld> {
        MKT_BEGIN_PROFILER();

        return eastl::make_unique<PhysicsWorld>( spec );
    }

    auto PhysicsWorld::GetFloat4x4F( const JPH::RMat44& m ) -> core::float4x4 {
        MKT_BEGIN_PROFILER();
        core::float4x4 out{};

        for ( core::i32 c{}; c < 4; ++c ) {
            // RMat44 stores row-major internally,
            // Jolt provides columns.
            JPH::Vec4 col{ m.GetColumn4( c ) };
            out[c][0] = col.GetX();
            out[c][1] = col.GetY();
            out[c][2] = col.GetZ();
            out[c][3] = col.GetW();
        }

        return out;
    }

    auto PhysicsWorld::GetFloat3F( const JPH::Vec3& v ) -> core::float3 {
        MKT_BEGIN_PROFILER();

        return core::float3{ v.GetX(), v.GetY(), v.GetZ() };
    }

    auto PhysicsWorld::GetQuatF( const JPH::Quat& q ) -> core::quat {
        MKT_BEGIN_PROFILER();

        return core::quat{ q.GetW(), q.GetX(), q.GetY(), q.GetZ() };
    }

    auto PhysicsWorld::GetFloat3F( const core::float3& v ) -> JPH::Vec3 {
        MKT_BEGIN_PROFILER();

        return JPH::Vec3{ v.x, v.y, v.z };
    }

    auto PhysicsWorld::GetQuatF( const core::float3& vec3EulerAnglesGLM ) -> JPH::Quat {
        MKT_BEGIN_PROFILER();
        core::quat gq{ core::quat{ vec3EulerAnglesGLM } };// requires GLM_FORCE_RADIANS
        return GetQuatF( gq );
    }

    auto PhysicsWorld::GetQuatF( const core::quat& q ) -> JPH::Quat {
        MKT_BEGIN_PROFILER();

        // glm stores (w, x, y, z)
        return JPH::Quat{ q.x, q.y, q.z, q.w };
    }

    auto ObjectLayerPairFilterImpl::ShouldCollide( JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2 ) const -> bool {
        MKT_BEGIN_PROFILER();

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

    auto ObjectVsBroadPhaseLayerFilterImpl::ShouldCollide( JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2 ) const -> bool {
        MKT_BEGIN_PROFILER();

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

    BPLayerInterfaceImpl::BPLayerInterfaceImpl() {
        MKT_BEGIN_PROFILER();

        // Create a mapping table from object to broad phase layer
        mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
    }

    auto BPLayerInterfaceImpl::GetNumBroadPhaseLayers() const -> core::u32 {
        MKT_BEGIN_PROFILER();

        return BroadPhaseLayers::NUM_LAYERS;
    }

    auto BPLayerInterfaceImpl::GetBroadPhaseLayer( JPH::ObjectLayer inLayer ) const -> JPH::BroadPhaseLayer {
        MKT_BEGIN_PROFILER();
        JPH_ASSERT( inLayer < Layers::NUM_LAYERS );

        return mObjectToBroadPhase[inLayer];
    }

    auto DebugBodyActivationListener::OnBodyActivated( const JPH::BodyID& inBodyID, JPH::uint64 inBodyUserData ) -> void {
        MKT_BEGIN_PROFILER();
        MKT_CORE_LOGGER_DEBUG( "A body got activated" );
    }

    auto DebugBodyActivationListener::OnBodyDeactivated( const JPH::BodyID& inBodyID, JPH::uint64 inBodyUserData ) -> void {
        MKT_BEGIN_PROFILER();
        MKT_CORE_LOGGER_DEBUG( "A body got activated" );
    }

    auto DebugContactListener::OnContactValidate( const JPH::Body& inBody1, const JPH::Body& inBody2, JPH::RVec3Arg inBaseOffset, const JPH::CollideShapeResult& inCollisionResult ) -> JPH::ValidateResult {
        MKT_BEGIN_PROFILER();
        MKT_CORE_LOGGER_DEBUG( "Contact validate callback" );

        // Allows you to ignore a contact before it is created (using layers to not make objects collide is cheaper!)
        return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
    }

    auto DebugContactListener::OnContactAdded( const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings ) -> void {
        MKT_BEGIN_PROFILER();
        MKT_CORE_LOGGER_DEBUG( "A contact was added" );
    }

    auto DebugContactListener::OnContactPersisted( const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings ) -> void {
        MKT_BEGIN_PROFILER();
        MKT_CORE_LOGGER_DEBUG( "A contact was persisted" );
    }

    auto DebugContactListener::OnContactRemoved( const JPH::SubShapeIDPair& inSubShapePair ) -> void {
        MKT_BEGIN_PROFILER();
        MKT_CORE_LOGGER_DEBUG( "A contact was removed" );
    }

    auto PhysicsWorld::GetBodyCount() const -> core::usize {
        MKT_BEGIN_PROFILER();

        return mSimulationInfo.mPhysicsSystem.GetNumBodies();
    }

    auto PhysicsWorld::GetScene() const -> scene::Scene* {
        MKT_BEGIN_PROFILER();

        return mScene;
    }

    auto PhysicsWorld::TakeContactEvents() -> eastl::vector<ContactEvent> {
        MKT_BEGIN_PROFILER();

        return mSimulationInfo.mContactListener.TakeEvents();
    }

    auto PhysicsWorld::GetDroppedContactEventCount() const -> core::usize {
        MKT_BEGIN_PROFILER();

        return mSimulationInfo.mContactListener.GetDroppedEventCount();
    }

    auto PhysicsWorld::GetGravity() const -> const core::float3& {
        return mGravity;
    }

    auto PhysicsWorld::RemoveBridges( entt::entity entity ) -> void {
        for ( auto it{ mBridges.begin() }; it != mBridges.end(); ) {
            auto& state{ it->second };

            if ( it->first != entity && eastl::find( state.mSegments.begin(), state.mSegments.end(), entity ) == state.mSegments.end() ) {
                ++it;
                continue;
            }

            // Constraints borrow bodies. Remove every link before any plank is
            // destroyed or recooked, and restore its normal collision group.
            for ( const auto& constraint: state.mConstraints ) {
                mSimulationInfo.mPhysicsSystem.RemoveConstraint( constraint );
            }

            for ( const auto segment: state.mSegments ) {
                const auto bodyState{ mEntityBodies.find( segment ) };

                if ( bodyState != mEntityBodies.end() ) {
                    if ( auto* body{ GetJoltBody( bodyState->second.mID ) } ) {
                        body->SetCollisionGroup( JPH::CollisionGroup{} );
                    }
                }
            }

            it = mBridges.erase( it );
        }
    }

    auto PhysicsWorld::SynchronizeBridges() -> void {
        auto& registry{ mScene->GetRegistry() };
        ankerl::unordered_dense::map<entt::entity, eastl::vector<entt::entity>> groups{};

        for ( const auto [entity, segment, relation]: registry.view<scene::BridgeSegmentComponent, scene::RelationComponent>().each() ) {
            if ( !relation.HasParent() ) {
                continue;
            }

            auto* parent{ mScene->FindByID( *relation.GetParent() ) };
            const auto found{ mEntityBodies.find( entity ) };

            if ( !parent || found == mEntityBodies.end() || !found->second.mShapeValid ||
                 found->second.mDescription.mKind != ColliderShapeKind::eBox ) {
                continue;
            }

            groups[parent->Get()].push_back( entity );
        }

        for ( auto& [parent, segments]: groups ) {
            eastl::sort( segments.begin(), segments.end(), [&]( entt::entity a, entt::entity b ) {
                return registry.get<scene::BridgeSegmentComponent>( a ).mIndex < registry.get<scene::BridgeSegmentComponent>( b ).mIndex;
            } );
        }

        eastl::vector<entt::entity> stale{};

        for ( const auto& [parent, state]: mBridges ) {
            const auto group{ groups.find( parent ) };

            if ( group == groups.end() || group->second != state.mSegments ) {
                stale.push_back( parent );
            }
        }

        for ( const auto parent: stale ) {
            RemoveBridges( parent );
        }

        for ( const auto& [parent, segments]: groups ) {
            if ( segments.size() < 2 || mBridges.contains( parent ) ) {
                continue;
            }

            bool contiguous{ true };

            for ( core::u32 index{}; index < segments.size(); ++index ) {
                contiguous &= registry.get<scene::BridgeSegmentComponent>( segments[index] ).mIndex == index;
            }

            if ( !contiguous ) {
                continue;
            }

            BridgeState state{};
            state.mSegments = segments;
            JPH::Ref<JPH::GroupFilterTable> filter{ new JPH::GroupFilterTable{ static_cast<JPH::uint>( segments.size() ) } };

            for ( core::u32 index{}; index + 1 < segments.size(); ++index ) {
                filter->DisableCollision( index, index + 1 );
            }

            JPH::Body* previous{};

            for ( core::u32 index{}; index < segments.size(); ++index ) {
                const auto& bodyState{ mEntityBodies.at( segments[index] ) };
                auto* body{ GetJoltBody( bodyState.mID ) };
                body->SetCollisionGroup( JPH::CollisionGroup{ filter, entt::to_integral( parent ), index } );

                if ( previous ) {
                    // Jolt's VehicleTest::CreateBridge uses two distance links
                    // per seam. Zero rest length joins the matching deck corners
                    // while allowing the planks to flex under the car.
                    const auto& previousShape{ mEntityBodies.at( segments[index - 1] ).mDescription };
                    const core::float3 a{ previousShape.mDimensions * glm::abs( previousShape.mScale ) };
                    const core::float3 b{ bodyState.mDescription.mDimensions * glm::abs( bodyState.mDescription.mScale ) };

                    for ( const float side: { -1.0f, 1.0f } ) {
                        JPH::DistanceConstraintSettings link{};
                        link.mPoint1 = previous->GetWorldTransform() * JPH::Vec3{ side * a.x, 0, a.z };
                        link.mPoint2 = body->GetWorldTransform() * JPH::Vec3{ side * b.x, 0, -b.z };
                        link.mMinDistance = 0.0f;
                        link.mMaxDistance = 0.0f;
                        JPH::Ref<JPH::Constraint> constraint{ link.Create( *previous, *body ) };
                        mSimulationInfo.mPhysicsSystem.AddConstraint( constraint );
                        state.mConstraints.push_back( constraint );
                    }
                }

                previous = body;
            }

            mBridges.emplace( parent, eastl::move( state ) );
        }
    }

    auto PhysicsWorld::RemoveVehicle( entt::entity entity ) -> void {
        const auto it{ mVehicles.find( entity ) };

        if ( it == mVehicles.end() ) {
            return;
        }

        // Both registrations retain access to the chassis. Detach before any
        // collider replacement, scene teardown, or rigid-body destruction.
        auto* constraint{ it->second.mConstraint.GetPtr() };

        if ( constraint ) {
            mSimulationInfo.mPhysicsSystem.RemoveStepListener( constraint );
            mSimulationInfo.mPhysicsSystem.RemoveConstraint( constraint );
        }

        for ( auto& wheel: it->second.mWheelConstraints ) {
            mSimulationInfo.mPhysicsSystem.RemoveConstraint( wheel );
        }

        for ( auto wheel: it->second.mWheelBodies ) {
            mSimulationInfo.mBodyInterface->RemoveBody( wheel );
            mSimulationInfo.mBodyInterface->DestroyBody( wheel );
        }

        it->second.mChassis->SetCollisionGroup( it->second.mOriginalCollisionGroup );
        mVehicles.erase( it );
    }

    auto PhysicsWorld::SynchronizeVehicles() -> void {
        auto& registry{ mScene->GetRegistry() };
        eastl::vector<entt::entity> removed{};

        for ( const auto& [entity, state]: mVehicles ) {
            if ( !registry.valid( entity ) || !registry.all_of<scene::VehicleComponent, scene::RigidBodyComponent>( entity ) ) {
                removed.push_back( entity );
            }
        }

        for ( const auto entity: removed ) {
            RemoveVehicle( entity );
        }

        for ( auto [entity, vehicle, rigidBody, transform]: registry.view<scene::VehicleComponent, scene::RigidBodyComponent, scene::TransformComponent>().each() ) {
            auto* body{ rigidBody.IsValidBodyID() ? GetJoltBody( rigidBody.GetBodyID() ) : nullptr };
            const auto& world{ transform.GetWorldTransform() };
            const bool unitScale{ glm::abs( glm::length( core::float3{ world[0] } ) - 1.0f ) < 0.001f &&
                                  glm::abs( glm::length( core::float3{ world[1] } ) - 1.0f ) < 0.001f &&
                                  glm::abs( glm::length( core::float3{ world[2] } ) - 1.0f ) < 0.001f && glm::determinant( world ) > 0.0f };

            if ( !body || !body->IsDynamic() || body->IsSensor() || !unitScale || !vehicle.IsValid() ) {
                RemoveVehicle( entity );
                continue;
            }

            auto it{ mVehicles.find( entity ) };

            if ( it != mVehicles.end() && ( it->second.mChassis != body || !vehicle.HasSameSettings( it->second.mSettings ) ) ) {
                RemoveVehicle( entity );
                it = mVehicles.end();
            }

            if ( it == mVehicles.end() ) {
                VehicleState state{};
                state.mChassis = body;
                state.mOriginalCollisionGroup = body->GetCollisionGroup();
                state.mSettings = vehicle;

                if ( vehicle.GetVehicleType() != scene::VehicleType::eSixDof ) {
                    state.mConstraint = CreateVehicleConstraint( *body, vehicle );
                }

                it = mVehicles.emplace( entity, eastl::move( state ) ).first;

                if ( it->second.mConstraint ) {
                    mSimulationInfo.mPhysicsSystem.AddConstraint( it->second.mConstraint );
                    mSimulationInfo.mPhysicsSystem.AddStepListener( it->second.mConstraint );
                }
                else {
                    CreateSixDofVehicle( entity, *body, vehicle );
                }
            }

            const auto input = []( core::f32 value, core::f32 minimum ) {
                return glm::clamp( value, minimum, 1.0f );
            };
            const core::f32 throttle{ input( vehicle.mThrottle, -1.0f ) };
            const core::f32 steer{ input( vehicle.mSteer, -1.0f ) };
            const core::f32 brake{ input( vehicle.mBrake, 0.0f ) };
            const core::f32 handBrake{ input( vehicle.mHandBrake, 0.0f ) };

            // A sleeping chassis does not run the vehicle solver. Driver input
            // must wake it, just as in Jolt's sample, including steering at rest.
            if ( throttle != 0.0f || steer != 0.0f || brake != 0.0f || handBrake != 0.0f ) {
                mSimulationInfo.mBodyInterface->ActivateBody( body->GetID() );
            }

            UpdateVehicleInputs( entity, vehicle );
        }
    }

    auto PhysicsWorld::UpdateVehicleWheels() -> void {
        if ( mVehicles.empty() ) {
            return;
        }

        auto& registry{ mScene->GetRegistry() };

        for ( auto [entity, wheel, relation, transform]: registry.view<scene::VehicleWheelComponent, scene::RelationComponent, scene::TransformComponent>().each() ) {
            if ( !relation.HasParent() ) {
                continue;
            }

            auto* parent{ mScene->FindByID( *relation.GetParent() ) };

            if ( !parent ) {
                continue;
            }

            const auto it{ mVehicles.find( parent->Get() ) };

            if ( it != mVehicles.end() ) {
                const auto& state{ it->second };
                JPH::RMat44 pose{};

                if ( state.mConstraint && wheel.mIndex < state.mConstraint->GetWheels().size() ) {
                    pose = state.mConstraint->GetWheelWorldTransform( wheel.mIndex, JPH::Vec3::sAxisX(), JPH::Vec3::sAxisY() );
                }
                else if ( wheel.mIndex < state.mWheelBodies.size() ) {
                    // Cylinder bodies use local Y as axle; visual wheel bindings use local X.
                    pose = mSimulationInfo.mBodyInterface->GetWorldTransform( state.mWheelBodies[wheel.mIndex] ) * JPH::Mat44::sRotationZ( -.5f * JPH::JPH_PI );
                }
                else {
                    continue;
                }

                if ( !wheel.mFollowSpin ) {
                    // Jolt's wheel basis excludes axle rotation, so brake calipers
                    // retain steering and suspension without spinning with the rim.
                    JPH::Vec3 forward{}, up{}, right{};

                    if ( state.mConstraint ) {
                        state.mConstraint->GetWheelLocalBasis( state.mConstraint->GetWheel( wheel.mIndex ), forward, up, right );
                        const auto chassis{ state.mChassis->GetWorldTransform() };
                        right = chassis.Multiply3x3( right );
                        up = chassis.Multiply3x3( up );
                        forward = -chassis.Multiply3x3( forward );
                    }
                    else {
                        // Physical cylinder wheels supply their axle directly.
                        right = pose.GetAxisX();
                        const auto chassisUp{ state.mChassis->GetRotation() * JPH::Vec3::sAxisY() };
                        forward = right.Cross( chassisUp ).NormalizedOr( pose.GetAxisZ() );
                        up = forward.Cross( right );
                    }

                    pose = JPH::RMat44{ JPH::Vec4{ right, 0 }, JPH::Vec4{ up, 0 }, JPH::Vec4{ forward, 0 }, pose.GetTranslation() };
                }

                transform.SetTransform( glm::inverse( parent->GetComponent<scene::TransformComponent>().GetWorldTransform() ) * GetFloat4x4F( pose ) * wheel.mVisualCorrection );
            }
        }

        // Entities transform have changed update hierarchy
        mScene->UpdateWorldTransformations();
    }

    auto ContactEventListener::OnContactAdded( const JPH::Body& a, const JPH::Body& b, const JPH::ContactManifold& contact, JPH::ContactSettings& settings ) -> void {
        MKT_BEGIN_PROFILER();
        std::lock_guard lock{ mMutex };
        ContactEvent event{ ContactEvent::Type::eEnter, a.GetUserData(), b.GetUserData(), settings.mIsSensor };
        mContacts[JPH::SubShapeIDPair{ a.GetID(), contact.mSubShapeID1, b.GetID(), contact.mSubShapeID2 }] = event;
        Push( event );
    }

    auto ContactEventListener::OnContactRemoved( const JPH::SubShapeIDPair& pair ) -> void {
        MKT_BEGIN_PROFILER();
        std::lock_guard lock{ mMutex };

        if ( auto it = mContacts.find( pair ); it != mContacts.end() ) {
            auto event = it->second;
            event.mType = ContactEvent::Type::eExit;
            Push( event );
            mContacts.erase( it );
        }
    }

    auto ContactEventListener::TakeEvents() -> eastl::vector<ContactEvent> {
        MKT_BEGIN_PROFILER();
        std::lock_guard lock{ mMutex };

        eastl::vector<ContactEvent> result{};
        result.swap( mEvents );

        return result;
    }

    auto ContactEventListener::GetDroppedEventCount() const -> usize {
        MKT_BEGIN_PROFILER();
        std::lock_guard lock{ mMutex };

        return mDropped;
    }

    auto ContactEventListener::Clear() -> void {
        MKT_BEGIN_PROFILER();
        std::lock_guard lock{ mMutex };
        mEvents.clear();
        mContacts.clear();
        mDropped = 0;
    }

    auto ContactEventListener::Push( const ContactEvent& event ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mEvents.size() < 16384 ) {
            mEvents.push_back( event );
        }
        else {
            ++mDropped;
        }
    }

    auto PhysicsWorld::ReadHeightField( entt::entity entity, ColliderShapeDescription& desc ) -> void {
        auto& registry{ mScene->GetRegistry() };
        auto& state{ mEntityBodies.at( entity ) };
        const auto& collider{ registry.get<scene::HeightFieldColliderComponent>( entity ) };
        scene::LandscapeComponent* landscape{};

        if ( collider.GetLandscapeEntityID() ) {
            auto* source{ mScene->FindByID( collider.GetLandscapeEntityID() ) };

            if ( !source || !source->HasComponent<scene::LandscapeComponent>() ) {
                MKT_THROW_RUNTIME_ERROR( "Heightfield references a missing landscape." );
            }

            landscape = &source->GetComponent<scene::LandscapeComponent>();
        }

        const auto map{ landscape ? landscape->GetHeightmap() : nullptr };
        const auto revision{ landscape ? landscape->GetRevision() : 0 };
        const bool changed{ !state.mHeightSamples || state.mHeightRevision != collider.GetRevision() || state.mLandscapeRevision != revision ||
                            state.mHeightAssetID != collider.GetHeightmap().GetID() || ( landscape && state.mHeightmap != map ) };
        renderer::WorldDescription terrain{};
        core::u32 width{}, height{};

        if ( landscape ) {
            terrain = landscape->GetWorldDescription();
            width = terrain.GetResolutionX();
            height = terrain.GetResolutionY();
        }

        if ( changed ) {
            state.mHeightmap = map;

            if ( !landscape && !collider.GetHeightmap().GetPath().IsEmpty() ) {
                state.mHeightmap = eastl::make_shared<renderer::TerrainHeightmap>( asset::ProcessImage2D( collider.GetHeightmap().GetPath(), true ) );
            }
        }

        if ( !landscape ) {
            if ( !state.mHeightmap ) {
                MKT_THROW_RUNTIME_ERROR( "Heightfield needs a landscape or heightmap." );
            }

            width = state.mHeightmap->mWidth;
            height = state.mHeightmap->mHeight;
        }

        const auto count{ eastl::max( 4u, eastl::max( width, height ) ) };

        if ( count > 4096 || width < 2 || height < 2 ) {
            MKT_THROW_RUNTIME_ERROR( "Heightfield dimensions must be between 2 and 4096 samples." );
        }

        if ( changed ) {
            // HeightFieldShapeTest: square numerical grid, with no-collision samples padding a rectangle.
            auto samples{ eastl::make_shared<eastl::vector<core::f32>>( core::usize{ count } * count, JPH::HeightFieldShapeConstants::cNoCollisionValue ) };

            for ( core::u32 z{}; z < height; ++z ) {
                for ( core::u32 x{}; x < width; ++x ) {
                    const auto uv{ core::float2{ static_cast<float>( x ) / ( width - 1 ), static_cast<float>( z ) / ( height - 1 ) } };
                    ( *samples )[core::usize{ z } * count + x] = state.mHeightmap ? state.mHeightmap->Sample( uv ) : 0.0f;
                }
            }

            state.mHeightSamples = eastl::move( samples );
            state.mHeightRevision = collider.GetRevision();
            state.mLandscapeRevision = revision;
            state.mHeightAssetID = collider.GetHeightmap().GetID();
        }

        desc.mHeights = state.mHeightSamples;
        desc.mSampleCount = count;
        desc.mDimensions = landscape ? core::float3{ terrain.mVertexSpacing, terrain.mHeightScale, terrain.mVertexSpacing }
                                     : core::float3{ collider.GetSampleSpacing().x, collider.GetHeightScale(), collider.GetSampleSpacing().y };
        desc.mOffset = collider.GetOffset() + ( landscape ? core::float3{ terrain.mOrigin.x, terrain.mBaseElevation, terrain.mOrigin.y } : core::float3{ 0, collider.GetBaseElevation(), 0 } );
    }

    auto PhysicsWorld::ReadCompound( entt::entity entity, ColliderShapeDescription& desc, eastl::vector<entt::entity>& ancestors ) -> void {
        if ( ancestors.size() >= 32 || eastl::find( ancestors.begin(), ancestors.end(), entity ) != ancestors.end() ) {
            MKT_THROW_RUNTIME_ERROR( "Compound collider references form a cycle or exceed 32 levels." );
        }

        ancestors.push_back( entity );
        auto& registry{ mScene->GetRegistry() };
        const auto& compound{ registry.get<scene::CompoundColliderComponent>( entity ) };
        const auto inverse{ glm::inverse( registry.get<scene::TransformComponent>( entity ).GetWorldTransform() ) };

        for ( auto guid: compound.GetColliderEntities() ) {
            auto* child{ mScene->FindByID( guid ) };

            if ( !child || !child->HasComponent<scene::TransformComponent>() || !scene::HasCollider( registry, child->Get() ) ) {
                MKT_THROW_RUNTIME_ERROR( "Compound references a missing collider." );
            }

            auto& state{ mEntityBodies[child->Get()] };
            bool trigger{};
            auto childDesc{ ReadColliderDescription( child->Get(), trigger, &ancestors ) };

            if ( trigger ) {
                MKT_THROW_RUNTIME_ERROR( "Compound children cannot be independent sensors." );
            }

            const auto local{ inverse * child->GetComponent<scene::TransformComponent>().GetWorldTransform() };
            core::float3 position{}, skew{};
            core::float4 perspective{};
            core::quat rotation{};

            if ( !glm::decompose( local, childDesc.mScale, rotation, position, skew, perspective ) || glm::length2( skew ) > .000001f ) {
                MKT_THROW_RUNTIME_ERROR( "Compound child transform contains shear." );
            }

            if ( !state.mShape || !( state.mDescription == childDesc ) ) {
                eastl::string error{};
                state.mShape = Shape::Create( childDesc, error );

                if ( !state.mShape ) {
                    MKT_THROW_RUNTIME_ERROR( error.c_str() );
                }

                if ( state.mShape->GetNativeShape()->MustBeStatic() ) {
                    MKT_THROW_RUNTIME_ERROR( "Compound children must have volume; use a separate static heightfield, plane or triangle mesh." );
                }

                state.mDescription = childDesc;
            }

            desc.mParts.push_back( { state.mShape, position, glm::normalize( rotation ) } );
        }

        ancestors.pop_back();
    }


    auto PhysicsWorld::CreateSixDofVehicle( entt::entity entity, JPH::Body& chassis, const scene::VehicleComponent& vehicle ) -> void {
        // VehicleSixDOFTest.cpp: physical cylinder wheels, suspension on Y and drive on X.
        // https://github.com/jrouwe/JoltPhysics/blob/e77f175595e64cb44218cc9d9d56fc365ad0e36a/Samples/Tests/Vehicle/VehicleSixDOFTest.cpp
        auto& state{ mVehicles.at( entity ) };
        JPH::Ref<JPH::GroupFilterTable> filter{ new JPH::GroupFilterTable{} };
        const JPH::CollisionGroup group{ filter, chassis.GetID().GetIndexAndSequenceNumber(), 0 };
        chassis.SetCollisionGroup( group );
        const auto wheels{ GetVehicleWheels( vehicle ) };

        for ( const auto& source: wheels ) {
            const bool left{ source.mPosition.x < 0 };
            const float travel{ .5f * ( source.mSuspensionMax - source.mSuspensionMin ) };
            const auto anchor{ chassis.GetWorldTransform() * GetFloat3F( source.mPosition ) };
            const auto position{ anchor + chassis.GetRotation() * JPH::Vec3{ 0, -travel, 0 } };
            JPH::Ref<JPH::CylinderShape> shape{ new JPH::CylinderShape{ .5f * source.mWidth, source.mRadius } };
            shape->SetDensity( 1.0e4f );
            JPH::BodyCreationSettings bodySettings{ shape, position,
                chassis.GetRotation() * JPH::Quat::sRotation( JPH::Vec3::sAxisZ(), .5f * JPH::JPH_PI ), JPH::EMotionType::Dynamic, Layers::MOVING };
            bodySettings.mCollisionGroup = group;
            bodySettings.mFriction = 1;
            bodySettings.mUserData = chassis.GetUserData();
            auto* wheel{ mSimulationInfo.mBodyInterface->CreateBody( bodySettings ) };

            if ( !wheel ) {
                RemoveVehicle( entity );
                MKT_CORE_LOGGER_WARN( "Cannot create 6DOF wheel: physics body capacity exceeded." );
                return;
            }

            mSimulationInfo.mBodyInterface->AddBody( wheel->GetID(), JPH::EActivation::Activate );
            state.mWheelBodies.push_back( wheel->GetID() );

            JPH::SixDOFConstraintSettings settings{};
            settings.mPosition1 = anchor;
            settings.mPosition2 = position;
            settings.mAxisX1 = settings.mAxisX2 = chassis.GetRotation() * ( left ? -JPH::Vec3::sAxisX() : JPH::Vec3::sAxisX() );
            settings.mAxisY1 = settings.mAxisY2 = chassis.GetRotation().RotateAxisY();
            settings.MakeFixedAxis( JPH::SixDOFConstraintSettings::EAxis::TranslationX );
            settings.SetLimitedAxis( JPH::SixDOFConstraintSettings::EAxis::TranslationY, -travel, travel );
            settings.MakeFixedAxis( JPH::SixDOFConstraintSettings::EAxis::TranslationZ );
            settings.mMotorSettings[1] = JPH::MotorSettings{ source.mSuspensionFrequency, source.mSuspensionDamping, 1.0e5f, 0 };
            settings.MakeFreeAxis( JPH::SixDOFConstraintSettings::EAxis::RotationX );
            settings.mMotorSettings[3] = JPH::MotorSettings{ 2, 1, 0, .5e4f };
            settings.MakeFixedAxis( JPH::SixDOFConstraintSettings::EAxis::RotationZ );

            if ( source.mSteered ) {
                settings.SetLimitedAxis( JPH::SixDOFConstraintSettings::EAxis::RotationY, -source.mMaxSteerAngle, source.mMaxSteerAngle );

                // The rotating wheel frame requires both Y and Z steering motors, as in the sample.
                settings.mMotorSettings[4] = settings.mMotorSettings[5] = JPH::MotorSettings{ 10, 1, 0, 1.0e6f };
            }
            else {
                settings.MakeFixedAxis( JPH::SixDOFConstraintSettings::EAxis::RotationY );
            }

            JPH::Ref<JPH::SixDOFConstraint> constraint{ static_cast<JPH::SixDOFConstraint*>( settings.Create( chassis, *wheel ) ) };
            constraint->SetTargetPositionCS( JPH::Vec3{ 0, -travel, 0 } );
            constraint->SetMotorState( JPH::SixDOFConstraint::EAxis::TranslationY, JPH::EMotorState::Position );

            if ( source.mSteered ) {
                constraint->SetMotorState( JPH::SixDOFConstraint::EAxis::RotationY, JPH::EMotorState::Position );
                constraint->SetMotorState( JPH::SixDOFConstraint::EAxis::RotationZ, JPH::EMotorState::Position );
            }

            mSimulationInfo.mPhysicsSystem.AddConstraint( constraint );
            state.mWheelConstraints.push_back( constraint );
        }
    }

    auto PhysicsWorld::UpdateVehicleInputs( entt::entity entity, const scene::VehicleComponent& vehicle ) -> void {
        const auto it{ mVehicles.find( entity ) };

        if ( it == mVehicles.end() ) {
            return;
        }

        auto& state{ it->second };
        const auto throttle{ glm::clamp( vehicle.mThrottle, -1.0f, 1.0f ) };
        const auto steer{ glm::clamp( vehicle.mSteer, -1.0f, 1.0f ) };
        const auto brake{ glm::clamp( vehicle.mBrake, 0.0f, 1.0f ) };
        const auto handBrake{ glm::clamp( vehicle.mHandBrake, 0.0f, 1.0f ) };

        if ( vehicle.GetVehicleType() == scene::VehicleType::eSixDof ) {
            const auto wheels{ GetVehicleWheels( vehicle ) };
            const float speed{ state.mChassis->GetLinearVelocity().Dot( state.mChassis->GetRotation().RotateAxisZ() ) };
            const bool stopping{ brake > 0 || handBrake > 0 || throttle * speed < 0 };

            for ( core::u32 i{}; i < state.mWheelConstraints.size(); ++i ) {
                auto& constraint{ state.mWheelConstraints[i] };
                const auto& wheel{ wheels[i] };

                if ( wheel.mSteered ) {
                    constraint->SetTargetOrientationCS( JPH::Quat::sRotation( JPH::Vec3::sAxisY(), -steer * wheel.mMaxSteerAngle ) );
                }

                const bool drive{ wheel.mDriven && throttle != 0 };
                constraint->SetTargetAngularVelocityCS( JPH::Vec3{ stopping ? 0.0f : ( wheel.mPosition.x < 0 ? -1.0f : 1.0f ) * throttle * 10.0f * JPH::JPH_PI, 0, 0 } );
                constraint->SetMotorState( JPH::SixDOFConstraint::EAxis::RotationX, stopping || drive ? JPH::EMotorState::Velocity : JPH::EMotorState::Off );
            }
        }
        else if ( vehicle.GetVehicleType() == scene::VehicleType::eTracked ) {
            // TankTest's slow-track steering and stationary pivot turn.
            float forward{ throttle }, left{ 1 }, right{ 1 };
            const float speed{ state.mChassis->GetLinearVelocity().Dot( state.mChassis->GetRotation().RotateAxisZ() ) };

            if ( steer != 0 ) {
                auto& ratio{ steer < 0 ? left : right };

                if ( brake == 0 && handBrake == 0 && forward == 0 && glm::abs( speed ) < 1 ) {
                    ratio = -1;
                    forward = 1;
                }
                else {
                    ratio = 1.0f - .4f * glm::abs( steer );
                }
            }

            static_cast<JPH::TrackedVehicleController*>( state.mConstraint->GetController() )->SetDriverInput( forward, left, right, glm::max( brake, handBrake ) );
        }
        else {
            float pressure{ brake };

            if ( vehicle.GetVehicleType() == scene::VehicleType::eMotorcycle ) {
                // MotorcycleTest reduces braking while leaned to avoid spinning out.
                const auto gravity{ mSimulationInfo.mPhysicsSystem.GetGravity() };
                const auto up{ gravity.IsNearZero() ? JPH::Vec3::sAxisY() : -gravity.Normalized() };
                const float lean{ glm::abs( up.Cross( state.mChassis->GetRotation().RotateAxisY() ).Dot( state.mChassis->GetRotation().RotateAxisZ() ) ) };
                pressure *= ( 1 - lean ) * ( 1 - lean );
            }

            static_cast<JPH::WheeledVehicleController*>( state.mConstraint->GetController() )->SetDriverInput( throttle, steer, pressure, handBrake );
        }
    }


    auto PhysicsWorld::FindEntityBody( core::u64 guid ) -> JPH::Body* {
        if ( guid == 0 ) {
            return &JPH::Body::sFixedToWorld;
        }

        auto* entity{ mScene->FindByID( guid ) };

        if ( !entity ) {
            return nullptr;
        }

        const auto found{ mEntityBodies.find( entity->Get() ) };

        return found == mEntityBodies.end() ? nullptr : GetJoltBody( found->second.mID );
    }

    auto PhysicsWorld::RemoveJoint( entt::entity entity ) -> void {
        const auto found{ mJoints.find( entity ) };

        if ( found == mJoints.end() ) {
            return;
        }

        // Gear/rack constraints borrow their hinges/sliders; detach dependants first.
        eastl::vector<entt::entity> dependants{};

        for ( const auto& [owner, state]: mJoints ) {
            if ( owner != entity && found->second.mConstraint && ( state.mDependencyA.GetPtr() == static_cast<JPH::Constraint*>( found->second.mConstraint.GetPtr() ) || state.mDependencyB.GetPtr() == static_cast<JPH::Constraint*>( found->second.mConstraint.GetPtr() ) ) ) {
                dependants.push_back( owner );
            }
        }

        for ( auto owner: dependants ) {
            RemoveJoint( owner );
        }

        // Erasing another dense-map entry may move this one.
        auto& state{ mJoints.at( entity ) };

        if ( state.mConstraint ) {
            mSimulationInfo.mPhysicsSystem.RemoveConstraint( state.mConstraint );

            // Jolt does not wake bodies when a constraint is detached.
            for ( auto* body: { state.mBodyA, state.mBodyB } ) {
                if ( body && body->IsDynamic() ) {
                    mSimulationInfo.mBodyInterface->ActivateBody( body->GetID() );
                }
            }
        }

        mJoints.erase( entity );
    }

    auto PhysicsWorld::RemoveJoints( entt::entity body ) -> void {
        const auto found{ mEntityBodies.find( body ) };
        JPH::Body* native{ found == mEntityBodies.end() ? nullptr : GetJoltBody( found->second.mID ) };
        eastl::vector<entt::entity> removed{};

        for ( const auto& [owner, state]: mJoints ) {
            if ( owner == body || ( native && ( state.mBodyA == native || state.mBodyB == native ) ) ) {
                removed.push_back( owner );
            }
        }

        for ( auto owner: removed ) {
            RemoveJoint( owner );
        }
    }

    auto PhysicsWorld::SynchronizeJoints() -> void {
        auto& registry{ mScene->GetRegistry() };
        std::set<entt::entity> present{};
        std::set<std::pair<core::u64, core::u64>> disabledPairs{};
        const auto dependency = [&]( core::u64 guid ) -> JPH::Constraint* {
            auto* entity{ mScene->FindByID( guid ) };

            if ( !entity ) {
                return nullptr;
            }

            const auto found{ mJoints.find( entity->Get() ) };

            return found == mJoints.end() ? nullptr : found->second.mConstraint.GetPtr();
        };

        // Hinges and sliders are visited before their gear/rack dependants.
        ForEachJointComponent( [&]<typename T>( const char* name ) {
            for ( auto [entity, joint]: registry.view<T>().each() ) {
                present.insert( entity );
                auto* a{ joint.GetBodyAEntityID() ? FindEntityBody( joint.GetBodyAEntityID() ) : nullptr };
                auto* b{ FindEntityBody( joint.GetBodyBEntityID() ) };
                JPH::Constraint *depA{}, *depB{};

                if constexpr ( std::is_same_v<T, GearJointComponent> ) {
                    depA = dependency( joint.GetHingeAEntityID() );
                    depB = dependency( joint.GetHingeBEntityID() );
                }

                if constexpr ( std::is_same_v<T, RackAndPinionJointComponent> ) {
                    depA = dependency( joint.GetHingeEntityID() );
                    depB = dependency( joint.GetSliderEntityID() );
                }

                auto it{ mJoints.find( entity ) };

                if ( it != mJoints.end() && ( it->second.mRevision != joint.GetRevision() || it->second.mType != entt::type_hash<T>::value() || it->second.mBodyA != a || it->second.mBodyB != b || it->second.mDependencyA.GetPtr() != depA || it->second.mDependencyB.GetPtr() != depB ) ) {
                    RemoveJoint( entity );
                    it = mJoints.end();
                }

                if ( !joint.IsEnabled() || !a || !b || a == b || ( !a->IsDynamic() && !b->IsDynamic() ) ) {
                    RemoveJoint( entity );
                    continue;
                }

                if ( it == mJoints.end() ) {
                    JointState state{};
                    state.mRevision = joint.GetRevision();
                    state.mType = entt::type_hash<T>::value();
                    state.mBodyA = a;
                    state.mBodyB = b;
                    state.mDependencyA = depA;
                    state.mDependencyB = depB;

                    try {
                        state.mConstraint = CreateJoint( joint, *a, *b, depA, depB );
                    } catch ( const std::exception& error ) {
                        state.mError = error.what();
                        MKT_CORE_LOGGER_WARN( "{} disabled: {}", name, error.what() );
                    }

                    it = mJoints.emplace( entity, eastl::move( state ) ).first;

                    if ( it->second.mConstraint ) {
                        mSimulationInfo.mPhysicsSystem.AddConstraint( it->second.mConstraint );

                        for ( auto* body: { a, b } ) {
                            if ( body->IsDynamic() ) {
                                mSimulationInfo.mBodyInterface->ActivateBody( body->GetID() );
                            }
                        }
                    }
                }

                if ( it->second.mConstraint && !joint.CanConnectedBodiesCollide() && joint.GetBodyBEntityID() ) {
                    disabledPairs.emplace( std::min( joint.GetBodyAEntityID(), joint.GetBodyBEntityID() ), std::max( joint.GetBodyAEntityID(), joint.GetBodyBEntityID() ) );
                }
            }
        } );
        eastl::vector<entt::entity> removed{};

        for ( const auto& [entity, state]: mJoints ) {
            if ( !present.contains( entity ) ) {
                removed.push_back( entity );
            }
        }

        for ( auto entity: removed ) {
            RemoveJoint( entity );
        }

        if ( mSimulationInfo.mContactListener.SetDisabledPairs( eastl::move( disabledPairs ) ) ) {
            for ( const auto& [id, body]: mBodies ) {
                mSimulationInfo.mBodyInterface->InvalidateContactCache( body->GetID() );
            }
        }
    }

    auto ContactEventListener::OnContactValidate( const JPH::Body& a, const JPH::Body& b, JPH::RVec3Arg, const JPH::CollideShapeResult& ) -> JPH::ValidateResult {
        const std::lock_guard lock{ mMutex };

        return mDisabledPairs.contains( { std::min( a.GetUserData(), b.GetUserData() ), std::max( a.GetUserData(), b.GetUserData() ) } )
                   ? JPH::ValidateResult::RejectAllContactsForThisBodyPair
                   : JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
    }

    auto ContactEventListener::SetDisabledPairs( std::set<std::pair<core::u64, core::u64>> pairs ) -> bool {
        const std::lock_guard lock{ mMutex };

        if ( pairs == mDisabledPairs ) {
            return false;
        }

        mDisabledPairs = std::move( pairs );

        return true;
    }


    auto PhysicsWorld::SetEntityWorldPose( entt::entity entity, const core::float3& position, const core::quat& rotation, const core::float3& scale ) -> void {
        auto& registry{ mScene->GetRegistry() };
        auto pose{ glm::translate( core::float4x4{ 1 }, position ) * glm::mat4_cast( rotation ) * glm::scale( core::float4x4{ 1 }, scale ) };

        if ( const auto* relation{ registry.try_get<scene::RelationComponent>( entity ) }; relation && relation->HasParent() ) {
            if ( auto* parent{ mScene->FindByID( *relation->GetParent() ) } ) {
                pose = glm::inverse( parent->GetComponent<scene::TransformComponent>().GetWorldTransform() ) * pose;
            }
        }

        registry.get<scene::TransformComponent>( entity ).SetTransform( pose );
    }

    auto PhysicsWorld::RemoveCharacter( entt::entity entity ) -> void {
        const auto found{ mCharacters.find( entity ) };

        if ( found == mCharacters.end() ) {
            return;
        }

        if ( found->second.mRigid ) {
            found->second.mRigid->RemoveFromPhysicsSystem();
        }

        if ( found->second.mVirtual ) {
            mCharacterCollisions.Remove( found->second.mVirtual );
        }

        mCharacters.erase( found );
    }

    auto PhysicsWorld::SynchronizeCharacters() -> void {
        auto& registry{ mScene->GetRegistry() };
        eastl::vector<entt::entity> removed{};

        for ( const auto& [entity, state]: mCharacters ) {
            if ( !registry.valid( entity ) || !registry.all_of<scene::CharacterControllerComponent, scene::TransformComponent>( entity ) || !registry.get<scene::CharacterControllerComponent>( entity ).IsEnabled() ) {
                removed.push_back( entity );
            }
        }

        for ( auto entity: removed ) {
            RemoveCharacter( entity );
        }

        for ( auto [entity, component, transform, tag]: registry.view<scene::CharacterControllerComponent, scene::TransformComponent, scene::TagComponent>().each() ) {
            if ( !component.IsEnabled() ) {
                continue;
            }

            const auto& world{ transform.GetWorldTransform() };
            const core::float3 position{ world[3] };
            const core::quat rotation{ glm::normalize( glm::quat_cast( world ) ) };
            auto it{ mCharacters.find( entity ) };

            if ( it != mCharacters.end() && it->second.mRevision != component.GetRevision() ) {
                RemoveCharacter( entity );
                it = mCharacters.end();
            }

            if ( it == mCharacters.end() ) {
                CharacterState state{};
                state.mRevision = component.GetRevision();
                state.mPosition = position;
                state.mRotation = rotation;

                try {
                    if ( component.GetRadius() <= 0 || component.GetHalfHeight() <= 0 || component.GetMaxSlopeAngle() < 0 || component.GetMaxSlopeAngle() >= .5f * JPH::JPH_PI || component.GetMaxStepHeight() < 0 || component.GetMaxStrength() < 0 || component.GetGravityFactor() < 0 ) {
                        MKT_THROW_RUNTIME_ERROR( "Invalid character capsule or movement settings." );
                    }

                    for ( int i{}; i < 3; ++i ) {
                        if ( glm::abs( glm::length( core::float3{ world[i] } ) - 1 ) > .001f ) {
                            MKT_THROW_RUNTIME_ERROR( "Characters require unit world scale; scale their visual children." );
                        }
                    }

                    // CharacterBaseTest places the entity origin at the capsule's feet.
                    JPH::RefConst<JPH::Shape> capsule{ new JPH::CapsuleShape{ component.GetHalfHeight(), component.GetRadius() } };
                    const auto shape{ JPH::RotatedTranslatedShapeSettings{ { 0, component.GetHalfHeight() + component.GetRadius(), 0 }, JPH::Quat::sIdentity(), capsule }.Create() };

                    if ( shape.HasError() ) {
                        MKT_THROW_RUNTIME_ERROR( shape.GetError().c_str() );
                    }

                    if ( component.GetControllerType() == scene::CharacterControllerType::eVirtual ) {
                        JPH::CharacterVirtualSettings settings{};
                        settings.mShape = shape.Get();
                        settings.mMaxSlopeAngle = component.GetMaxSlopeAngle();
                        settings.mMaxStrength = component.GetMaxStrength();
                        settings.mSupportingVolume = JPH::Plane{ JPH::Vec3::sAxisY(), -component.GetRadius() };
                        state.mVirtual = new JPH::CharacterVirtual{ &settings, GetFloat3F( position ), GetQuatF( rotation ), tag.GetGuid(), &mSimulationInfo.mPhysicsSystem };
                        state.mVirtual->SetCharacterVsCharacterCollision( &mCharacterCollisions );
                        mCharacterCollisions.Add( state.mVirtual );
                    }
                    else {
                        JPH::CharacterSettings settings{};
                        settings.mShape = shape.Get();
                        settings.mMaxSlopeAngle = component.GetMaxSlopeAngle();
                        settings.mLayer = Layers::MOVING;
                        settings.mGravityFactor = component.GetGravityFactor();
                        settings.mFriction = .5f;
                        settings.mSupportingVolume = JPH::Plane{ JPH::Vec3::sAxisY(), -component.GetRadius() };
                        state.mRigid = new JPH::Character{ &settings, GetFloat3F( position ), GetQuatF( rotation ), tag.GetGuid(), &mSimulationInfo.mPhysicsSystem };
                        state.mRigid->AddToPhysicsSystem( JPH::EActivation::Activate );
                    }
                } catch ( const std::exception& error ) {
                    state.mError = error.what();
                    MKT_CORE_LOGGER_WARN( "Character disabled: {}", error.what() );
                }

                it = mCharacters.emplace( entity, eastl::move( state ) ).first;
            }

            auto& state{ it->second };

            if ( glm::length2( state.mPosition - position ) > .000001f || glm::abs( glm::dot( state.mRotation, rotation ) ) < .99999f ) {
                if ( state.mRigid ) {
                    state.mRigid->SetPositionAndRotation( GetFloat3F( position ), GetQuatF( rotation ) );
                }

                if ( state.mVirtual ) {
                    state.mVirtual->SetPosition( GetFloat3F( position ) );
                    state.mVirtual->SetRotation( GetQuatF( rotation ) );
                }

                state.mPosition = position;
                state.mRotation = rotation;
            }
        }
    }

    auto PhysicsWorld::UpdateCharacters( core::f32 dt ) -> void {
        auto& registry{ mScene->GetRegistry() };

        for ( auto& [entity, state]: mCharacters ) {
            auto& component{ registry.get<scene::CharacterControllerComponent>( entity ) };

            if ( state.mVirtual ) {
                // CharacterVirtualTest::HandleInput: inherit ground motion before jump and gravity.
                auto& character{ *state.mVirtual };
                character.UpdateGroundVelocity();
                const auto up{ character.GetUp() };
                const auto vertical{ character.GetLinearVelocity().Dot( up ) * up };
                const auto ground{ character.GetGroundVelocity() };
                JPH::Vec3 velocity{ vertical };

                if ( character.GetGroundState() == JPH::CharacterBase::EGroundState::OnGround && ( vertical - ground ).Dot( up ) < .1f ) {
                    velocity = ground;
                    velocity += up * glm::max( 0.0f, component.mJumpSpeed );
                }

                const auto desired{ GetFloat3F( component.mDesiredVelocity ) };
                velocity += desired - up * desired.Dot( up ) + mSimulationInfo.mPhysicsSystem.GetGravity() * component.GetGravityFactor() * dt;
                character.SetLinearVelocity( velocity );
                JPH::CharacterVirtual::ExtendedUpdateSettings settings{};
                settings.mWalkStairsStepUp = up * component.GetMaxStepHeight();

                if ( !component.DoesStickToFloor() ) {
                    settings.mStickToFloorStepDown = JPH::Vec3::sZero();
                }

                auto& system{ mSimulationInfo.mPhysicsSystem };
                character.ExtendedUpdate( dt, system.GetGravity() * component.GetGravityFactor(), settings,
                    system.GetDefaultBroadPhaseLayerFilter( Layers::MOVING ), system.GetDefaultLayerFilter( Layers::MOVING ), {}, {}, *mSimulationInfo.mTempAllocator );
            }
            else if ( state.mRigid ) {
                // CharacterTest::HandleInput: project motion away from unwalkable surfaces.
                auto& character{ *state.mRigid };
                auto desired{ GetFloat3F( component.mDesiredVelocity ) };
                desired.SetY( 0 );

                if ( character.GetGroundState() == JPH::CharacterBase::EGroundState::OnSteepGround || character.GetGroundState() == JPH::CharacterBase::EGroundState::NotSupported ) {
                    auto normal{ character.GetGroundNormal() };
                    normal.SetY( 0 );
                    const float dot{ normal.Dot( desired ) };

                    if ( dot < 0 && normal.LengthSq() > .0001f ) {
                        desired -= dot * normal / normal.LengthSq();
                    }
                }

                desired.SetY( character.GetLinearVelocity().GetY() );
                auto velocity{ .75f * character.GetLinearVelocity() + .25f * desired };

                if ( character.GetGroundState() == JPH::CharacterBase::EGroundState::OnGround ) {
                    velocity += JPH::Vec3::sAxisY() * glm::max( 0.0f, component.mJumpSpeed );
                }

                character.SetLinearVelocity( velocity );
            }

            component.mJumpSpeed = 0;
        }
    }

    auto PhysicsWorld::PostUpdateCharacters() -> void {
        mScene->UpdateWorldTransformations();

        for ( auto& [entity, state]: mCharacters ) {
            if ( state.mRigid ) {
                state.mRigid->PostSimulation( .05f );
                state.mPosition = GetFloat3F( state.mRigid->GetPosition() );
                state.mRotation = GetQuatF( state.mRigid->GetRotation() );
            }
            else if ( state.mVirtual ) {
                state.mPosition = GetFloat3F( state.mVirtual->GetPosition() );
                state.mRotation = GetQuatF( state.mVirtual->GetRotation() );
            }
            else {
                continue;
            }

            const JPH::CharacterBase* character{ state.mRigid ? static_cast<JPH::CharacterBase*>( state.mRigid.GetPtr() ) : state.mVirtual.GetPtr() };
            mScene->GetRegistry().get<scene::CharacterControllerComponent>( entity ).mGrounded = character->GetGroundState() == JPH::CharacterBase::EGroundState::OnGround;
            SetEntityWorldPose( entity, state.mPosition, state.mRotation );
        }

        mScene->UpdateWorldTransformations();
    }

    auto PhysicsWorld::ApplyBuoyancy( core::f32 dt ) -> void {
        auto& registry{ mScene->GetRegistry() };

        for ( auto [entity, buoyancy]: registry.view<scene::BuoyancyComponent>().each() ) {
            if ( !buoyancy.IsEnabled() ) {
                continue;
            }

            const auto found{ mEntityBodies.find( entity ) };
            auto* body{ found == mEntityBodies.end() ? nullptr : GetJoltBody( found->second.mID ) };
            auto* water{ mScene->FindByID( buoyancy.GetWaterVolumeEntityID() ) };

            if ( !body || !body->IsDynamic() || !water || !water->HasComponent<scene::WaterVolumeComponent>() ) {
                continue;
            }

            const auto& volume{ water->GetComponent<scene::WaterVolumeComponent>() };

            if ( !volume.IsEnabled() || volume.GetDensity() <= 0 ) {
                continue;
            }

            const auto& transform{ water->GetComponent<scene::TransformComponent>().GetWorldTransform() };
            const auto inverse{ glm::inverse( transform ) };
            const auto bounds{ body->GetWorldSpaceBounds() };
            const auto center{ core::float3{ inverse * core::float4{ GetFloat3F( bounds.GetCenter() ), 1 } } };
            const auto extent{ glm::abs( core::float3{ inverse[0] } ) * bounds.GetExtent().GetX() +
                               glm::abs( core::float3{ inverse[1] } ) * bounds.GetExtent().GetY() +
                               glm::abs( core::float3{ inverse[2] } ) * bounds.GetExtent().GetZ() };

            if ( glm::any( glm::greaterThan( glm::abs( center ), volume.GetHalfExtents() + extent ) ) ) {
                continue;
            }

            core::float3 normal{ glm::normalize( core::float3{ glm::transpose( glm::inverse( transform ) )[1] } ) };
            auto point{ core::float3{ transform * core::float4{ 0, volume.GetSurfaceOffset(), 0, 1 } } };

            if ( auto* surface{ mScene->FindByID( volume.GetWaterSurfaceEntityID() ) } ) {
                // A referenced surface supplies its current plane; wave sampling belongs to the water simulation.
                const auto& world{ surface->GetComponent<scene::TransformComponent>().GetWorldTransform() };
                point = core::float3{ world[3] };
                normal = glm::normalize( core::float3{ world[1] } );
            }

            const auto flow{ core::float3{ transform * core::float4{ volume.GetFlowVelocity(), 0 } } };

            // WaterShapeTest: Jolt computes submerged volume, drag and the resulting impulse.
            body->ApplyBuoyancyImpulse( GetFloat3F( point ), GetFloat3F( normal ), buoyancy.GetBuoyancyFactor() * volume.GetDensity() / 1000.0f,
                buoyancy.GetLinearDrag(), buoyancy.GetAngularDrag(), GetFloat3F( flow ), mSimulationInfo.mPhysicsSystem.GetGravity(), dt );
        }
    }


    auto PhysicsWorld::SynchronizeRagdolls() -> void {
        auto& registry{ mScene->GetRegistry() };
        eastl::vector<entt::entity> removed{};

        for ( const auto& [entity, state]: mRagdolls ) {
            if ( !registry.valid( entity ) || !registry.all_of<scene::RagdollComponent>( entity ) || !registry.get<scene::RagdollComponent>( entity ).IsEnabled() || registry.get<scene::RagdollComponent>( entity ).GetRevision() != state.mRevision ) {
                removed.push_back( entity );
            }
        }

        for ( auto entity: removed ) {
            if ( auto* animator{ animation::AnimationSystem::Get()->GetAnimator( mRagdolls.at( entity ).mAnimatorID ) } ) {
                animator->EvaluatePose();
            }

            mRagdolls.erase( entity );
        }

        for ( auto [entity, component, transform]: registry.view<scene::RagdollComponent, scene::TransformComponent>().each() ) {
            if ( !component.IsEnabled() || mRagdolls.contains( entity ) ) {
                continue;
            }

            RagdollState state{};
            state.mRevision = component.GetRevision();

            try {
                if ( auto* owner{ registry.try_get<scene::AnimatorComponent>( entity ) } ) {
                    state.mAnimatorID = owner->GetAnimatorID();
                }

                auto* animator{ animation::AnimationSystem::Get()->GetAnimator( state.mAnimatorID ) };

                if ( !component.GetSkeleton().GetPath().IsEmpty() ) {
                    state.mSkeleton = asset::AssetRegistryService::Get()->LoadAssetHandle<asset::Skeleton>( component.GetSkeleton().GetPath() );
                    const auto skeleton{ state.mSkeleton.GetIfReady() };

                    if ( !skeleton || !animator || skeleton->GetResource() != animator->GetModel()->GetSkeleton() ) {
                        MKT_THROW_RUNTIME_ERROR( "Ragdoll skeleton must match its owner's animator rig." );
                    }
                }

                if ( animator ) {
                    animator->CopyAnimationPose( state.mAnimationPose );
                }

                for ( const auto& bone: component.GetBoneMappings() ) {
                    auto* body{ FindEntityBody( bone.mBodyEntityID ) };

                    if ( !body || !bone.mBodyEntityID || !body->IsDynamic() ) {
                        MKT_THROW_RUNTIME_ERROR( "Ragdoll bone needs an authored dynamic body." );
                    }

                    if ( bone.mJointEntityID ) {
                        auto* joint{ mScene->FindByID( bone.mJointEntityID ) };

                        if ( !joint || !mJoints.contains( joint->Get() ) || !mJoints.at( joint->Get() ).mConstraint ) {
                            MKT_THROW_RUNTIME_ERROR( "Ragdoll bone references a missing joint." );
                        }
                    }

                    if ( animator ) {
                        const auto index{ animator->FindJoint( bone.mBoneName ) };

                        if ( index < 0 || static_cast<core::usize>( index ) >= state.mAnimationPose.size() ) {
                            MKT_THROW_RUNTIME_ERROR( "Ragdoll mapping references a missing skeleton bone." );
                        }

                        state.mBindings.push_back( { bone.mBodyEntityID, index,
                            glm::inverse( GetFloat4x4F( body->GetWorldTransform() ) ) * transform.GetWorldTransform() * state.mAnimationPose[index] } );
                    }
                }

                state.mPhysicsPose.resize( state.mAnimationPose.size() );
            } catch ( const std::exception& error ) {
                state.mError = error.what();
                MKT_CORE_LOGGER_WARN( "Ragdoll binding disabled: {}", error.what() );
            }

            mRagdolls.emplace( entity, eastl::move( state ) );
        }
    }

    auto PhysicsWorld::UpdateRagdollPoses() -> void {
        auto& registry{ mScene->GetRegistry() };

        for ( auto& [entity, state]: mRagdolls ) {
            auto* animator{ animation::AnimationSystem::Get()->GetAnimator( state.mAnimatorID ) };

            if ( !animator || !state.mError.empty() ) {
                continue;
            }

            const auto& component{ registry.get<scene::RagdollComponent>( entity ) };
            animator->CopyAnimationPose( state.mAnimationPose );
            state.mPhysicsPose = state.mAnimationPose;
            const auto inverse{ glm::inverse( registry.get<scene::TransformComponent>( entity ).GetWorldTransform() ) };

            // Jolt Ragdoll::GetPose exposes body world transforms. Here the ECS already owns
            // those bodies/constraints, so map them to skinning bones without creating duplicate bodies.
            for ( const auto& binding: state.mBindings ) {
                if ( auto* body{ FindEntityBody( binding.mEntityID ) } ) {
                    state.mPhysicsPose[binding.mJoint] = inverse * GetFloat4x4F( body->GetWorldTransform() ) * binding.mBodyToJoint;
                }
            }

            const auto parents{ animator->GetModel()->GetSkeleton()->GetOzzSkeleton()->joint_parents() };

            for ( core::usize i{}; i < state.mPhysicsPose.size(); ++i ) {
                const bool mapped{ eastl::any_of( state.mBindings.begin(), state.mBindings.end(), [&]( const auto& binding ) {
                    return binding.mJoint == i;
                } ) };

                if ( !mapped && parents[i] >= 0 ) {
                    state.mPhysicsPose[i] = state.mPhysicsPose[parents[i]] * glm::inverse( state.mAnimationPose[parents[i]] ) * state.mAnimationPose[i];
                }
            }

            animator->PublishPhysicsPose( state.mPhysicsPose, component.GetSimulationBlend() );
        }
    }

    auto PhysicsWorld::PickSoftBody( const RayCast& ray ) const -> SoftBodyHit {
        const JPH::RRayCast query{ JPH::RVec3{ GetFloat3F( ray.GetOrigin() ) }, GetFloat3F( ray.GetDirection() ) };
        JPH::RayCastResult hit{};

        if ( !mSimulationInfo.mPhysicsSystem.GetNarrowPhaseQuery().CastRay( query, hit, {}, {}, SoftBodyPickFilter{} ) ) {
            return {};
        }

        const JPH::BodyLockRead lock{ mSimulationInfo.mPhysicsSystem.GetBodyLockInterface(), hit.mBodyID };

        if ( !lock.Succeeded() || !lock.GetBody().IsSoftBody() ) {
            return {};
        }

        const auto& body{ lock.GetBody() };
        const auto* entity{ mScene->FindByID( body.GetUserData() ) };

        if ( !entity ) {
            return {};
        }

        const auto& motion{ static_cast<const JPH::SoftBodyMotionProperties&>( *body.GetMotionProperties() ) };
        const auto& shape{ static_cast<const JPH::SoftBodyShape&>( *body.GetShape() ) };
        const auto& face{ motion.GetSettings()->mFaces[shape.GetFaceIndex( hit.mSubShapeID2 )] };
        const JPH::Vec3 localHit{ body.GetInverseCenterOfMassTransform() * query.GetPointOnRay( hit.mFraction ) };

        // Restrict the grab to the hit triangle so overlapping cloth layers stay distinct.
        auto nearest{ face.mVertex[0] };
        auto distance{ ( motion.GetVertex( nearest ).mPosition - localHit ).LengthSq() };

        for ( const auto index: face.mVertex ) {
            const auto candidate{ ( motion.GetVertex( index ).mPosition - localHit ).LengthSq() };
            if ( candidate < distance ) {
                nearest = index;
                distance = candidate;
            }
        }

        return {
            .mEntity = entity->Get(),
            .mParticle = nearest,
            .mPosition = GetFloat3F( JPH::Vec3{ body.GetCenterOfMassTransform() * motion.GetVertex( nearest ).mPosition } )
        };
    }

    auto PhysicsWorld::GetSoftBodyParticles( entt::entity entity, eastl::vector<SoftBodyParticle>& particles ) const -> void {
        particles.clear();

        const auto found{ mSoftBodies.find( entity ) };

        if ( found == mSoftBodies.end() || !found->second.mBody ) {
            return;
        }

        const JPH::BodyLockRead lock{ mSimulationInfo.mPhysicsSystem.GetBodyLockInterface(), found->second.mBody->GetID() };

        if ( !lock.Succeeded() ) {
            return;
        }

        const auto& body{ lock.GetBody() };
        const auto& motion{ static_cast<const JPH::SoftBodyMotionProperties&>( *body.GetMotionProperties() ) };
        const auto world{ body.GetCenterOfMassTransform() };

        particles.reserve( motion.GetVertices().size() );

        for ( const auto& vertex: motion.GetVertices() ) {
            particles.push_back( {
                .mPosition = GetFloat3F( JPH::Vec3{ world * vertex.mPosition } ),
                .mPinned = vertex.mInvMass == 0.0f
            } );
        }
    }

    // Adapted from Jolt's interactive soft-body grab. Save and restore the original pin state.
    // https://github.com/jrouwe/JoltPhysics/blob/v5.6.0/Samples/SamplesApp.cpp#L1864-L1978
    auto PhysicsWorld::BeginSoftBodyGrab( const SoftBodyHit& hit ) -> bool {
        if ( IsSoftBodyGrabActive() || ( !mScene->IsPlaying() && !mScene->IsSimulating() ) ) {
            return false;
        }

        const auto found{ mSoftBodies.find( hit.mEntity ) };

        if ( found == mSoftBodies.end() || !found->second.mBody ) {
            return false;
        }

        const JPH::BodyLockWrite lock{ mSimulationInfo.mPhysicsSystem.GetBodyLockInterface(), found->second.mBody->GetID() };

        if ( !lock.Succeeded() ) {
            return false;
        }

        auto& body{ lock.GetBody() };
        auto& motion{ static_cast<JPH::SoftBodyMotionProperties&>( *body.GetMotionProperties() ) };

        if ( hit.mParticle >= motion.GetVertices().size() ) {
            return false;
        }

        auto& vertex{ motion.GetVertex( hit.mParticle ) };
        mSoftBodyGrab = {
            .mBody = body.GetID(),
            .mParticle = hit.mParticle,
            .mInverseMass = vertex.mInvMass,
            .mTarget = hit.mPosition
        };
        vertex.mInvMass = 0.0f;

        return true;
    }

    auto PhysicsWorld::SetSoftBodyGrabTarget( const core::float3& target ) -> void {
        mSoftBodyGrab.mTarget = target;
    }

    auto PhysicsWorld::EndSoftBodyGrab() -> void {
        if ( !IsSoftBodyGrabActive() ) {
            return;
        }

        const JPH::BodyLockWrite lock{ mSimulationInfo.mPhysicsSystem.GetBodyLockInterface(), mSoftBodyGrab.mBody };

        if ( lock.Succeeded() ) {
            auto& motion{ static_cast<JPH::SoftBodyMotionProperties&>( *lock.GetBody().GetMotionProperties() ) };
            auto& vertex{ motion.GetVertex( mSoftBodyGrab.mParticle ) };
            vertex.mInvMass = mSoftBodyGrab.mInverseMass;

            if ( vertex.mInvMass == 0.0f ) {
                vertex.mVelocity = JPH::Vec3::sZero();
            }
        }

        mSoftBodyGrab = {};
    }

    auto PhysicsWorld::IsSoftBodyGrabActive() const -> bool {
        return !mSoftBodyGrab.mBody.IsInvalid();
    }

    auto PhysicsWorld::UpdateSoftBodyGrab( core::f32 timeStep ) -> void {
        if ( !IsSoftBodyGrabActive() ) {
            return;
        }

        const auto id{ mSoftBodyGrab.mBody };

        {
            const JPH::BodyLockWrite lock{ mSimulationInfo.mPhysicsSystem.GetBodyLockInterface(), id };

            if ( !lock.Succeeded() ) {
                mSoftBodyGrab = {};
                return;
            }

            auto& body{ lock.GetBody() };
            auto& motion{ static_cast<JPH::SoftBodyMotionProperties&>( *body.GetMotionProperties() ) };
            auto& vertex{ motion.GetVertex( mSoftBodyGrab.mParticle ) };

            // Advance through the solver so connected particles react during the step.
            const JPH::Vec3 localTarget{ body.GetInverseCenterOfMassTransform() * JPH::RVec3{ GetFloat3F( mSoftBodyGrab.mTarget ) } };
            vertex.mVelocity = ( localTarget - vertex.mPosition ) / timeStep;
        }

        mSimulationInfo.mBodyInterface->ActivateBody( id );
    }

    auto PhysicsWorld::RemoveSoftBody( entt::entity entity ) -> void {
        const auto found{ mSoftBodies.find( entity ) };

        if ( found == mSoftBodies.end() ) {
            return;
        }

        if ( auto* body{ found->second.mBody } ) {
            if ( mSoftBodyGrab.mBody == body->GetID() ) {
                mSoftBodyGrab = {};
            }

            mSimulationInfo.mBodyInterface->RemoveBody( body->GetID() );
            mSimulationInfo.mBodyInterface->DestroyBody( body->GetID() );
        }

        auto& registry{ mScene->GetRegistry() };

        if ( registry.valid( entity ) ) {
            registry.remove<scene::DeformedMeshComponent>( entity );
        }

        mSoftBodies.erase( entity );
    }

    auto PhysicsWorld::SynchronizeSoftBodies() -> void {
        auto& registry{ mScene->GetRegistry() };
        eastl::vector<entt::entity> removed{};

        for ( const auto& [entity, state]: mSoftBodies ) {
            if ( !registry.valid( entity ) || !registry.all_of<scene::TransformComponent>( entity ) || !registry.any_of<scene::SoftBodyComponent, scene::ClothComponent>( entity ) ) {
                removed.push_back( entity );
            }
        }

        for ( auto entity: removed ) {
            RemoveSoftBody( entity );
        }

        const auto synchronize = [&]<typename T>() {
            constexpr bool cloth{ std::is_same_v<T, scene::ClothComponent> };

            for ( auto [entity, component, transform, tag]: registry.view<T, scene::TransformComponent, scene::TagComponent>().each() ) {
                if constexpr ( !cloth ) {
                    if ( registry.all_of<scene::ClothComponent>( entity ) ) {
                        continue;
                    }
                }

                if ( !component.IsEnabled() ) {
                    RemoveSoftBody( entity );
                    continue;
                }

                core::float3 scale{}, position{}, skew{};
                core::quat rotation{};
                core::float4 perspective{};
                const bool decomposed{ glm::decompose( transform.GetWorldTransform(), scale, rotation, position, skew, perspective ) };
                auto* mesh{ registry.try_get<scene::MeshComponent>( entity ) };
                auto it{ mSoftBodies.find( entity ) };
                const bool sourceChanged{ mesh && mesh->GetMesh() && it != mSoftBodies.end() && mesh->GetMesh()->GetRenderData() != it->second.mSource };

                if ( it != mSoftBodies.end() && ( it->second.mRevision != component.GetRevision() || it->second.mCloth != cloth || sourceChanged || glm::any( glm::greaterThan( glm::abs( it->second.mScale - scale ), core::float3{ .0001f } ) ) ) ) {
                    RemoveSoftBody( entity );
                    it = mSoftBodies.end();
                }

                if ( it == mSoftBodies.end() ) {
                    SoftBodyState state{};
                    state.mRevision = component.GetRevision();
                    state.mCloth = cloth;
                    state.mPosition = position;
                    state.mRotation = rotation;
                    state.mScale = scale;

                    try {
                        if ( !decomposed || glm::length2( skew ) > .000001f || glm::any( glm::lessThanEqual( scale, core::float3{ .00001f } ) ) ) {
                            MKT_THROW_RUNTIME_ERROR( "Soft bodies require positive scale and no shear." );
                        }

                        if ( component.GetVertexMass() <= 0 || component.GetGravityFactor() < 0 || component.GetLinearDamping() < 0 || component.GetPressure() < 0 || component.GetEdgeCompliance() < 0 ) {
                            MKT_THROW_RUNTIME_ERROR( "Invalid soft-body mass, damping, pressure or compliance." );
                        }

                        if ( !component.GetModel().GetPath().IsEmpty() ) {
                            state.mModel = asset::AssetRegistryService::Get()->LoadAsset<asset::Model>( component.GetModel().GetPath() );

                            if ( !state.mModel || component.GetMeshIndex() >= state.mModel->GetMeshNodeCount() ) {
                                MKT_THROW_RUNTIME_ERROR( "Soft-body source mesh is missing." );
                            }

                            registry.emplace_or_replace<scene::MeshComponent>( entity, state.mModel, static_cast<core::i32>( component.GetMeshIndex() ) );
                            mesh = registry.try_get<scene::MeshComponent>( entity );
                        }
                        else if ( mesh ) {
                            state.mModel = mesh->GetModel();
                        }

                        if ( !mesh || !mesh->GetMesh() ) {
                            MKT_THROW_RUNTIME_ERROR( "Soft body needs an indexed mesh." );
                        }

                        const auto& node{ *mesh->GetMesh() };
                        state.mSource = node.GetRenderData();

                        if ( !state.mSource || state.mSource->IsSkinned() ) {
                            MKT_THROW_RUNTIME_ERROR( "Soft-body source must be an unskinned mesh." );
                        }

                        const auto& vertices{ node.GetCollisionVertices() };
                        const auto indices{ state.mSource->GetIndices() };

                        if ( vertices.empty() || vertices.size() != state.mSource->GetVertexCount() || indices.size() % 3 != 0 ) {
                            MKT_THROW_RUNTIME_ERROR( "Soft-body CPU geometry is unavailable." );
                        }

                        state.mSettings = new JPH::SoftBodySharedSettings{};
                        auto& settings{ *state.mSettings };

                        // Weld render seams so duplicated visual vertices share one simulated particle.
                        std::map<std::tuple<float, float, float>, core::u32> welded{};

                        for ( const auto& vertex: vertices ) {
                            const auto point{ vertex * scale };
                            const auto [found, inserted]{ welded.emplace( std::tuple{ point.x, point.y, point.z }, static_cast<core::u32>( settings.mVertices.size() ) ) };

                            if ( inserted ) {
                                settings.mVertices.emplace_back( JPH::Float3{ point.x, point.y, point.z }, JPH::Float3{ 0, 0, 0 }, 1.0f / component.GetVertexMass() );
                            }

                            state.mVertexMap.push_back( found->second );
                        }

                        // Pinned vertices keep their authored position; all others retain their vertex mass.
                        for ( auto index: component.GetPinnedVertices() ) {
                            if ( index >= state.mVertexMap.size() ) {
                                MKT_THROW_RUNTIME_ERROR( "Pinned vertex is outside the source mesh." );
                            }

                            settings.mVertices[state.mVertexMap[index]].mInvMass = 0;
                        }

                        for ( core::usize i{}; i < indices.size(); i += 3 ) {
                            JPH::SoftBodySharedSettings::Face face{ state.mVertexMap[indices[i]], state.mVertexMap[indices[i + 1]], state.mVertexMap[indices[i + 2]] };

                            if ( !face.IsDegenerate() ) {
                                settings.AddFace( face );
                            }
                        }

                        if ( settings.mFaces.empty() ) {
                            MKT_THROW_RUNTIME_ERROR( "Soft body has no nondegenerate triangles." );
                        }

                        // Jolt Samples/Utils/SoftBodyCreator.cpp: build edge/bend constraints, then optimize.
                        JPH::SoftBodySharedSettings::VertexAttributes attributes{};
                        attributes.mCompliance = attributes.mShearCompliance = component.GetEdgeCompliance();

                        if constexpr ( cloth ) {
                            attributes.mBendCompliance = component.GetBendCompliance();
                        }

                        settings.CreateConstraints( &attributes, 1, JPH::SoftBodySharedSettings::EBendType::Dihedral );
                        settings.Optimize();

                        // The native body owns simulation; the private mesh publishes deformation to rendering.
                        JPH::SoftBodyCreationSettings bodySettings{ &settings, GetFloat3F( position ), GetQuatF( rotation ), Layers::MOVING };
                        bodySettings.mPressure = component.GetPressure();
                        bodySettings.mGravityFactor = component.GetGravityFactor();
                        bodySettings.mLinearDamping = component.GetLinearDamping();
                        bodySettings.mMakeRotationIdentity = false;
                        bodySettings.mUpdatePosition = component.GetPinnedVertices().empty();
                        bodySettings.mUserData = tag.GetGuid();
                        bodySettings.mFacesDoubleSided = cloth;
                        state.mBody = mSimulationInfo.mBodyInterface->CreateSoftBody( bodySettings );

                        if ( !state.mBody ) {
                            MKT_THROW_RUNTIME_ERROR( "Soft-body capacity exceeded." );
                        }

                        mSimulationInfo.mBodyInterface->AddBody( state.mBody->GetID(), JPH::EActivation::Activate );
                        state.mDeformed = core::Ref<renderer::MeshRenderData>::New( *state.mSource );
                        state.mPositions.resize( vertices.size() );
                        registry.emplace_or_replace<scene::DeformedMeshComponent>( entity, state.mDeformed );
                    } catch ( const std::exception& error ) {
                        state.mError = error.what();
                        MKT_CORE_LOGGER_WARN( "Soft body disabled: {}", error.what() );

                        if ( state.mBody ) {
                            mSimulationInfo.mBodyInterface->RemoveBody( state.mBody->GetID() );
                            mSimulationInfo.mBodyInterface->DestroyBody( state.mBody->GetID() );
                            state.mBody = nullptr;
                        }
                    }

                    it = mSoftBodies.emplace( entity, eastl::move( state ) ).first;
                }

                auto& state{ it->second };

                if ( state.mBody && ( glm::length2( state.mPosition - position ) > .000001f || glm::abs( glm::dot( state.mRotation, rotation ) ) < .99999f ) ) {
                    mSimulationInfo.mBodyInterface->SetPositionAndRotation( state.mBody->GetID(), GetFloat3F( position ), GetQuatF( rotation ), JPH::EActivation::Activate );
                    state.mPosition = position;
                    state.mRotation = rotation;
                }
            }
        };
        synchronize.template operator()<scene::SoftBodyComponent>();
        synchronize.template operator()<scene::ClothComponent>();
    }

    auto PhysicsWorld::PostUpdateSoftBodies() -> void {
        for ( auto& [entity, state]: mSoftBodies ) {
            if ( !state.mBody || !state.mDeformed ) {
                continue;
            }

            const auto* motion{ static_cast<const JPH::SoftBodyMotionProperties*>( state.mBody->GetMotionProperties() ) };
            const auto& vertices{ motion->GetVertices() };
            bool changed{};

            for ( core::usize i{}; i < state.mVertexMap.size(); ++i ) {
                const auto position{ GetFloat3F( vertices[state.mVertexMap[i]].mPosition ) / state.mScale };
                changed |= state.mPositions[i] != position;
                state.mPositions[i] = position;
            }

            // Publish the last simulated pose even when this step puts the body to sleep.
            if ( changed ) {
                state.mDeformed->UpdatePositions( state.mPositions );
            }

            state.mPosition = GetFloat3F( state.mBody->GetPosition() );
            state.mRotation = GetQuatF( state.mBody->GetRotation() );
            SetEntityWorldPose( entity, state.mPosition, state.mRotation, state.mScale );
        }

        mScene->UpdateWorldTransformations();
    }
}// namespace mikoto::physics
