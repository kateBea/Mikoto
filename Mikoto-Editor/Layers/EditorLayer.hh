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
#include <Core/LayerStack.hh>
#include <ImGui/ImGuiUtility.hh>
#include <Library/Data/Registry.hh>
#include <Material/TextureCube.hh>
#include <Panels/Panel.hh>
#include <Platform/Window.hh>
#include <Scene/Entity.hh>
#include <Scene/Scene.hh>
#include <Scene/SceneCamera.hh>
#include <Scene/SceneSerializer.hh>
#include <Renderer/Core/SceneRenderer.hh>

namespace Mikoto {

    struct EditorState {
        Entity* SelectedEntity{};

        // Used when scene is not simulating
        SceneCamera* EditorCamera{};

        // Scene currently active
        Scene* ActiveEditorScene{};

        // The final composition from the scene renderer
        TextureHandle FinalComposition{};

        // Panels close flags
        bool StatsPanelVisible{ true };
        bool ContentBrowser{ true };
        bool ConsolePanel{ true };
        bool RendererPanel{ true };
        bool SettingPanelVisible{ true };
        bool HierarchyPanelVisible{ true };
        bool InspectorPanelVisible{ true };
        bool ScenePanelVisible{ true };

        ImGuiUtils::GuizmoManipulationMode Manipulation{ ImGuiUtils::GuizmoManipulationMode::TRANSLATION };
    };

    struct EditorLayerCreateInfo {
        std::string_view Name{ nullptr };
        Window* TargetWindow{ nullptr };
        Path ModelsRootDirectory{};
    };

    class EditorLayer final : public ILayer {
    public:
        explicit EditorLayer(const EditorLayerCreateInfo& createInfo);

        auto OnCreate() -> void override;
        auto OnDestroy() -> void override;
        auto OnUpdate(float timeStep) -> void override;

        auto OnEvent(Event &event) -> void override;

    private:
        auto UpdatePanels(float timeStep) -> void;

        auto SaveScene() const -> void;
        auto LoadScene() -> void;
        auto InitializeEmptyScene(std::string_view name) -> void;

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

        auto PrepareRenderer(double timeStep) -> void;
        auto PrepareCamera(double timeStep) -> void;

        auto SetupRenderer() -> void ;

        auto SetupEditorState() -> void ;

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
        Unique<EditorState> m_EditorState{};

        Window* m_Window{ nullptr };

        Path m_ModelsRootDirectory{};
        Path m_FontsRootDirectory{};

        Unique<Scene> m_ActiveScene{};
        Unique<SceneRenderer> m_SceneRenderer{};

        TextureHandle m_TextureCubeMap{};

        Unique<SceneCamera> m_EditorCamera{};
        Unique<SceneSerializer> m_SceneSerializer{};

        DockControlFlags m_ControlFlags{};

        Registry<Panel> m_PanelRegistry{};
    };
}

#endif // MIKOTO_EDITOR_LAYER_HH
