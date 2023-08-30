/**
 * ScenePanel.hh
 * Created by kate on 6/27/23.
 * */

#ifndef MIKOTO_SCENE_PANEL_HH
#define MIKOTO_SCENE_PANEL_HH

// C++ Standard Library
#include <memory>

// Third-Party Libraries
#include <volk.h>

// Project Headers
#include <Utility/Common.hh>
#include <Editor/Panels/Panel.hh>
#include <Editor/Panels/PanelData.hh>

namespace Mikoto {
    /**
     * Abstracts away the active ImGui render backend according
     * to the currently active graphics API
     * */
    class ScenePanelInterface {
    public:
       virtual auto Init_Impl(std::shared_ptr<ScenePanelData> data) -> void = 0;
       virtual auto OnUpdate_Impl() -> void = 0;
       virtual auto OnEvent_Impl(Event& event) -> void = 0;

       MKT_NODISCARD auto IsHovered() const -> bool { return m_Hovered; }
       MKT_NODISCARD auto IsFocused() const -> bool { return m_Focused; }

       virtual ~ScenePanelInterface() = default;

    protected:
        std::shared_ptr<ScenePanelData> m_Data{};
        bool m_Hovered{};
        bool m_Focused{};
    };


    class ScenePanel : public Panel<ScenePanel> {
    public:
        explicit ScenePanel() = default;
        explicit ScenePanel(const std::shared_ptr<ScenePanelData> &data, const Path_T& iconPath = {});

        auto operator=(ScenePanel&& other) -> ScenePanel& = default;

        /**
         * Updates the state of this panel
         * */
        auto OnUpdate() -> void;

        /**
         * Must be called everytime we want to propagate an event to
         * this panel to be handled
         * @param event event to be handled
         * */
        auto OnEvent(Event& event) ->  void;

        /**
         * Hides or reveals this panel in the docking space.
         * @param value if false, hides this panel, otherwise it may always be visible
         * */
        auto MakeVisible(bool value) ->  void;

        /**
         * Destructor, defaulted
         * */
        ~ScenePanel() = default;
    protected:
        std::shared_ptr<ScenePanelInterface> m_Implementation{};
    };
}

#endif // MIKOTO_SCENE_PANEL_HH
