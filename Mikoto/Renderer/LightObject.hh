//
// Created by zanet on 4/6/2025.
//

#ifndef LIGHTOBJECT_HH
#define LIGHTOBJECT_HH

#include <utility>

#include <glm/glm.hpp>

namespace Mikoto {
    class LightObject {
    public:

        template<typename... Args>
        auto SetColor(Args&&... args) -> void  {
            m_Color = glm::vec3(std::forward<Args>(args)...);
        }

    protected:
        glm::vec3 m_Color{};
    };

}

#endif //LIGHTOBJECT_HH
