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

#ifndef MIKOTO_PHYSICS_UTILITY_HH
#define MIKOTO_PHYSICS_UTILITY_HH

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Vehicle/VehicleConstraint.h>

#include <Scene/Component.hh>

namespace mikoto::physics {

    /**
     * @brief  Backend factory; requires validated settings and a dynamic, non-sensor
     * chassis. The caller registers the constraint and its step listener with
     * the world, and removes both before destroying the chassis.
     * @param body Validated dynamic, non-sensor vehicle chassis.
     * @param settings Validated vehicle wheel, suspension and controller settings.
     * @returns Retained constraint; the caller registers it with the physics world.
     */
    auto CreateVehicleConstraint( JPH::Body& body, const scene::VehicleComponent& settings ) -> JPH::Ref<JPH::VehicleConstraint>;

    /** Resolve authored wheels, or the matching Jolt sample's default layout. */
    auto GetVehicleWheels( const scene::VehicleComponent& settings ) -> eastl::vector<scene::VehicleWheelDescription>;

    /**
     * @brief Converts a rigid-body type to Jolt's motion type.
     * @param bodyType Scene rigid-body motion type.
     * @returns Matching static, kinematic or dynamic motion type.
     */
    auto GetJoltMotionType( scene::RigidBodyComponent::BodyType bodyType ) -> JPH::EMotionType;

    // Callback for asserts, connect this to your own assert handler if you have one
    /**
     * @brief Logs a Jolt assertion and requests a debugger break.
     * @param inExpression Assertion expression reported by Jolt.
     * @param inMessage Additional assertion text from Jolt.
     * @param inFile Source file containing the failed assertion.
     * @param inLine Source line of the failed assertion.
     * @returns True to request a break after reporting the assertion.
     */
    bool AssertFailedImpl( const char* inExpression, const char* inMessage, const char* inFile, core::u32 inLine );

}// namespace mikoto::physics

#endif// MIKOTO_PHYSICS_UTILITY_HH
