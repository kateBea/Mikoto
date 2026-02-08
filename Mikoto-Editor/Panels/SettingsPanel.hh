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

#include <memory>

#include <Common/Common.hh>
#include <Panels/Panel.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {

    struct EditorState;

    struct SettingsPanelData {
        // Camera
        float EditorCameraMovementSpeed{ 70 };
        float EditorCameraRotationSpeed{ 30 };
        float NearPlane{ 0.1f };
        float FarPlane{ 2000.0f };
        float FieldOfView{ 45.0f };
        float DampingFactor{ 15.0f };
        bool WantXAxisRotation{ true };
        bool WantYAxisRotation{ true };

        // Selected entity
        bool LockCameraToTarget{ false };

        // Infinite grid
        float GridSize{};
        float GridCellSize{ 0.5f };
        float GridMinPixelsBetweenCells{ 2.0f };
        Vec4F GridColorThin{ 0.5f, 0.5f, 0.5f, 1.0f };
        Vec4F GridColorThick{ 0.0f, 0.0f, 0.0f, 1.0f };
    };

    struct SettingsPanelCreateInfo {
        EditorState* State{};
    };

    class SettingsPanel final : public Panel {
    public:
        explicit SettingsPanel(const SettingsPanelCreateInfo& data);

        auto OnUpdate(float timeStep) -> void override;

        MKT_NODISCARD auto GetData() -> SettingsPanelData& { return m_Data; }
        MKT_NODISCARD auto GetData() const -> const SettingsPanelData& { return m_Data; }

    private:
        auto DrawCameraConfig() -> void;
        auto DrawInfiniteGridConfig() -> void;

    private:

        EditorState* m_EditorState{};
        SettingsPanelData m_Data{};
    };
}

#endif // MIKOTO_SETTINGS_PANEL_HH
