/**
 * Math.hh
 * Created by kate on 10/6/23.
 * */

#ifndef MIKOTO_MATH_HH
#define MIKOTO_MATH_HH

#include <cmath>

// Third-Party Libraries
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.inl>

// Project Headers
#include <Common/Common.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto::Math {
    constexpr glm::vec3 UNIT_VECTOR_X{ 1.0f, 0.0f, 0.0f };
    constexpr glm::vec3 UNIT_VECTOR_Y{ 0.0f, 1.0f, 0.0f };
    constexpr glm::vec3 UNIT_VECTOR_Z{ 0.0f, 0.0f, 1.0f };

    constexpr glm::mat4 IDENTITY_MAT4{ glm::mat4(1.0) };

    /**
	 * Defines a position vector
	 * */
    MKT_UNUSED_FUNC MKT_NODISCARD inline auto MakePos( float x, float y, float z ) -> glm::vec4 {
        return { x, y, z, 1.0f };
    }

    /**
	 * Defines a direction vector
	 * */
    MKT_UNUSED_FUNC MKT_NODISCARD inline auto MakeDir( float x, float y, float z ) -> glm::vec4 {
        return { x, y, z, .0f };
    }

    MKT_NODISCARD inline auto Round( const double value, const Size decimalsCount ) -> double {
        const double factor{ std::pow( 10, decimalsCount ) };
        return std::round( value * factor ) / factor;
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


    /**
         * Computes the model matrix as in Translate * Ry * Rx * Rz * Scale (where R represents a
         * rotation in the desired axis. Rotation convention uses Tait-Bryan angles with axis order
         * Y(1), X(2), Z(3)
         * */
    MKT_NODISCARD inline auto RecomputeTransform( const glm::vec3& position, const glm::vec3& size, const glm::vec3& angles = glm::vec3( 0.0f ) ) -> glm::mat4 {
        // Compute scale matrix
        const glm::mat4 scale{ glm::scale( IDENTITY_MAT4, size ) };

        // Compute rotation matrix
        glm::mat4 rotation{ glm::rotate( IDENTITY_MAT4, ( float )glm::radians( angles.y ), UNIT_VECTOR_Y ) };
        rotation = glm::rotate( rotation, ( float )glm::radians( angles.x ), UNIT_VECTOR_X );
        rotation = glm::rotate( rotation, ( float )glm::radians( angles.z ), UNIT_VECTOR_Z );

        return glm::translate( IDENTITY_MAT4, position ) * rotation * scale;
    }

    template<typename T>
    MKT_NODISCARD inline auto IsBetween(const T& value, const T& lowerBound, const T& upperBound) -> bool {
        return value >= lowerBound && value <= upperBound;
    }

    template<typename T>
    MKT_NODISCARD inline auto Clamp(const T& value, const T& min, const T& max) -> T {
        return std::max(min, std::min(value, max));
    }
}// namespace Mikoto::Math

#endif// MIKOTO_MATH_HH
