/**
 * EditorLayer.cc
 * Created by kate on 6/12/23.
 * */

// C++ Standard Library
#include <memory>

// Third-Party Libraries
#include <glm/gtc/type_ptr.hpp>

// Project Headers
#include <Utility/Random.hh>
#include <Core/Application.hh>
#include <Platform/InputManager.hh>
#include <Renderer/RenderCommand.hh>
#include <Scene/Scene.hh>
#include <Editor/Editor.hh>
#include <Editor/EditorLayer.hh>

namespace Mikoto {
    auto EditorLayer::OnAttach() -> void {
        InitializePanels();
        AddSceneTestEntities();
        InitializeSceneCameras();

        {
            // scripting test
            // TODO: does not compile
            // m_MainCamEntity.GetComponent<NativeScriptComponent>().Bind<CameraController>();
        }
    }

    auto EditorLayer::OnDetach() -> void {

    }

    auto EditorLayer::OnUpdate(double ts) -> void {
        RenderCommand::SetClearColor(m_SettingsPanelInfo->ClearColor);
        if (m_ScenePanel->IsFocused()) {
            m_EditorCamera->OnUpdate(ts);
        }


        m_EditorCamera->SetViewportSize(m_ScenePanelInfo->ViewPortWidth, m_ScenePanelInfo->ViewPortHeight);
        m_ScenePanelInfo->Viewport->OnEditorUpdate(ts, *m_EditorCamera);
    }

    auto EditorLayer::OnEvent(Event& event) -> void {
        m_EditorCamera->OnEvent(event);

        // Events for panels
        m_SettingsPanel->OnEvent(event);
        m_HierarchyPanel->OnEvent(event);
        m_InspectorPanel->OnEvent(event);
        m_ScenePanel->OnEvent(event);
        m_StatsPanel->OnEvent(event);
    }

    auto EditorLayer::OnImGuiRender() -> void {
        Editor::OnDockSpaceUpdate();
        auto& controlFlags{ Editor::GetControlFlags() };

        m_SettingsPanel->MakeVisible(controlFlags.SettingPanelVisible);
        m_HierarchyPanel->MakeVisible(controlFlags.HierarchyPanelVisible);
        m_InspectorPanel->MakeVisible(controlFlags.InspectorPanelVisible);
        m_ScenePanel->MakeVisible(controlFlags.ScenePanelVisible);
        m_StatsPanel->MakeVisible(controlFlags.StatsPanelVisible);

        m_SettingsPanel->OnUpdate();
        m_HierarchyPanel->OnUpdate();
        m_InspectorPanel->OnUpdate();
        m_ScenePanel->OnUpdate();
        m_StatsPanel->OnUpdate();

        if (controlFlags.ApplicationCloseFlag)
            Application::Get().Stop();
    }

    auto EditorLayer::InitializePanels() -> void {
        // Settings Panel data setup
        m_SettingsPanelInfo = std::make_shared<SettingsPanelData>();
        m_SettingsPanelInfo->ClearColor = glm::vec4(0.2f, 0.2f, 0.2f, 1.0f);

        // Scene Panel data setup
        m_ScenePanelInfo = std::make_shared<ScenePanelData>();
        m_ScenePanelInfo->Viewport = std::make_unique<Scene>();
        m_ScenePanelInfo->ViewPortWidth = 1280;
        m_ScenePanelInfo->ViewPortHeight = 720;

        // Statistics Panel data setup
        m_StatsPanelInfo = std::make_shared<StatsPanelData>();

        // Panels setup
        m_HierarchyPanel = std::make_shared<HierarchyPanel>(m_ScenePanelInfo->Viewport);
        m_InspectorPanel = std::make_shared<InspectorPanel>(m_HierarchyPanel);
        m_SettingsPanel = std::make_shared<SettingsPanel>(m_SettingsPanelInfo);
        m_ScenePanel = std::make_shared<ScenePanel>(m_ScenePanelInfo);
        m_StatsPanel = std::make_shared<StatsPanel>(m_StatsPanelInfo);
    }

    auto EditorLayer::AddSceneTestEntities() -> void {
        // Scene pre setup
        auto ent1{ Scene::CreateEmptyObject("Sprite (Red)", m_ScenePanelInfo->Viewport) };
        ent1.AddComponent<SpriteRendererComponent>(glm::vec4{ 0.8f, 0.2f, 0.15f, 1.0f });
        ent1.GetComponent<TransformComponent>().SetTranslation({ 0.0f, 0.0f, 0.0f });
        ent1.GetComponent<TransformComponent>().SetRotation({ 0.0f, 0.0f, 45.0f });

        auto ent2{ Scene::CreateEmptyObject("Sprite (Green)", m_ScenePanelInfo->Viewport) };
        ent2.GetComponent<TransformComponent>().SetTranslation({ 0.0f, 0.0f, 0.0f });
        ent2.AddComponent<SpriteRendererComponent>(glm::vec4{ 0.2f, 0.8f, 0.25f, 0.5f });
        ent2.GetComponent<TransformComponent>().SetRotation({ 0.0f, 0.0f, 1.0f });
    }

    auto EditorLayer::InitializeSceneCameras() -> void {
        constexpr float FIELD_OF_VIEW{ 45.0f };
        constexpr float ASPECT_RATIO{ 1.778f };
        constexpr float NEAR_PLANE{ 0.1f };
        constexpr float FAR_PLANE{ 1000.0f };

        // Initialize cameras
        auto& window{ Application::Get().GetMainWindow() };
        double aspect{ window.GetWidth() / (double)window.GetHeight() };
        m_RuntimeCamera = std::make_shared<SceneCamera>(glm::ortho(-aspect, aspect, -1.0, 1.0));
        m_EditorCamera = std::make_shared<EditorCamera>(FIELD_OF_VIEW, ASPECT_RATIO, NEAR_PLANE, FAR_PLANE);
    }
}
