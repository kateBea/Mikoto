/**
 * SettingsPanel.hh
 * Created by kate on 6/27/23.
 * */

#ifndef MIKOTO_SETTINGS_PANEL_HH
#define MIKOTO_SETTINGS_PANEL_HH

// C++ Standard Library
#include <memory>

// Project Headers
#include <Editor/Panel.hh>
#include <Utility/Common.hh>

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

    class SettingsPanel : public Panel {
    public:
        explicit SettingsPanel(const Path_T& iconPath = {});
        auto operator=(SettingsPanel&& other) -> SettingsPanel& = default;

        auto OnUpdate() -> void override;
        auto SetColor(const glm::vec4& color) { m_Data.ClearColor = color; }
        auto SetFieldOfView(float fov) { m_Data.FieldOfView = fov; }

        MKT_NODISCARD auto GetData() const -> const SettingsPanelData& { return m_Data; }

    private:
        static constexpr Size_T REQUIRED_IDS{ 3 };

        SettingsPanelData m_Data{};
        std::vector<Random::GUID::UUID> m_Guids{};
    };
}

#endif // MIKOTO_SETTINGS_PANEL_HH
