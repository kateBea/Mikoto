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

#ifndef MIKOTO_SCENE_UTILITY_HH
#define MIKOTO_SCENE_UTILITY_HH

#include <entt/entity/registry.hpp>

#include <Scene/Component.hh>
#include <Scene/Entity.hh>

namespace mikoto::scene {

    /**
     * @brief Tests whether a non-null entity has the requested component.
     * @tparam ComponentType Component type to test for on the entity.
     * @param entity Entity to query; may be null.
     * @returns True when the entity exists and owns ComponentType.
     */
    template<typename ComponentType>
    MKT_NODISCARD static inline auto IsPresent( Entity *entity ) -> bool {
        MKT_BEGIN_PROFILER();
        if ( entity == nullptr ) {
            return false;
        }

        return entity->HasComponent<ComponentType>();
    }

    /**
     * @brief Reserves a group-iteration hook; currently performs no work.
     * @tparam Callback Callable type reserved for future group iteration.
     * @tparam ComponentTypes Component types reserved for a future group filter.
     * @param c Unused callback reserved for future group iteration.
     */
    template<typename Callback, typename... ComponentTypes>
    auto ForEachGroup( const Callback &c ) -> void {
    }

    template<typename Visitor>
    auto ForEachColliderComponent( Visitor &&visitor ) -> void {
        visitor.template operator()<BoxColliderComponent>();
        visitor.template operator()<SphereColliderComponent>();
        visitor.template operator()<CapsuleColliderComponent>();
        visitor.template operator()<MeshColliderComponent>();
        visitor.template operator()<HeightFieldColliderComponent>();
        visitor.template operator()<CompoundColliderComponent>();
        visitor.template operator()<CylinderColliderComponent>();
        visitor.template operator()<TaperedCapsuleColliderComponent>();
        visitor.template operator()<TaperedCylinderColliderComponent>();
        visitor.template operator()<PlaneColliderComponent>();
    }

    MKT_NODISCARD inline auto HasCollider( const entt::registry &registry, entt::entity entity ) -> bool {
        bool present{};
        ForEachColliderComponent( [&]<typename T>() {
            present = present || registry.all_of<T>( entity );
        } );
        return present;
    }

// Shared authored fields keep scene persistence, duplication and inspector edits in agreement.
#define MKT_PHYSICS_FIELD( Name, Getter, Setter ) \
    {                                             \
        auto value{ component.Getter() };         \
        if ( visitor( Name, value ) ) {           \
            component.Setter( value );            \
        }                                         \
    }

    template<typename Visitor>
    auto VisitPhysicsFields( HeightFieldColliderComponent &component, Visitor &&visitor ) -> void {
        MKT_PHYSICS_FIELD( "LandscapeEntityID", GetLandscapeEntityID, SetLandscapeEntityID );
        MKT_PHYSICS_FIELD( "HeightmapPath", GetHeightmap, SetHeightmap );
        MKT_PHYSICS_FIELD( "SampleSpacing", GetSampleSpacing, SetSampleSpacing );
        MKT_PHYSICS_FIELD( "HeightScale", GetHeightScale, SetHeightScale );
        MKT_PHYSICS_FIELD( "BaseElevation", GetBaseElevation, SetBaseElevation );
        MKT_PHYSICS_FIELD( "Offset", GetOffset, SetOffset );
        MKT_PHYSICS_FIELD( "Trigger", IsTrigger, SetTrigger );
    }

    template<typename Visitor>
    auto VisitPhysicsFields( CompoundColliderComponent &component, Visitor &&visitor ) -> void {
        MKT_PHYSICS_FIELD( "ColliderEntities", GetColliderEntities, SetColliderEntities );
        MKT_PHYSICS_FIELD( "Mode", GetMode, SetMode );
        MKT_PHYSICS_FIELD( "Offset", GetOffset, SetOffset );
        MKT_PHYSICS_FIELD( "Trigger", IsTrigger, SetTrigger );
    }

    template<typename Visitor>
    auto VisitPhysicsFields( CylinderColliderComponent &component, Visitor &&visitor ) -> void {
        MKT_PHYSICS_FIELD( "Radius", GetRadius, SetRadius );
        MKT_PHYSICS_FIELD( "HalfHeight", GetHalfHeight, SetHalfHeight );
        MKT_PHYSICS_FIELD( "Offset", GetOffset, SetOffset );
        MKT_PHYSICS_FIELD( "Trigger", IsTrigger, SetTrigger );
    }

