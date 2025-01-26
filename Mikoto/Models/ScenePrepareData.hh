//
// Created by kate on 1/3/25.
//

#ifndef SCENEPREPAREDATA_HH
#define SCENEPREPAREDATA_HH

#include <Scene/Camera/SceneCamera.hh>

namespace Mikoto {
    struct ScenePrepareData {
        const SceneCamera* RuntimeCamera{};
        const SceneCamera* StaticCamera{};
        glm::vec4 CameraPosition{};
    };
}
#endif //SCENEPREPAREDATA_HH
