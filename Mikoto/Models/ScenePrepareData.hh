//
// Created by kate on 1/3/25.
//

#ifndef SCENEPREPAREDATA_HH
#define SCENEPREPAREDATA_HH

#include "Scene/EditorCamera.hh"
#include "Scene/SceneCamera.hh"

namespace Mikoto {
    struct ScenePrepareData {
        const SceneCamera* RuntimeCamera{};
        const EditorCamera* StaticCamera{};
    };
}
#endif //SCENEPREPAREDATA_HH