    template<typename Visitor>
    auto VisitPhysicsFields( TaperedCapsuleColliderComponent &component, Visitor &&visitor ) -> void {
        MKT_PHYSICS_FIELD( "TopRadius", GetTopRadius, SetTopRadius );
        MKT_PHYSICS_FIELD( "BottomRadius", GetBottomRadius, SetBottomRadius );
        MKT_PHYSICS_FIELD( "HalfHeight", GetHalfHeight, SetHalfHeight );
        MKT_PHYSICS_FIELD( "Offset", GetOffset, SetOffset );
        MKT_PHYSICS_FIELD( "Trigger", IsTrigger, SetTrigger );
    }

    template<typename Visitor>
    auto VisitPhysicsFields( TaperedCylinderColliderComponent &component, Visitor &&visitor ) -> void {
        MKT_PHYSICS_FIELD( "TopRadius", GetTopRadius, SetTopRadius );
        MKT_PHYSICS_FIELD( "BottomRadius", GetBottomRadius, SetBottomRadius );
        MKT_PHYSICS_FIELD( "HalfHeight", GetHalfHeight, SetHalfHeight );
        MKT_PHYSICS_FIELD( "Offset", GetOffset, SetOffset );
        MKT_PHYSICS_FIELD( "Trigger", IsTrigger, SetTrigger );
    }

    template<typename Visitor>
    auto VisitPhysicsFields( PlaneColliderComponent &component, Visitor &&visitor ) -> void {
        MKT_PHYSICS_FIELD( "Normal", GetNormal, SetNormal );
        MKT_PHYSICS_FIELD( "Distance", GetDistance, SetDistance );
        MKT_PHYSICS_FIELD( "Offset", GetOffset, SetOffset );
        MKT_PHYSICS_FIELD( "Trigger", IsTrigger, SetTrigger );
    }

    template<typename Visitor>
    auto VisitPhysicsFields( JointComponent &component, Visitor &&visitor ) -> void {
        MKT_PHYSICS_FIELD( "BodyAEntityID", GetBodyAEntityID, SetBodyAEntityID );
        MKT_PHYSICS_FIELD( "BodyBEntityID", GetBodyBEntityID, SetBodyBEntityID );
        MKT_PHYSICS_FIELD( "AnchorA", GetAnchorA, SetAnchorA );
        MKT_PHYSICS_FIELD( "AnchorB", GetAnchorB, SetAnchorB );
        MKT_PHYSICS_FIELD( "RotationA", GetRotationA, SetRotationA );
        MKT_PHYSICS_FIELD( "RotationB", GetRotationB, SetRotationB );
        MKT_PHYSICS_FIELD( "CollideConnected", CanConnectedBodiesCollide, SetCollideConnected );
        MKT_PHYSICS_FIELD( "Enabled", IsEnabled, SetEnabled );
    }

    template<typename Visitor>
    auto VisitPhysicsFields( DistanceJointComponent &component, Visitor &&visitor ) -> void {
        VisitPhysicsFields( static_cast<JointComponent &>( component ), visitor );
        MKT_PHYSICS_FIELD( "MinDistance", GetMinDistance, SetMinDistance );
        MKT_PHYSICS_FIELD( "MaxDistance", GetMaxDistance, SetMaxDistance );
        MKT_PHYSICS_FIELD( "Spring", GetSpring, SetSpring );
    }

    template<typename Visitor>
    auto VisitPhysicsFields( FixedJointComponent &component, Visitor &&visitor ) -> void {
        VisitPhysicsFields( static_cast<JointComponent &>( component ), visitor );
    }

    template<typename Visitor>
    auto VisitPhysicsFields( PointJointComponent &component, Visitor &&visitor ) -> void {
        VisitPhysicsFields( static_cast<JointComponent &>( component ), visitor );
    }

    template<typename Visitor>
    auto VisitPhysicsFields( HingeJointComponent &component, Visitor &&visitor ) -> void {
        VisitPhysicsFields( static_cast<JointComponent &>( component ), visitor );
        MKT_PHYSICS_FIELD( "MinAngle", GetMinAngle, SetMinAngle );
        MKT_PHYSICS_FIELD( "MaxAngle", GetMaxAngle, SetMaxAngle );
        MKT_PHYSICS_FIELD( "LimitSpring", GetLimitSpring, SetLimitSpring );
        MKT_PHYSICS_FIELD( "Motor", GetMotor, SetMotor );
    }

