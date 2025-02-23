/**
 * EditorLayer.cc
 * Created by kate on 6/12/23.
 * */

// C++ Standard Library
#include <memory>

// Third-Party Libraries
#include "glm/gtc/type_ptr.hpp"

// Project Headers
#include <Core/Engine.hh>
#include <Core/Events/CoreEvents.hh>
#include <Core/Events/Event.hh>
#include <Core/System/AssetsSystem.hh>
#include <Core/System/EventSystem.hh>
#include <Core/System/FileSystem.hh>
#include <Core/System/InputSystem.hh>
#include <Core/System/TimeSystem.hh>
#include <EditorModels/Enums.hh>
#include <GUI/ImGuiUtils.hh>
#include <Layers/EditorLayer.hh>
#include <Library/Filesystem/PathBuilder.hh>
#include <Library/Random/Random.hh>
#include <Models/Enums.hh>
#include <Panels/ConsolePanel.hh>
#include <Panels/ContentBrowserPanel.hh>
#include <Panels/HierarchyPanel.hh>
#include <Panels/InspectorPanel.hh>
#include <Panels/Panel.hh>
#include <Panels/RendererPanel.hh>
#include <Panels/ScenePanel.hh>
#include <Panels/SettingsPanel.hh>
#include <Panels/StatsPanel.hh>
#include <Renderer/Core/RenderQueue.hh>
#include <Scene/Camera/SceneCamera.hh>
#include <Scene/Scene/Scene.hh>

namespace Mikoto {
    EditorLayer::EditorLayer(const EditorLayerCreateInfo& createInfo)
        : Layer{ "EditorLayer" },
        m_AssetsRootDirectory{ createInfo.AssetsRootDirectory },
        m_GraphicsAPI{ createInfo.GraphicsAPI },
    m_Window{ createInfo.TargetWindow }
    {
    }

    auto EditorLayer::OnAttach() -> void {
        CreateCameras();

        PrepareSerialization();

        LoadPrefabModels();

        PrepareNewScene();

        // Need to pass in other stuff to the renderer like (wireframe on startup, the device, etc)
        m_EditorRenderer = RendererBackend::Create( RendererCreateInfo{
            .ViewportWidth{ static_cast<UInt32_T>(m_Window->GetWidth()) },
            .ViewportHeight{ static_cast<UInt32_T>(m_Window->GetHeight()) },
            .Api{ m_GraphicsAPI },
        } );

        if (m_EditorRenderer) {
            m_EditorRenderer->Init();
        } else {
            MKT_APP_LOGGER_ERROR( "EditorLayer::OnAttach - Failed to create the editor renderer." );
        }

        CreatePanels();

        m_EditorCamera->SetMovementSpeed( m_PanelRegistry.Get<SettingsPanel>()->GetData().EditorCameraMovementSpeed );
        m_EditorCamera->SetRotationSpeed( m_PanelRegistry.Get<SettingsPanel>()->GetData().EditorCameraRotationSpeed );
    }

    auto EditorLayer::OnDetach() -> void {
        m_ActiveScene = nullptr;
        m_ActiveScene = nullptr;

        m_EditorCamera = nullptr;
        m_SceneSerializer = nullptr;

        m_Project = nullptr;

        m_EditorRenderer->Shutdown();
        m_EditorRenderer = nullptr;

        m_PanelRegistry.Clear();
    }

