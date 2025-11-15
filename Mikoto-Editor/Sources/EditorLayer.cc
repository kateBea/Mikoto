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
#include <Panels/SettingsPanel.hh>
#include <Panels/StatsPanel.hh>
#include <Physics/PhysicService.hh>
#include <Renderer/RenderService.hh>
#include <Scene/Component.hh>

namespace Mikoto {

    static auto ShowDockingDisabledMessage() -> void {
        ImGuiIO& io{ ImGui::GetIO() };

        ImGui::Text( "ERROR: Docking is not enabled! See Demo > Configuration." );
        ImGui::Text( "Set io.ConfigFlags |= ImGuiConfigFlags_DockingEnable in your code" );
        ImGui::SameLine( 0.0f, 0.0f );

        if ( ImGui::SmallButton( "Click here" ) ) {
            io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        }
    }

    EditorLayer::EditorLayer( const EditorLayerCreateInfo& createInfo )
        : ILayer{ createInfo.Name }, m_Window{ createInfo.TargetWindow } {}

    auto EditorLayer::OnCreate() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        SetupRenderer();

        CreateCameras();

        PrepareNewScene();
        PrepareSerialization();

        LoadPrefabModels();
        LoadPrefabFonts();

        SetupEditorState();

        CreatePanels();
    }

    auto EditorLayer::SetupRenderer() -> void {
        SceneRendererCreateInfo spec{};
        spec.WithName( "Scene renderer" )
                .WithDevice( RenderService::Get()->GetGpuDevice() );

        m_SceneRenderer = SceneRenderer::Create( spec );

        if ( m_SceneRenderer ) {
            m_SceneRenderer->Init();
        }
    }

    auto EditorLayer::SetupEditorState() -> void {
        m_EditorState = CreateScope<EditorState>();

        m_EditorState->EditorCamera = m_EditorCamera.get();
        m_EditorState->FinalComposition = m_SceneRenderer->GetFinalComposition();
        m_EditorState->ActiveEditorScene = m_ActiveScene.get();

        m_EditorState->SelectedEntity = m_ActiveScene->FindFirstByName( "Npc" );
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

        m_ActiveScene->SetState( SceneState::IDLE );

        SetupCamera( timeStep );
        SetupRenderer( timeStep );

        m_SceneRenderer->SetScene( m_ActiveScene.get() );
        m_SceneRenderer->SetCamera( m_EditorCamera.get() );
        m_SceneRenderer->SetViewport( 1920, 1080 );

        m_ActiveScene->Update( timeStep );
        m_SceneRenderer->Render( timeStep );

        UpdateDockSpace();

        // Panels must appear after dock space
        // so they can become part of it
        UpdatePanels( timeStep );
    }

    auto EditorLayer::OnEvent( Event& event ) -> void {
    }

    auto EditorLayer::UpdatePanels( float timeStep ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        for ( const auto& panel : m_PanelRegistry | std::ranges::views::values) {
            panel->OnUpdate( timeStep );
        }

    }

    auto EditorLayer::SaveScene() const -> void {
    }

    auto EditorLayer::LoadScene() -> void {
    }

    auto EditorLayer::SaveProject() -> void {
    }

    auto EditorLayer::OpenProject() -> void {
    }

    auto EditorLayer::CreateProject() -> void {
    }

    auto EditorLayer::HandleWindowScreenMode() const -> void {
    }

    auto EditorLayer::SetRendererResolution() const -> void {
    }

    auto EditorLayer::SetupCamera( double timeStep ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_EditorCamera->SetMovementSpeed( 13.f );
        m_EditorCamera->SetRotationSpeed( 13.f );

        m_EditorCamera->SetFarPlane( 20000.0f );
        m_EditorCamera->SetNearPlane( 1.0f );

        m_EditorCamera->WantRotation( true, true );

        m_EditorCamera->SetFieldOfView( 45 );

        // Set viewport to the currently active window we can either expand
        // the final composition to occupy the whole screen or just an ImGui viewport
        ScenePanel* scenePanel{ m_PanelRegistry.Get<ScenePanel>() };
        m_EditorCamera->SetViewportSize( scenePanel->GetWidth(), scenePanel->GetHeight() );

        if ( InputService::Get()->IsMouseKeyPressed( Mouse_Button_Right ) ) {
            m_EditorCamera->EnableCamera( true );
        } else {
            m_EditorCamera->EnableCamera( false );
        }

        m_EditorCamera->UpdateState( timeStep );
    }

    auto EditorLayer::CreatePanels() -> void {
        StatsPanelCreateInfo statsCreateInfo{};
        statsCreateInfo.State = m_EditorState.get();
        m_PanelRegistry.Register<StatsPanel>(statsCreateInfo);

        ConsolePanelCreateInfo consoleCreateInfo{};
        consoleCreateInfo.State = m_EditorState.get();
        m_PanelRegistry.Register<ConsolePanel>(consoleCreateInfo);

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

        ImGuiUtils::ImGuiScopedStyleVar popupBorder{ ImGuiStyleVar_PopupBorderSize, 1.0f };
        ImGuiUtils::ImGuiScopedStyleVar itemSpacing{ ImGuiStyleVar_ItemSpacing, ImVec2{ 11.0f, 11.0f } };
        ImGuiUtils::ImGuiScopedStyleVar windowPadding{ ImGuiStyleVar_WindowPadding, ImVec2{ 12.0f, 12.0f } };

        if ( ImGui::BeginMenuBar() ) {
            ImGui::PushStyleVar( ImGuiStyleVar_PopupBorderSize, 1.0f );

            if ( ImGui::BeginMenu( "File" ) ) {
                // Disabling fullscreen would allow the window to be moved to the front of other windows,
                // which we can't undo at the moment without finer window depth/z control.

                if ( ImGui::MenuItem( "New scene", "Ctrl + N" ) ) { InitializeEmptyScene( "Sandbox3D" ); }
                if ( ImGui::MenuItem( "Open scene", "Ctrl + L" ) ) { LoadScene(); }
                if ( ImGui::MenuItem( "Save scene", "Ctrl + S" ) ) { SaveScene(); }

                ImGui::Separator();
                if ( ImGui::MenuItem( "New project", "Ctrl + P" ) ) { CreateProject(); }
                if ( ImGui::MenuItem( "Open project", "Ctrl + P" ) ) { OpenProject(); }
                if ( ImGui::MenuItem( "Save project", "Ctrl + G" ) ) { SaveProject(); }

                ImGui::Separator();

                if ( ImGui::BeginMenu( "Manipulation Mode" ) ) {
                    if ( ImGui::MenuItem( "Translate", nullptr, m_EditorState->Manipulation == ImGuiUtils::GuizmoManipulationMode::TRANSLATION ) ) {
                        m_EditorState->Manipulation = ImGuiUtils::GuizmoManipulationMode::TRANSLATION;
                    }

                    if ( ImGui::MenuItem( "Rotate", nullptr, m_EditorState->Manipulation == ImGuiUtils::GuizmoManipulationMode::ROTATION ) ) {
                        m_EditorState->Manipulation = ImGuiUtils::GuizmoManipulationMode::ROTATION;
                    }

                    if ( ImGui::MenuItem( "Scale", nullptr, m_EditorState->Manipulation == ImGuiUtils::GuizmoManipulationMode::SCALE ) ) {
                        m_EditorState->Manipulation = ImGuiUtils::GuizmoManipulationMode::SCALE;
                    }

                    ImGui::EndMenu();
                }

                // Screen mode
                static std::string screenMode{};

                screenMode = m_Window->IsMaximized() ? "Windowed" : "Fullscreen";
                if ( ImGui::MenuItem( screenMode.c_str(), "Windows + H" ) ) { HandleWindowScreenMode(); }

                ImGui::Separator();

                if ( ImGui::MenuItem( "Close", nullptr, false ) ) {
                    m_ControlFlags.ApplicationCloseFlag = true;
                }

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

            ImGuiUtils::HelpMarker( "This menu helps to change window stuff like the theme" );

            if ( ImGui::BeginMenu( "Rendering" ) ) {
                // Disabling fullscreen would allow the window to be moved to the front of other windows,
                // which we can't undo at the moment without finer window depth/z control.

                SetRendererResolution();

                if ( ImGui::MenuItem( "Enable SSAO", nullptr ) ) {
                }

                ImGui::EndMenu();
            }

            ImGuiUtils::HelpMarker( "Configuration about the main scene rendering." );

            if ( ImGui::BeginMenu( "Help" ) ) {
                constexpr ImGuiPopupFlags popUpFlags{ ImGuiPopupFlags_None };

                if ( ImGui::MenuItem( "About" ) ) {
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

    auto EditorLayer::PrepareNewScene() -> void {
        InitializeEmptyScene( "Sandbox" );
    }

    auto EditorLayer::InitializeEmptyScene( std::string_view name ) -> void {
        m_ActiveScene = CreateScope<Scene>( name );

        ModelLoadDescription descFirst{
            .ModelFile{ FileService::Get()->LoadFile( "./Resources/Models/1 - Box texture/BoxTexture.obj" ) },
            .WantTextures{ true }
        };

        ModelHandle box{ AssetsService::Get()->LoadAsset<Model>( descFirst ) };

        m_ActiveScene = Scene::Create( "Hello World" );
        m_ActiveScene->SetName( "Change name just for fun" );

        // You need to specify the Scene the physics are simulated on
        PhysicService::Get()->SetSimulationScene( m_ActiveScene.get() );

        // This emitting sounds
        EntityCreateInfo groundDesc{
            .Root{ nullptr },
            .Name { "Ground" },
            .Model{ box }
        };
        Entity *ground{ m_ActiveScene->CreateEntity( groundDesc ) };
        if (ground) {
            ground->AddComponent<ScriptComponent>( "./hello_world.lua" );

            TransformComponent& transformComponent{ ground->GetComponent<TransformComponent>() };
            transformComponent.SetScale( { 5.0f, 0.5f, 5.00f } );
            transformComponent.SetTranslation( { 0.0f, 0.0f, 0.0f } );
        }

        // Second ground
        Entity* ground2{ m_ActiveScene->CreateEntity( groundDesc ) };
        if (ground2) {
            ground2->AddComponent<ScriptComponent>( "./hello_world.lua" );

            TransformComponent& transformComponent{ ground2->GetComponent<TransformComponent>() };
            transformComponent.SetScale( { 5.0f, 0.5f, 5.00f } );
            transformComponent.SetTranslation( { -10.0f, 0.0f, 0.0f } );
        }

        Entity* light{ m_ActiveScene->CreateEntity( "Light" ) };
        if (light) {
            light->AddComponent<ScriptComponent>( "./hello_world.lua" );
            LightComponent& lightComp{ light->AddComponent<LightComponent>() };
            lightComp.SetActiveType( LightType::POINT_LIGHT_TYPE );

            auto& pointLightData{ lightComp.Get<PointLight>() };
            pointLightData.SetIntensity( 31.81f );
            pointLightData.SetRadius( 7.44f );

            TransformComponent& transformComponent{ light->GetComponent<TransformComponent>() };
            transformComponent.SetTranslation( { 0.0f, 4.0f, 0.0f } );
        }

        //
        // // This emitting sounds
        // Entity *entity{ m_ActiveScene->CreateEntity( "Ball" ) };
        // if (entity) {
        //     entity->AddComponent<ScriptComponent>( "hello_world.lua" );
        //     entity->AddComponent<AudioSourceComponent>( "my_song.mp3" );
        // }
        //
        // // Load a model with multiple mesh nodes for testing
        // Entity *multipleNodes{ m_ActiveScene->CreateEntity( EntityCreateInfo{
        //     .Root{ entity },
        //     .Name{ "Npc" },
        //     .Model{ m_ModelMultipleMeshes },
        // } ) };
        //
        // if (entity) {
        //     multipleNodes->AddComponent<ScriptComponent>( "idle.lua" );
        //     multipleNodes->AddComponent<AudioSourceComponent>( "quack.mp3" );
        // }
        //
        // // Load a model with multiple mesh nodes for testing
        // Entity *multipleNodesNoRoot{ m_ActiveScene->CreateEntity( EntityCreateInfo{
        //     .Root{ nullptr },
        //     .Name{ "Npc 1" },
        //     //.Model{ m_ModelMultipleMeshes },
        // } ) };
        //
        // if (multipleNodesNoRoot) {
        //     multipleNodesNoRoot->AddComponent<ScriptComponent>( "idle.lua" );
        //     multipleNodesNoRoot->AddComponent<AudioSourceComponent>( "quack.mp3" );
        //
        //     multipleNodesNoRoot->AddComponent<MeshComponent>( );
        // }
        //
        // // Load a model 1 node mesh nodes for testing
        // Entity *rootNoMultiple{ m_ActiveScene->CreateEntity( EntityCreateInfo{
        //     .Root{ multipleNodesNoRoot },
        //     .Name{ "Npc 2" },
        // } ) };
        //
        // if (rootNoMultiple) {
        //     rootNoMultiple->AddComponent<ScriptComponent>( "idle.lua" );
        //     rootNoMultiple->AddComponent<AudioSourceComponent>( "quack.mp3" );
        // }
        //
        // // This can hear sound and has a camera
        // // it would make sense as we generally want stuff close to the camera to be heard
        // // the further they are from the camera, the less we can hear sources
        // Entity* m_Listener = m_ActiveScene->CreateEntity( "PlushCat" );
        // if (m_Listener) {
        //     m_Listener->AddComponent<CameraComponent>();
        //     m_Listener->AddComponent<ScriptComponent>( "hello_world.lua" );
        //     AudioListenerComponent &listenerComp{ m_Listener->AddComponent<AudioListenerComponent>() };
        //     AudioListener &audioListener{ listenerComp.GetListener() };
        //     audioListener.Apply();
        //
        //     RigidBodyComponent& rigidBody{ m_Listener->AddComponent<RigidBodyComponent>() };
        //     rigidBody.SetBodyType( RigidBodyComponent::BodyType::DYNAMIC );
        //     rigidBody.SetFriction( 0 );
        //
        //     // This requires the simulation scene to have been specified before
        //     m_ActiveScene->AttachRigidBody( m_Listener );
        // }
        //
        // // Some checks just to test the Scene interface
        // if (m_ActiveScene->ExistsByName( "PlushCat" )) {
        //     MKT_CORE_LOGGER_WARN( "Entity with name {} exists.", "PlushCat" );
        // } else {
        //     MKT_CORE_LOGGER_WARN( "Entity with name {} not exists.", "PlushCat" );
        // }
        //
        // if (m_ActiveScene->ExistsByID( 4 )) {
        //     MKT_CORE_LOGGER_WARN( "Entity with ID {} exists.", 4 );
        // } else {
        //     MKT_CORE_LOGGER_WARN( "Entity with ID {} does not exist.", 4 );
        // }
    }

    auto EditorLayer::PrepareSerialization() -> void {
        m_SceneSerializer = CreateScope<SceneSerializer>();
    }

    auto EditorLayer::LoadPrefabModels() const -> void {
        // ModelLoadDescription modelLoadInfo{
        //     .WantTextures{ true }
        // };
        //
        // std::string uri{ PathBuilder()
        //                          .WithPath( m_ModelsRootDirectory.string() )
        //                          .WithPath( "Prefabs" )
        //                          .WithPath( "sponza" )
        //                          .WithPath( "sponza.obj" )
        //                          .Build()
        //                          .string() };
        // AssetsService::GetInstance()->LoadAssetAsync<Model>( modelLoadInfo, uri );
        //
        // uri = PathBuilder()
        //               .WithPath( m_ModelsRootDirectory.string() )
        //               .WithPath( "Prefabs" )
        //               .WithPath( "cube" )
        //               .WithPath( "gltf" )
        //               .WithPath( "scene.gltf" )
        //               .Build()
        //               .string();
        // AssetsService::GetInstance()->LoadAssetAsync<Model>( modelLoadInfo, uri );
        //
        // uri = PathBuilder()
        //               .WithPath( m_ModelsRootDirectory.string() )
        //               .WithPath( "Prefabs" )
        //               .WithPath( "sphere" )
        //               .WithPath( "gltf" )
        //               .WithPath( "scene.gltf" )
        //               .Build()
        //               .string();
        // AssetsService::GetInstance()->LoadAssetAsync<Model>( modelLoadInfo, uri );
        //
        // uri = PathBuilder()
        //               .WithPath( m_ModelsRootDirectory.string() )
        //               .WithPath( "Prefabs" )
        //               .WithPath( "cylinder" )
        //               .WithPath( "gltf" )
        //               .WithPath( "scene.gltf" )
        //               .Build()
        //               .string();
        // AssetsService::GetInstance()->LoadAssetAsync<Model>( modelLoadInfo, uri );
        //
        // uri = PathBuilder()
        //               .WithPath( m_ModelsRootDirectory.string() )
        //               .WithPath( "Prefabs" )
        //               .WithPath( "cone" )
        //               .WithPath( "gltf" )
        //               .WithPath( "scene.gltf" )
        //               .Build()
        //               .string();
        // AssetsService::GetInstance()->LoadAssetAsync<Model>( modelLoadInfo, uri );
    }

    auto EditorLayer::LoadPrefabFonts() const -> void {
        // FileService::GetInstance()->LoadFileAsync(
        //                                   PathBuilder()
        //                                           .WithPath( m_FontsRootDirectory.string() )
        //                                           .WithPath( "JetBrainsMono" )
        //                                           .WithPath( "fonts" )
        //                                           .WithPath( "ttf" )
        //                                           .WithPath( "JetBrainsMono-Regular.ttf" )
        //                                           .Build() )
        //         ->SetOnCompleteTask( []( File* file ) -> void {
        //             AssetsService::GetInstance()->LoadAsset<Font>( FontLoadDescription{ .FontFile{ file }, .PixelSize{ 1.0f } } );
        //         } );
    }

    auto EditorLayer::SetupRenderer( double timeStep ) -> void {
        // const SettingsPanel& settingsPanel{ *m_PanelRegistry.Get<SettingsPanel>() };
        //
        // // Setup renderer
        // m_EditorRenderer->SetClearColor( settingsPanel.GetData().ClearColor );
        // m_EditorRenderer->EnableWireframe( settingsPanel.GetData().RenderWireframeMode );
    }

#if false
//
//
//     auto EditorLayer::OnAttach() -> void {
//         CreateCameras();
//         PrepareSerialization();
//         LoadPrefabModels();
//         LoadPrefabFonts();
//         PrepareNewScene();
//
//         // Create the backend Renderer
//         m_EditorRenderer = RenderService::GetInstance()->CreateBackend();
//
//         // Create a renderer for our scene
//         m_SceneRenderer = SceneRenderer::Create( SceneRendererCreateInfo{
//                 .Name{ "Editor Scene Renderer" },
//                 .ViewportWidth{ static_cast<UInt32_T>( m_Window->GetWidth() ) },
//                 .ViewportHeight{ static_cast<UInt32_T>( m_Window->GetHeight() ) },
//                 .RenderGraphPath{},
//                 .Device{ RenderService::GetInstance()->GetGpuDevice() },
//         } );
//
//         if ( m_SceneRenderer ) {
//             m_SceneRenderer->Init();
//         } else {
//             MKT_APP_LOGGER_ERROR( "EditorLayer::OnAttach - Failed to create the editor scene renderer." );
//         }
//
//         CreatePanels();
//
//         m_EditorCamera->SetMovementSpeed( m_PanelRegistry.Get<SettingsPanel>()->GetData().EditorCameraMovementSpeed );
//         m_EditorCamera->SetRotationSpeed( m_PanelRegistry.Get<SettingsPanel>()->GetData().EditorCameraRotationSpeed );
//     }
//
//
//
//     auto EditorLayer::PushImGuiDrawItems( const double timeStep ) -> void {
//         UpdateDockSpace();
//
//         auto& [applicationCloseFlag,
//                hierarchyPanelVisible,
//                inspectorPanelVisible,
//                scenePanelVisible,
//                settingPanelVisible,
//                statsPanelVisible,
//                contentBrowserVisible,
//                consolePanelVisible,
//                rendererPanelVisible]{ m_ControlFlags };
//
//         auto& settingsPanel{ *m_PanelRegistry.Get<SettingsPanel>() };
//         auto& hierarchyPanel{ *m_PanelRegistry.Get<HierarchyPanel>() };
//         auto& inspectorPanel{ *m_PanelRegistry.Get<InspectorPanel>() };
//         auto& scenePanel{ *m_PanelRegistry.Get<ScenePanel>() };
//         auto& statsPanel{ *m_PanelRegistry.Get<StatsPanel>() };
//         auto& contentsBrowserPanel{ *m_PanelRegistry.Get<ContentBrowserPanel>() };
//         auto& consolePanel{ *m_PanelRegistry.Get<ConsolePanel>() };
//         auto& rendererPanel{ *m_PanelRegistry.Get<RendererPanel>() };
//
//         settingsPanel.MakeVisible( settingPanelVisible );
//         hierarchyPanel.MakeVisible( hierarchyPanelVisible );
//         inspectorPanel.MakeVisible( inspectorPanelVisible );
//         scenePanel.MakeVisible( scenePanelVisible );
//         statsPanel.MakeVisible( statsPanelVisible );
//         contentsBrowserPanel.MakeVisible( contentBrowserVisible );
//         consolePanel.MakeVisible( consolePanelVisible );
//         rendererPanel.MakeVisible( rendererPanelVisible );
//
//         settingsPanel.OnUpdate( timeStep );
//         hierarchyPanel.OnUpdate( timeStep );
//         inspectorPanel.OnUpdate( timeStep );
//         scenePanel.OnUpdate( timeStep );
//         statsPanel.OnUpdate( timeStep );
//         contentsBrowserPanel.OnUpdate( timeStep );
//         consolePanel.OnUpdate( timeStep );
//         rendererPanel.OnUpdate( timeStep );
//
//         hierarchyPanelVisible = hierarchyPanel.IsVisible();
//         inspectorPanelVisible = inspectorPanel.IsVisible();
//         scenePanelVisible = scenePanel.IsVisible();
//         settingPanelVisible = settingsPanel.IsVisible();
//         statsPanelVisible = statsPanel.IsVisible();
//         contentBrowserVisible = contentsBrowserPanel.IsVisible();
//         consolePanelVisible = consolePanel.IsVisible();
//         rendererPanelVisible = rendererPanel.IsVisible();
//
//         if ( applicationCloseFlag ) {
//             EventService::GetInstance()->Trigger<AppClose>();
//         }
//     }
//
//     auto EditorLayer::CreatePanels() -> void {
//         const auto getSelectedEntity{
//             [&]() -> Entity* {
//                 return m_SelectedEntity;
//             }
//         };
//
//         const auto setCurrentSelectedEntity{
//             [&]( Entity* target ) -> void {
//                 m_SelectedEntity = target;
//
//                 if ( target == nullptr ) {
//                     m_EditorRenderer->EnableOutline( false );
//                 } else {
//                     m_EditorRenderer->EnableOutline( true );
//                     m_EditorRenderer->SetOutlineRenderTargetEntity( target->GetComponent<TagComponent>().GetGUID() );
//                 }
//             }
//         };
//
//         ScenePanelCreateInfo scenePanelCreateInfo{
//             .Width{ static_cast<UInt32_T>( m_Window->GetWidth() ) },
//             .Height{ static_cast<UInt32_T>( m_Window->GetHeight() ) },
//             .TargetScene{ m_ActiveScene.get() },
//             .Renderer{ m_EditorRenderer },
//             .EditorMainCamera{ m_EditorCamera.get() },
//
//             .GetActiveEntityCallback{ getSelectedEntity }
//         };
//
//         SettingsPanelCreateInfo settingsPanelCreateInfo{
//             .Data{
//                     .ClearColor{ glm::vec4( 0.2f, 0.2f, 0.2f, 1.0f ) },
//                     .FieldOfView{ 45.0f },
//                     .EditorCamera{ m_EditorCamera.get() } },
//         };
//
//         HierarchyPanelCreateInfo hierarchyPanelCreateInfo{
//             .TargetScene{ m_ActiveScene.get() },
//             .GetActiveEntityCallback{ getSelectedEntity },
//             .SetActiveEntityCallback{
//                     setCurrentSelectedEntity },
//         };
//
//         InspectorPanelCreateInfo inspectorPanelCreateInfo{
//             .TargetScene{ m_ActiveScene.get() },
//             .GetActiveEntityCallback{ getSelectedEntity },
//             .SetActiveEntityCallback{ setCurrentSelectedEntity },
//         };
//
//         RendererPanelCreateInfo rendererPanelCreateInfo{
//             .Width{ static_cast<UInt32_T>( m_Window->GetWidth() ) },
//             .Height{ static_cast<UInt32_T>( m_Window->GetHeight() ) },
//             .TargetScene{ m_ActiveScene.get() },
//             .Renderer{ m_EditorRenderer },
//             .EditorMainCamera{ m_EditorCamera.get() },
//         };
//
//         m_PanelRegistry.Register<StatsPanel>();
//         m_PanelRegistry.Register<ConsolePanel>();
//         m_PanelRegistry.Register<RendererPanel>( rendererPanelCreateInfo );
//         m_PanelRegistry.Register<HierarchyPanel>( hierarchyPanelCreateInfo );
//         m_PanelRegistry.Register<SettingsPanel>( settingsPanelCreateInfo );
//         m_PanelRegistry.Register<InspectorPanel>( inspectorPanelCreateInfo );
//         m_PanelRegistry.Register<ContentBrowserPanel>();
//         m_PanelRegistry.Register<ScenePanel>( scenePanelCreateInfo );
//     }
//
//
//     auto EditorLayer::HandleWindowScreenMode() const -> void {
//         if ( !m_Window->IsMaximized() ) {
//             m_Window->SetScreenMode( ScreenMode::WINDOW_MODE_FULLSCREEN );
//         } else {
//             m_Window->SetScreenMode( ScreenMode::WINDOW_MODE_WINDOWED );
//         }
//     }
//
//     auto EditorLayer::SetRendererResolution() const -> void {
//
//         if ( ImGui::BeginMenu( "Resolution" ) ) {
//             if ( ImGui::MenuItem( "HD - 720p", nullptr,
//                                   m_EditorRenderer->GetRenderResolution() == RenderResolution::RENDER_RESOLUTION_HD ) ) {
//                 m_SceneRenderer->SetRenderResolution( RenderResolution::RENDER_RESOLUTION_HD );
//             }
//
//             if ( ImGui::MenuItem( "FHD - 1080p", nullptr,
//                                   m_EditorRenderer->GetRenderResolution() == RenderResolution::RENDER_RESOLUTION_FHD ) ) {
//                 m_SceneRenderer->SetRenderResolution( RenderResolution::RENDER_RESOLUTION_FHD );
//             }
//
//             if ( ImGui::MenuItem( "QHD - 1440p", nullptr,
//                                   m_EditorRenderer->GetRenderResolution() == RenderResolution::RENDER_RESOLUTION_QHD ) ) {
//                 m_SceneRenderer->SetRenderResolution( RenderResolution::RENDER_RESOLUTION_QHD );
//             }
//
//             if ( ImGui::MenuItem( "UHD - 2160p", nullptr,
//                                   m_EditorRenderer->GetRenderResolution() == RenderResolution::RENDER_RESOLUTION_UHD ) ) {
//                 m_SceneRenderer->SetRenderResolution( RenderResolution::RENDER_RESOLUTION_UHD );
//             }
//
//             ImGui::EndMenu();
//         }
//     }
//
//     auto EditorLayer::PrepareNewScene() -> void {
//         InitializeEmptyScene( "Sandbox" );
//     }
//
//
//     auto EditorLayer::SaveScene() const -> void {
//         // File filters
//         const std::initializer_list<std::pair<std::string, std::string>> filters{
//             { "Mikoto Scene files", "mkts,mktscene" },
//             { "Mikoto Project Files", "mkt,mktp,mktproject" }
//         };
//
//         const Path_T savePath{ FileService::GetInstance()->SaveDialog( "Mikoto Scene", filters ) };
//
//         m_SceneSerializer->Serialize( *m_ActiveScene, savePath );
//     }
//
//     auto EditorLayer::LoadScene() -> void {
//         // prepare filters for the dialog
//         std::initializer_list<std::pair<std::string, std::string>> filters{
//             { "Mikoto Scene files", "mkts,mktscene" },
//             { "Mikoto Project Files", "mkt,mktp,mktproject" }
//         };
//
//         const Path_T sceneSavePath{ FileService::GetInstance()->OpenDialog( filters ) };
//
//         m_SelectedEntity = nullptr;
//         m_ActiveScene = m_SceneSerializer->Deserialize( sceneSavePath );
//     }
//
//     auto EditorLayer::InitializeEmptyScene( const std::string_view name ) -> void {
//         m_SelectedEntity = nullptr;
//         m_ActiveScene = CreateScope<Scene>( name );
//
//         // Ground
//         const std::string uri{ PathBuilder()
//                             .WithPath( m_ModelsRootDirectory.string() )
//                             .WithPath( "Prefabs" )
//                             .WithPath( "cube" )
//                             .WithPath( "gltf" )
//                             .WithPath( "scene.gltf" )
//                             .Build()
//                             .string() };
//         Entity* groundObjectHolder{ m_ActiveScene->CreateEntity( {
//                 .Name{ "Ground" },
//                 .Root{ nullptr },
//                 .ModelMesh{ AssetsService::GetInstance()->GetAssetByUri<Model>(uri) },
//         } ) };
//
//         // Because models usually have multiple meshes, we need to create a child entity for each mesh,
//         // And the way Mikoto works right now is that if a model has multiple meshes, it will create a child entity for each mesh
//         auto relations{ groundObjectHolder->GetComponent<RelationComponent>() };
//
//         if ( !relations.HasChildren() ) {
//             Entity* groundObject{ m_ActiveScene->FindByID( relations.At( 0 ) ) };
//
//             TransformComponent& transformComponent{ groundObject->GetComponent<TransformComponent>() };
//             transformComponent.SetScale( { 5.0f, 0.5f, 5.00f } );
//             transformComponent.SetTranslation( { 0.0f, 0.0f, 0.0f } );
//         }
//
//         // Point light
//         Entity* lightObject{ m_ActiveScene->CreateEntity( {
//                 .Name{ "Light" },
//                 .Root{ nullptr },
//                 .ModelMesh{ Ref<Model>::CreateEmpty() },
//         } ) };
//
//         // TODO: the scene does not know this entity is a light
//         // or rather does not have it registered as a light
//         LightComponent& light{ lightObject->AddComponent<LightComponent>() };
//         light.SetActiveType( LightType::POINT_LIGHT_TYPE );
//
//         // Scene camera
//         Entity* cameraObject{ m_ActiveScene->CreateEntity( {
//                 .Name{ "Camera" },
//                 .Root{ nullptr },
//                 .ModelMesh{ Ref<Model>::CreateEmpty() },
//         } ) };
//
//         constexpr float NEAR_PLANE{ 0.1f };
//         constexpr float FAR_PLANE{ 1000.0f };
//         constexpr float FIELD_OF_VIEW{ 45.0f };
//         constexpr float ASPECT_RATIO{ 1920.0 / 1080.0 };
//
//         cameraObject->AddComponent<CameraComponent>( CreateScope<SceneCamera>( FIELD_OF_VIEW, ASPECT_RATIO, NEAR_PLANE, FAR_PLANE ) );
//
//         // Load environment map
//         TextureLoadDescription loadInfo{};
//         loadInfo
//         .WithFile( nullptr )
//                 .WithType( TextureType::TEXTURE_CUBE );
//
//         const std::string uriCube{
//             PathBuilder()
//                 .WithPath( m_ModelsRootDirectory.string() )
//                 .WithPath( "Textures" )
//                 .WithPath( "Cube-maps" )
//                 .WithPath( "skybox.png" )
//                 .Build()
//                 .string()
//         };
//
//         AssetsService::GetInstance()->LoadAssetAsync<Texture>( loadInfo, uriCube );
//     }
//
//     auto EditorLayer::SaveProject() -> void {
//         MKT_APP_LOGGER_INFO( "EditorLayer::SaveProject - Saving project" );
//     }
//
//     auto EditorLayer::OpenProject() -> void {
//         MKT_APP_LOGGER_INFO( "EditorLayer::OpenProject - Saving project" );
//     }
//
//     auto EditorLayer::CreateProject() -> void {
//         MKT_APP_LOGGER_INFO( "EditorLayer::CreateProject - Saving project" );
//     }
//
//
//     auto EditorLayer::LoadPrefabModels() const -> void {
//         ModelLoadDescription modelLoadInfo{
//             .WantTextures{ true }
//         };
//
//         std::string uri{ PathBuilder()
//                                  .WithPath( m_ModelsRootDirectory.string() )
//                                  .WithPath( "Prefabs" )
//                                  .WithPath( "sponza" )
//                                  .WithPath( "sponza.obj" )
//                                  .Build()
//                                  .string() };
//         AssetsService::GetInstance()->LoadAssetAsync<Model>( modelLoadInfo, uri );
//
//         uri = PathBuilder()
//                       .WithPath( m_ModelsRootDirectory.string() )
//                       .WithPath( "Prefabs" )
//                       .WithPath( "cube" )
//                       .WithPath( "gltf" )
//                       .WithPath( "scene.gltf" )
//                       .Build()
//                       .string();
//         AssetsService::GetInstance()->LoadAssetAsync<Model>( modelLoadInfo, uri );
//
//         uri = PathBuilder()
//                       .WithPath( m_ModelsRootDirectory.string() )
//                       .WithPath( "Prefabs" )
//                       .WithPath( "sphere" )
//                       .WithPath( "gltf" )
//                       .WithPath( "scene.gltf" )
//                       .Build()
//                       .string();
//         AssetsService::GetInstance()->LoadAssetAsync<Model>( modelLoadInfo, uri );
//
//         uri = PathBuilder()
//                       .WithPath( m_ModelsRootDirectory.string() )
//                       .WithPath( "Prefabs" )
//                       .WithPath( "cylinder" )
//                       .WithPath( "gltf" )
//                       .WithPath( "scene.gltf" )
//                       .Build()
//                       .string();
//         AssetsService::GetInstance()->LoadAssetAsync<Model>( modelLoadInfo, uri );
//
//         uri = PathBuilder()
//                       .WithPath( m_ModelsRootDirectory.string() )
//                       .WithPath( "Prefabs" )
//                       .WithPath( "cone" )
//                       .WithPath( "gltf" )
//                       .WithPath( "scene.gltf" )
//                       .Build()
//                       .string();
//         AssetsService::GetInstance()->LoadAssetAsync<Model>( modelLoadInfo, uri );
//     }
//
//     auto EditorLayer::LoadPrefabFonts() const -> void {
//         FileService::GetInstance()->LoadFileAsync(
//                                           PathBuilder()
//                                                   .WithPath( m_FontsRootDirectory.string() )
//                                                   .WithPath( "JetBrainsMono" )
//                                                   .WithPath( "fonts" )
//                                                   .WithPath( "ttf" )
//                                                   .WithPath( "JetBrainsMono-Regular.ttf" )
//                                                   .Build() )
//                 ->SetOnCompleteTask( []( File* file ) -> void {
//                     AssetsService::GetInstance()->LoadAsset<Font>( FontLoadDescription{ .FontFile{ file }, .PixelSize{ 1.0f } } );
//                 } );
//     }
//
//     auto EditorLayer::SetupRenderer( double timeStep ) -> void {
//         const SettingsPanel& settingsPanel{ *m_PanelRegistry.Get<SettingsPanel>() };
//
//         // Setup renderer
//         m_EditorRenderer->SetClearColor( settingsPanel.GetData().ClearColor );
//         m_EditorRenderer->EnableWireframe( settingsPanel.GetData().RenderWireframeMode );
//     }
//
//     auto EditorLayer::SetupCamera( const double timeStep ) -> void {
//         const SettingsPanel& settingsPanel{ *m_PanelRegistry.Get<SettingsPanel>() };
//         const SettingsPanelData& settingsPanelCurrentData{ settingsPanel.GetData() };
//
//         // Setup camera
//         m_EditorCamera->SetMovementSpeed( settingsPanelCurrentData.EditorCameraMovementSpeed );
//         m_EditorCamera->SetRotationSpeed( settingsPanelCurrentData.EditorCameraRotationSpeed );
//
//         m_EditorCamera->SetFarPlane( settingsPanelCurrentData.FarPlane );
//         m_EditorCamera->SetNearPlane( settingsPanelCurrentData.NearPlane );
//
//         m_EditorCamera->WantRotation( settingsPanelCurrentData.WantXAxisRotation, settingsPanelCurrentData.WantYAxisRotation );
//
//         m_EditorCamera->SetFieldOfView( settingsPanelCurrentData.FieldOfView );
//
//         const ScenePanel& scenePanel{ *m_PanelRegistry.Get<ScenePanel>() };
//         m_EditorCamera->SetViewportSize( scenePanel.GetViewportWidth(), scenePanel.GetViewportHeight() );
//
//         if ( scenePanel.IsHovered() && InputService::GetInstance()->IsMouseKeyPressed( Mouse_Button_Right ) ) {
//             m_EditorCamera->EnableCamera( true );
//         } else {
//             m_EditorCamera->EnableCamera( false );
//         }
//
//         m_EditorCamera->UpdateState( timeStep );
//     }
#endif
}// namespace Mikoto
