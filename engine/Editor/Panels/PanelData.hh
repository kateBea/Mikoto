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
        float EditorCameraMovementSpeed{};
        float EditorCameraRotationSpeed{};
        float NearPlane{ 0.1f };
        float FarPlane{ 1000.0f };
        float FieldOfView{ 45.0f };
        bool WantXAxisRotation{ true };
        bool WantYAxisRotation{ true };
        bool VerticalSyncEnabled{ true };
        bool RenderWireframeMode{ false };
    };

    struct ScenePanelData {
        glm::vec4 ClearColor{};
        float ViewPortWidth{};
        float ViewPortHeight{};
        // TODO: look into making this unique_ptr?
        std::shared_ptr<Scene> Viewport{};
    };
}

#endif // MIKOTO_PANEL_DATA_HH