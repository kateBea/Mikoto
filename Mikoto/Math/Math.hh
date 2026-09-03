//    Copyright 2025 ケイト
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

#ifndef MIKOTO_MATH_HH
#define MIKOTO_MATH_HH

// I love Windows.h defining min and max macros that break everything
#include <Platform/PlatformWin32.hh>

#include <vector>
#include <numbers>
#include <algorithm>
#include <type_traits>

#include <EASTL/span.h>
#include <EASTL/vector.h>
#include <EASTL/utility.h>
#include <EASTL/algorithm.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.inl>

#include <Core/Core.hh>
#include <Core/Types.hh>

namespace mikoto::math::constants {

    using namespace mikoto::core;

    template<typename T>
    MKT_NODISCARD constexpr auto Identity() -> decltype(auto) {
        return glm::identity<T>();
    }

    // Math Constants
    inline constexpr auto kPi{ std::numbers::pi_v<double> };

    inline constexpr auto max_u64{ std::numbers::pi_v<double> };

    inline constexpr auto kUnitVectorX{ core::float3{ 1.0f, 0.0f, 0.0f }};
    inline constexpr auto kUnitVectorY{ core::float3{ 0.0f, 1.0f, 0.0f } };
    inline constexpr auto kUnitVectorZ{ core::float3{ 0.0f, 0.0f, 1.0f } };
    inline constexpr auto kIdentityMat{ Identity<core::float4x4>() };
}

namespace mikoto::math {

    using namespace mikoto::core;

    MKT_NODISCARD inline auto MakePos( float x, float y, float z ) -> core::float4 {
        return { x, y, z, 1.0f };
    }

    MKT_NODISCARD inline auto MakeDir( float x, float y, float z ) -> core::float4 {
        return { x, y, z, .0f };
    }

    MKT_NODISCARD inline auto Round( const double value, const core::usize decimalsCount ) -> double {
        const double factor{ glm::pow( 10, decimalsCount ) };
        return glm::round( value * factor ) / factor;
    }

    template<typename T>
    MKT_NODISCARD auto Min( const T& a, const T& b ) -> const T& {
        return  b < a ? b : a;
    }

    template<typename T>
    MKT_NODISCARD auto Max( const T& a, const T& b ) -> const T& {
        return  a < b ? b : a;
    }

    template<typename T>
    MKT_NODISCARD inline auto Rotate(T& mat, const float3& angle, float degrees) -> T {
        return glm::rotate( mat, glm::radians( degrees ), angle );
    }

    MKT_NODISCARD inline auto DecomposeTransform( const glm::mat4& transform, glm::vec3& translation,
                                    glm::vec3& rotation, glm::vec3& scale ) -> bool {
        // From glm::decompose in matrix_decompose.inl

        using namespace glm;
        using T = float;

        mat4 LocalMatrix( transform );

        // Normalize the matrix.
        if ( epsilonEqual( LocalMatrix[3][3], static_cast<float>( 0 ), epsilon<T>() ) )
            return false;

        // First, isolate perspective.  This is the messiest.
        if (
                epsilonNotEqual( LocalMatrix[0][3], static_cast<T>( 0 ), epsilon<T>() ) ||
                epsilonNotEqual( LocalMatrix[1][3], static_cast<T>( 0 ), epsilon<T>() ) ||
                epsilonNotEqual( LocalMatrix[2][3], static_cast<T>( 0 ), epsilon<T>() ) ) {
            // Clear the perspective partition
            LocalMatrix[0][3] = LocalMatrix[1][3] = LocalMatrix[2][3] = static_cast<T>( 0 );
            LocalMatrix[3][3] = static_cast<T>( 1 );
        }

        // Next take care of translation (easy).
        translation = vec3( LocalMatrix[3] );
        LocalMatrix[3] = vec4( 0, 0, 0, LocalMatrix[3].w );

        vec3 Row[3], Pdum3;

        // Now get scale and shear.
        for ( length_t i = 0; i < 3; ++i )
            for ( length_t j = 0; j < 3; ++j )
                Row[i][j] = LocalMatrix[i][j];

        // Compute X scale factor and normalize first row.
        scale.x = length( Row[0] );
        Row[0] = detail::scale( Row[0], static_cast<T>( 1 ) );
        scale.y = length( Row[1] );
        Row[1] = detail::scale( Row[1], static_cast<T>( 1 ) );
        scale.z = length( Row[2] );
        Row[2] = detail::scale( Row[2], static_cast<T>( 1 ) );

        // At this point, the matrix (in rows[]) is orthonormal.
        // Check for a coordinate system flip.  If the determinant
        // is -1, then negate the matrix and the scaling factors.
#if 0
		Pdum3 = cross(Row[1], Row[2]); // v3Cross(row[1], row[2], Pdum3);
		if (dot(Row[0], Pdum3) < 0)
		{
			for (length_t i = 0; i < 3; i++)
			{
				scale[i] *= static_cast<T>(-1);
				Row[i] *= static_cast<T>(-1);
			}
		}
#endif

        rotation.y = asin( -Row[0][2] );
        if ( cos( rotation.y ) != 0 ) {
            rotation.x = atan2( Row[1][2], Row[2][2] );
            rotation.z = atan2( Row[0][1], Row[0][0] );
        } else {
            rotation.x = atan2( -Row[2][0], Row[1][1] );
            rotation.z = 0;
        }

        return true;
    }

