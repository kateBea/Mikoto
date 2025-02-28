//
// Created by kate on 1/3/25.
//

#ifndef LIGHTRENDERDATA_HH
#define LIGHTRENDERDATA_HH

#define MAX_LIGHTS_PER_SCENE 50

#include <glm/glm.hpp>

namespace Mikoto {
    struct DirectionalLight {
        // if Direction.w == 1, we use the lights position
        // to compute the rays directions
        glm::vec4 Direction{ 1.0f, 1.0f, 1.0f, 0.0f };
        glm::vec4 Position{ 1.0f, 1.0f, 1.0f, 1.0f };

        glm::vec4 Ambient{ 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec4 Diffuse{ 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec4 Specular{ 1.0f, 1.0f, 1.0f, 1.0f };
    };

    // Point lights for now
    struct PointLight {
        glm::vec4 Position{ 0.0f, 0.0f, 0.0f, 1.0f };

        glm::vec4 Ambient{ 1.0f, 1.0f, 1.0f, 0.1f };
        glm::vec4 Diffuse{ 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec4 Specular{ 1.0f, 1.0f, 1.0f, 0.1f };

        // x = intensity, y = radius
        glm::vec4 AttenuationParams{ 1.0f, 1.0f, 1.0f, 0.1f };

    };

    struct SpotLight {
        glm::vec4 Position{};
        glm::vec4 Direction{ 0.0f, -1.0f, 0.0f, 0.0f }; // facing down by default

        glm::vec4 Ambient{ 1.0f, 1.0f, 1.0f, 0.1f };
        glm::vec4 Diffuse{ 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec4 Specular{ 1.0f, 1.0f, 1.0f, 0.1f };

        // cutoff
        // x=cutOff, y=outerCutOff, z = intensity, w = radius
        glm::vec4 Params{ 0.2f, 0.4f, 0.0f, 0.0f };
    };

    /**
     * @brief Holds light related information.
     * Contains the relevant data specific for the three types of light:
     * directional light, spot light, point light. Used to store light information
     * in the light component.
     * */
    struct LightData {
        // Directional light
        DirectionalLight DireLightData{};

        // Point light
        PointLight PointLightDat{};

        // Spotlight
        SpotLight SpotLightData{};
    };
}
#endif //LIGHTRENDERDATA_HH
