/**
 * EditorLayer.hh
 * Created by kate on 6/12/23.
 * */

#ifndef MIKOTO_EDITOR_LAYER_HH
#define MIKOTO_EDITOR_LAYER_HH

// C++ Standard Library
#include <memory>

// Project Headers
#include "Core/Event.hh"
#include "Core/Layer.hh"
#include "Panels/ConsolePanel.hh"
#include "Panels/ContentBrowserPanel.hh"
#include "Panels/HierarchyPanel.hh"
#include "Panels/InspectorPanel.hh"
#include "Panels/RendererPanel.hh"
#include "Panels/ScenePanel.hh"
#include "Panels/SettingsPanel.hh"
#include "Panels/StatsPanel.hh"
#include "Scene/Scene.hh"

namespace Mikoto {

    struct DockControlFlags {
        bool ApplicationCloseFlag{};

        bool HierarchyPanelVisible{ true };
        bool InspectorPanelVisible{ true };
        bool ScenePanelVisible{ true };
        bool SettingPanelVisible{ true };
        bool StatsPanelVisible{ true };
        bool ContentBrowser{ true };
        bool ConsolePanel{ true };
        bool RendererPanel{ true };
    };

    struct DockSpaceCallbacks {
        // This function is called when we click on the Save scene of the File Menu
        std::function<void()> OnSceneSaveCallback{};

        // This function is called when we click on the Load scene of the File Menu
        std::function<void()> OnSceneLoadCallback{};

        // This function is called when we click on the New scene of the File Menu
        std::function<void()> OnSceneNewCallback{};
    };

    inline DockControlFlags s_ControlFlags{};
    inline DockSpaceCallbacks s_DockSpaceCallbacks{};


    auto DrawAboutModalPopup() -> void;

    auto ShowDockingDisabledMessage() -> void;
    auto OnDockSpaceUpdate() -> void;

    MKT_NODISCARD inline auto GetControlFlags() -> DockControlFlags& { return s_ControlFlags; }
    MKT_NODISCARD inline auto GetDockSpaceCallbacks() -> DockSpaceCallbacks& { return s_DockSpaceCallbacks; }

    class EditorLayer : public Layer {
    public:
        /**
         * @brief Initializes this layer.
         * */
        auto OnAttach() -> void override;


        /**
         * @brief Performs cleanup for this layer
         * */
        auto OnDetach() -> void override;


        /**
         * @brief Updates the layer state. Must be called once per frame.
         *
         * @param ts Time elapsed since the last frame.
         * */
        auto OnUpdate(double ts) -> void override;


        /**
         * @brief Pushes ImGui draw items.
         * */
        auto PushImGuiDrawItems() -> void override;


    private:
        auto InitializePanels() -> void;


        /**
         * @breif Properly initializes and adds two cameras to the scene.
         *
         * One of them is the editor camera which will be active when we are editing the scene.
         * The other is the runtime camera which is active when we are in the "play mode"
         * This last camera should generally be added at runtime since the user
         * is responsible of having a view to the world when it is playing, if
         * there's no runtime camera we see nothing when the scene is playing.
         * */
        auto InitializeSceneCameras() -> void;


    private:
        // [Cameras]
        std::shared_ptr<SceneCamera> m_RuntimeCamera{};
        std::shared_ptr<EditorCamera> m_EditorCamera{};

        // [Panels]
        std::unique_ptr<ScenePanel> m_ScenePanel{};
        std::unique_ptr<StatsPanel> m_StatsPanel{};
        std::unique_ptr<ConsolePanel> m_ConsolePanel{};
        std::unique_ptr<RendererPanel> m_RendererPanel{};
        std::unique_ptr<SettingsPanel> m_SettingsPanel{};
        std::unique_ptr<HierarchyPanel> m_HierarchyPanel{};
        std::unique_ptr<InspectorPanel> m_InspectorPanel{};
        std::unique_ptr<ContentBrowserPanel> m_ContentBrowserPanel{};
    };
}

#endif // MIKOTO_EDITOR_LAYER_HH
