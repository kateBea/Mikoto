/**
 * SettingsPanel.hh
 * Created by kate on 6/27/23.
 * */

#ifndef MIKOTO_SETTINGS_PANEL_HH
#define MIKOTO_SETTINGS_PANEL_HH

// C++ Standard Library
#include <memory>

// Project Headers
#include <Common/Common.hh>
#include <Panels/Panel.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {
    struct SettingsPanelData {
        glm::vec4 ClearColor{};
        float EditorCameraMovementSpeed{ 70 };
        float EditorCameraRotationSpeed{ 30 };
        float NearPlane{ 0.1f };
        float FarPlane{ 2000.0f };
        float FieldOfView{ 45.0f };
        bool WantXAxisRotation{ true };
        bool WantYAxisRotation{ true };
        bool VerticalSyncEnabled{ true };
        bool RenderWireframeMode{ false };
    };

    struct SettingsPanelCreateInfo {
        SettingsPanelData Data{};
    };

    class SettingsPanel final : public Panel {
    public:
        explicit SettingsPanel();
        explicit SettingsPanel(const SettingsPanelCreateInfo& data);

        auto OnUpdate(float timeStep) -> void override;
        auto SetRenderBackgroundColor(const glm::vec4& color) { m_Data.ClearColor = color; }
        auto SetRenderFieldOfView( const float fov) -> void { m_Data.FieldOfView = fov; }

        MKT_NODISCARD auto GetData() const -> const SettingsPanelData& { return m_Data; }

    private:
        SettingsPanelData m_Data{};

    };
}

#endif // MIKOTO_SETTINGS_PANEL_HH
