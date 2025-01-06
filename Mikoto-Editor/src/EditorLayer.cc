/**
 * EditorLayer.cc
 * Created by kate on 6/12/23.
 * */

// C++ Standard Library
#include <memory>

// Third-Party Libraries
#include "glm/gtc/type_ptr.hpp"

// Project Headers
#include "Core/CoreEvents.hh"
#include "Core/EventManager.hh"
#include "Core/FileManager.hh"
#include "Core/TimeManager.hh"
#include "GUI/ImGuiUtils.hh"
#include "Layers/EditorLayer.hh"
#include "Renderer/Core/RenderCommand.hh"
#include "Scene/SceneManager.hh"
#include "Tools/ConsoleManager.hh"

namespace Mikoto {
    auto EditorLayer::OnAttach() -> void {
        // Initialize cameras first, they are needed for some panels
        InitializeSceneCameras();

        InitializePanels();

        m_EditorCamera->SetMovementSpeed( m_SettingsPanel->GetData().EditorCameraMovementSpeed );
        m_EditorCamera->SetRotationSpeed( m_SettingsPanel->GetData().EditorCameraRotationSpeed );

        {
            // scripting test
            // TODO: does not compile
            // m_MainCamEntity.GetComponent<NativeScriptComponent>().Bind<CameraController>();
        }

        // Initialize Editor DockSpace
        auto& [OnSceneSaveCallback, OnSceneLoadCallback, OnSceneNewCallback]{ m_DockSpaceCallbacks };

        OnSceneNewCallback =
                []() -> void {
            // destroy the currently active scene, for now we will
            // not prompt the user with a window to save changes
            SceneManager::DestroyScene( SceneManager::GetActiveScene() );
            SceneManager::DisableActiveSelection();

            // create a new empty scene
            auto& newScene{ SceneManager::MakeNewScene( "Empty Scene" ) };

            // make the newly created scene the active one
            SceneManager::SetActiveScene( newScene );
        };

        OnSceneLoadCallback =
                []() -> void {
            // prepare filters for the dialog
            std::initializer_list<std::pair<std::string, std::string>> filters{
                { "Mikoto Scene files", "mkts,mktscene" },
                { "Mikoto Project Files", "mkt,mktp,mktproject" }
            };

            std::string sceneFilePath{ FileManager::OpenDialog( filters ) };

            // user canceled file dialog
            if ( sceneFilePath.empty() ) {
                ConsoleManager::PushMessage( ConsoleLogLevel::CONSOLE_WARNING, "User canceled open dialog" );
                return;
            }

            // We need to clear the scene before we load the serialized entities
            SceneManager::DestroyScene( SceneManager::GetActiveScene() );
            SceneManager::DisableActiveSelection();

            FileManager::SceneSerializer::Deserialize( sceneFilePath );
            ConsoleManager::PushMessage( ConsoleLogLevel::CONSOLE_DEBUG, fmt::format( "Loaded new scene [{}]", SceneManager::GetActiveScene().GetName() ) );
        };

        OnSceneSaveCallback =
                [&]() -> void {
            // prepare filters for the dialog
            std::initializer_list<std::pair<std::string, std::string>> filters{
                { "Mikoto Scene files", "mkts,mktscene" },
                { "Mikoto Project Files", "mkt,mktp,mktproject" }
            };

            Scene* activeScene{ std::addressof( SceneManager::GetActiveScene() ) };
            auto path{ FileManager::SaveDialog( "Mikoto Scene", filters ) };
            FileManager::SceneSerializer::Serialize( *activeScene, path );
            ConsoleManager::PushMessage( ConsoleLogLevel::CONSOLE_WARNING, fmt::format( "Saved scene to [{}]", path ) );
        };

        SceneManager::SetActiveScene( SceneManager::MakeNewScene( "Empty Scene" ) );
    }

    auto EditorLayer::OnDetach() -> void {
    }

    auto EditorLayer::OnUpdate( const double ts ) -> void {
        RenderCommand::SetClearColor( m_SettingsPanel->GetData().ClearColor );

        // Move and rotation speeds
        const auto& settingsPanelCurrentData{ m_SettingsPanel->GetData() };
        m_EditorCamera->SetMovementSpeed( settingsPanelCurrentData.EditorCameraMovementSpeed );
        m_EditorCamera->SetRotationSpeed( settingsPanelCurrentData.EditorCameraRotationSpeed );

        // Clip planes
        m_EditorCamera->SetFarPlane( settingsPanelCurrentData.FarPlane );
        m_EditorCamera->SetNearPlane( settingsPanelCurrentData.NearPlane );

        // Field of view
        m_EditorCamera->SetFieldOfView( settingsPanelCurrentData.FieldOfView );

        m_EditorCamera->OnUpdate( ts );

        auto& activeScene{ SceneManager::GetActiveScene() };
        const auto& [viewPortWidth, viewPortHeight]{ m_ScenePanel->GetData() };

        m_EditorCamera->UpdateViewMatrix();
        m_EditorCamera->UpdateProjection();
        m_EditorCamera->SetViewportSize( viewPortWidth, viewPortHeight );
        activeScene.OnEditorUpdate( ts, *m_EditorCamera );
    }

