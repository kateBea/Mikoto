/**
 * InspectorPanel.hh
 * Created by kate on 6/25/23.
 * */

#ifndef MIKOTO_INSPECTOR_PANEL_HH
#define MIKOTO_INSPECTOR_PANEL_HH

// C++ Standard Library
#include <memory>

// Third-Party Libraries

// Project Headers
#include <Utility/Common.hh>
#include <Scene/Scene.hh>
#include <Editor/Panels/Panel.hh>
#include <Editor/Panels/HierarchyPanel.hh>

namespace Mikoto {
    class InspectorPanel : public Panel {
    public:

        explicit InspectorPanel(const std::shared_ptr<HierarchyPanel>& hierarchy, const Path_T& iconPath = {});

        auto operator=(InspectorPanel&& other) -> InspectorPanel& = default;

        /**
         * Updates the state of this panel
         * */
        auto OnUpdate() -> void override;

        /**
         * Must be called everytime we want to propagate an event to
         * this panel to be handled
         * @param event event to be handled
         * */
        auto OnEvent(Event& event) ->  void override;

        /**
         * Destructor, defaulted
         * */
        ~InspectorPanel() = default;

    private:
        /*************************************************************
        * DATA MEMBERS
        * ***********************************************************/
        std::shared_ptr<HierarchyPanel> m_Hierarchy{};
    };
}


#endif // MIKOTO_INSPECTOR_PANEL_HH