    template<typename Visitor>
    auto VisitPhysicsFields( SliderJointComponent &component, Visitor &&visitor ) -> void {
        VisitPhysicsFields( static_cast<JointComponent &>( component ), visitor );
        MKT_PHYSICS_FIELD( "MinDistance", GetMinDistance, SetMinDistance );
        MKT_PHYSICS_FIELD( "MaxDistance", GetMaxDistance, SetMaxDistance );
        MKT_PHYSICS_FIELD( "LimitSpring", GetLimitSpring, SetLimitSpring );
        MKT_PHYSICS_FIELD( "Motor", GetMotor, SetMotor );
    }

    template<typename Visitor>
    auto VisitPhysicsFields( ConeJointComponent &component, Visitor &&visitor ) -> void {
        VisitPhysicsFields( static_cast<JointComponent &>( component ), visitor );
        MKT_PHYSICS_FIELD( "HalfConeAngle", GetHalfConeAngle, SetHalfConeAngle );
    }

    template<typename Visitor>
    auto VisitPhysicsFields( SwingTwistJointComponent &component, Visitor &&visitor ) -> void {
        VisitPhysicsFields( static_cast<JointComponent &>( component ), visitor );
        MKT_PHYSICS_FIELD( "NormalHalfConeAngle", GetNormalHalfConeAngle, SetNormalHalfConeAngle );
        MKT_PHYSICS_FIELD( "PlaneHalfConeAngle", GetPlaneHalfConeAngle, SetPlaneHalfConeAngle );
        MKT_PHYSICS_FIELD( "MinTwistAngle", GetMinTwistAngle, SetMinTwistAngle );
        MKT_PHYSICS_FIELD( "MaxTwistAngle", GetMaxTwistAngle, SetMaxTwistAngle );
        MKT_PHYSICS_FIELD( "Motors", GetMotors, SetMotors );
    }

    template<typename Visitor>
    auto VisitPhysicsFields( SixDofJointComponent &component, Visitor &&visitor ) -> void {
        VisitPhysicsFields( static_cast<JointComponent &>( component ), visitor );
        MKT_PHYSICS_FIELD( "TranslationMotion", GetTranslationMotion, SetTranslationMotion );
        MKT_PHYSICS_FIELD( "RotationMotion", GetRotationMotion, SetRotationMotion );
        MKT_PHYSICS_FIELD( "MinTranslation", GetMinTranslation, SetMinTranslation );
        MKT_PHYSICS_FIELD( "MaxTranslation", GetMaxTranslation, SetMaxTranslation );
        MKT_PHYSICS_FIELD( "MinRotation", GetMinRotation, SetMinRotation );
        MKT_PHYSICS_FIELD( "MaxRotation", GetMaxRotation, SetMaxRotation );
        MKT_PHYSICS_FIELD( "Motors", GetMotors, SetMotors );
    }

    template<typename Visitor>
    auto VisitPhysicsFields( PathJointComponent &component, Visitor &&visitor ) -> void {
        VisitPhysicsFields( static_cast<JointComponent &>( component ), visitor );
        MKT_PHYSICS_FIELD( "ControlPoints", GetControlPoints, SetControlPoints );
        MKT_PHYSICS_FIELD( "Looping", IsLooping, SetLooping );
        MKT_PHYSICS_FIELD( "InitialFraction", GetInitialFraction, SetInitialFraction );
        MKT_PHYSICS_FIELD( "MaxFrictionForce", GetMaxFrictionForce, SetMaxFrictionForce );
        MKT_PHYSICS_FIELD( "Motor", GetMotor, SetMotor );
    }

    template<typename Visitor>
    auto VisitPhysicsFields( GearJointComponent &component, Visitor &&visitor ) -> void {
        VisitPhysicsFields( static_cast<JointComponent &>( component ), visitor );
        MKT_PHYSICS_FIELD( "HingeAEntityID", GetHingeAEntityID, SetHingeAEntityID );
        MKT_PHYSICS_FIELD( "HingeBEntityID", GetHingeBEntityID, SetHingeBEntityID );
        MKT_PHYSICS_FIELD( "Ratio", GetRatio, SetRatio );
    }

