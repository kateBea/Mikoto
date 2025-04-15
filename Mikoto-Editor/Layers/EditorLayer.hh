/**
 * EditorLayer.hh
 * Created by kate on 6/12/23.
 * */

#ifndef MIKOTO_EDITOR_LAYER_HH
#define MIKOTO_EDITOR_LAYER_HH

// C++ Standard Library
#include <memory>

// Project Headers
#include <Assets/Model.hh>
#include <Layer.hh>
#include <Library/Data/Registry.hh>
#include <Material/TextureCube.hh>
#include <Panels/Panel.hh>
#include <Platform/Window.hh>
#include <Project/Project.hh>
#include <Project/ProjectSerializer.hh>
#include <Renderer/RendererBackend.hh>
#include <Scene/Entity.hh>
#include <Scene/Scene.hh>
#include <Scene/SceneCamera.hh>
#include <Scene/SceneSerializer.hh>
#include <Renderer/SceneRenderer.hh>

namespace Mikoto {

    struct EditorLayerCreateInfo {
        Window* TargetWindow{ nullptr };
        Path_T ModelsRootDirectory{};
    };

    class EditorLayer final : public Layer {
    public:
        explicit EditorLayer() = default;
        explicit EditorLayer(const EditorLayerCreateInfo& createInfo);

        auto OnAttach() -> void override;
        auto OnDetach() -> void override;
        auto OnUpdate(double timeStep) -> void override;
        auto PushImGuiDrawItems(double timeStep) -> void override;

    private:
        auto SaveScene() const -> void;
        auto LoadScene() -> void;
        auto CreateScene(std::string_view name) -> void;

        auto SaveProject() -> void;
        auto OpenProject() -> void;
        auto CreateProject() -> void;

        auto CreatePanels() -> void;
        auto CreateCameras() -> void;
        auto HandleWindowScreenMode() const -> void;
        auto SetRendererResolution() const -> void;
        auto UpdateDockSpace() -> void;

        auto PrepareNewScene() -> void;
        auto PrepareSerialization() -> void;

        auto LoadPrefabModels() const -> void;
        auto LoadPrefabFonts() const -> void;

        auto SetupRenderer(double timeStep) -> void;
        auto SetupCamera(double timeStep) -> void;

    private:
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

    private:
        Window* m_Window{ nullptr };

        Entity* m_SelectedEntity{};

        Scope_T<Scene> m_ActiveScene{};
        Scope_T<SceneRenderer> m_SceneRenderer{};

        TextureCube* m_TextureCubeMap{};

        Scope_T<SceneCamera> m_EditorCamera{};
        Scope_T<SceneSerializer> m_SceneSerializer{};

        Scope_T<Project> m_Project{};
        Scope_T<ProjectSerializer> m_ProjectSerializer{};

        RendererBackend* m_EditorRenderer{};

        DockControlFlags m_ControlFlags{};

        Registry<Panel> m_PanelRegistry{};
    };
}

#endif // MIKOTO_EDITOR_LAYER_HH