    auto EditorLayer::PushImGuiDrawItems() -> void {
        OnDockSpaceUpdate();

        auto& [applicationCloseFlag,
               hierarchyPanelVisible,
               inspectorPanelVisible,
               scenePanelVisible,
               settingPanelVisible,
               statsPanelVisible,
               contentBrowser,
               consolePanel,
               rendererPanel]{ m_ControlFlags };

        m_SettingsPanel->MakeVisible( settingPanelVisible );
        m_HierarchyPanel->MakeVisible( hierarchyPanelVisible );
        m_InspectorPanel->MakeVisible( inspectorPanelVisible );
        m_ScenePanel->MakeVisible( scenePanelVisible );
        m_StatsPanel->MakeVisible( statsPanelVisible );
        m_ContentBrowserPanel->MakeVisible( contentBrowser );
        m_ConsolePanel->MakeVisible( consolePanel );
        m_RendererPanel->MakeVisible( rendererPanel );

        const auto timeStep{ static_cast<float>( TimeManager::GetTimeStep( TimeUnit::SECONDS ) ) };

        m_SettingsPanel->OnUpdate( timeStep );
        m_HierarchyPanel->OnUpdate( timeStep );
        m_InspectorPanel->OnUpdate( timeStep );
        m_ScenePanel->OnUpdate( timeStep );
        m_StatsPanel->OnUpdate( timeStep );
        m_ContentBrowserPanel->OnUpdate( timeStep );
        m_ConsolePanel->OnUpdate( timeStep );
        m_RendererPanel->OnUpdate( timeStep );

        if ( applicationCloseFlag ) {
            EventManager::Trigger<AppClose>();
        }
    }

    auto EditorLayer::InitializePanels() -> void {
        // Panels setup
        m_StatsPanel = std::make_unique<StatsPanel>();
        m_SettingsPanel = std::make_unique<SettingsPanel>();
        m_HierarchyPanel = std::make_unique<HierarchyPanel>();
        m_InspectorPanel = std::make_unique<InspectorPanel>();
        m_ContentBrowserPanel = std::make_unique<ContentBrowserPanel>( "../Resources" );
        m_ConsolePanel = std::make_unique<ConsolePanel>();
        m_RendererPanel = std::make_unique<RendererPanel>();

        ScenePanelCreateInfo scenePanelCreateInfo{
            .EditorMainCamera = m_EditorCamera.get()
        };

        m_ScenePanel = std::make_unique<ScenePanel>( std::move( scenePanelCreateInfo ) );

        // Panels post setup
        m_SettingsPanel->SetRenderFieldOfView( 45.0f );
        m_SettingsPanel->SetRenderBackgroundColor( glm::vec4( 0.2f, 0.2f, 0.2f, 1.0f ) );
    }

    auto EditorLayer::InitializeSceneCameras() -> void {
        constexpr float fieldOfView{ 45.0f };
        constexpr float aspectRatio{ 1.778f };
        constexpr float nearPlane{ 0.1f };
        constexpr float farPlane{ 1000.0f };

        // Initialize cameras
        constexpr double aspect{ 1920.0 / 1080.0 };
        m_RuntimeCamera = std::make_shared<SceneCamera>( glm::ortho( -aspect, aspect, -1.0, 1.0 ) );
        ( void )m_RuntimeCamera;

        m_EditorCamera = std::make_shared<EditorCamera>( fieldOfView, aspectRatio, nearPlane, farPlane );
    }

    auto EditorLayer::ShowDockingDisabledMessage() -> void {
        ImGuiIO& io{ ImGui::GetIO() };

        ImGui::Text( "ERROR: Docking is not enabled! See Demo > Configuration." );
        ImGui::Text( "Set io.ConfigFlags |= ImGuiConfigFlags_DockingEnable in your code" );
        ImGui::SameLine( 0.0f, 0.0f );

        if ( ImGui::SmallButton( "Click here" ) ) {
            io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        }
    }

