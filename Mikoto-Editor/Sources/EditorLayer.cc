/**
 * EditorLayer.cc
 * Created by kate on 6/12/23.
 * */

// C++ Standard Library
#include <memory>

// Third-Party Libraries
#include <imgui.h>

#include "glm/gtc/type_ptr.hpp"

// Project Headers
#include <Core/InputService.hh>
#include <Core/Profiler.hh>
#include <Core/RuntimeConsole.hh>
#include <ImGui/ImGuiUtility.hh>
#include <Layers/EditorLayer.hh>
#include <Panels/ConsolePanel.hh>
#include <Panels/ContentBrowserPanel.hh>
#include <Panels/HierarchyPanel.hh>
#include <Panels/InspectorPanel.hh>
#include <Panels/ScenePanel.hh>
#include <Panels/PassVisualizerPanel.hh>
#include <Panels/SettingsPanel.hh>
#include <Panels/ScenePropertiesPanel.hh>
#include <Panels/LightingDebugPanel.hh>
#include <Panels/StatsPanel.hh>
#include <Physics/PhysicService.hh>
#include <Renderer/Core/RenderService.hh>
#include <Renderer/Core/FrameBlackboard.hh>
#include <Scene/Component.hh>
#include <Scene/SceneManager.hh>

#include "Application/EditorUtility.hh"
#include "Core/CoreEvents.hh"
#include "Core/LocalizationService.hh"
#include "ImGui/ImGuiService.hh"
#include "Panels/AssetsPanel.hh"

namespace Mikoto {

    static auto ShowDockingDisabledMessage() -> void {
        ImGuiIO &io{ ImGui::GetIO() };

        ImGui::Text( "ERROR: Docking is not enabled! See Demo > Configuration." );
        ImGui::Text( "Set io.ConfigFlags |= ImGuiConfigFlags_DockingEnable in your code" );
        ImGui::SameLine( 0.0f, 0.0f );

        if (ImGui::SmallButton( "Click here" )) { io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; }
    }


    // Used for debugging purposes. This method was used to display array of integers from the Prime number compute pass
    static auto DrawSimpleComputeDebugWindow( const std::vector<UInt32> &data ) -> void {
        if (!ImGui::Begin( "Simple Compute Pass – Primes" )) {
            ImGui::End();
            return;
        }

        ImGui::Text( "Compute buffer output (SSBO readback)" );
        ImGui::Separator();

        ImGui::Text( "Element count: %zu", data.size() );
        ImGui::Spacing();

        if (ImGui::BeginTable( "PrimeTable", 4,
                               ImGuiTableFlags_RowBg |
                               ImGuiTableFlags_Borders |
                               ImGuiTableFlags_Resizable )) {
            ImGui::TableSetupColumn( "Index" );
            ImGui::TableSetupColumn( "Value" );
            ImGui::TableSetupColumn( "Is Prime" );
            ImGui::TableSetupColumn( "Raw" );
            ImGui::TableHeadersRow();

            for (Size i{}; i < data.size(); ++i) {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex( 0 );
                ImGui::Text( "%zu", i );

                ImGui::TableSetColumnIndex( 1 );
                ImGui::Text( "%u", data[i] );

                ImGui::TableSetColumnIndex( 2 );
                if (data[i] != 0) {
                    ImGui::TextColored(
                            ImVec4( 0.2f, 0.9f, 0.2f, 1.0f ),
                            "YES"
                            );
                } else {
                    ImGui::TextColored(
                            ImVec4( 0.9f, 0.2f, 0.2f, 1.0f ),
                            "NO"
                            );
                }

                ImGui::TableSetColumnIndex( 3 );
                ImGui::Text( "0x%08X", data[i] );
            }

            ImGui::EndTable();
        }

        ImGui::End();
    }


    EditorLayer::EditorLayer( const EditorLayerCreateInfo &createInfo )
        : ILayer{ createInfo.Name }, m_Window{ createInfo.TargetWindow } {}