    auto EditorLayer::OnUpdate( const double timeStep ) -> void {
        const SettingsPanel& settingsPanel{ *m_PanelRegistry.Get<SettingsPanel>() };
        const SettingsPanelData& settingsPanelCurrentData{ settingsPanel.GetData() };

        // Setup camera
        m_EditorCamera->SetMovementSpeed( settingsPanelCurrentData.EditorCameraMovementSpeed );
        m_EditorCamera->SetRotationSpeed( settingsPanelCurrentData.EditorCameraRotationSpeed );

        m_EditorCamera->SetFarPlane( settingsPanelCurrentData.FarPlane );
        m_EditorCamera->SetNearPlane( settingsPanelCurrentData.NearPlane );

        m_EditorCamera->WantRotation( settingsPanelCurrentData.WantXAxisRotation, settingsPanelCurrentData.WantYAxisRotation );

        m_EditorCamera->SetFieldOfView( settingsPanelCurrentData.FieldOfView );

        const ScenePanel& scenePanel{ *m_PanelRegistry.Get<ScenePanel>() };
        m_EditorCamera->SetViewportSize( scenePanel.GetViewportWidth(), scenePanel.GetViewportHeight() );

        InputSystem& inputSystem{ Engine::GetSystem<InputSystem>() };

        if (scenePanel.IsHovered() && inputSystem.IsMouseKeyPressed( Mouse_Button_Right )) {
            m_EditorCamera->EnableCamera( true );
        } else {
            m_EditorCamera->EnableCamera( false );
        }

        m_EditorCamera->UpdateState( timeStep );

        // Setup editor
        m_EditorRenderer->SetClearColor( settingsPanel.GetData().ClearColor );

        // Setup scene
        m_ActiveScene->SetCamera( *m_EditorCamera );
        m_ActiveScene->SetRenderer( *m_EditorRenderer );

        m_ActiveScene->Update( timeStep );
    }

    auto EditorLayer::PushImGuiDrawItems() -> void {
        UpdateDockSpace();

        auto& [applicationCloseFlag,
               hierarchyPanelVisible,
               inspectorPanelVisible,
               scenePanelVisible,
               settingPanelVisible,
               statsPanelVisible,
               contentBrowserVisible,
               consolePanelVisible,
               rendererPanelVisible]{ m_ControlFlags };

        auto& settingsPanel{ *m_PanelRegistry.Get<SettingsPanel>() };
        auto& hierarchyPanel{ *m_PanelRegistry.Get<HierarchyPanel>() };
        auto& inspectorPanel{ *m_PanelRegistry.Get<InspectorPanel>() };
        auto& scenePanel{ *m_PanelRegistry.Get<ScenePanel>() };
        auto& statsPanel{ *m_PanelRegistry.Get<StatsPanel>() };
        auto& contentsBrowserPanel{ *m_PanelRegistry.Get<ContentBrowserPanel>() };
        auto& consolePanel{ *m_PanelRegistry.Get<ConsolePanel>() };
        auto& rendererPanel{ *m_PanelRegistry.Get<RendererPanel>() };

        settingsPanel.MakeVisible( settingPanelVisible );
        hierarchyPanel.MakeVisible( hierarchyPanelVisible );
        inspectorPanel.MakeVisible( inspectorPanelVisible );
        scenePanel.MakeVisible( scenePanelVisible );
        statsPanel.MakeVisible( statsPanelVisible );
        contentsBrowserPanel.MakeVisible( contentBrowserVisible );
        consolePanel.MakeVisible( consolePanelVisible );
        rendererPanel.MakeVisible( rendererPanelVisible );

        const auto& timeSystem{ Engine::GetSystem<TimeSystem>() };
        const auto timeStep{ static_cast<float>( timeSystem.GetTimeStep( TimeUnit::SECONDS ) ) };

        settingsPanel.OnUpdate( timeStep );
        hierarchyPanel.OnUpdate( timeStep );
        inspectorPanel.OnUpdate( timeStep );
        scenePanel.OnUpdate( timeStep );
        statsPanel.OnUpdate( timeStep );
        contentsBrowserPanel.OnUpdate( timeStep );
        consolePanel.OnUpdate( timeStep );
        rendererPanel.OnUpdate( timeStep );

        hierarchyPanelVisible = hierarchyPanel.IsVisible();
        inspectorPanelVisible = inspectorPanel.IsVisible();
        scenePanelVisible = scenePanel.IsVisible();
        settingPanelVisible = settingsPanel.IsVisible();
        statsPanelVisible = statsPanel.IsVisible();
        contentBrowserVisible = contentsBrowserPanel.IsVisible();
        consolePanelVisible = consolePanel.IsVisible();
        rendererPanelVisible = rendererPanel.IsVisible();

        auto& eventSystem{ Engine::GetSystem<EventSystem>() };
        if ( applicationCloseFlag ) {
            eventSystem.Trigger<AppClose>();
        }
    }

