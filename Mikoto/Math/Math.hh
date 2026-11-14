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

#ifndef MIKOTO_MATH_UTILITY_HH
#define MIKOTO_MATH_UTILITY_HH

#include <vector>
#include <numbers>
#include <algorithm>
#include <type_traits>

#include <EASTL/utility.h>
#include <EASTL/span.h>
#include <EASTL/vector.h>
#include <EASTL/algorithm.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Platform/Windows/Windows.hh>

#include <cmath>
#include <random>

#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/compatibility.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <Math/Bounds.hh>

// https://www.ryanjuckett.com/damped-springs/

namespace mikoto::math {

    namespace constants {

        /**
         * @brief Constructs an identity value for the requested GLM type.
         * @tparam T Value type used by this operation.
         * @returns Identity matrix or quaternion of type T.
         */
        template<typename T>
        MKT_NODISCARD constexpr auto Identity() -> decltype(auto) {
            return glm::identity<T>();
        }

        // Math Constants
        inline constexpr auto kPi{ std::numbers::pi_v<double> };

        inline constexpr auto kUnitVectorX{ core::float3{ 1.0f, 0.0f, 0.0f }};
        inline constexpr auto kUnitVectorY{ core::float3{ 0.0f, 1.0f, 0.0f } };
        inline constexpr auto kUnitVectorZ{ core::float3{ 0.0f, 0.0f, 1.0f } };
        inline constexpr auto kIdentityMat{ Identity<core::float4x4>() };

    }// namespace constants

    /**
     * @brief Constructs a homogeneous position with w equal to one.
     * @param x X component of the position.
     * @param y Y component of the position.
     * @param z Z component of the vector.
     * @returns Position vector (x, y, z, 1).
     */
    MKT_NODISCARD auto MakePos( core::f32 x, core::f32 y, core::f32 z ) -> core::float4;

    /**
     * @brief Constructs a homogeneous direction with w equal to zero.
     * @param x X component of the direction.
     * @param y Y component of the direction.
     * @param z Z component of the vector.
     * @returns Direction vector (x, y, z, 0).
     */
    MKT_NODISCARD auto MakeDir( core::f32 x, core::f32 y, core::f32 z ) -> core::float4;

    /**
     * @brief Rounds a value to a requested number of decimal places.
     * @param value Value to inspect or edit.
     * @param decimalsCount Number of decimal places to retain.
     * @returns Rounded value.
     */
    MKT_NODISCARD auto Round( core::f64 value, core::usize decimalsCount ) -> double;

    /**
     * @brief Selects the smallest supplied value.
     * @tparam T Value type used by this operation.
     * @param a First input value.
     * @param b Second input value.
     * @returns Reference to the smaller input.
     */
    template<typename T>
    MKT_NODISCARD auto Min( const T& a, const T& b ) -> const T& {
        return  b < a ? b : a;
    }

    /**
     * @brief Selects the largest supplied value.
     * @tparam T Value type used by this operation.
     * @param a First input value.
     * @param b Second input value.
     * @returns Reference to the larger input.
     */
    template<typename T>
    MKT_NODISCARD auto Max( const T& a, const T& b ) -> const T& {
        return  a < b ? b : a;
    }

    /**
     * @brief Selects the largest supplied value.
     * @tparam T Value type used by this operation.
     * @tparam Ts Types of the remaining values used to deduce the common result.
     * @param first First value participating in the comparison.
     * @param args Additional values forwarded to the operation.
     * @returns Largest supplied value.
     */
    template<typename T, typename... Ts>
    auto Max( T first, Ts... args ) -> eastl::common_type_t<T, Ts...> {
        using ReturnType = std::common_type_t<T, Ts...>;

        ReturnType result{ first };
        ( ( result = glm::max( result, static_cast<ReturnType>( args ) ) ), ... );
        return result;
    }

    /**
     * @brief Selects the smallest supplied value.
     * @tparam T Value type used by this operation.
     * @tparam Ts Types of the remaining values used to deduce the common result.
     * @param first First value participating in the comparison.
     * @param args Additional values forwarded to the operation.
     * @returns Smallest supplied value.
     */
    template<typename T, typename... Ts>
    auto Min( T first, Ts... args ) -> eastl::common_type_t<T, Ts...> {
        using ReturnType = std::common_type_t<T, Ts...>;
        ReturnType result{ first };

        ( ( result = glm::min( result, static_cast<ReturnType>( args ) ) ), ... );
        return result;
    }

