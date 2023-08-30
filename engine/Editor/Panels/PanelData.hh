/**
 * PanelData.hh
 * Created by kate on 6/27/23.
 * */

#ifndef MIKOTO_PANEL_DATA_HH
#define MIKOTO_PANEL_DATA_HH

// C++ Standard Library
#include <memory>

// Third-Party Libraries
#include <glm/glm.hpp>

// Project Headers
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
        float ViewPortWidth{};
        float ViewPortHeight{};
        std::shared_ptr<Scene> Viewport{};
        bool VerticalSyncEnabled{ true };
        bool RenderWireframeMode{ false };
    };

    struct StatsPanelData {

    };
}

#endif // MIKOTO_PANEL_DATA_HH