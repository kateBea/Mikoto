/**
 * ScenePanel.hh
 * Created by kate on 6/27/23.
 * */

#ifndef MIKOTO_SCENE_PANEL_HH
#define MIKOTO_SCENE_PANEL_HH

// C++ Standard Library
#include <memory>

// Third-Party Libraries
#include <volk.h>

// Project Headers
#include <Assets/Texture.hh>
#include <Library/Utility/Types.hh>
#include <Panels/Panel.hh>
#include <Scene/Scene.hh>

namespace Mikoto {

    class EditorState;

    enum class GuizmoType {
        TRANSLATION,
        ROTATION,
        SCALE,
    };

    struct ScenePanelCreateInfo {
        UInt32 Width{};
        UInt32 Height{};

        TextureHandle DisplayTarget{};

        EditorState* State{};
    };

    class ScenePanel final : public Panel {
    public:
        explicit ScenePanel(const ScenePanelCreateInfo& createInfo);

        auto OnUpdate(float ts) -> void override;
        auto SetManipulation( GuizmoType mode ) -> void;

        ~ScenePanel() override = default;

        MKT_NODISCARD auto GetWidth() const -> float;
        MKT_NODISCARD auto GetHeight() const -> float;

    private:
        auto ShowUtilitiesOverlay() -> void;
        auto IsDisplayTextureValid() const -> bool;
        auto UpdateViewport() -> void;
        auto SetupManipulation() const -> void;
        auto DrawManipulationGuizmos() -> void;
        auto DrawSceneToolbar() const -> void;
        auto CreateImguiTextureID() -> void;

        auto CreateWireframeImguiTextureID() -> void;

    private:
        EditorState* m_EditorState{};

        GuizmoType m_GuizmoType{ GuizmoType::TRANSLATION };

        float m_ViewPortWidth{};
        float m_ViewPortHeight{};

        glm::vec2 m_ViewportBounds[2]{};
        glm::vec2 m_GizmoPosition = glm::vec2(1.0f);

        ImTextureID m_ColorImageID{};
        ImTextureID m_WireframeImageID{};
    };
}

#endif // MIKOTO_SCENE_PANEL_HH