    auto EditorLayer::CreatePanels() -> void {
        ScenePanelCreateInfo scenePanelCreateInfo{
            .Width{ static_cast<UInt32_T>(m_Window->GetWidth()) },
            .Height{ static_cast<UInt32_T>(m_Window->GetHeight()) },
            .TargetScene{ m_ActiveScene.get() },
            .Renderer{ m_EditorRenderer.get() },
            .EditorMainCamera{ m_EditorCamera.get() },

            .GetActiveEntityCallback{
                [&]() -> Entity* {
                    return m_SelectedEntity;
                }
            }
        };

        SettingsPanelCreateInfo settingsPanelCreateInfo{
            .Data{
                .ClearColor{ glm::vec4( 0.2f, 0.2f, 0.2f, 1.0f ) },
                .FieldOfView{ 45.0f },
                .EditorCamera{ m_EditorCamera.get() }
            },
        };

        HierarchyPanelCreateInfo hierarchyPanelCreateInfo{
            .TargetScene{ m_ActiveScene.get() },
            .GetActiveEntityCallback{
                [&]() -> Entity* {
                    return m_SelectedEntity;
                }
            },
            .SetActiveEntityCallback{
                [&](Entity* target) -> void {
                    m_SelectedEntity = target;
                }
            },
        };

        InspectorPanelCreateInfo inspectorPanelCreateInfo{
            .TargetScene{ m_ActiveScene.get() },
            .GetActiveEntityCallback{
                [&]() -> Entity* {
                    return m_SelectedEntity;
                }
            },
            .SetActiveEntityCallback{
                [&](Entity* target) -> void {
                    m_SelectedEntity = target;
                }
            },
        };

        RendererPanelCreateInfo rendererPanelCreateInfo{
            .Width{ static_cast<UInt32_T>(m_Window->GetWidth()) },
            .Height{ static_cast<UInt32_T>(m_Window->GetHeight()) },
            .TargetScene{ m_ActiveScene.get() },
            .Renderer{ m_EditorRenderer.get() },
            .EditorMainCamera{ m_EditorCamera.get() },
        };

        m_PanelRegistry.Register<StatsPanel>();
        m_PanelRegistry.Register<ConsolePanel>();
        m_PanelRegistry.Register<RendererPanel>(rendererPanelCreateInfo);
        m_PanelRegistry.Register<HierarchyPanel>(hierarchyPanelCreateInfo);
        m_PanelRegistry.Register<SettingsPanel>( settingsPanelCreateInfo );
        m_PanelRegistry.Register<InspectorPanel>(inspectorPanelCreateInfo);
        m_PanelRegistry.Register<ContentBrowserPanel>();
        m_PanelRegistry.Register<ScenePanel>(scenePanelCreateInfo);
    }

    auto EditorLayer::CreateCameras() -> void {
        constexpr float NEAR_PLANE{ 0.1f };
        constexpr float FAR_PLANE{ 1000.0f };
        constexpr float FIELD_OF_VIEW{ 45.0f };
        constexpr float ASPECT_RATIO{ 1920.0 / 1080.0 };

        m_EditorCamera = CreateScope<SceneCamera>( FIELD_OF_VIEW, ASPECT_RATIO, NEAR_PLANE, FAR_PLANE );
    }

    auto EditorLayer::PrepareNewScene() -> void {
        CreateScene( "Sandbox" );
    }

    auto EditorLayer::PrepareSerialization() -> void {
        m_SceneSerializer = CreateScope<SceneSerializer>();
    }

    auto EditorLayer::SaveScene() const -> void {
        // prepare filters for the dialog
        std::initializer_list<std::pair<std::string, std::string>> filters{
                    { "Mikoto Scene files", "mkts,mktscene" },
                    { "Mikoto Project Files", "mkt,mktp,mktproject" }
        };

        auto& fileSystem{ Engine::GetSystem<FileSystem>() };
        const Path_T sceneLoadPath{ fileSystem.SaveDialog( "Mikoto Scene", filters ) };
        m_SceneSerializer->Serialize(*m_ActiveScene, sceneLoadPath );
    }

    auto EditorLayer::LoadScene() -> void {
        // prepare filters for the dialog
        std::initializer_list<std::pair<std::string, std::string>> filters{
                    { "Mikoto Scene files", "mkts,mktscene" },
                    { "Mikoto Project Files", "mkt,mktp,mktproject" }
        };

        auto& fileSystem{ Engine::GetSystem<FileSystem>() };
        const Path_T sceneSavePath{ fileSystem.OpenDialog( filters ) };

        m_SelectedEntity = nullptr;
        m_ActiveScene = m_SceneSerializer->Deserialize( sceneSavePath );
    }

