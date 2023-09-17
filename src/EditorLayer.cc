/**
 * EditorLayer.cc
 * Created by kate on 6/12/23.
 * */

// C++ Standard Library
#include <memory>

// Third-Party Libraries
#include <glm/gtc/type_ptr.hpp>

// Project Headers
#include <Core/Application.hh>
#include <Platform/InputManager.hh>
#include <Renderer/RenderCommand.hh>
#include <Scene/Scene.hh>
#include <Editor/Editor.hh>
#include <Editor/EditorLayer.hh>

namespace Mikoto {
    auto EditorLayer::OnAttach() -> void {
        InitializePanels();
        InitializeSceneCameras();

        m_EditorCamera->SetMovementSpeed(m_SettingsPanel->GetData().EditorCameraMovementSpeed);
        m_EditorCamera->SetRotationSpeed(m_SettingsPanel->GetData().EditorCameraRotationSpeed);

        {
            // scripting test
            // TODO: does not compile
            // m_MainCamEntity.GetComponent<NativeScriptComponent>().Bind<CameraController>();
        }
    }

    auto EditorLayer::OnDetach() -> void {

    }

    auto EditorLayer::OnUpdate(double ts) -> void {
        RenderCommand::SetClearColor(m_SettingsPanel->GetData().ClearColor);

        // Move and rotation speeds
        const auto& settingsPanelCurrentData{ m_SettingsPanel->GetData() };
        m_EditorCamera->SetMovementSpeed(settingsPanelCurrentData.EditorCameraMovementSpeed);
        m_EditorCamera->SetRotationSpeed(settingsPanelCurrentData.EditorCameraRotationSpeed);

        // Clip planes
        m_EditorCamera->SetFarPlane(settingsPanelCurrentData.FarPlane);
        m_EditorCamera->SetNearPlane(settingsPanelCurrentData.NearPlane);

        // Field of view
        m_EditorCamera->SetFieldOfView(settingsPanelCurrentData.FieldOfView);

        if (m_ScenePanel->IsFocused()) {
            m_EditorCamera->OnUpdate(ts);
        }

        const auto& sceneData{ m_ScenePanel->GetData() };

        m_EditorCamera->UpdateViewMatrix();
        m_EditorCamera->UpdateProjection();
        m_EditorCamera->SetViewportSize(sceneData.ViewPortWidth, sceneData.ViewPortHeight);
        sceneData.Viewport->OnEditorUpdate(ts, *m_EditorCamera);
    }

    auto EditorLayer::OnEvent(Event& event) -> void {
        if (m_ScenePanel->IsFocused() && m_ScenePanel->IsHovered()) {
            m_EditorCamera->OnEvent(event);
        }

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
        // Panels setup
        m_ScenePanel = std::make_shared<ScenePanel>();
        const auto& sceneData{ m_ScenePanel->GetData() };

        m_HierarchyPanel = std::make_shared<HierarchyPanel>(sceneData.Viewport);
        m_InspectorPanel = std::make_shared<InspectorPanel>(m_HierarchyPanel);
        m_SettingsPanel = std::make_shared<SettingsPanel>();
        m_StatsPanel = std::make_shared<StatsPanel>();

        // Panels post setup
        m_SettingsPanel->SetColor(glm::vec4(0.2f, 0.2f, 0.2f, 1.0f));
        m_SettingsPanel->SetFieldOfView(45.0f);
    }

    MKT_UNUSED_FUNC auto EditorLayer::AddSceneTestEntities() -> void {
        const auto& sceneData{ m_ScenePanel->GetData() };

        // Scene pre setup
        auto ent1{ Scene::CreateEmptyObject("Sprite (Red)", sceneData.Viewport) };
        ent1.AddComponent<SpriteRendererComponent>(glm::vec4{ 0.8f, 0.2f, 0.15f, 1.0f });
        ent1.GetComponent<TransformComponent>().SetTranslation({ 0.0f, 0.0f, 0.0f });
        ent1.GetComponent<TransformComponent>().SetRotation({ 0.0f, 0.0f, 45.0f });

        auto ent2{ Scene::CreateEmptyObject("Sprite (Green)", sceneData.Viewport) };
        ent2.GetComponent<TransformComponent>().SetTranslation({ 0.0f, 0.0f, 0.0f });
        ent2.AddComponent<SpriteRendererComponent>(glm::vec4{ 0.2f, 0.8f, 0.25f, 0.5f });
        ent2.GetComponent<TransformComponent>().SetRotation({ 0.0f, 0.0f, 1.0f });
    }

    auto EditorLayer::InitializeSceneCameras() -> void {
        constexpr float fieldOfView{ 45.0f };
        constexpr float aspectRatio{ 1.778f };
        constexpr float nearPlane{ 0.1f };
        constexpr float farPlane{ 1000.0f };

        // Initialize cameras
        auto& window{ Application::Get().GetMainWindow() };
        double aspect{ window.GetWidth() / (double)window.GetHeight() };
        m_RuntimeCamera = std::make_shared<SceneCamera>(glm::ortho(-aspect, aspect, -1.0, 1.0)); (void)m_RuntimeCamera;
        m_EditorCamera = std::make_shared<EditorCamera>(fieldOfView, aspectRatio, nearPlane, farPlane);
    }
}
