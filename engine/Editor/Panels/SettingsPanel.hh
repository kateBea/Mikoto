//
// Created by kate on 6/27/23.
//

#ifndef KATE_ENGINE_SETTINGS_PANEL_HH
#define KATE_ENGINE_SETTINGS_PANEL_HH

#include <Utility/Common.hh>
#include <Editor/Panels/PanelData.hh>
#include <Editor/Panels/Panel.hh>

namespace Mikoto {
    class SettingsPanel : public Panel {
    public:
        explicit SettingsPanel() = default;
        explicit SettingsPanel(const std::shared_ptr<SettingsPanelData> &data, const Path_T &iconPath = {});

        SettingsPanel(const SettingsPanel& other) = default;
        SettingsPanel(SettingsPanel&& other) = default;

        auto operator=(const SettingsPanel& other) -> SettingsPanel& = default;
        auto operator=(SettingsPanel&& other) -> SettingsPanel& = default;

        auto OnUpdate() -> void override;
        auto OnEvent(Event& event) ->  void override;
        auto MakeVisible(bool value) ->  void override { m_Visible = value; }

        MKT_NODISCARD auto IsHovered() const -> bool override { return m_Hovered; }
        MKT_NODISCARD auto IsFocused() const -> bool override { return m_Focused; }
        MKT_NODISCARD auto IsVisible() const -> bool override { return m_Visible; }
    private:
        bool m_Visible{};
        bool m_Hovered{};
        bool m_Focused{};

        std::shared_ptr<SettingsPanelData> m_Data{};
    };
}


#endif//KATE_ENGINE_SETTINGS_PANEL_HH