    auto EditorLayer::CreateScene(const std::string_view name) -> void {
        m_SelectedEntity = nullptr;
        m_ActiveScene = CreateScope<Scene>( name );

        AssetsSystem& assetsSystem{ Engine::GetSystem<AssetsSystem>() };

        // Ground
        Entity* groundObjectHolder{ m_ActiveScene->CreateEntity( {
            .Name{ "Ground" },
            .Root{ nullptr },
            .ModelMesh{ assetsSystem.GetModel( GetCubePrefabName() ) },
        } )};


        // Because models usually have multiple meshes, we need to create a child entity for each mesh
        // And the way Mikoto works right now is that if a model has multipled meshes, it will create a child entity for each mesh
        auto groundObjectHolderChildren{ m_ActiveScene->FindChildrenByID( groundObjectHolder->GetComponent<TagComponent>().GetGUID() ) };

        // sanity check
        if (!groundObjectHolderChildren.empty()) {
            Entity* groundObject{ groundObjectHolderChildren[0] };

            TransformComponent& transformComponent{ groundObject->GetComponent<TransformComponent>() };
            transformComponent.SetScale( { 5.0f, 0.5f, 5.00f } );
            transformComponent.SetTranslation( { 0.0f, 0.0f, 0.0f } );
        }

        // Point light
        Entity* lightObject{ m_ActiveScene->CreateEntity( {
            .Name{ "Light" },
            .Root{ nullptr },
            .ModelMesh{ nullptr },
        } )};

        LightComponent& light{ lightObject->AddComponent<LightComponent>() };
        light.SetType(LightType::POINT_LIGHT_TYPE);

        // Scene camera
        Entity* cameraObject{ m_ActiveScene->CreateEntity( {
            .Name{ "Camera" },
            .Root{ nullptr },
            .ModelMesh{ nullptr },
        } )};

        constexpr float NEAR_PLANE{ 0.1f };
        constexpr float FAR_PLANE{ 1000.0f };
        constexpr float FIELD_OF_VIEW{ 45.0f };
        constexpr float ASPECT_RATIO{ 1920.0 / 1080.0 };

        cameraObject->AddComponent<CameraComponent>( CreateScope<SceneCamera>( FIELD_OF_VIEW, ASPECT_RATIO, NEAR_PLANE, FAR_PLANE ) );
    }

    auto EditorLayer::SaveProject() -> void {
        MKT_APP_LOGGER_INFO( "EditorLayer::SaveProject - Saving project" );
    }

    auto EditorLayer::OpenProject() -> void {
        MKT_APP_LOGGER_INFO( "EditorLayer::OpenProject - Saving project" );
    }

    auto EditorLayer::CreateProject() -> void {
        MKT_APP_LOGGER_INFO( "EditorLayer::CreateProject - Saving project" );
    }

    static auto ShowDockingDisabledMessage() -> void {
        ImGuiIO& io{ ImGui::GetIO() };

        ImGui::Text( "ERROR: Docking is not enabled! See Demo > Configuration." );
        ImGui::Text( "Set io.ConfigFlags |= ImGuiConfigFlags_DockingEnable in your code" );
        ImGui::SameLine( 0.0f, 0.0f );

        if ( ImGui::SmallButton( "Click here" ) ) {
            io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        }
    }

