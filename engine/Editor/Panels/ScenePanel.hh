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
#include <Editor/Panels/HierarchyPanel.hh>

namespace Mikoto {
    /**
     * Abstracts away the active ImGui render backend according
     * to the currently active graphics API. This objects is exclusively managed by
     * the ScenePanel
     * */
    class ScenePanelInterface {
    public:
       virtual auto Init_Impl(ScenePanelData&& data) -> void = 0;
       virtual auto OnUpdate_Impl() -> void = 0;
       virtual auto OnEvent_Impl(Event& event) -> void = 0;

       auto HandleGuizmos() -> void;

       MKT_NODISCARD auto IsHovered() const -> bool { return m_Hovered; }
       MKT_NODISCARD auto IsFocused() const -> bool { return m_Focused; }
       MKT_NODISCARD auto GetData() const -> const ScenePanelData& { return m_Data; }

       auto SetSceneCamera(const EditorCamera* camera) -> void { m_EditorMainCamera = camera; }
       auto SetContextSelection(const Entity& target) -> void { m_CurrentContextSelection = target; }

       virtual ~ScenePanelInterface() = default;

    protected:
        ScenePanelData m_Data{};
        Entity m_CurrentContextSelection{};
        const EditorCamera* m_EditorMainCamera{};
        bool m_Hovered{};
        bool m_Focused{};
    };


    struct ScenePanelCreateInfo {
        // This camera is holds the eye to the world (the scene we are editing)
        const EditorCamera* EditorMainCamera{};
    };

    struct ScenePanelCallbacks {
        std::function<void(HierarchyPanel*)> EntitySelectionCallback{};
        std::function<void(HierarchyPanel*)> EntityDeselectionCallback{};
    };

    class ScenePanel : public Panel {
    public:
        explicit ScenePanel(ScenePanelCreateInfo&& createInfo, const Path_T &iconPath = {});
        auto operator=(ScenePanel&& other) -> ScenePanel& = default;

        /**
         * Returns read only access to the data from this panel. See ScenePanelData
         * for the fields with in the data's structure
         * @returns this panel's data
         * */
        MKT_NODISCARD auto GetData() const -> const ScenePanelData& { return m_Implementation->GetData(); }
        MKT_NODISCARD auto GetCallbacks() const -> const ScenePanelCallbacks& { return m_Callbacks; }

        /**
         * Updates the state of this panel
         * */
        auto OnUpdate() -> void override;

        /**
         * Must be called everytime we want to propagate an event to this panel to be handled
         * @param event event to be handled
         * */
        auto OnEvent(Event& event) ->  void override;

        /**
         * Destructor, defaulted
         * */
        ~ScenePanel() override = default;

    private:
        auto CreateCallbacks() -> void;

    protected:
        // Contains the ImGui implementation for the currently active graphics backend
        std::shared_ptr<ScenePanelInterface> m_Implementation{};
        ScenePanelCreateInfo m_CreateInfo{};
        ScenePanelCallbacks m_Callbacks{};
    };
}

#endif // MIKOTO_SCENE_PANEL_HH