    auto Decompose( const core::float4x4& transform, core::float3& translation, core::float3& rotation, core::float3& scale ) -> void;
    auto Recompose( core::float4x4& transform, const core::float3& translation, const core::float3& rotation, const core::float3& scale ) -> void;

    // Rotates the object around the standard basis axis [1, 0, 0], [0, 1, 0], [0, 0, 1]
    // which might not be what you want, often times you want to rotate the vertices
    // around what is supposed to be the center of the whole object.
    // For example the way you'd do it for 2D objects (for not complicated morphs) you sum all vertices and divide by count
    // this way you get the Z vector you are supposed to rotate the mesh around, you get a proper rotation
    // around the center of the mesh even if it has like 2 tringles (a rectangle)
    MKT_NODISCARD auto RecomputeTransform( const core::float3& position, const core::float3& size, const core::float3& angles ) -> core::float4x4;
    MKT_NODISCARD auto RecomputeTransform( const core::float3& position, const core::float3& scale, const core::float3& angles, const core::float3& pivot ) -> core::float4x4;

    MKT_NODISCARD auto Floor(double value) -> double;

    MKT_NODISCARD auto Log2(double value) -> double;

    MKT_NODISCARD auto ToRadians(double value) -> double;
    MKT_NODISCARD auto ToDegrees(double value) -> double;

    MKT_NODISCARD auto Abs(double value) -> double;

    MKT_NODISCARD auto Lerp(float a, float b, float f) -> double;

    MKT_NODISCARD auto NextPowerOf2( core::u32 value ) -> core::u32;

    auto DumpMat4FList( eastl::span<const core::float4x4> matrices ) -> void;
    auto DumpMat4FListBeautify( eastl::span<const core::float4x4> matrices ) -> void;

    template<typename T, typename... Ts>
    auto Max( T first, Ts... args ) -> eastl::common_type_t<T, Ts...> {
        using ReturnT = std::common_type_t<T, Ts...>;
        ReturnT result{ first };
        ( ( result = glm::max( result, static_cast<ReturnT>( args ) ) ), ... );
        return result;
    }

    template<typename T, typename... Ts>
    auto Min( T first, Ts... args ) -> eastl::common_type_t<T, Ts...> {
        using ReturnT = std::common_type_t<T, Ts...>;
        ReturnT result{ first };
        ( ( result = glm::min( result, static_cast<ReturnT>( args ) ) ), ... );
        return result;
    }

    template<typename T>
    MKT_NODISCARD auto IsBetween( const T& value, const T& lowerBound, const T& upperBound ) -> bool {
        return value >= lowerBound && value <= upperBound;
    }

    template<typename T>
    MKT_NODISCARD auto Clamp( const T& value, const T& min, const T& max ) -> T {
        return glm::max( min, glm::min( value, max ) );
    }
}

#include <Math/Math.inl>

#endif //MIKOTO_MATH_HH