    auto EditorLayer::OnDockSpaceUpdate() -> void {
        // If you strip some features of, this demo is pretty much equivalent to calling DockSpaceOverViewport()!
        // In most cases you should be able to just call DockSpaceOverViewport() and ignore all the code below!
        // In this specific demo, we are not using DockSpaceOverViewport() because:
        // - we allow the host window to be floating/moveable instead of filling the viewport (when opt_fullscreen == false)
        // - we allow the host window to have padding (when optPadding == true)
        // - we have a local menu bar in the host window (vs. you could use BeginMainMenuBar() + DockSpaceOverViewport() in your code!)
        // TL;DR; this demo is more complicated than what you would normally use.
        // If we removed all the options we are showcasing, this demo would become:
        //     void ShowExampleAppDockSpace()
        //     {
        //         ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());
        //     }

        constexpr auto optPadding{ false };
        constexpr ImGuiDockNodeFlags dockSpaceConfigFlags{ ImGuiDockNodeFlags_None };

        // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
        // because it would be confusing to have two docking targets within each others.
        ImGuiWindowFlags windowFlags{ ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking };

        // Docks-pace always takes the full screen
        const ImGuiViewport* viewport{ ImGui::GetMainViewport() };
        ImGui::SetNextWindowPos( viewport->WorkPos );
        ImGui::SetNextWindowSize( viewport->WorkSize );
        ImGui::SetNextWindowViewport( viewport->ID );
        ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, 0.0f );
        ImGui::PushStyleVar( ImGuiStyleVar_WindowBorderSize, 0.0f );
        windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background
        // and handle the pass-thru hole, so we ask Begin() to not render a background.
        if constexpr ( dockSpaceConfigFlags & ImGuiDockNodeFlags_PassthruCentralNode ) {
            windowFlags |= ImGuiWindowFlags_NoBackground;
        }

