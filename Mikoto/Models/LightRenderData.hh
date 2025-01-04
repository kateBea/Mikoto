//
// Created by kate on 1/3/25.
//

#ifndef LIGHTRENDERDATA_HH
#define LIGHTRENDERDATA_HH

#define MAX_LIGHTS_PER_SCENE 200

#include <Models/LightTypeData.hh>

namespace Mikoto {
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
