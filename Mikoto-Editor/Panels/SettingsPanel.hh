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
#include <Scene/SceneCamera.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {

    struct EditorState;

    struct SettingsPanelData {
        Vec4F ClearColor{ 0.2f, 0.3f, 0.5f, 1.0f  };
        Vec4F OutlineColor{ 1.0f, 1.0f, 1.0f, 1.0f };
        float EditorCameraMovementSpeed{ 70 };
        float EditorCameraRotationSpeed{ 30 };
        float Outline{ 1.5f };
        float NearPlane{ 0.1f };
        float FarPlane{ 2000.0f };
        float FieldOfView{ 45.0f };
        float DampingFactor{ 10.0f };
        bool WantXAxisRotation{ true };
        bool WantYAxisRotation{ true };
        bool VerticalSyncEnabled{ true };
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
        EditorState* m_EditorState{};
        SettingsPanelData m_Data{};

    };
}

#endif // MIKOTO_SETTINGS_PANEL_HH
