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
    class SettingsPanel : public Panel<SettingsPanel> {
    public:
        explicit SettingsPanel() = default;
        explicit SettingsPanel(const std::shared_ptr<SettingsPanelData> &data, const Path_T &iconPath = {});

        SettingsPanel(const SettingsPanel& other) = default;
        SettingsPanel(SettingsPanel&& other) = default;

        auto operator=(const SettingsPanel& other) -> SettingsPanel& = default;
        auto operator=(SettingsPanel&& other) -> SettingsPanel& = default;

        auto OnUpdate() -> void;
        auto OnEvent(Event& event) ->  void;
        auto MakeVisible(bool value) ->  void { m_PanelIsVisible = value; }

    private:
        std::shared_ptr<SettingsPanelData> m_Data{};
    };
}

#endif // MIKOTO_SETTINGS_PANEL_HH
