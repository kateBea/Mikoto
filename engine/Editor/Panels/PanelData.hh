//
// Created by kate on 6/27/23.
//

#ifndef KATE_ENGINE_PANEL_DATA_HH
#define KATE_ENGINE_PANEL_DATA_HH

#include <glm/glm.hpp>

#include <Utility/Common.hh>
#include <Scene/Scene.hh>
#include <Renderer/Buffers/FrameBuffer.hh>

namespace Mikoto {
    struct SettingsPanelData {
        glm::vec4 ClearColor{};
        bool VerticalSyncEnabled{ true };
        bool RenderWireframeMode{ false };
    };

    struct ScenePanelData {
        glm::vec4 ClearColor{};
        bool VerticalSyncEnabled{ true };
        bool RenderWireframeMode{ false };

        float ViewPortWidth{};
        float ViewPortHeight{};

        // TODO: remove, port to the opengl renderer (only relevant here when rendering with opengl)
        std::shared_ptr<FrameBuffer> SceneFrameBuffer{};
        std::shared_ptr<Scene> Viewport{};
    };

    struct StatsPanelData {

    };
}

#endif//KATE_ENGINE_PANEL_DATA_HH