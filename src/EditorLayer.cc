/**
 * EditorLayer.cc
 * Created by kate on 6/12/23.
 * */

// C++ Standard Library
#include <memory>

// Third-Party Libraries
#include <glm/gtc/type_ptr.hpp>

// Project Headers
#include <Scene/Scene.hh>
#include <Editor/Editor.hh>
#include <Core/Serializer.hh>
#include <Core/CoreEvents.hh>
#include <Scene/SceneManager.hh>
#include <Editor/EditorLayer.hh>
#include <Core/EventManager.hh>
#include <Platform/InputManager.hh>
#include <Renderer/RenderCommand.hh>

namespace Mikoto {
    auto EditorLayer::OnAttach() -> void {
        // Initialize cameras first, they are needed for some panels
        InitializeSceneCameras();

        InitializePanels();

        m_EditorCamera->SetMovementSpeed(m_SettingsPanel->GetData().EditorCameraMovementSpeed);
        m_EditorCamera->SetRotationSpeed(m_SettingsPanel->GetData().EditorCameraRotationSpeed);

        {
            // scripting test
            // TODO: does not compile
            // m_MainCamEntity.GetComponent<NativeScriptComponent>().Bind<CameraController>();
        }

        // Initialize Editor DockSpace
        auto& dockSpaceCallbacks{ Editor::GetDockSpaceCallbacks() };

        dockSpaceCallbacks.OnSceneNewCallback =
            [&]() -> void {
                SceneManager::DestroyScene(SceneManager::GetActiveScene());
                auto& newScene{ SceneManager::MakeNewScene("Empty Scene") };
                SceneManager::SetActiveScene(newScene);
            };

        dockSpaceCallbacks.OnSceneLoadCallback =
            [&]() -> void {
                // We need to clear the scene before we load the serialized entities
                SceneManager::DestroyScene(SceneManager::GetActiveScene());
                auto& newScene{ SceneManager::MakeNewScene("Empty Scene") };
                SceneManager::SetActiveScene(newScene);

                // prepare filters for the dialog
                std::initializer_list<std::pair<std::string, std::string>> filters{
                        { "Mikoto Scene files", "mkts,mktscene" },
                        { "Mikoto Project Files", "mkt,mktp,mktproject" }
                };

                Serializer::SceneSerializer::Deserialize(Serializer::OpenDialog(filters), newScene);
            };

        dockSpaceCallbacks.OnSceneSaveCallback =
            [&]() -> void {
                // prepare filters for the dialog
                std::initializer_list<std::pair<std::string, std::string>> filters{
                        { "Mikoto Scene files", "mkts,mktscene" },
                        { "Mikoto Project Files", "mkt,mktp,mktproject" }
                };

                Scene* activeScene{ std::addressof(SceneManager::GetActiveScene()) };
                Serializer::SceneSerializer::Serialize(activeScene, Serializer::SaveDialog("Mikoto Scene", filters));
            };
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

        m_EditorCamera->OnUpdate(ts);

        const auto& sceneData{ m_ScenePanel->GetData() };
        auto& activeScene{ SceneManager::GetActiveScene() };

        m_EditorCamera->UpdateViewMatrix();
        m_EditorCamera->UpdateProjection();
        m_EditorCamera->SetViewportSize(sceneData.ViewPortWidth, sceneData.ViewPortHeight);
        activeScene.OnEditorUpdate(ts, *m_EditorCamera);
    }

    auto EditorLayer::PushImGuiDrawItems() -> void {
        Editor::OnDockSpaceUpdate();
        auto& controlFlags{ Editor::GetControlFlags() };

        m_SettingsPanel->MakeVisible(controlFlags.SettingPanelVisible);
        m_HierarchyPanel->MakeVisible(controlFlags.HierarchyPanelVisible);
        m_InspectorPanel->MakeVisible(controlFlags.InspectorPanelVisible);
        m_ScenePanel->MakeVisible(controlFlags.ScenePanelVisible);
        m_StatsPanel->MakeVisible(controlFlags.StatsPanelVisible);
        m_ContentBrowserPanel->MakeVisible(controlFlags.ContentBrowser);

        m_SettingsPanel->OnUpdate();
        m_HierarchyPanel->OnUpdate();
        m_InspectorPanel->OnUpdate();
        m_ScenePanel->OnUpdate();
        m_StatsPanel->OnUpdate();
        m_ContentBrowserPanel->OnUpdate();


        if (controlFlags.ApplicationCloseFlag) { EventManager::Trigger<AppClose>(); }
    }

    auto EditorLayer::InitializePanels() -> void {
        // Panels setup
        m_StatsPanel = std::make_unique<StatsPanel>();
        m_SettingsPanel = std::make_unique<SettingsPanel>();
        m_HierarchyPanel = std::make_unique<HierarchyPanel>();
        m_InspectorPanel = std::make_unique<InspectorPanel>();
        m_ContentBrowserPanel = std::make_unique<ContentBrowserPanel>("../assets");

        ScenePanelCreateInfo scenePanelCreateInfo{};
        scenePanelCreateInfo.EditorMainCamera = m_EditorCamera.get();
        m_ScenePanel = std::make_unique<ScenePanel>(std::move( scenePanelCreateInfo ));

        // Panels post setup
        m_SettingsPanel->SetFieldOfView(45.0f);
        m_SettingsPanel->SetColor(glm::vec4(0.2f, 0.2f, 0.2f, 1.0f));
    }

    auto EditorLayer::InitializeSceneCameras() -> void {
        constexpr float fieldOfView{ 45.0f };
        constexpr float aspectRatio{ 1.778f };
        constexpr float nearPlane{ 0.1f };
        constexpr float farPlane{ 1000.0f };

        // Initialize cameras
        constexpr double aspect{ 1920.0 / 1080.0 };
        m_RuntimeCamera = std::make_shared<SceneCamera>(glm::ortho(-aspect, aspect, -1.0, 1.0));
        (void)m_RuntimeCamera;

        m_EditorCamera = std::make_shared<EditorCamera>(fieldOfView, aspectRatio, nearPlane, farPlane);
    }
}
