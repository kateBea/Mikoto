/**
 * SettingsPanel.hh
 * Created by kate on 6/27/23.
 * */

#ifndef MIKOTO_SETTINGS_PANEL_HH
#define MIKOTO_SETTINGS_PANEL_HH

// C++ Standard Library
#include <memory>

// Project Headers
#include <Utility/Common.hh>
#include <Editor/Panels/PanelData.hh>
#include <Editor/Panels/Panel.hh>

namespace Mikoto {
    class SettingsPanel : public Panel {
    public:
        explicit SettingsPanel(const Path_T& iconPath = {});

        auto operator=(SettingsPanel&& other) -> SettingsPanel& = default;

        auto OnUpdate() -> void override;
        auto OnEvent(Event& event) ->  void override;

        MKT_NODISCARD auto GetData() const -> const SettingsPanelData& { return m_Data; }

        auto SetFieldOfView(float fov) { m_Data.FieldOfView = fov; }
        auto SetColor(const glm::vec4& color) { m_Data.ClearColor = color; }

    private:
        /*************************************************************
        * DATA MEMBERS
        * ***********************************************************/
        SettingsPanelData m_Data{};
    };
}

#endif // MIKOTO_SETTINGS_PANEL_HH