    /**
     * @brief Builds a rotated copy of a matrix using an angle in degrees.
     * @tparam T Value type used by this operation.
     * @param mat Input matrix; it is not modified.
     * @param angle Rotation axis, expected to be normalized.
     * @param degrees Rotation angle in degrees.
     * @returns Rotated matrix; the input matrix is unchanged.
     */
    template<typename T>
    MKT_NODISCARD auto Rotate(T& mat, const core::float3& angle, float degrees) -> T {
        return glm::rotate( mat, glm::radians( degrees ), angle );
    }

    /**
     * @brief Tests integer divisibility by a nonzero divisor.
     * @tparam T Value type used by this operation.
     * @param value Value to inspect or edit.
     * @param divisor Nonzero integer divisor.
     * @returns True when division leaves no remainder.
     */
    template<typename T>
    MKT_NODISCARD constexpr auto IsMultiple(const T& value, const T& divisor) -> bool {
        return value % divisor == 0;
    }

    // Entity transforms use degrees in YXZ order. The exact matrix remains
    // authoritative when shear cannot be expressed as translation/rotation/scale.
    /**
     * @brief Extracts translation, YXZ rotation in degrees and signed scale.
     * @param transform Affine matrix to decompose or rebuild.
     * @param[out] translation Translation vector.
     * @param[out] rotation YXZ Euler rotation angles in degrees.
     * @param[out] scale Scale along the local axes.
     * @returns True on success; outputs remain unchanged on failure.
     */
    MKT_NODISCARD auto DecomposeTransform(
        const glm::mat4& transform,
        glm::vec3& translation,
        glm::vec3& rotation,
        glm::vec3& scale ) -> bool;

    /**
     * @brief Extracts editable transform fields, retaining the previous rotation on failure.
     * @param transform Affine matrix to decompose or rebuild.
     * @param[out] translation Translation vector.
     * @param[out] rotation YXZ Euler rotation angles in degrees.
     * @param[out] scale Scale along the local axes.
     */
    auto Decompose(
        const core::float4x4& transform,
        core::float3& translation,
        core::float3& rotation,
        core::float3& scale ) -> void;

    /**
     * @brief Rebuilds a matrix from translation, YXZ rotation in degrees and scale.
     * @param[out] transform Receives the recomposed affine matrix.
     * @param translation Translation vector.
     * @param rotation YXZ Euler rotation angles in degrees.
     * @param scale Scale along the local axes.
     */
    auto Recompose(
        core::float4x4& transform,
        const core::float3& translation,
        const core::float3& rotation,
        const core::float3& scale ) -> void;

    // Rotates the object around the standard basis axis [1, 0, 0], [0, 1, 0], [0, 0, 1]
    // which might not be what you want, often times you want to rotate the vertices
    // around what is supposed to be the center of the whole object.
    // For example the way you'd do it for 2D objects (for not complicated morphs) you sum all vertices and divide by count
    // this way you get the Z vector you are supposed to rotate the mesh around, you get a proper rotation
    // around the center of the mesh even if it has like 2 tringles (a rectangle)
    /**
     * @brief Builds an entity matrix using translation, Y * X * Z rotation and scale.
     * @param position Position in the coordinate space used by the caller.
     * @param size Scale factors along the local axes.
     * @param angles Euler angles in degrees, composed Y * X * Z.
     * @returns Composed affine transform.
     */
    MKT_NODISCARD auto RecomputeTransform(
        const core::float3& position,
        const core::float3& size,
        const core::float3& angles ) -> core::float4x4;

    /**
     * @brief Builds an entity matrix using translation, Y * X * Z rotation and scale.
     * @param position Position in the coordinate space used by the caller.
     * @param scale Scale along the local axes.
     * @param angles Euler angles in degrees, composed Y * X * Z.
     * @param pivot Local pivot about which rotation and scale are applied.
     * @returns Composed affine transform.
     */
    MKT_NODISCARD auto RecomputeTransform(
        const core::float3& position,
        const core::float3& scale,
        const core::float3& angles,
        const core::float3& pivot ) -> core::float4x4;

    /**
     * @brief Rounds a value toward negative infinity.
     * @param value Value to inspect or edit.
     * @returns Largest integral value not greater than the input.
     */
    MKT_NODISCARD auto Floor(double value) -> double;

    /**
     * @brief Computes a base-two logarithm.
     * @param value Value to inspect or edit.
     * @returns Base-two logarithm of the input.
     */
    MKT_NODISCARD auto Log2(double value) -> double;

    /**
     * @brief Computes a scalar's absolute value.
     * @param value Value to inspect or edit.
     * @returns Magnitude of the input.
     */
    MKT_NODISCARD auto Abs(double value) -> double;