    template<typename Visitor>
    auto VisitPhysicsFields( RackAndPinionJointComponent &component, Visitor &&visitor ) -> void {
        VisitPhysicsFields( static_cast<JointComponent &>( component ), visitor );
        MKT_PHYSICS_FIELD( "HingeEntityID", GetHingeEntityID, SetHingeEntityID );
        MKT_PHYSICS_FIELD( "SliderEntityID", GetSliderEntityID, SetSliderEntityID );
        MKT_PHYSICS_FIELD( "Ratio", GetRatio, SetRatio );
    }

    template<typename Visitor>
    auto VisitPhysicsFields( PulleyJointComponent &component, Visitor &&visitor ) -> void {
        VisitPhysicsFields( static_cast<JointComponent &>( component ), visitor );
        MKT_PHYSICS_FIELD( "FixedPointA", GetFixedPointA, SetFixedPointA );
        MKT_PHYSICS_FIELD( "FixedPointB", GetFixedPointB, SetFixedPointB );
        MKT_PHYSICS_FIELD( "Ratio", GetRatio, SetRatio );
        MKT_PHYSICS_FIELD( "MinLength", GetMinLength, SetMinLength );
        MKT_PHYSICS_FIELD( "MaxLength", GetMaxLength, SetMaxLength );
    }

    template<typename Visitor>
    auto VisitPhysicsFields( CharacterControllerComponent &component, Visitor &&visitor ) -> void {
        MKT_PHYSICS_FIELD( "ControllerType", GetControllerType, SetControllerType );
        MKT_PHYSICS_FIELD( "Radius", GetRadius, SetRadius );
        MKT_PHYSICS_FIELD( "HalfHeight", GetHalfHeight, SetHalfHeight );
        MKT_PHYSICS_FIELD( "MaxSlopeAngle", GetMaxSlopeAngle, SetMaxSlopeAngle );
        MKT_PHYSICS_FIELD( "MaxStepHeight", GetMaxStepHeight, SetMaxStepHeight );
        MKT_PHYSICS_FIELD( "MaxStrength", GetMaxStrength, SetMaxStrength );
        MKT_PHYSICS_FIELD( "GravityFactor", GetGravityFactor, SetGravityFactor );
        MKT_PHYSICS_FIELD( "StickToFloor", DoesStickToFloor, SetStickToFloor );
        MKT_PHYSICS_FIELD( "Enabled", IsEnabled, SetEnabled );
    }

    template<typename Visitor>
    auto VisitPhysicsFields( RagdollComponent &component, Visitor &&visitor ) -> void {
        MKT_PHYSICS_FIELD( "SkeletonPath", GetSkeleton, SetSkeleton );
        MKT_PHYSICS_FIELD( "BoneMappings", GetBoneMappings, SetBoneMappings );
        MKT_PHYSICS_FIELD( "SimulationBlend", GetSimulationBlend, SetSimulationBlend );
        MKT_PHYSICS_FIELD( "Enabled", IsEnabled, SetEnabled );
    }

    template<typename Visitor>
    auto VisitPhysicsFields( SoftBodyComponent &component, Visitor &&visitor ) -> void {
        MKT_PHYSICS_FIELD( "ModelPath", GetModel, SetModel );
        MKT_PHYSICS_FIELD( "MeshIndex", GetMeshIndex, SetMeshIndex );
        MKT_PHYSICS_FIELD( "VertexMass", GetVertexMass, SetVertexMass );
        MKT_PHYSICS_FIELD( "GravityFactor", GetGravityFactor, SetGravityFactor );
        MKT_PHYSICS_FIELD( "LinearDamping", GetLinearDamping, SetLinearDamping );
        MKT_PHYSICS_FIELD( "Pressure", GetPressure, SetPressure );
        MKT_PHYSICS_FIELD( "EdgeCompliance", GetEdgeCompliance, SetEdgeCompliance );
        MKT_PHYSICS_FIELD( "PinnedVertices", GetPinnedVertices, SetPinnedVertices );
        MKT_PHYSICS_FIELD( "Enabled", IsEnabled, SetEnabled );
    }

    template<typename Visitor>
    auto VisitPhysicsFields( ClothComponent &component, Visitor &&visitor ) -> void {
        VisitPhysicsFields( static_cast<SoftBodyComponent &>( component ), visitor );
        MKT_PHYSICS_FIELD( "BendCompliance", GetBendCompliance, SetBendCompliance );
        MKT_PHYSICS_FIELD( "Drag", GetDrag, SetDrag );
        MKT_PHYSICS_FIELD( "Lift", GetLift, SetLift );
        MKT_PHYSICS_FIELD( "WindVelocity", GetWindVelocity, SetWindVelocity );
        MKT_PHYSICS_FIELD( "WindGustStrength", GetWindGustStrength, SetWindGustStrength );
    }

