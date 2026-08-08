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

#ifndef MIKOTO_SETTINGS_PANEL_HH
#define MIKOTO_SETTINGS_PANEL_HH

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Panels/Panel.hh>

namespace mikoto::editor {

    struct EditorState;

    struct SettingsPanelData {
        // Camera
        core::f32 mEditorCameraMovementSpeed{ 70 };
        core::f32 mEditorCameraRotationSpeed{ 30 };
        core::f32 mNearPlane{ 0.1f };
        core::f32 mFarPlane{ 5000.0f };
        core::f32 mFieldOfView{ 45.0f };
        core::f32 mDampingFactor{ 6.0f };
        bool mWantXAxisRotation{ true };
        bool mWantYAxisRotation{ true };

        // Selected entity
        bool mLockCameraToTarget{ false };

        // Infinite grid
        core::f32 mGridSize{};
        core::f32 mGridCellSize{ 0.5f };
        core::f32 mGridMinPixelsBetweenCells{ 2.0f };
        core::float4 mGridColorThin{ 0.5f, 0.5f, 0.5f, 1.0f };
        core::float4 mGridColorThick{ 0.0f, 0.0f, 0.0f, 1.0f };
    };

    struct SettingsPanelCreateInfo {
        EditorState* mState{};
    };

    class SettingsPanel final : public Panel {
    public:
        explicit SettingsPanel(const SettingsPanelCreateInfo& data);

        auto OnUpdate(float timeStep) -> void override;

        MKT_NODISCARD auto GetData() -> SettingsPanelData&;
        MKT_NODISCARD auto GetData() const -> const SettingsPanelData&;

    private:
        auto DrawCameraConfig() -> void;
        auto DrawCameraProperties() -> void;

    private:
        EditorState* mEditorState{};
        SettingsPanelData mData{};
    };
}

#endif // MIKOTO_SETTINGS_PANEL_HH