    /**
     * @brief Linearly interpolates or extrapolates between two values.
     * @param a First input value.
     * @param b Second input value.
     * @param f Interpolation factor; values outside [0, 1] extrapolate.
     * @returns a + f * (b - a).
     */
    MKT_NODISCARD auto Lerp(float a, float b, float f) -> double;

    /**
     * @brief Converts an angle from degrees to radians.
     * @param value Angle in degrees.
     * @returns Angle in radians.
     */
    MKT_NODISCARD auto ToRadians(double value) -> double;

    /**
     * @brief Converts an angle from radians to degrees.
     * @param value Angle in radians.
     * @returns Angle in degrees.
     */
    MKT_NODISCARD auto ToDegrees(double value) -> double;

    /**
     * @brief Rounds an integer upward to a power-of-two size.
     * @param value Unsigned size to round upward.
     * @returns Next power of two; zero for zero input or unsigned overflow.
     */
    MKT_NODISCARD auto NextPowerOf2( core::u32 value ) -> core::u32;

    /**
     * @brief Writes a list of matrices to standard output.
     * @param matrices Matrices to write, in display order.
     */
    auto DumpMat4FList( eastl::span<const core::float4x4> matrices ) -> void;

    /**
     * @brief Writes a formatted list of matrices to standard output.
     * @param matrices Matrices to write, in display order.
     */
    auto DumpMat4FListBeautify( eastl::span<const core::float4x4> matrices ) -> void;

    /**
     * @brief Tests whether a value lies within inclusive bounds.
     * @tparam T Value type used by this operation.
     * @param value Value to inspect or edit.
     * @param lowerBound Lower limit of the requested interval.
     * @param upperBound Inclusive upper limit.
     * @returns True when lowerBound <= value <= upperBound.
     */
    template<typename T>
    MKT_NODISCARD auto IsBetween( const T& value, const T& lowerBound, const T& upperBound ) -> bool {
        return value >= lowerBound && value <= upperBound;
    }

    /**
     * @brief Limits a value to an inclusive interval.
     * @tparam T Value type used by this operation.
     * @param value Value to inspect or edit.
     * @param min Inclusive lower limit.
     * @param max Inclusive upper limit.
     * @returns Input clamped to [min, max].
     */
    template<typename T>
    MKT_NODISCARD auto Clamp( const T& value, const T& min, const T& max ) -> T {
        return glm::max( min, glm::min( value, max ) );
    }

    // Exact critically damped spring step for a target held constant during dt.
    // Velocity is persistent state, in units/second. No integration substeps are needed.
    /**
     * @brief Advances an exact critically damped spring toward a fixed target.
     * @param current Value at the start of this update.
     * @param target Target held constant during the update.
     * @param[in,out] velocity Persistent velocity in value units per second.
     * @param smoothTime Response time in seconds.
     * @param dt Time step in seconds.
     * @returns Updated value; velocity is written back for the next step.
     */
    MKT_NODISCARD auto SmoothDamp( float current, float target, float& velocity, float smoothTime, float dt ) -> float;

    /**
     * @brief Damps a radian-valued angle along its shortest wrapped displacement.
     * @param current Current angle in radians.
     * @param target Target angle in radians.
     * @param[in,out] velocity Persistent angular velocity in radians per second.
     * @param smoothTime Response time in seconds.
     * @param dt Time step in seconds.
     * @returns Updated angle in radians; angular velocity is written back.
     */
    MKT_NODISCARD auto SmoothDampAngle( float current, float target, float& velocity, float smoothTime, float dt ) -> float;

    /**
     * @brief Advances an exact critically damped spring toward a fixed target.
     * @param current Value at the start of this update.
     * @param target Target held constant during the update.
     * @param[in,out] velocity Persistent velocity in value units per second.
     * @param smoothTime Response time in seconds.
     * @param dt Time step in seconds.
     * @returns Updated value; velocity is written back for the next step.
     */
    MKT_NODISCARD auto SmoothDamp( const core::float3& current, const core::float3& target, core::float3& velocity, float smoothTime, float dt ) -> core::float3;

    using GizmoBounds = Bounds;

    /**
     * @brief Moves a transform's origin to its local gizmo pivot.
     * @param world Object world transform.
     * @param localCenter Pivot expressed in the object local coordinate system.
     * @returns World transform centered on localCenter.
     */
    auto MakeGizmoTransform( const core::float4x4& world, const core::float3& localCenter ) -> core::float4x4;

    /**
     * @brief Builds a rigid rotation handle at the supplied centered transform.
     * @param centeredWorld Object transform translated to the chosen pivot.
     * @return Unit-scale orientation and pivot position; scale/shear stay on the object.
     */
    auto MakeGizmoRotationFrame( const core::float4x4& centeredWorld ) -> core::float4x4;