    template<typename Visitor>
    auto VisitPhysicsFields( WindAreaComponent &component, Visitor &&visitor ) -> void {
        MKT_PHYSICS_FIELD( "Shape", GetShape, SetShape );
        MKT_PHYSICS_FIELD( "Velocity", GetVelocity, SetVelocity );
        MKT_PHYSICS_FIELD( "Dimensions", GetDimensions, SetDimensions );
        MKT_PHYSICS_FIELD( "Falloff", GetFalloff, SetFalloff );
        MKT_PHYSICS_FIELD( "GustStrength", GetGustStrength, SetGustStrength );
        MKT_PHYSICS_FIELD( "GustFrequency", GetGustFrequency, SetGustFrequency );
        MKT_PHYSICS_FIELD( "GustWavelength", GetGustWavelength, SetGustWavelength );
        MKT_PHYSICS_FIELD( "Turbulence", GetTurbulence, SetTurbulence );
        MKT_PHYSICS_FIELD( "Seed", GetSeed, SetSeed );
        MKT_PHYSICS_FIELD( "Enabled", IsEnabled, SetEnabled );
    }

    template<typename Visitor>
    auto VisitPhysicsFields( BuoyancyComponent &component, Visitor &&visitor ) -> void {
        MKT_PHYSICS_FIELD( "WaterVolumeEntityID", GetWaterVolumeEntityID, SetWaterVolumeEntityID );
        MKT_PHYSICS_FIELD( "BuoyancyFactor", GetBuoyancyFactor, SetBuoyancyFactor );
        MKT_PHYSICS_FIELD( "LinearDrag", GetLinearDrag, SetLinearDrag );
        MKT_PHYSICS_FIELD( "AngularDrag", GetAngularDrag, SetAngularDrag );
        MKT_PHYSICS_FIELD( "Enabled", IsEnabled, SetEnabled );
    }

    template<typename Visitor>
    auto VisitPhysicsFields( WaterVolumeComponent &component, Visitor &&visitor ) -> void {
        MKT_PHYSICS_FIELD( "HalfExtents", GetHalfExtents, SetHalfExtents );
        MKT_PHYSICS_FIELD( "SurfaceOffset", GetSurfaceOffset, SetSurfaceOffset );
        MKT_PHYSICS_FIELD( "Density", GetDensity, SetDensity );
        MKT_PHYSICS_FIELD( "FlowVelocity", GetFlowVelocity, SetFlowVelocity );
        MKT_PHYSICS_FIELD( "WaterSurfaceEntityID", GetWaterSurfaceEntityID, SetWaterSurfaceEntityID );
        MKT_PHYSICS_FIELD( "Enabled", IsEnabled, SetEnabled );
    }

    template<typename Visitor>
    auto VisitPhysicsFields( VehicleWheelDescription &component, Visitor &&visitor ) -> void {
        ( void )visitor( "Position", component.mPosition );
        ( void )visitor( "SuspensionDirection", component.mSuspensionDirection );
        ( void )visitor( "Radius", component.mRadius );
        ( void )visitor( "Width", component.mWidth );
        ( void )visitor( "SuspensionMin", component.mSuspensionMin );
        ( void )visitor( "SuspensionMax", component.mSuspensionMax );
        ( void )visitor( "SuspensionFrequency", component.mSuspensionFrequency );
        ( void )visitor( "SuspensionDamping", component.mSuspensionDamping );
        ( void )visitor( "MaxSteerAngle", component.mMaxSteerAngle );
        ( void )visitor( "MaxBrakeTorque", component.mMaxBrakeTorque );
        ( void )visitor( "MaxHandBrakeTorque", component.mMaxHandBrakeTorque );
        ( void )visitor( "Driven", component.mDriven );
        ( void )visitor( "Steered", component.mSteered );
    }

    template<typename Visitor>
    auto VisitPhysicsFields( JointSpringSettings &component, Visitor &&visitor ) -> void {
        ( void )visitor( "Frequency", component.mFrequency );
        ( void )visitor( "Damping", component.mDamping );
    }

    template<typename Visitor>
    auto VisitPhysicsFields( JointMotorSettings &component, Visitor &&visitor ) -> void {
        ( void )visitor( "Mode", component.mMode );
        ( void )visitor( "TargetVelocity", component.mTargetVelocity );
        ( void )visitor( "TargetPosition", component.mTargetPosition );
        ( void )visitor( "MaxForceOrTorque", component.mMaxForceOrTorque );
        ( void )visitor( "Spring", component.mSpring );
    }

