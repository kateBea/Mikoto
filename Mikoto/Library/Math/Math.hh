/**
 * Math.hh
 * Created by kate on 10/6/23.
 * */

#ifndef MIKOTO_MATH_HH
#define MIKOTO_MATH_HH

#include <cmath>

// Third-Party Libraries
#include "glm/glm.hpp"

// Project Headers
#include <Common/Common.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto::Math {
    /**
     * Defines a position vector
     * */
    MKT_UNUSED_FUNC MKT_NODISCARD inline auto MakePos(float x, float y, float z) -> glm::vec4 {
        return { x, y, z, 1.0f };
    }

    /**
     * Defines a direction vector
     * */
    MKT_UNUSED_FUNC MKT_NODISCARD inline auto MakeDir(float x, float y, float z) -> glm::vec4 {
        return { x, y, z, .0f };
    }

    MKT_NODISCARD inline auto Round( const double value, const Size_T decimalsCount) -> double {
        const double factor{ std::pow(10, decimalsCount) };
        return std::round( value * factor ) / factor;
    }
}

#endif // MIKOTO_MATH_HH
