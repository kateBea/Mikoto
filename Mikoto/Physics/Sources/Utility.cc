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

#include <glm/gtx/compatibility.hpp>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Vehicle/MotorcycleController.h>
#include <Jolt/Physics/Vehicle/VehicleCollisionTester.h>
#include <Jolt/Physics/Vehicle/WheeledVehicleController.h>
#include <Jolt/Physics/Vehicle/TrackedVehicleController.h>

#include <Core/Profiler.hh>

#include <Logging/Logger.hh>

#include <Physics/Utility.hh>
#include <Physics/PhysicsWorld.hh>

#include <Scene/Component.hh>

namespace mikoto::physics {

    using namespace core;
    using namespace scene;

    auto GetVehicleWheels( const VehicleComponent& vehicle ) -> eastl::vector<VehicleWheelDescription> {
        if ( !vehicle.GetWheelDescriptions().empty() ) {
            return vehicle.GetWheelDescriptions();
        }

        eastl::vector<VehicleWheelDescription> wheels{};
        const auto type{ vehicle.GetVehicleType() };
        const u32 count{ type == VehicleType::eTracked ? 18u : type == VehicleType::eMotorcycle ? 2u
                                                                                                : 4u };

        for ( u32 i{}; i < count; ++i ) {
            VehicleWheelDescription wheel{};
            wheel.mRadius = vehicle.mWheelRadius;
            wheel.mWidth = vehicle.mWheelWidth;
            wheel.mSuspensionMin = vehicle.mSuspensionMin;
            wheel.mSuspensionMax = vehicle.mSuspensionMax;
            wheel.mSuspensionFrequency = vehicle.mSuspensionFrequency;
            wheel.mSuspensionDamping = vehicle.mSuspensionDamping;
            wheel.mPosition = { i % 2 == 0 ? vehicle.mHalfTrack : -vehicle.mHalfTrack,
                vehicle.mWheelAnchorHeight, i < 2 ? vehicle.mHalfWheelbase : -vehicle.mHalfWheelbase };
            wheel.mSteered = i < 2;
            wheel.mDriven = vehicle.mFourWheelDrive || i < 2;
            wheel.mMaxSteerAngle = glm::radians( vehicle.mMaxSteerDegrees );
            wheel.mMaxHandBrakeTorque = i < 2 ? 0.0f : 4000.0f;

            if ( type == VehicleType::eMotorcycle ) {
                // MotorcycleTest: caster on the front fork, rear-wheel drive and separate brakes.
                wheel.mPosition = { 0, vehicle.mWheelAnchorHeight, i == 0 ? vehicle.mHalfWheelbase : -vehicle.mHalfWheelbase };
                wheel.mSteered = i == 0;
                wheel.mDriven = i == 1;
                wheel.mMaxBrakeTorque = i == 0 ? 500.0f : 250.0f;
                wheel.mMaxHandBrakeTorque = 0;

                if ( i == 0 ) {
                    wheel.mSuspensionFrequency *= .75f;
                    wheel.mSuspensionDirection = glm::normalize( float3{ 0, -1, glm::tan( glm::radians( 30.0f ) ) } );
                }
            }
            else if ( type == VehicleType::eTracked ) {
                // TankTest: nine wheels per track; end wheels have no suspension travel.
                constexpr float z[]{ 2.95f, 2.1f, 1.4f, .7f, 0, -.7f, -1.4f, -2.1f, -2.75f };
                const auto index{ i % 9 };
                const bool end{ index == 0 || index == 8 };
                wheel.mPosition = { i < 9 ? vehicle.mHalfTrack : -vehicle.mHalfTrack,
                    vehicle.mWheelAnchorHeight + ( end ? 0.0f : -.3f ), z[index] };

                if ( end ) {
                    wheel.mSuspensionMax = wheel.mSuspensionMin;
                }

                wheel.mSteered = false;
            }

            wheels.push_back( wheel );
        }

        return wheels;
    }

