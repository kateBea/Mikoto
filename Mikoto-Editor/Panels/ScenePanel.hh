//    Copyright 2026 ケイト
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

#include <imgui.h>

#include <EASTL/array.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Panels/Panel.hh>

#include <ImGui/ImGuiUtility.hh>

#include <Renderer/Core/Rhi.hh>

namespace mikoto::editor {

    struct EditorState;

    struct ScenePanelCreateInfo {
        EditorState* mState{};
        TextureHandle mImage{};
    };

    class ScenePanel final : public Panel {
    public:
        explicit ScenePanel(const ScenePanelCreateInfo& createInfo);

        auto OnUpdate(float ts) -> void override;

        auto SetManipulation( gui::GuizmoType mode ) -> void;
        auto SetTexture( TextureHandle texture ) -> void;

        ~ScenePanel() override = default;

        MKT_NODISCARD auto GetWidth() const -> float;
        MKT_NODISCARD auto GetHeight() const -> float;

    private:
        auto DrawOrientationAxis() -> void;

        auto UpdateViewport() -> void;
        auto UpdateManipulation() -> void;

        auto DrawUtilitiesOverlay() -> void;

        auto DrawSceneToolbar() -> void;
        auto DrawManipulationGuizmos() -> void;

        MKT_NODISCARD auto IsDisplayTextureValid() const -> bool;

    private:
        EditorState* mEditorState{};
        ImTextureID mColorImageID{};

        f32 mViewportWidth{ 1920 };
        f32 mViewportHeight{ 1080 };

        float2 mGuizmoPosition{ 1.0f };

        gui::GuizmoType mManipulationType{ gui::GuizmoType::eTranslation };
    };
}

#endif // MIKOTO_SCENE_PANEL_HH