    auto EditorLayer::UpdateDockSpace() -> void {
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
                if ( ImGui::MenuItem( "New scene", "Ctrl + N" ) ) { CreateScene( "Sandbox3D" ); }
                if ( ImGui::MenuItem( "Open scene", "Ctrl + L" ) ) { LoadScene(); }
                if ( ImGui::MenuItem( "Save scene", "Ctrl + S" ) ) { SaveScene(); }
                if ( ImGui::MenuItem( "New project", "Ctrl + P" ) ) { CreateProject(); }
                if ( ImGui::MenuItem( "Open project", "Ctrl + P" ) ) { OpenProject(); }
                if ( ImGui::MenuItem( "Save project", "Ctrl + G" ) ) { SaveProject(); }

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

    static auto EnsureModelLoaded( const Model* model, const Path_T& path) -> void {
        if (model == nullptr) {
            MKT_THROW_RUNTIME_ERROR( fmt::format("EnsureModelLoad - Erro model [{}] was not loaded.", path.string()) );
        }
    }

    auto EditorLayer::LoadPrefabModels() const -> void {
        AssetsSystem& assetsManager{ Engine::GetSystem<AssetsSystem>() };
        FileSystem& fileSystem{ Engine::GetSystem<FileSystem>() };

        const bool invertedY{ m_GraphicsAPI == GraphicsAPI::VULKAN_API };

        ModelLoadInfo modelLoadInfo{
            .InvertedY{ invertedY },
            .WantTextures{ true }
        };

        modelLoadInfo.Path = GetSponzaPrefabName(PathBuilder()
            .WithPath( fileSystem.GetAssetsRootPath().string() )
            .WithPath( "Prefabs" )
            .WithPath( "sponza" )
            .WithPath( "glTF" )
            .WithPath( "Sponza.gltf" )
            .Build().string());
        EnsureModelLoaded(assetsManager.LoadModel(modelLoadInfo), GetSponzaPrefabName(modelLoadInfo.Path.string()));

        modelLoadInfo.Path = GetCubePrefabName(PathBuilder()
            .WithPath( fileSystem.GetAssetsRootPath().string() )
            .WithPath( "Prefabs" )
            .WithPath( "cube" )
            .WithPath( "gltf" )
            .WithPath( "scene.gltf" )
            .Build().string());
        EnsureModelLoaded(assetsManager.LoadModel(modelLoadInfo), GetCubePrefabName(modelLoadInfo.Path.string()));

        modelLoadInfo.Path = GetSpherePrefabName(PathBuilder()
            .WithPath( fileSystem.GetAssetsRootPath().string() )
            .WithPath( "Prefabs" )
            .WithPath( "sphere" )
            .WithPath( "gltf" )
            .WithPath( "scene.gltf" )
            .Build().string());
        EnsureModelLoaded(assetsManager.LoadModel(modelLoadInfo), GetSpherePrefabName(modelLoadInfo.Path.string()));

        modelLoadInfo.Path = GetCylinderPrefabName(PathBuilder()
            .WithPath( fileSystem.GetAssetsRootPath().string() )
            .WithPath( "Prefabs" )
            .WithPath( "cylinder" )
            .WithPath( "gltf" )
            .WithPath( "scene.gltf" )
            .Build().string());
        EnsureModelLoaded(assetsManager.LoadModel(modelLoadInfo), GetCylinderPrefabName(modelLoadInfo.Path.string()));

        modelLoadInfo.Path = GetConePrefabName(PathBuilder()
            .WithPath( fileSystem.GetAssetsRootPath().string() )
            .WithPath( "Prefabs" )
            .WithPath( "cone" )
            .WithPath( "gltf" )
            .WithPath( "scene.gltf" )
            .Build().string());
        EnsureModelLoaded(assetsManager.LoadModel(modelLoadInfo), GetConePrefabName(modelLoadInfo.Path.string()));
    }

    auto EditorLayer::GetPrefabModel( const PrefabSceneObject type ) -> Model* {
        Model* result{ nullptr };
        auto& assetsManager{ Engine::GetSystem<AssetsSystem>() };

        switch ( type ) {
            case PrefabSceneObject::SPRITE_PREFAB_OBJECT:
                result = assetsManager.GetModel( GetSpritePrefabName() );
                break;
            case PrefabSceneObject::CUBE_PREFAB_OBJECT:
                result = assetsManager.GetModel( GetCubePrefabName() );
                break;
            case PrefabSceneObject::SPHERE_PREFAB_OBJECT:
                result = assetsManager.GetModel( GetSpherePrefabName() );
                break;
            case PrefabSceneObject::CYLINDER_PREFAB_OBJECT:
                result = assetsManager.GetModel( GetCylinderPrefabName() );
                break;
            case PrefabSceneObject::CONE_PREFAB_OBJECT:
                result = assetsManager.GetModel( GetConePrefabName() );
                break;
            case PrefabSceneObject::SPONZA_PREFAB_OBJECT:
                result = assetsManager.GetModel( GetSponzaPrefabName() );
                break;
            default:
                MKT_APP_LOGGER_WARN( "EditorLayer::GetPrefabModel - Attempting to get prefab model not loaded/non existent." );
                break;
        }

        return result;
    }
}