    /**
     * @brief Converts an edited world gizmo to an entity's local matrix.
     * @param gizmo Edited world-space gizmo transform.
     * @param localCenter Pivot expressed in the object local coordinate system.
     * @param parentWorld Parent world transform.
     * @param[out] local Local matrix, written only when conversion succeeds.
     * @returns True when the parent is invertible.
     */
    auto GizmoToLocalTransform( const core::float4x4& gizmo, const core::float3& localCenter,
                                       const core::float4x4& parentWorld, core::float4x4& local ) -> bool;

    // Entity rotation fields are degrees, composed Y * X * Z. Keep imported
    // matrices, gizmo edits and quaternion setters in that same convention.
    /**
     * @brief Builds a Y * X * Z rotation matrix from Euler angles in degrees.
     * @param degrees Euler rotation angles in degrees.
     * @returns Homogeneous rotation matrix.
     */
    MKT_NODISCARD auto EntityRotationMatrix( const core::float3& degrees ) -> core::float4x4;

    /**
     * @brief Extracts YXZ Euler angles from a rotation matrix.
     * @param rotation YXZ Euler rotation angles in degrees.
     * @returns Angles in degrees.
     */
    MKT_NODISCARD auto EntityRotationDegrees( const core::float4x4& rotation ) -> core::float3;

    namespace random {

        /**
         * Returns a seed for random number generation
         * @returns seed
         */
        MKT_NODISCARD auto GetSeed() -> std::random_device&;

        /**
         * @brief Generates a uniform unsigned 64-bit integer.
         * @param seed Random device used to initialize this thread generator on its first call.
         * @returns Random value over the complete unsigned 64-bit range.
         */
        MKT_NODISCARD auto GetRandomUInt64(std::random_device& seed = GetSeed()) -> core::u64;

        /**
         * @brief Generates a uniform unsigned 32-bit integer.
         * @param seed Random device used to initialize this thread generator on its first call.
         * @returns Random value over the complete unsigned 32-bit range.
         */
        MKT_NODISCARD auto GetRandomUInt32(std::random_device& seed = GetSeed()) -> core::u32;

        /**
         * @brief Generates a uniform integer between inclusive bounds.
         * @param lowerBound Lower limit of the requested interval.
         * @param upperbound Inclusive upper limit.
         * @param seed Random device used to initialize this thread generator on its first call.
         * @returns Random integer in [lowerBound, upperbound].
         * @throws core::RuntimeException If lowerBound exceeds upperbound.
         */
        MKT_NODISCARD auto GetRandomInt(core::i32 lowerBound, core::i32 upperbound, std::random_device& seed = GetSeed()) -> core::i32;

        /**
         * @brief Generates a uniform real value within the requested interval.
         * @param lowerBound Lower limit of the requested interval.
         * @param upperbound Exclusive upper limit.
         * @param seed Random device used to initialize this thread generator on its first call.
         * @returns Random real value in [lowerBound, upperbound).
         * @throws core::RuntimeException If lowerBound exceeds upperbound.
         */
        MKT_NODISCARD auto GetRandomReal(double lowerBound, double upperbound, std::random_device& seed = GetSeed()) -> double;

        /**
         * @brief Generates a list of random integers.
         * @param length Number of values to generate.
         * @param lowerBound Lower limit of the requested interval.
         * @param upperbound Inclusive upper limit.
         * @param seed Random device used to initialize this thread generator on its first call.
         * @returns List containing length random integers.
         * @throws core::RuntimeException If lowerBound exceeds upperbound.
         */
        MKT_NODISCARD auto GetRandomIntNumberList(core::usize length, core::i32 lowerBound = -1000, core::i32 upperbound = 1000, std::random_device& seed = GetSeed()) -> std::vector<core::i32>;

        /**
         * @brief Generates a list of random real values.
         * @param length Number of values to generate.
         * @param lowerBound Lower limit of the requested interval.
         * @param upperbound Exclusive upper limit.
         * @param seed Random device used to initialize this thread generator on its first call.
         * @returns List containing length random real values.
         * @throws core::RuntimeException If lowerBound exceeds upperbound.
         */
        MKT_NODISCARD auto GetRandomRealNumberList(core::usize length, double lowerBound = -1000.0, double upperbound = 1000.0, std::random_device& seed = GetSeed()) -> std::vector<double>;

    }// namespace random

}// namespace mikoto::math

#endif// MIKOTO_MATH_UTILITY_HH
