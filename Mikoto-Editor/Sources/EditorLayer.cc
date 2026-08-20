//    Copyright 2026 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <imgui.h>

#include <Application/EditorApp.hh>
#include <Application/EditorUtility.hh>
#include <Core/Core.hh>
#include <ImGui/ImGuiService.hh>
#include <ImGui/ImGuiUtility.hh>
#include <Layers/EditorLayer.hh>
#include <Panels/Panels.hh>
#include <Physics/PhysicService.hh>
#include <Renderer/Core/DebugRenderer.hh>
#include <Renderer/Core/RenderService.hh>
#include <Scene/Component.hh>
#include <Scene/SceneManager.hh>
#include <algorithm>
#include <glm/gtc/type_ptr.hpp>
#include <memory>
#include <ranges>

#include "Common/String.hh"

#include <Renderer/Core/Buffer.hh>
#include <Renderer/Core/RenderUtility.hh>

namespace Mikoto {

    static auto ShowDockingDisabledMessage() -> void {
        ImGuiIO &io{ ImGui::GetIO() };

        ImGui::Text( "ERROR: Docking is not enabled! See Demo > Configuration." );
        ImGui::Text( "Set io.ConfigFlags |= ImGuiConfigFlags_DockingEnable in your code" );
        ImGui::SameLine( 0.0f, 0.0f );

        if (ImGui::SmallButton( "Click here" )) {
            io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        }
    }

    auto EditorState::IsEntityAnySelected() const -> bool {
        return SelectedEntity != nullptr || !SelectedEntities.empty();
    }
    auto EditorState::IsEntitySelected( Entity *entity ) const -> bool {
        return SelectedEntity != entity || SelectedEntities.contains( entity );
    }

    auto EditorState::GetSelectedEntity() const -> Entity * {
        return SelectedEntity;
    }

    auto EditorState::GetSelectedEntities() const -> const ankerl::unordered_dense::set<Entity*> & {
        return SelectedEntities;
    }

    auto EditorState::RegisterSelection( Entity *entity ) -> void {
        SelectedEntity = entity;
        SelectedEntity->GetComponent<HighlightComponent>().SetHighlighted( true );
    }

    auto EditorState::RemoveSingleSelection() -> void {
        SelectedEntity = nullptr;
    }

    auto EditorState::RemoveSelections( const std::vector<Entity *> &list ) -> void {
        // TODO:
    }

    auto EditorState::RegisterSelections( const std::vector<Entity *> &list ) -> void {
        // TODO:
    }

    EditorLayer::EditorLayer( Window* window)
        : ILayer{ "Editor Layer" }, m_Window{ window } {}

    auto EditorLayer::OnCreate() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_EditorState = CreateScope<EditorState>();

        SetupRenderer();

        CreateCameras();
        PrepareNewScene();

        SetupEditorState();
        CreatePanels();

        LoadResources();

