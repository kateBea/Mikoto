/**
 * EditorLayer.hh
 * Created by kate on 6/12/23.
 * */

#ifndef MIKOTO_EDITOR_LAYER_HH
#define MIKOTO_EDITOR_LAYER_HH

// C++ Standard Library
#include <memory>

// Project Headers
#include <Core/Layer.hh>
#include <Core/Event.hh>
#include <Scene/Scene.hh>

#include <Editor/ContentBrowserPanel.hh>
#include <Editor/HierarchyPanel.hh>
#include <Editor/InspectorPanel.hh>
#include <Editor/ScenePanel.hh>
#include <Editor/SettingsPanel.hh>
#include <Editor/StatsPanel.hh>
#include <Editor/ConsolePanel.hh>
#include <Editor/RendererPanel.hh>

namespace Mikoto {

    class EditorLayer : public Layer {
    public:
        auto OnAttach() -> void override;
        auto OnDetach() -> void override;
        auto OnUpdate(double ts) -> void override;
        auto PushImGuiDrawItems() -> void override;
    private:
        auto InitializePanels() -> void;

        /**
         * Properly initializes and adds two cameras to the scene. One of them
         * is the editor camera which will be active when we are editing the scene.
         * The other is the runtime camera which is active when we are in the "play mode"
         * This last camera should generally be added at runtime since the user
         * is responsible of having a view to the world when it is playing, if
         * there's no runtime camera we see nothing when the scene is playing
         * */
        auto InitializeSceneCameras() -> void;

    private:
        // Cameras
        std::shared_ptr<SceneCamera> m_RuntimeCamera{};
        std::shared_ptr<EditorCamera> m_EditorCamera{};

        // Panels
        std::unique_ptr<ContentBrowserPanel> m_ContentBrowserPanel{};
        std::unique_ptr<HierarchyPanel> m_HierarchyPanel{};
        std::unique_ptr<InspectorPanel> m_InspectorPanel{};
        std::unique_ptr<SettingsPanel> m_SettingsPanel{};
        std::unique_ptr<ScenePanel> m_ScenePanel{};
        std::unique_ptr<StatsPanel> m_StatsPanel{};
        std::unique_ptr<ConsolePanel> m_ConsolePanel{};
        std::unique_ptr<RendererPanel> m_RendererPanel{};
    };
}

#endif // MIKOTO_EDITOR_LAYER_HH
