/**
 * Constants.hh
 * Created by kate on 9/9/23.
 * */

#ifndef MIKOTO_CONSTANTS_HH
#define MIKOTO_CONSTANTS_HH

// Third-Party Libraries
#include "glm/glm.hpp"

namespace Mikoto {
    constexpr glm::vec3 GLM_UNIT_VECTOR_X{ 1.0f, 0.0f, 0.0f };
    constexpr glm::vec3 GLM_UNIT_VECTOR_Y{ 0.0f, 1.0f, 0.0f };
    constexpr glm::vec3 GLM_UNIT_VECTOR_Z{ 0.0f, 0.0f, 1.0f };
    constexpr glm::mat4 GLM_IDENTITY_MAT4{ glm::mat4(1.0) };

}

#endif // MIKOTO_CONSTANTS_HH