        PreparePreviewTargets();
    }
    
    auto EditorLayer::SetupRenderer() -> void {
        MKT_BEGIN_PROFILER_NAMED();

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
        MKT_BEGIN_PROFILER_NAMED();

        m_EditorState->EditorCamera = m_EditorCamera.get();
        m_EditorState->ActiveEditorScene = m_ActiveScene;
        m_EditorState->FinalComposition = m_SceneRenderer->GetTexture( "Tonemap_ColorTarget" );
        m_EditorState->SelectedEntity = m_ActiveScene->FindFirstByName( "Ground" );
    }

    auto EditorLayer::SetupPresentTarget( Event &event ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // To store panels visibility state to restore later, when switching back to panel rendering
        static std::unordered_map<Panel*, bool> panelsVisibilityState{};

        if (const auto *keyPressed{ dynamic_cast<KeyPressedEvent *>( std::addressof( event ) ) }) {
            if (keyPressed->GetKeyCode() == Key_F11) {
                if (m_RenderScreenTarget == RenderScreenTarget::PANEL) {
                    m_RenderScreenTarget = RenderScreenTarget::WINDOW;

                    // Save panel visibility state before hiding them
                    for ( const auto &panel: m_PanelRegistry | std::ranges::views::values ) {
                        panelsVisibilityState[panel.get()] = panel->IsVisible();
                    }
                } else {
                    m_RenderScreenTarget = RenderScreenTarget::PANEL;

                    // Restore panel visibility state
                    for ( const auto &panel: m_PanelRegistry | std::ranges::views::values ) {
                        panel->SetVisible( panelsVisibilityState[panel.get()] );
                    }
                }

                event.SetHandled( true );
            }
        }
    }

    auto EditorLayer::LoadResources() -> void {
        MKT_BEGIN_PROFILER_NAMED();

    }

    auto EditorLayer::SetPresentTarget() -> void {
        if (m_RenderScreenTarget == RenderScreenTarget::PANEL) {
            m_EditorState->RenderImage = ImGuiService::Get()->GetFinalComposition();
        } else {
            if (!m_EditorState->ShowWireframe) {
                m_EditorState->RenderImage = m_EditorState->FinalComposition;
            } else {
                m_EditorState->RenderImage = m_EditorState->EditorSceneRenderer->GetTexture( "Wireframe_ColorTarget" );
            }
        }

        RenderService::Get()->SetPresentTarget( m_EditorState->RenderImage );
    }

    auto EditorLayer::SimpleScene() -> void {
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

        if (Entity *groundEntity{ m_ActiveScene->CreateEntity( groundDesc ) }) {
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

        if (Entity *boxEntity{ m_ActiveScene->CreateEntity( boxDesc ) }) {
            boxEntity->AddComponent<ScriptComponent>( "Resources/Script-Examples/Player1.lua" );
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

        if (Entity *box2Entity{ m_ActiveScene->CreateEntity( box2Desc ) }) {
            box2Entity->AddComponent<ScriptComponent>( "Resources/Script-Examples/Player2.lua" );
            TransformComponent &transformComponent{ box2Entity->GetComponent<TransformComponent>() };
            transformComponent.SetTranslation( { 1.0f, 30.0f, 0.0f } );

            RigidBodyComponent &rigidBody{ box2Entity->AddComponent<RigidBodyComponent>() };
            rigidBody.SetBodyType( RigidBodyComponent::BodyType::DYNAMIC );
        }

        Entity *light{ m_ActiveScene->CreateEntity( "Light" ) };
        if (light) {
            LightComponent &lightComp{ light->AddComponent<LightComponent>() };
            lightComp.SetActiveType( LightType::POINT_LIGHT_TYPE );

            auto &pointLightData{ lightComp.Get<PointLight>() };
            pointLightData.SetIntensity( 112.81f );
            pointLightData.SetRadius( 30.44f );

            TransformComponent &transformComponent{ light->GetComponent<TransformComponent>() };
            transformComponent.SetTranslation( { 0.0f, 4.0f, 0.0f } );
        }
    }

    auto EditorLayer::DebugManyLightsTest() -> void {
        // This is just to test clustered forward shading
        // We generate an empty object and 'lightCount' lights in random positions attached to it
        constexpr UInt32 lightCount{ 256 };
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
                transformComponent.SetTranslation( { GetRandomReal(-500.0f, 500.0f), 2.0f, GetRandomReal(-500.0f, 500.0f) } );

                // Test heatmaps, by accumulating many lights into small area
                // transformComponent.SetTranslation( { GetRandomReal(0, 10.0f), 2.0f, GetRandomReal(0, 15) } );
            }
        }
    }

    auto EditorLayer::DebugSpheresProperties() -> void {
        ModelHandle sphere{ AssetsService::Get()->LoadAsset<Model>( EditorApp::GetPrefabUri( PrefabModels::SPHERE ) ) };

        Entity *root{ m_ActiveScene->CreateEntity( "InstancingGridSpheres" ) };

        EntityCreateInfo info{
            .Root{ root },
            .Model{ sphere }
        };

        constexpr UInt32 gridSize{ 5 };// gridSize * gridSize spheres
        constexpr float spacing{ 30.0f };// Distance between spheres

        for (UInt32 x{}; x < gridSize; ++x) {
            for ( UInt32 y{}; y < gridSize; ++y ) {

                info.Name = fmt::format( "Sphere_{}_{}", x, y );

                if ( Entity * e{ m_ActiveScene->CreateEntity( info ) } ) {
                    auto &t{ e->GetComponent<TransformComponent>() };
                    t.SetTranslation(
                            { static_cast<float>( x ) * spacing,
                              static_cast<float>( y ) * spacing, 0.0f } );
                    auto& pbr{ e->GetComponent<MaterialComponent>() };

                    PhysicalMaterial *pbrMat{ pbr.GetMaterial().Dynamic<PhysicalMaterial>() };
                    if (pbrMat) {
                        pbrMat->SetAlphaMaskCutoff( 1.0f );
                        pbrMat->SetMetallicFactor( static_cast<float>( x ) / static_cast<float>( gridSize - 1 ) );
                        pbrMat->SetRoughnessFactor( static_cast<float>( y ) / static_cast<float>( gridSize - 1 ) );
                    }
                }
                
            }
        }
    }
        
    auto EditorLayer::DebugInstancingTest() -> void {
        ModelHandle box{ AssetsService::Get()->LoadAsset<Model>( "Resources/Models/1 - Box texture/Box.gltf" ) };

        constexpr UInt32 gridSize{ 40 }; // gridSize * gridSize cubes
        constexpr float spacing{ 15.0f }; // Distance between cubes

        Entity *root{ m_ActiveScene->CreateEntity( "InstancingGridBoxes" ) };

        EntityCreateInfo info{
            .Root{ root },
            .Name{ "" },
            .Model{ box }
        };
        
        for ( UInt32 x{}; x < gridSize; ++x ) {
            for ( UInt32 y{}; y < gridSize; ++y ) {
                for ( UInt32 z{}; z < gridSize; ++z ) {

                    info.Name = fmt::format( "Cube_{}_{}_{}", x, y, z );

                    if ( Entity * e{ m_ActiveScene->CreateEntity( info ) } ) {
                        auto &t{ e->GetComponent<TransformComponent>() };
                        t.SetTranslation(
                                { static_cast<float>( x ) * spacing,
                                  static_cast<float>( y ) * spacing,
                                  static_cast<float>( z ) * spacing } );

                        auto &pbr{ e->GetComponent<MaterialComponent>() };
                        PhysicalMaterial *pbrMat{ pbr.GetMaterial().Dynamic<PhysicalMaterial>() };
                        if ( pbrMat ) {
                            pbrMat->SetAlphaMaskCutoff( 1.0f );
                            pbrMat->SetColor( Vec4F{ GetRandomizedVec3F( 0.0f, 1.0f ), 1.0f } );
                        }
                    }

                }
            }
        }
    }
    
    auto EditorLayer::DebugDamagedHelmet() -> void {
        ModelLoadDescription descFirst{
            .ModelFile{ FileService::Get()->LoadFile( "Resources/Models/9 - Helmet/DamagedHelmet/glTF-Embedded/DamagedHelmet.gltf" ) },
            .WantTextures{ true }
        };

        ModelHandle box{ AssetsService::Get()->LoadAsset<Model>( descFirst ) };

        // This emitting sounds
        EntityCreateInfo groundDesc{
            .Root{ nullptr },
            .Name{ "Helmet" },
            .Model{ box }
        };

        if (Entity *groundEntity{ m_ActiveScene->CreateEntity( groundDesc ) }) {
        }

        if (Entity *light{ m_ActiveScene->CreateEntity( "Directional Light" ) }) {
            LightComponent &lightComp{ light->AddComponent<LightComponent>() };
            lightComp.SetActiveType( LightType::DIRECTIONAL_LIGHT_TYPE );

            auto &direLightInfo{ lightComp.Get<DirectionalLight>() };
            direLightInfo.SetIntensity( 10.0f );
        }
    }

    auto EditorLayer::OnDestroy() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_EditorState = nullptr;

        m_ActiveScene = nullptr;

        m_EditorCamera = nullptr;

        m_SceneRenderer->Shutdown();
        m_SceneRenderer = nullptr;

        m_PanelRegistry.Clear();
    }

    auto EditorLayer::OnUpdate( float timeStep ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        PrepareCamera( timeStep );
        PrepareRenderer( timeStep );

        m_ActiveScene->SetState( SceneState::IDLE );
        m_ActiveScene->Update( timeStep );

        m_SceneRenderer->Render( m_ActiveScene );

        UpdateDockSpace();

        // Panels must appear after dock space
        // so they can become part of it
        UpdatePanels( timeStep );

        SetPresentTarget();
    }

    auto EditorLayer::OnEvent( Event &event ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

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
        MKT_BEGIN_PROFILER_NAMED();
        SceneManager::Get()->SaveToDisk( m_ActiveScene );
    }

    auto EditorLayer::LoadScene() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // use the scene manager to add this loaded scene
    }

    auto EditorLayer::SaveProject() -> void {
        MKT_BEGIN_PROFILER_NAMED();

    }

    auto EditorLayer::OpenProject() -> void {
        MKT_BEGIN_PROFILER_NAMED();

    }

    auto EditorLayer::CreateProject() -> void {
        MKT_BEGIN_PROFILER_NAMED();

    }

    auto EditorLayer::HandleWindowScreenMode() const -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (!m_Window->IsMaximized()) {
            m_Window->SetScreenMode( ScreenMode::WINDOW_MODE_FULLSCREEN );
        } else {
            m_Window->SetScreenMode( ScreenMode::WINDOW_MODE_WINDOWED );
        }
    }

    auto EditorLayer::SetRendererResolution() const -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (ImGui::BeginMenu( "Resolution" )) {
            if ( ImGui::MenuItem( "HD - 720p", nullptr, m_SceneRenderer->IsRenderResolution(RenderResolution::HD_720P)) ) {
                m_SceneRenderer->SetRenderResolution( RenderResolution::HD_720P );
            }

            if ( ImGui::MenuItem( "FHD - 1080p", nullptr, m_SceneRenderer->IsRenderResolution(RenderResolution::FHD_1080)) ) {
                m_SceneRenderer->SetRenderResolution( RenderResolution::FHD_1080 );
            }

            if ( ImGui::MenuItem( "QHD - 1440p", nullptr, m_SceneRenderer->IsRenderResolution(RenderResolution::QHD_1440P)) ) {
                m_SceneRenderer->SetRenderResolution( RenderResolution::QHD_1440P );
            }

            if ( ImGui::MenuItem( "UHD - 2160p", nullptr, m_SceneRenderer->IsRenderResolution(RenderResolution::UHD_3120P)) ) {
                m_SceneRenderer->SetRenderResolution( RenderResolution::UHD_3120P );
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
        if (m_RenderScreenTarget == RenderScreenTarget::PANEL) {
            m_EditorCamera->SetViewportSize( scenePanel->GetWidth(), scenePanel->GetHeight() );
        } else {
            m_EditorCamera->SetViewportSize( m_Window->GetWidth(), m_Window->GetHeight() );
        }

        if (InputService::Get()->IsMouseKeyPressed( Mouse_Button_Right ) && (scenePanel->IsHovered() || m_RenderScreenTarget == RenderScreenTarget::WINDOW)) {
            m_EditorCamera->EnableCamera( true );

            if (!m_Window->IsCursorMode( CursorMode::DISABLED )) {
                m_Window->SetCursorMode( CursorMode::DISABLED );
            }

        } else {
            m_Window->SetCursorMode( CursorMode::NORMAL );
            m_EditorCamera->EnableCamera( false );
        }

        // Camera target
        m_EditorCamera->LockCameraToTarget( configuration.LockCameraToTarget );

        if (configuration.LockCameraToTarget && m_EditorState->SelectedEntity) {
            auto& transformComp{ m_EditorState->SelectedEntity->GetComponent<TransformComponent>() };
            m_EditorCamera->SetCameraTarget( transformComp.GetTranslation() );
        }

        m_EditorCamera->Update( timeStep );
    }

    auto EditorLayer::PreparePreviewTargets() -> void {
        m_EditorState->PassesCompositions.try_emplace( "1. Triangle", m_SceneRenderer->GetTexture( "HelloTriangle_ColorTarget" ) );
        m_EditorState->PassesCompositions.try_emplace( "2. Texture2D", m_SceneRenderer->GetTexture( "HelloTexture_ColorTarget" ) );
        m_EditorState->PassesCompositions.try_emplace( "3. BRDF LUT", m_SceneRenderer->GetTexture( "BRDFLutPass_ColorTarget" ) );
        m_EditorState->PassesCompositions.try_emplace( "5. ShadowMap", m_SceneRenderer->GetTexture( "DirectionalShadowMapPass_ColorTarget" ) );
        m_EditorState->PassesCompositions.try_emplace( "6. Bloom", m_SceneRenderer->GetTexture( "BloomBlend_ColorTarget" ) );
        m_EditorState->PassesCompositions.try_emplace( "7. Gradient", m_SceneRenderer->GetTexture( "ColorGradient_ColorTarget" ) );
        m_EditorState->PassesCompositions.try_emplace( "8. Chroma", m_SceneRenderer->GetTexture( "ChromaticAberration_ColorTarget" ) );
    }

    auto EditorLayer::CreatePanels() -> void {
        MKT_BEGIN_PROFILER_NAMED();

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
        auto* passVisualizer{ m_PanelRegistry.Register<PassVisualizerPanel>( passVisualizerDescription ) };
        passVisualizer->SetVisible( false );

        LightingDebugPanelCreateInfo lightingDebugPanelCreateInfo{};
        lightingDebugPanelCreateInfo.State = m_EditorState.get();
        m_PanelRegistry.Register<LightingDebugPanel>( lightingDebugPanelCreateInfo );

        ScenePropertiesPanelCreateInfo scenePropertiesPanel{};
        scenePropertiesPanel.State = m_EditorState.get();
        m_PanelRegistry.Register<ScenePropertiesPanel>( scenePropertiesPanel );

        RendererPanelCreateInfo rendererPanelCreateInfo{};
        rendererPanelCreateInfo.State = m_EditorState.get();
        m_PanelRegistry.Register<RendererPanel>( rendererPanelCreateInfo );

        LightingPanelCreateInfo lightingPanelCreateInfo{};
        lightingPanelCreateInfo.State = m_EditorState.get();
        m_PanelRegistry.Register<LightingPanel>( lightingPanelCreateInfo );
    }

    auto EditorLayer::CreateCameras() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        constexpr float nearPlane{ 0.1f };
        constexpr float farPlane{ 1000.0f };
        constexpr float fov{ 45.0f };
        const float aspectRatio{ static_cast<float>( m_Window->GetWidth() ) / static_cast<float>( m_Window->GetHeight() ) };

        SceneCameraDescription cameraDescription{
            .Fov{ 45.0 },
            .AspectRatio{ static_cast<float>( m_Window->GetWidth() ) / static_cast<float>( m_Window->GetHeight() ) },
            .NearPlane{ 0.1f },
            .FarPlane{ 3000.0f },
            .TargetWindow{ m_Window }
        };

        m_EditorCamera = CreateScope<SceneCamera>( cameraDescription );
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
        if constexpr (dockSpaceConfigFlags & ImGuiDockNodeFlags_PassthruCentralNode) {
            windowFlags |= ImGuiWindowFlags_NoBackground;
        }

        // Important: note that we proceed even if Begin() returns false (aka window is collapsed).
        // This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
        // all active windows docked into it will lose their parent and become undocked.
        // We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
        // any change of docks-pace/settings would lead to windows being stuck in limbo and never being visible.
        if constexpr (!optPadding) {
            ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0.0f, 0.0f ) );
        }

        ImGui::Begin( "DockSpace Demo", std::addressof( m_EditorState->ApplicationCloseFlag ), windowFlags );

        if constexpr (!optPadding) {
            ImGui::PopStyleVar();
        }
        
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
        } else {
            ShowDockingDisabledMessage();
        }

        style.WindowMinSize.x = minimumPanelsWidth;

        ImGuiUtils::ImGuiScopedStyleVar popupBorder{ ImGuiStyleVar_PopupBorderSize, 1.0f };
        ImGuiUtils::ImGuiScopedStyleVar itemSpacing{ ImGuiStyleVar_ItemSpacing, ImVec2{ 11.0f, 11.0f } };
        ImGuiUtils::ImGuiScopedStyleVar windowPadding{ ImGuiStyleVar_WindowPadding, ImVec2{ 12.0f, 12.0f } };

        if (ImGui::BeginMenuBar()) {
            ImGui::PushStyleVar( ImGuiStyleVar_PopupBorderSize, 1.0f );

            if (ImGui::BeginMenu( "File" )) {
                // Disabling fullscreen would allow the window to be moved to the front of other windows,
                // which we can't undo at the moment without finer window depth/z control.

                if (ImGui::MenuItem( "New scene", "Ctrl + N" )) { InitializeEmptyScene( "Empty scene" ); }
                if (ImGui::MenuItem( "Open scene", "Ctrl + L" )) { LoadScene(); }
                if (ImGui::MenuItem( "Save scene", "Ctrl + S" )) { SaveScene(); }

                ImGui::Separator();
                if (ImGui::MenuItem( "New project", "Ctrl + P" )) { CreateProject(); }
                if (ImGui::MenuItem( "Open project", "Ctrl + P" )) { OpenProject(); }
                if (ImGui::MenuItem( "Save project", "Ctrl + G" )) { SaveProject(); }

                ImGui::Separator();

                if (ImGui::BeginMenu( "Manipulation Mode" )) {
                    if (ImGui::MenuItem( "Translate", nullptr, m_EditorState->Manipulation == ImGuiUtils::GuizmoManipulationMode::TRANSLATION )) {
                        m_EditorState->Manipulation = ImGuiUtils::GuizmoManipulationMode::TRANSLATION;
                    }

                    if (ImGui::MenuItem( "Rotate", nullptr, m_EditorState->Manipulation == ImGuiUtils::GuizmoManipulationMode::ROTATION )) {
                        m_EditorState->Manipulation = ImGuiUtils::GuizmoManipulationMode::ROTATION;
                    }

                    if (ImGui::MenuItem( "Scale", nullptr, m_EditorState->Manipulation == ImGuiUtils::GuizmoManipulationMode::SCALE )) {
                        m_EditorState->Manipulation = ImGuiUtils::GuizmoManipulationMode::SCALE;
                    }

                    ImGui::EndMenu();
                }

                // Screen mode
                static std::string screenMode{};

                screenMode = m_Window->IsMaximized() ? "Windowed" : "Fullscreen";
                if (ImGui::MenuItem( screenMode.c_str(), "Windows + H" )) {
                    HandleWindowScreenMode();
                }

                ImGui::Separator();

                if (ImGui::MenuItem( "Close", nullptr, false )) {
                    m_EditorState->ApplicationCloseFlag = true;
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu( "Window" )) {
                if (ImGui::BeginMenu( "Panels" )) {
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
                        ImGuiService::Get()->SetThemeDarkModeDefault();
                    }
                    if (ImGui::MenuItem( "Dark Alternative" )) {
                        ImGui::StyleColorsDark();
                        ImGuiService::Get()->SetThemeDarkModeAlt();
                    }
                    if (ImGui::MenuItem( "Focused" )) { ImGui::StyleColorsDark(); }
                    if (ImGui::MenuItem( "Blindness" )) { ImGui::StyleColorsLight(); }

                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }

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

                    for ( const ISOLanguage lang: languages) {
                        const bool isSelected{ ( lang == current ) };

                        if (ImGui::MenuItem( GetISOName( lang ).data(), nullptr, isSelected )) {
                            LocalizationService::Get()->SetLanguage( lang );
                        }
                    }

                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }

            if ( ImGui::BeginMenu( "Tools" ) ) {
                // Disabling fullscreen would allow the window to be moved to the front of other windows,
                // which we can't undo at the moment without finer window depth/z control.

                SetRendererResolution();

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

#if !defined(NDEBUG)
            ImGui::TextUnformatted( StringUtil::Format(" | Build type [DEBUG]. Framerate: {:.1f}", ImGui::GetIO().Framerate ).c_str() );
#else
            ImGui::TextUnformatted( StringUtil::Format( " | Build type [RELEASE]. Framerate: {:.1f}", ImGui::GetIO().Framerate ).c_str() );
#endif

            ImGui::EndMenuBar();
            ImGui::PopStyleVar();
        }

        ImGui::End();
    }

    auto EditorLayer::PrepareNewScene() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        constexpr std::string_view sceneDefaultName{ "Sandbox" };
        InitializeEmptyScene( sceneDefaultName );
    }

    auto EditorLayer::InitializeEmptyScene( const std::string_view name ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_ActiveScene = SceneManager::Get()->CreateScene( name );

        //SimpleScene();
        //DebugInstancingTest();
        //DebugManyLightsTest();
        //DebugDamagedHelmet();

        DebugSpheresProperties();
    }

    auto EditorLayer::PrepareRenderer( double ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_SceneRenderer->SetCamera( m_EditorCamera.get() );

        m_SceneRenderer->EnableSkybox( m_ActiveScene->IsSceneBackground(SceneBackground::SKYBOX) );

        RendererPanel* settings{ m_PanelRegistry.Get<RendererPanel>() };
        m_SceneRenderer->SetWireframeEnable(m_EditorState->ShowWireframe);

        m_SceneRenderer->UseLDRCubeMap( settings->EnableSkyboxLDR() );
    }
}