//
// Created by kate on 1/4/25.
//

#ifndef RENDERERDRAWDATA_HH
#define RENDERERDRAWDATA_HH

#include "Scene/EditorCamera.hh"
#include "Scene/SceneCamera.hh"

namespace Mikoto {
    struct  RendererDrawData {
        const SceneCamera* RuntimeCamera{};
        const EditorCamera* StaticCamera{};
    };
}
#endif //RENDERERDRAWDATA_HH