    template<typename Visitor>
    auto VisitPhysicsFields( JointPathPoint &component, Visitor &&visitor ) -> void {
        ( void )visitor( "Position", component.mPosition );
        ( void )visitor( "Tangent", component.mTangent );
        ( void )visitor( "Normal", component.mNormal );
    }

    template<typename Visitor>
    auto VisitPhysicsFields( RagdollBoneDescription &component, Visitor &&visitor ) -> void {
        ( void )visitor( "BoneName", component.mBoneName );
        ( void )visitor( "BodyEntityID", component.mBodyEntityID );
        ( void )visitor( "JointEntityID", component.mJointEntityID );
    }

#undef MKT_PHYSICS_FIELD

    template<typename Visitor>
    auto ForEachJointComponent( Visitor &&visitor ) -> void {
        visitor.template operator()<DistanceJointComponent>( "DistanceJointComponent" );
        visitor.template operator()<FixedJointComponent>( "FixedJointComponent" );
        visitor.template operator()<PointJointComponent>( "PointJointComponent" );
        visitor.template operator()<HingeJointComponent>( "HingeJointComponent" );
        visitor.template operator()<SliderJointComponent>( "SliderJointComponent" );
        visitor.template operator()<ConeJointComponent>( "ConeJointComponent" );
        visitor.template operator()<SwingTwistJointComponent>( "SwingTwistJointComponent" );
        visitor.template operator()<SixDofJointComponent>( "SixDofJointComponent" );
        visitor.template operator()<PathJointComponent>( "PathJointComponent" );
        visitor.template operator()<GearJointComponent>( "GearJointComponent" );
        visitor.template operator()<RackAndPinionJointComponent>( "RackAndPinionJointComponent" );
        visitor.template operator()<PulleyJointComponent>( "PulleyJointComponent" );
    }

    template<typename Visitor>
    auto ForEachExtendedPhysicsComponent( Visitor &&visitor ) -> void {
        visitor.template operator()<HeightFieldColliderComponent>( "HeightFieldColliderComponent" );
        visitor.template operator()<CompoundColliderComponent>( "CompoundColliderComponent" );
        visitor.template operator()<CylinderColliderComponent>( "CylinderColliderComponent" );
        visitor.template operator()<TaperedCapsuleColliderComponent>( "TaperedCapsuleColliderComponent" );
        visitor.template operator()<TaperedCylinderColliderComponent>( "TaperedCylinderColliderComponent" );
        visitor.template operator()<PlaneColliderComponent>( "PlaneColliderComponent" );
        visitor.template operator()<DistanceJointComponent>( "DistanceJointComponent" );
        visitor.template operator()<FixedJointComponent>( "FixedJointComponent" );
        visitor.template operator()<PointJointComponent>( "PointJointComponent" );
        visitor.template operator()<HingeJointComponent>( "HingeJointComponent" );
        visitor.template operator()<SliderJointComponent>( "SliderJointComponent" );
        visitor.template operator()<ConeJointComponent>( "ConeJointComponent" );
        visitor.template operator()<SwingTwistJointComponent>( "SwingTwistJointComponent" );
        visitor.template operator()<SixDofJointComponent>( "SixDofJointComponent" );
        visitor.template operator()<PathJointComponent>( "PathJointComponent" );
        visitor.template operator()<GearJointComponent>( "GearJointComponent" );
        visitor.template operator()<RackAndPinionJointComponent>( "RackAndPinionJointComponent" );
        visitor.template operator()<PulleyJointComponent>( "PulleyJointComponent" );
        visitor.template operator()<CharacterControllerComponent>( "CharacterControllerComponent" );
        visitor.template operator()<RagdollComponent>( "RagdollComponent" );
        visitor.template operator()<SoftBodyComponent>( "SoftBodyComponent" );
        visitor.template operator()<ClothComponent>( "ClothComponent" );
        visitor.template operator()<WindAreaComponent>( "WindAreaComponent" );
        visitor.template operator()<BuoyancyComponent>( "BuoyancyComponent" );
        visitor.template operator()<WaterVolumeComponent>( "WaterVolumeComponent" );
    }

}// namespace mikoto::scene

#endif// MIKOTO_SCENE_UTILITY_HH