    auto EditorLayer::OnCreate() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_EditorState = CreateScope<EditorState>();

        SetupRenderer();

        CreateCameras();

        PrepareNewScene();
        PrepareSerialization();

        SetupEditorState();

        CreatePanels();

        // Add passes for panel preview visualizer
        FrameBlackboard *blackboard{ m_SceneRenderer->GetGraph().GetBlackboard() };
        m_EditorState->PassesCompositions.try_emplace( "TrianglePass", blackboard->GetTexture( "HelloTrianglePass_ColorTarget" ) );
        m_EditorState->PassesCompositions.try_emplace( "TexturePass", blackboard->GetTexture( "HelloTexture_ColorTarget" ) );
        m_EditorState->PassesCompositions.try_emplace( "FinalComposition", blackboard->GetTexture( "FinalCompositionPass_ColorTarget" ) );
        m_EditorState->PassesCompositions.try_emplace( "FontRenderPass", blackboard->GetTexture( "TextRenderPass_ColorTarget" ) );
        m_EditorState->PassesCompositions.try_emplace( "SkyBox", blackboard->GetTexture( "SkyboxPass_ColorTarget" ) );
    }

    auto EditorLayer::SetupRenderer() -> void {
        SceneRendererCreateInfo spec{};
        spec.WithName( "Scene renderer" )
            .WithDevice( RenderService::Get()->GetGpuDevice() );

        m_SceneRenderer = SceneRenderer::Create( spec );

        if (m_SceneRenderer) {
            m_SceneRenderer->Init();
        }

        m_EditorState->EditorSceneRenderer = m_SceneRenderer.get();
    }

    auto EditorLayer::SetupEditorState() -> void {
        m_EditorState->EditorCamera = m_EditorCamera.get();

        m_EditorState->ActiveEditorScene = m_ActiveScene;

        FrameBlackboard *blackboard{ m_SceneRenderer->GetGraph().GetBlackboard() };
        m_EditorState->FinalComposition = blackboard->GetTexture( "FinalCompositionPass_ColorTarget" );

        m_EditorState->SelectedEntity = m_ActiveScene->FindFirstByName( "Ground" );

    }

    auto EditorLayer::SetupPresentTarget( Event &event ) -> void {
        if (const auto *keyPressed{ dynamic_cast<KeyPressedEvent *>( std::addressof( event ) ) }) {
            if (keyPressed->GetKeyCode() == Key_F11) {
                if (m_RenderScreenTarget == RenderScreenTarget::PANEL) { m_RenderScreenTarget = RenderScreenTarget::WINDOW; } else { m_RenderScreenTarget = RenderScreenTarget::PANEL; }

                event.SetHandled( true );
            }
        }
    }

    auto EditorLayer::OnDestroy() -> void {
        m_EditorState = nullptr;

        m_ActiveScene = nullptr;

        m_EditorCamera = nullptr;
        m_SceneSerializer = nullptr;

        m_SceneRenderer->Shutdown();
        m_SceneRenderer = nullptr;

        m_PanelRegistry.Clear();
    }

    auto EditorLayer::OnUpdate( float timeStep ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        TextureCubeLoadDescription loadDesc{};
        loadDesc.WithType( TextureType::TEXTURE_CUBE )
            .WithBasePath("Resources/Cubemaps/Lycksele2")
            .WithFacePath( "posx.jpg" )
            .WithFacePath( "negx.jpg" )

            .WithFacePath( "negy.jpg" )
            .WithFacePath( "posy.jpg" )

            .WithFacePath( "posz.jpg" )
            .WithFacePath( "negz.jpg" );

        TextureHandle skybox{ AssetsService::Get()->LoadAsset<TextureCube>( loadDesc ) };

        if (m_ActiveScene->IsSkyboxEnabled()) {
            m_ActiveScene->SetSkybox( skybox );
        }

        m_ActiveScene->SetState( SceneState::IDLE );

        PrepareCamera( timeStep );
        PrepareRenderer( timeStep );

        m_ActiveScene->Update( timeStep );
        m_SceneRenderer->Render( timeStep );

        UpdateDockSpace();

        // Panels must appear after dock space
        // so they can become part of it
        UpdatePanels( timeStep );

        if (m_RenderScreenTarget == RenderScreenTarget::PANEL) {
            m_EditorState->RenderImage = ImGuiService::Get()->GetFinalComposition();
        } else {
            m_EditorState->RenderImage = m_EditorState->FinalComposition;
        }

        RenderService::Get()->SetPresentTarget( m_EditorState->RenderImage );
    }

    auto EditorLayer::OnEvent( Event &event ) -> void {
        if (event.IsType( EventType::KEY_PRESSED_EVENT )) {
            SetupPresentTarget( event );
        }
    }

    auto EditorLayer::UpdatePanels( const float timeStep ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        for (const auto &panel: m_PanelRegistry | std::ranges::views::values) {
            panel->OnUpdate( timeStep );
        }

    }

    auto EditorLayer::SaveScene() const -> void {
        // File filters
        SceneManager::Get()->SaveSceneFromDisk( m_ActiveScene, m_SceneSerializer.get() );
    }

    auto EditorLayer::LoadScene() -> void {
        // use the scene manager to add this loaded scene
    }

    auto EditorLayer::SaveProject() -> void {}

    auto EditorLayer::OpenProject() -> void {}

    auto EditorLayer::CreateProject() -> void {}

    auto EditorLayer::HandleWindowScreenMode() const -> void {
        if (!m_Window->IsMaximized()) {
            m_Window->SetScreenMode( ScreenMode::WINDOW_MODE_FULLSCREEN );
        } else {
            m_Window->SetScreenMode( ScreenMode::WINDOW_MODE_WINDOWED );
        }
    }

    auto EditorLayer::SetRendererResolution() const -> void {
        if (ImGui::BeginMenu( "Resolution" )) {
            if ( ImGui::MenuItem( "HD - 720p", nullptr, m_SceneRenderer->IsRenderResolution(RenderResolution::RES_HD_720P)) ) {
                m_SceneRenderer->SetRenderResolution( RenderResolution::RES_HD_720P );
            }

            if ( ImGui::MenuItem( "FHD - 1080p", nullptr, m_SceneRenderer->IsRenderResolution(RenderResolution::RES_FHD_1080)) ) {
                m_SceneRenderer->SetRenderResolution( RenderResolution::RES_FHD_1080 );
            }

            if ( ImGui::MenuItem( "QHD - 1440p", nullptr, m_SceneRenderer->IsRenderResolution(RenderResolution::RES_QHD_1440P)) ) {
                m_SceneRenderer->SetRenderResolution( RenderResolution::RES_QHD_1440P );
            }

            if ( ImGui::MenuItem( "UHD - 2160p", nullptr, m_SceneRenderer->IsRenderResolution(RenderResolution::RES_UHD_3120P)) ) {
                m_SceneRenderer->SetRenderResolution( RenderResolution::RES_UHD_3120P );
            }

            ImGui::EndMenu();
        }
    }

    auto EditorLayer::PrepareCamera( double timeStep ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        SettingsPanel *settingsPanel{ m_PanelRegistry.Get<SettingsPanel>() };
        const auto &configuration{ settingsPanel->GetData() };

        m_EditorCamera->SetMovementSpeed( configuration.EditorCameraMovementSpeed );
        m_EditorCamera->SetRotationSpeed( configuration.EditorCameraRotationSpeed );

        m_EditorCamera->SetFarPlane( configuration.FarPlane );
        m_EditorCamera->SetNearPlane( configuration.NearPlane );

        m_EditorCamera->WantRotation( configuration.WantXAxisRotation, configuration.WantYAxisRotation );

        m_EditorCamera->SetFieldOfView( configuration.FieldOfView );

        // Set viewport to the currently active window we can either expand
        // the final composition to occupy the whole screen or just an ImGui viewport
        ScenePanel *scenePanel{ m_PanelRegistry.Get<ScenePanel>() };
        if (m_RenderScreenTarget == RenderScreenTarget::PANEL) { m_EditorCamera->SetViewportSize( scenePanel->GetWidth(), scenePanel->GetHeight() ); } else { m_EditorCamera->SetViewportSize( m_Window->GetWidth(), m_Window->GetHeight() ); }

        if (InputService::Get()->IsMouseKeyPressed( Mouse_Button_Right ) && scenePanel->IsHovered()) { m_EditorCamera->EnableCamera( true ); } else { m_EditorCamera->EnableCamera( false ); }

        m_EditorCamera->UpdateState( timeStep );
    }

    auto EditorLayer::CreatePanels() -> void {
        StatsPanelCreateInfo statsCreateInfo{};
        statsCreateInfo.State = m_EditorState.get();
        m_PanelRegistry.Register<StatsPanel>( statsCreateInfo );

        ConsolePanelCreateInfo consoleCreateInfo{};
        consoleCreateInfo.State = m_EditorState.get();
        m_PanelRegistry.Register<ConsolePanel>( consoleCreateInfo );

        ScenePanelCreateInfo scenePanelCreateInfo{
            .Width = static_cast<UInt32>( m_Window->GetWidth() ),
            .Height = static_cast<UInt32>( m_Window->GetHeight() ),
            .DisplayTarget = m_EditorState->FinalComposition,
            .State = m_EditorState.get()
        };

        m_PanelRegistry.Register<ScenePanel>( scenePanelCreateInfo );

        SettingsPanelCreateInfo settingsPanelCreateInfo{};
        settingsPanelCreateInfo.State = m_EditorState.get();

        m_PanelRegistry.Register<SettingsPanel>( settingsPanelCreateInfo );

        ContentBrowserPanelDescription contentsBrowserPanelCreateInfo{};
        contentsBrowserPanelCreateInfo.Device = RenderService::Get()->GetGpuDevice();
        contentsBrowserPanelCreateInfo.State = m_EditorState.get();
        m_PanelRegistry.Register<ContentBrowserPanel>( contentsBrowserPanelCreateInfo );

        HierarchyPanelCreateInfo hierarchyPanelCreateInfo{};
        hierarchyPanelCreateInfo.State = m_EditorState.get();
        m_PanelRegistry.Register<HierarchyPanel>( hierarchyPanelCreateInfo );

        InspectorPanelCreateInfo inspectorPanelCreateInfo{};
        inspectorPanelCreateInfo.State = m_EditorState.get();
        m_PanelRegistry.Register<InspectorPanel>( inspectorPanelCreateInfo );

        AssetsPanelDescription assetsPanelDescription{};
        assetsPanelDescription.State = m_EditorState.get();
        m_PanelRegistry.Register<AssetsPanel>( assetsPanelDescription );

        PassVisualizerDescription passVisualizerDescription{};
        passVisualizerDescription.State = m_EditorState.get();
        m_PanelRegistry.Register<PassVisualizerPanel>( passVisualizerDescription );

        LightingDebugPanelCreateInfo lightingDebugPanelCreateInfo{};
        lightingDebugPanelCreateInfo.State = m_EditorState.get();
        m_PanelRegistry.Register<LightingDebugPanel>( lightingDebugPanelCreateInfo );

        ScenePropertiesPanelCreateInfo scenePropertiesPanel{};
        scenePropertiesPanel.State = m_EditorState.get();
        m_PanelRegistry.Register<ScenePropertiesPanel>( scenePropertiesPanel );
    }

    auto EditorLayer::CreateCameras() -> void {
        constexpr float NEAR_PLANE{ 0.1f };
        constexpr float FAR_PLANE{ 1000.0f };
        constexpr float FIELD_OF_VIEW{ 45.0f };
        const float ASPECT_RATIO{
            static_cast<float>( m_Window->GetWidth() ) / static_cast<float>( m_Window->GetHeight() )
        };

        m_EditorCamera = CreateScope<SceneCamera>( FIELD_OF_VIEW, ASPECT_RATIO, NEAR_PLANE, FAR_PLANE );
        m_EditorCamera->SetTargetWindow( m_Window );
    }

    auto EditorLayer::UpdateDockSpace() -> void {
        MKT_BEGIN_PROFILER_NAMED();

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
        const ImGuiViewport *viewport{ ImGui::GetMainViewport() };
        ImGui::SetNextWindowPos( viewport->WorkPos );
        ImGui::SetNextWindowSize( viewport->WorkSize );
        ImGui::SetNextWindowViewport( viewport->ID );
        ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, 0.0f );
        ImGui::PushStyleVar( ImGuiStyleVar_WindowBorderSize, 0.0f );
        windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background
        // and handle the pass-thru hole, so we ask Begin() to not render a background.
        if constexpr (dockSpaceConfigFlags & ImGuiDockNodeFlags_PassthruCentralNode) { windowFlags |= ImGuiWindowFlags_NoBackground; }

        // Important: note that we proceed even if Begin() returns false (aka window is collapsed).
        // This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
        // all active windows docked into it will lose their parent and become undocked.
        // We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
        // any change of docks-pace/settings would lead to windows being stuck in limbo and never being visible.
        if constexpr (!optPadding) { ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0.0f, 0.0f ) ); }

        ImGui::Begin( "DockSpace Demo", std::addressof( m_EditorState->ApplicationCloseFlag ), windowFlags );

        if constexpr (!optPadding) { ImGui::PopStyleVar(); }

        // DockSpace is always fullscreen
        ImGui::PopStyleVar( 2 );

        // Submit the DockSpace
        const ImGuiIO &io{ ImGui::GetIO() };
        ImGuiStyle &style{ ImGui::GetStyle() };
        style.WindowMinSize.x = 450;

        // minimum imgui windows width to avoid making them flat
        const float minimumPanelsWidth{ style.WindowMinSize.x };
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
            const ImGuiID dockSpaceId = ImGui::GetID( "MikotoDockEditor" );
            ImGui::DockSpace( dockSpaceId, ImVec2( 0.0f, 0.0f ), dockSpaceConfigFlags );
        } else { ShowDockingDisabledMessage(); }

        style.WindowMinSize.x = minimumPanelsWidth;

        ImGuiUtils::ImGuiScopedStyleVar popupBorder{ ImGuiStyleVar_PopupBorderSize, 1.0f };
        ImGuiUtils::ImGuiScopedStyleVar itemSpacing{ ImGuiStyleVar_ItemSpacing, ImVec2{ 11.0f, 11.0f } };
        ImGuiUtils::ImGuiScopedStyleVar windowPadding{ ImGuiStyleVar_WindowPadding, ImVec2{ 12.0f, 12.0f } };

        if (ImGui::BeginMenuBar()) {
            ImGui::PushStyleVar( ImGuiStyleVar_PopupBorderSize, 1.0f );

            if (ImGui::BeginMenu( "File" )) {
                // Disabling fullscreen would allow the window to be moved to the front of other windows,
                // which we can't undo at the moment without finer window depth/z control.

                if (ImGui::MenuItem( "New scene", "Ctrl + N" )) { InitializeEmptyScene( "Sandbox3D" ); }
                if (ImGui::MenuItem( "Open scene", "Ctrl + L" )) { LoadScene(); }
                if (ImGui::MenuItem( "Save scene", "Ctrl + S" )) { SaveScene(); }

                ImGui::Separator();
                if (ImGui::MenuItem( "New project", "Ctrl + P" )) { CreateProject(); }
                if (ImGui::MenuItem( "Open project", "Ctrl + P" )) { OpenProject(); }
                if (ImGui::MenuItem( "Save project", "Ctrl + G" )) { SaveProject(); }

                ImGui::Separator();

                if (ImGui::BeginMenu( "Manipulation Mode" )) {
                    if (ImGui::MenuItem( "Translate", nullptr, m_EditorState->Manipulation == ImGuiUtils::GuizmoManipulationMode::TRANSLATION )) { m_EditorState->Manipulation = ImGuiUtils::GuizmoManipulationMode::TRANSLATION; }

                    if (ImGui::MenuItem( "Rotate", nullptr, m_EditorState->Manipulation == ImGuiUtils::GuizmoManipulationMode::ROTATION )) { m_EditorState->Manipulation = ImGuiUtils::GuizmoManipulationMode::ROTATION; }

                    if (ImGui::MenuItem( "Scale", nullptr, m_EditorState->Manipulation == ImGuiUtils::GuizmoManipulationMode::SCALE )) { m_EditorState->Manipulation = ImGuiUtils::GuizmoManipulationMode::SCALE; }

                    ImGui::EndMenu();
                }

                // Screen mode
                static std::string screenMode{};

                screenMode = m_Window->IsMaximized() ? "Windowed" : "Fullscreen";
                if (ImGui::MenuItem( screenMode.c_str(), "Windows + H" )) { HandleWindowScreenMode(); }

                ImGui::Separator();

                if (ImGui::MenuItem( "Close", nullptr, false )) { m_EditorState->ApplicationCloseFlag = true; }

                ImGui::EndMenu();
            }

            ImGuiUtils::HelpMarker(
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

            if (ImGui::BeginMenu( "Window" )) {
                if (ImGui::BeginMenu( "Panels" )) {

                    // TODO: Loop

                    for (auto& panel : m_PanelRegistry | std::ranges::views::values) {
                        bool isActive{ panel->IsVisible() };
                        if (ImGui::MenuItem( panel->GetName().data(), nullptr, std::addressof( isActive ) )) {
                            panel->SetVisible( isActive );
                        }
                    }

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu( "Theme" )) {
                    if (ImGui::MenuItem( "Classic" )) { ImGui::StyleColorsClassic(); }
                    if (ImGui::MenuItem( "Dark Default" )) {
                        ImGui::StyleColorsDark();
                        ImGuiUtils::ThemeDarkModeDefault();
                    }
                    if (ImGui::MenuItem( "Dark Alternative" )) {
                        ImGui::StyleColorsDark();
                        ImGuiUtils::ThemeDarkModeAlt();
                    }
                    if (ImGui::MenuItem( "Focused" )) { ImGui::StyleColorsDark(); }
                    if (ImGui::MenuItem( "Blindness" )) { ImGui::StyleColorsLight(); }

                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }

            ImGuiUtils::HelpMarker( "This menu helps to change window stuff like the theme" );

            if (ImGui::BeginMenu( "Rendering" )) {
                // Disabling fullscreen would allow the window to be moved to the front of other windows,
                // which we can't undo at the moment without finer window depth/z control.

                SetRendererResolution();

                if (ImGui::MenuItem( "Enable SSAO", nullptr )) {}

                ImGui::EndMenu();
            }

            ImGuiUtils::HelpMarker( "Configuration about the main scene rendering." );

            if (ImGui::BeginMenu( "Language" )) {
                static constexpr std::array languages{
                    ISOLanguage::EN_US,
                    ISOLanguage::EN_GB,
                    ISOLanguage::ES_ES,
                    ISOLanguage::JA_JP,
                    ISOLanguage::ZH_CN
                };

                if (ImGui::BeginMenu( MKT_LOC( "menu_language" ).c_str() )) {
                    const ISOLanguage current{ LocalizationService::Get()->GetCurrentLanguage() };

                    for (ISOLanguage lang: languages) {
                        const bool isSelected{ ( lang == current ) };

                        if (ImGui::MenuItem( GetISOName( lang ).data(), nullptr, isSelected )) {
                            LocalizationService::Get()->SetLanguage( lang );
                        }
                    }

                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu( "Help" )) {
                constexpr ImGuiPopupFlags popUpFlags{ ImGuiPopupFlags_None };

                if (ImGui::MenuItem( "About" )) { ImGui::OpenPopup( "About", popUpFlags ); }

                // Always center this window when appearing
                const ImVec2 center{ ImGui::GetMainViewport()->GetCenter() };
                ImGui::SetNextWindowPos( center, ImGuiCond_Appearing, ImVec2( 0.5f, 0.5f ) );

                if (ImGui::BeginPopupModal( "About", nullptr, ImGuiWindowFlags_AlwaysAutoResize )) {
                    ImGui::Text(
                            "Mikoto is an open source 3D graphics\n"
                            "engine currently on development.\n"
                            "\nContributors:\n"
                            "kateBea: github.com/kateBea" );

                    ImGui::Separator();

                    if (ImGui::Button( "Accept", ImVec2{ 120, 0 } )) { ImGui::CloseCurrentPopup(); }

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

    auto EditorLayer::PrepareNewScene() -> void {
        InitializeEmptyScene( "Sandbox" );
    }

    auto EditorLayer::InitializeEmptyScene( std::string_view name ) -> void {
        m_ActiveScene = SceneManager::Get()->CreateScene( name );

        ModelLoadDescription descFirst{
            .ModelFile{ FileService::Get()->LoadFile( "./Resources/Models/1 - Box texture/BoxTexture.obj" ) },
            .WantTextures{ true }
        };

        ModelHandle box{ AssetsService::Get()->LoadAsset<Model>( descFirst ) };

        // This emitting sounds
        EntityCreateInfo groundDesc{
            .Root{ nullptr },
            .Name{ "Ground" },
            .Model{ box }
        };
        Entity *groundEntity{ m_ActiveScene->CreateEntity( groundDesc ) };
        if (groundEntity) {
            groundEntity->AddComponent<ScriptComponent>( "Resources/Script-Examples/console_rpg.lua" );

            TransformComponent &transformComponent{ groundEntity->GetComponent<TransformComponent>() };
            transformComponent.SetScale( { 100.0f, 0.5f, 100.00f } );
            transformComponent.SetTranslation( { 0.0f, 0.0f, 0.0f } );

            RigidBodyComponent &rigidBody{ groundEntity->AddComponent<RigidBodyComponent>() };
            rigidBody.SetBodyType( RigidBodyComponent::BodyType::STATIC );

            FontHandle font{ AssetsService::Get()->LoadAsset<Font>( Path{ "Resources/Fonts/Google_Sans_Code/GoogleSansCode-Italic-VariableFont_wght.ttf" } ) };

            TextComponent &text{ groundEntity->AddComponent<TextComponent>() };
            text.SetFont( font );
        }

        // First box
        EntityCreateInfo boxDesc{
            .Root{ nullptr },
            .Name{ "FirstBox" },
            .Model{ box }
        };
        Entity *boxEntity{ m_ActiveScene->CreateEntity( boxDesc ) };
        if (boxEntity) {
            boxEntity->AddComponent<ScriptComponent>( "Resources/Script-Examples/hello_world.lua" );

            TransformComponent &transformComponent{ boxEntity->GetComponent<TransformComponent>() };
            transformComponent.SetTranslation( { 0.0f, 10.0f, 0.0f } );

            RigidBodyComponent &rigidBody{ boxEntity->AddComponent<RigidBodyComponent>() };
            rigidBody.SetBodyType( RigidBodyComponent::BodyType::DYNAMIC );
        }

        // Second box
        EntityCreateInfo box2Desc{
            .Root{ nullptr },
            .Name{ "SecondBox" },
            .Model{ box }
        };
        Entity *box2Entity{ m_ActiveScene->CreateEntity( box2Desc ) };
        if (box2Entity) {
            box2Entity->AddComponent<ScriptComponent>( "Resources/Script-Examples/hello_world.lua" );

            TransformComponent &transformComponent{ box2Entity->GetComponent<TransformComponent>() };
            transformComponent.SetTranslation( { 1.0f, 30.0f, 0.0f } );

            RigidBodyComponent &rigidBody{ box2Entity->AddComponent<RigidBodyComponent>() };
            rigidBody.SetBodyType( RigidBodyComponent::BodyType::DYNAMIC );
        }

        Entity *light{ m_ActiveScene->CreateEntity( "Light" ) };
        if (light) {
            light->AddComponent<ScriptComponent>( "Resources/Script-Examples/hello_world.lua" );
            LightComponent &lightComp{ light->AddComponent<LightComponent>() };
            lightComp.SetActiveType( LightType::POINT_LIGHT_TYPE );

            auto &pointLightData{ lightComp.Get<PointLight>() };
            pointLightData.SetIntensity( 112.81f );
            pointLightData.SetRadius( 30.44f );

            TransformComponent &transformComponent{ light->GetComponent<TransformComponent>() };
            transformComponent.SetTranslation( { 0.0f, 4.0f, 0.0f } );
        }

        // This is just to test clustered forward shading
        // We generate an empty object and 'lightCount' lights in random positions attached to it
        constexpr UInt32 lightCount{ 16 };
        Entity* lightCluster{ m_ActiveScene->CreateEntity( "LightCluster" ) };
        for (UInt32 count{}; count < lightCount; count++) {
            if (Entity *clusteredLight{ m_ActiveScene->CreateEntity( lightCluster, fmt::format( "Light {}", count ) ) }) {
                LightComponent &lightComp{ clusteredLight->AddComponent<LightComponent>() };
                lightComp.SetActiveType( LightType::POINT_LIGHT_TYPE );

                auto &pointLightData{ lightComp.Get<PointLight>() };
                pointLightData.SetIntensity( 50.0f );
                pointLightData.SetRadius( 15.0f );
                pointLightData.SetColor( GetRandomizedVec3F(0.0f, 1.0f ) );

                TransformComponent &transformComponent{ clusteredLight->GetComponent<TransformComponent>() };
                transformComponent.SetTranslation( { GetRandomReal(-66.0f, 125.0f), 2.0f, GetRandomReal(-100.0f, 100.0f) } );

                // Test heatmaps
                //transformComponent.SetTranslation( { GetRandomReal(0, 10.0f), 2.0f, GetRandomReal(0, 15) } );
            }
        }
    }

    auto EditorLayer::PrepareSerialization() -> void { m_SceneSerializer = CreateScope<SceneSerializer>(); }

    auto EditorLayer::PrepareRenderer( double ) -> void {
        const SettingsPanel &settingsPanel{ *m_PanelRegistry.Get<SettingsPanel>() };

        // Setup renderer
        const auto& settings{ settingsPanel.GetData() };

        m_SceneRenderer->SetScene( m_ActiveScene );
        m_SceneRenderer->SetCamera( m_EditorCamera.get() );
        m_SceneRenderer->SetViewport( 1920, 1080 );
        m_SceneRenderer->SetClusterDebugVisualizer( m_EditorState->HeatMapVisualizer );

        m_SceneRenderer->SetSkyBox( m_ActiveScene->GetSkybox() );
        m_SceneRenderer->EnableSkybox( m_ActiveScene->IsSkyboxEnabled() );

        m_SceneRenderer->SetClearColor( settings.ClearColor );
    }
}// namespace Mikoto