    auto CreateVehicleConstraint( JPH::Body& body, const scene::VehicleComponent& vehicle ) -> JPH::Ref<JPH::VehicleConstraint> {
        // Based off Jolt's vehicle sample, virtual wheel contacts apply
        // suspension and tire impulses to one chassis, not four bodies (thats for the 6DOFVehicle one).
        // https://github.com/jrouwe/JoltPhysics/blob/master/Samples/Tests/Vehicle/VehicleConstraintTest.cpp
        JPH::VehicleConstraintSettings settings{};

        const auto wheels{ GetVehicleWheels( vehicle ) };
        const auto type{ vehicle.GetVehicleType() };
        settings.mMaxPitchRollAngle = glm::radians( vehicle.mMaxPitchRollDegrees );

        if ( type == VehicleType::eTracked ) {
            // Jolt Samples/Tests/Vehicle/TankTest.cpp (bundled Jolt revision).
            auto* tracked{ new JPH::TrackedVehicleControllerSettings{} };
            tracked->mEngine.mMaxTorque = vehicle.mEngineTorque;
            settings.mController = tracked;

            for ( u32 i{}; i < wheels.size(); ++i ) {
                const auto& source{ wheels[i] };
                auto* wheel{ new JPH::WheelSettingsTV{} };
                wheel->mPosition = { source.mPosition.x, source.mPosition.y, source.mPosition.z };
                wheel->mSuspensionDirection = JPH::Vec3{ source.mSuspensionDirection.x, source.mSuspensionDirection.y, source.mSuspensionDirection.z }.Normalized();
                wheel->mRadius = source.mRadius;
                wheel->mWidth = source.mWidth;
                wheel->mSuspensionMinLength = source.mSuspensionMin;
                wheel->mSuspensionMaxLength = source.mSuspensionMax;
                wheel->mSuspensionSpring.mFrequency = source.mSuspensionFrequency;
                wheel->mSuspensionSpring.mDamping = source.mSuspensionDamping;
                const auto trackIndex{ source.mPosition.x >= 0 ? 0 : 1 };
                auto& track{ tracked->mTracks[trackIndex] };
                track.mWheels.push_back( i );
                track.mDrivenWheel = i;
                settings.mWheels.push_back( wheel );
            }

            JPH::Ref<JPH::VehicleConstraint> constraint{ new JPH::VehicleConstraint{ body, settings } };
            constraint->SetVehicleCollisionTester( new JPH::VehicleCollisionTesterRay{ Layers::MOVING } );

            return constraint;
        }

        for ( const auto& source: wheels ) {
            auto* wheel{ new JPH::WheelSettingsWV{} };
            wheel->mPosition = { source.mPosition.x, source.mPosition.y, source.mPosition.z };
            wheel->mSuspensionDirection = JPH::Vec3{ source.mSuspensionDirection.x, source.mSuspensionDirection.y, source.mSuspensionDirection.z }.Normalized();
            wheel->mSteeringAxis = -wheel->mSuspensionDirection;
            wheel->mRadius = source.mRadius;
            wheel->mWidth = source.mWidth;
            wheel->mSuspensionMinLength = source.mSuspensionMin;
            wheel->mSuspensionMaxLength = source.mSuspensionMax;
            wheel->mSuspensionSpring.mFrequency = source.mSuspensionFrequency;
            wheel->mSuspensionSpring.mDamping = source.mSuspensionDamping;
            wheel->mMaxSteerAngle = source.mSteered ? source.mMaxSteerAngle : 0.0f;
            wheel->mMaxBrakeTorque = source.mMaxBrakeTorque;
            wheel->mMaxHandBrakeTorque = source.mMaxHandBrakeTorque;

            settings.mWheels.push_back( wheel );
        }

        // Differentials
        JPH::WheeledVehicleControllerSettings* controller{};

        if ( type == VehicleType::eMotorcycle ) {
            // MotorcycleTest's transmission and primary/final drive ratios.
            auto* motorcycle{ new JPH::MotorcycleControllerSettings{} };
            motorcycle->mEngine.mMinRPM = 1000;
            motorcycle->mEngine.mMaxRPM = 10000;
            motorcycle->mTransmission.mShiftDownRPM = 2000;
            motorcycle->mTransmission.mShiftUpRPM = 8000;
            motorcycle->mTransmission.mGearRatios = { 2.27f, 1.63f, 1.3f, 1.09f, .96f, .88f };
            motorcycle->mTransmission.mReverseGearRatios = { -4.0f };
            motorcycle->mTransmission.mClutchStrength = 2;
            controller = motorcycle;
        }
        else {
            controller = new JPH::WheeledVehicleControllerSettings{};
        }

        settings.mController = controller;
        controller->mEngine.mMaxTorque = vehicle.mEngineTorque;

        for ( u32 i{}; i < wheels.size(); ++i ) {
            if ( !wheels[i].mDriven ) {
                continue;
            }

            JPH::VehicleDifferentialSettings differential{};
            differential.mLeftWheel = -1;
            differential.mRightWheel = i;

            if ( type == VehicleType::eMotorcycle ) {
                differential.mDifferentialRatio = 1.93f * 40.0f / 16.0f;
            }
            else if ( i + 1 < wheels.size() && wheels[i + 1].mDriven ) {
                differential.mLeftWheel = i++;
                differential.mRightWheel = i;
            }

            controller->mDifferentials.push_back( differential );
        }

        for ( auto& differential: controller->mDifferentials ) {
            differential.mEngineTorqueRatio = 1.0f / controller->mDifferentials.size();
        }

        // Anti rollbars
        for ( core::u32 axle{}; type == VehicleType::eWheeled && axle < wheels.size() / 2; ++axle ) {
            JPH::VehicleAntiRollBar bar{};
            bar.mLeftWheel = axle * 2;
            bar.mRightWheel = axle * 2 + 1;
            settings.mAntiRollBars.push_back( bar );
        }

        // Create collition testers
        JPH::Ref<JPH::VehicleConstraint> constraint{ new JPH::VehicleConstraint{ body, settings } };

        if ( vehicle.mCylinderWheelCast || type == VehicleType::eMotorcycle ) {
            constraint->SetVehicleCollisionTester( new JPH::VehicleCollisionTesterCastCylinder{ Layers::MOVING, type == VehicleType::eMotorcycle ? 1.0f : .1f } );
        }
        else {
            constraint->SetVehicleCollisionTester( new JPH::VehicleCollisionTesterRay{ Layers::MOVING } );
        }

        // Jolt's demo preserves its historical longitudinal grip with a 10x
        // multiplier. Keep this explicit and per vehicle, not a global change.
        as<JPH::WheeledVehicleController*>( constraint->GetController() )->SetTireMaxImpulseCallback( [scale = vehicle.mLongitudinalImpulseScale]( JPH::uint, float& longitudinal, float& lateral, float suspension, float longitudinalFriction, float lateralFriction, float, float, float ) {
            longitudinal = scale * longitudinalFriction * suspension;
            lateral = lateralFriction * suspension;
        } );

        return constraint;
    }

    auto GetJoltMotionType( scene::RigidBodyComponent::BodyType bodyType ) -> JPH::EMotionType {
        MKT_BEGIN_PROFILER();

        switch ( bodyType ) {
            case scene::RigidBodyComponent::BodyType::eStatic:
                return JPH::EMotionType::Static;
            case scene::RigidBodyComponent::BodyType::eKinematic:
                return JPH::EMotionType::Kinematic;
            default:;
        }

        return JPH::EMotionType::Dynamic;
    }

    bool AssertFailedImpl( const char* inExpression, const char* inMessage, const char* inFile, core::u32 inLine ) {
        MKT_BEGIN_PROFILER();

        // Print to the TTY
        MKT_CORE_LOGGER_DEBUG( "inExpression: {}", inExpression );
        MKT_CORE_LOGGER_DEBUG( "inMessage: {}", inMessage ? inMessage : "" );
        MKT_CORE_LOGGER_DEBUG( "inFile: {}", inFile );
        MKT_CORE_LOGGER_DEBUG( "inLine: {}", inLine );

        // Breakpoint
        return true;
    }

}// namespace mikoto::physics