        // Important: note that we proceed even if Begin() returns false (aka window is collapsed).
        // This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
        // all active windows docked into it will lose their parent and become undocked.
        // We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
        // any change of docks-pace/settings would lead to windows being stuck in limbo and never being visible.
        if constexpr ( !optPadding ) {
            ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0.0f, 0.0f ) );
        }

        ImGui::Begin( "DockSpace Demo", std::addressof( m_ControlFlags.ApplicationCloseFlag ), windowFlags );

        if constexpr ( !optPadding ) {
            ImGui::PopStyleVar();
        }

        // DockSpace is always fullscreen
        ImGui::PopStyleVar( 2 );

        // Submit the DockSpace
        const ImGuiIO& io{ ImGui::GetIO() };
        ImGuiStyle& style{ ImGui::GetStyle() };
        style.WindowMinSize.x = 450;

        // minimum imgui windows width to avoid making them flat
        const float minimumPanelsWidth{ style.WindowMinSize.x };
        if ( io.ConfigFlags & ImGuiConfigFlags_DockingEnable ) {
            const ImGuiID dockSpaceId = ImGui::GetID( "MikotoDockEditor" );
            ImGui::DockSpace( dockSpaceId, ImVec2( 0.0f, 0.0f ), dockSpaceConfigFlags );
        } else {
            ShowDockingDisabledMessage();
        }

        style.WindowMinSize.x = minimumPanelsWidth;

        if ( ImGui::BeginMenuBar() ) {
            ImGui::PushStyleVar( ImGuiStyleVar_PopupBorderSize, 1.0f );

            if ( ImGui::BeginMenu( "File" ) ) {
                // Disabling fullscreen would allow the window to be moved to the front of other windows,
                // which we can't undo at the moment without finer window depth/z control.
                if ( ImGui::MenuItem( "New scene", "Ctrl + N" ) ) { m_DockSpaceCallbacks.OnSceneNewCallback(); }
                if ( ImGui::MenuItem( "Open scene", "Ctrl + L" ) ) { m_DockSpaceCallbacks.OnSceneLoadCallback(); }
                if ( ImGui::MenuItem( "Save scene", "Ctrl + S" ) ) { m_DockSpaceCallbacks.OnSceneSaveCallback(); }

                ImGui::Separator();

                if ( ImGui::MenuItem( "Close", nullptr, false ) ) {
                    m_ControlFlags.ApplicationCloseFlag = true;
                }

                ImGui::EndMenu();
            }

            HelpMarker(
                    "When docking is enabled, you can ALWAYS dock MOST window into another! Try it now!"
                    "\n"
                    "- Drag from window title bar or their tab to dock/undock."
                    "\n"
                    "- Drag from window menu button (upper-left button) to undock an entire node (all windows)."
                    "\n"
                    "- Hold SHIFT to disable docking (if io.ConfigDockingWithShift == false, default)"
                    "\n"
                    "- Hold SHIFT to enable docking (if io.ConfigDockingWithShift == true)"
                    "\n"
                    "This demo app has nothing to do with enabling docking!"
                    "\n\n"
                    "This demo app only demonstrate the use of ImGui::DockSpace() which allows you to manually create a docking node _within_ another window."
                    "\n\n"
                    "Read comments in ShowExampleAppDockSpace() for more details." );

            if ( ImGui::BeginMenu( "Window" ) ) {
                if ( ImGui::BeginMenu( "Panels" ) ) {
                    if ( ImGui::MenuItem( "Hierarchy", nullptr, m_ControlFlags.HierarchyPanelVisible ) ) m_ControlFlags.HierarchyPanelVisible = !m_ControlFlags.HierarchyPanelVisible;
                    if ( ImGui::MenuItem( "Inspector", nullptr, m_ControlFlags.InspectorPanelVisible ) ) m_ControlFlags.InspectorPanelVisible = !m_ControlFlags.InspectorPanelVisible;
                    if ( ImGui::MenuItem( "Scene", nullptr, m_ControlFlags.ScenePanelVisible ) ) m_ControlFlags.ScenePanelVisible = !m_ControlFlags.ScenePanelVisible;
                    if ( ImGui::MenuItem( "Settings", nullptr, m_ControlFlags.SettingPanelVisible ) ) m_ControlFlags.SettingPanelVisible = !m_ControlFlags.SettingPanelVisible;
                    if ( ImGui::MenuItem( "Statistics", nullptr, m_ControlFlags.StatsPanelVisible ) ) m_ControlFlags.StatsPanelVisible = !m_ControlFlags.StatsPanelVisible;
                    if ( ImGui::MenuItem( "Content Browser", nullptr, m_ControlFlags.ContentBrowser ) ) m_ControlFlags.ContentBrowser = !m_ControlFlags.ContentBrowser;
                    if ( ImGui::MenuItem( "Console", nullptr, m_ControlFlags.ConsolePanel ) ) m_ControlFlags.ConsolePanel = !m_ControlFlags.ConsolePanel;
                    if ( ImGui::MenuItem( "Renderer", nullptr, m_ControlFlags.RendererPanel ) ) m_ControlFlags.RendererPanel = !m_ControlFlags.RendererPanel;

                    ImGui::EndMenu();
                }

                if ( ImGui::BeginMenu( "Theme" ) ) {
                    if ( ImGui::MenuItem( "Classic" ) ) {
                        ImGui::StyleColorsClassic();
                    }
                    if ( ImGui::MenuItem( "Dark Default" ) ) {
                        ImGui::StyleColorsDark();
                        ImGuiUtils::ThemeDarkModeDefault();
                    }
                    if ( ImGui::MenuItem( "Dark Alternative" ) ) {
                        ImGui::StyleColorsDark();
                        ImGuiUtils::ThemeDarkModeAlt();
                    }
                    if ( ImGui::MenuItem( "Focused" ) ) {
                        ImGui::StyleColorsDark();
                    }
                    if ( ImGui::MenuItem( "Blindness" ) ) {
                        ImGui::StyleColorsLight();
                    }

                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }

            HelpMarker( "This menu helps to change window stuff like the theme" );

            if ( ImGui::BeginMenu( "Help" ) ) {
                constexpr ImGuiPopupFlags popUpFlags{ ImGuiPopupFlags_None };

                if ( ImGui::Button( "About" ) ) {
                    ImGui::OpenPopup( "About", popUpFlags );
                }

                // Always center this window when appearing
                const ImVec2 center{ ImGui::GetMainViewport()->GetCenter() };
                ImGui::SetNextWindowPos( center, ImGuiCond_Appearing, ImVec2( 0.5f, 0.5f ) );

                if ( ImGui::BeginPopupModal( "About", nullptr, ImGuiWindowFlags_AlwaysAutoResize ) ) {
                    ImGui::Text(
                            "Mikoto is an open source 3D graphics\n"
                            "engine currently on development.\n"
                            "\nContributors:\n"
                            "kateBea: github.com/kateBea" );

                    ImGui::Separator();

                    if ( ImGui::Button( "Accept", ImVec2{ 120, 0 } ) ) {
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::SetItemDefaultFocus();
                    ImGui::EndPopup();
                }

                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
            ImGui::PopStyleVar();
        }

        ImGui::End();
    }
}
