/**
 * EditorLayer.hh
 * Created by kate on 6/12/23.
 * */

#ifndef MIKOTO_EDITOR_LAYER_HH
#define MIKOTO_EDITOR_LAYER_HH

// C++ Standard Library
#include <memory>

// Project Headers
#include <Scene/Camera/SceneCamera.hh>

#include "Core/Layer.hh"
#include "Models/DockSPaceCallbacks.hh"
#include "Models/DockSpaceControlFlags.hh"
#include "Panels/ConsolePanel.hh"
#include "Panels/ContentBrowserPanel.hh"
#include "Panels/HierarchyPanel.hh"
#include "Panels/InspectorPanel.hh"
#include "Panels/RendererPanel.hh"
#include "Panels/ScenePanel.hh"
#include "Panels/SettingsPanel.hh"
#include "Panels/StatsPanel.hh"

#include <Core/CommandLineParser.hh>

namespace Mikoto {
    class EditorLayer final : public Layer {
    public:
        auto OnAttach() -> void override;
        auto OnDetach() -> void override;
        auto OnUpdate(double ts) -> void override;
        auto PushImGuiDrawItems() -> void override;

    private:
        auto SaveScene() -> void;
        auto LoadScene() -> void;

        auto SaveProject() -> void;
        auto LoadProject() -> void;

        auto CreatePanels() -> void;
        auto CreateCameras() -> void;
        auto UpdateDockSpace() -> void;

        static auto ShowDockingDisabledMessage() -> void;

    private:
        Ref_T<Entity> m_SelectedEntity{};
        Ref_T<Scene> m_ActiveScene{};

        DockControlFlags m_ControlFlags{};
        DockSpaceCallbacks m_DockSpaceCallbacks{};

        std::shared_ptr<SceneCamera> m_RuntimeCamera{};
        std::shared_ptr<SceneCamera> m_EditorCamera{};

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
