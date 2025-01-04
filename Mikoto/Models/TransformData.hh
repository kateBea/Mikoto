//
// Created by kate on 1/3/25.
//

#ifndef TRANSFORMDATA_HH
#define TRANSFORMDATA_HH

#include "glm/glm.hpp"

namespace Mikoto {
    struct TransformData {
        glm::mat4 View{};
        glm::mat4 Projection{};
        glm::mat4 Transform{};
    };
}
#endif //TRANSFORMDATA_HH
