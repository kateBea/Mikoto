//    Copyright 2025 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MIKOTO_SCENE_PANEL_HH
#define MIKOTO_SCENE_PANEL_HH

#include <Library/Utility/Types.hh>

#include <ImGui/ImGuiUtility.hh>
#include <Assets/Texture.hh>
#include <Panels/Panel.hh>

namespace Mikoto {

    struct EditorState;

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
        auto SetManipulation( ImGuiUtils::GuizmoManipulationMode mode ) -> void;

        ~ScenePanel() override = default;

        MKT_NODISCARD auto GetWidth() const -> float;
        MKT_NODISCARD auto GetHeight() const -> float;

    private:
        auto DrawOrientationAxis() -> void;

        auto ShowUtilitiesOverlay() -> void;
        auto IsDisplayTextureValid() const -> bool;
        auto IsWireframeDisplayTextureValid() const -> bool;
        auto UpdateViewport() -> void;
        auto SetupManipulation() const -> void;
        auto DrawManipulationGuizmos() -> void;
        auto DrawSceneToolbar() const -> void;
        auto CreateImguiTextureID() -> void;

        auto CreateWireframeImguiTextureID() -> void;

    private:
        EditorState* m_EditorState{};

        ImGuiUtils::GuizmoManipulationMode m_GuizmoType{ ImGuiUtils::GuizmoManipulationMode::TRANSLATION };

        float m_ViewPortWidth{};
        float m_ViewPortHeight{};

        glm::vec2 m_ViewportBounds[2]{};
        glm::vec2 m_GizmoPosition = glm::vec2(1.0f);

        ImTextureID m_ColorImageID{};
        ImTextureID m_WireframeImageID{};
    };
}

#endif // MIKOTO_SCENE_PANEL_HH
