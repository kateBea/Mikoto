/**
 * Math.hh
 * Created by kate on 10/6/23.
 * */

#ifndef MIKOTO_MATH_HH
#define MIKOTO_MATH_HH

// Third-Party Libraries
#include "glm/glm.hpp"

// Project Headers
#include "Common.hh"
#include "Types.hh"

namespace Mikoto::Math {
    /**
     * Defines a position vector
     * */
    MKT_NODISCARD inline auto MakePos(float x, float y, float z) -> glm::vec4 {
        return { x, y, z, 1.0f };
    }

    /**
     * Defines a direction vector
     * */
    MKT_NODISCARD inline auto MakeDir(float x, float y, float z) -> glm::vec4 {
        return { x, y, z, .0f };
    }
}

#endif // MIKOTO_MATH_HH
