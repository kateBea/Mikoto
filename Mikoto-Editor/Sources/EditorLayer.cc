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

#include <Core/Core.hh>
#include <Core/Timer.hh>
#include <Core/Types.hh>
#include <Core/Event.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>
#include <Core/CoreEvents.hh>
#include <Core/InputSystem.hh>
#include <Core/TimeService.hh>
#include <Core/ActionManager.hh>
#include <Core/LocalizationService.hh>

#include <Assets/ImageProcessor.hh>

#include <Scene/SceneManager.hh>

#include <Filesystem/FileWatcherService.hh>

#include <ImGui/ImGuiService.hh>
#include <ImGui/ImGuiUtility.hh>

#include <Layers/EditorLayer.hh>

#include <Memory/Allocator.hh>

#include <Renderer/Core/RenderSystem.hh>

#include <Panels/ScenePanel.hh>
#include <Panels/StatsPanel.hh>
#include <Panels/ProjectPanel.hh>
#include <Panels/LightingPanel.hh>
#include <Panels/RendererPanel.hh>
#include <Panels/SettingsPanel.hh>
#include <Panels/HierarchyPanel.hh>
#include <Panels/InspectorPanel.hh>
#include <Panels/ScriptEditPanel.hh>
#include <Panels/MaterialEditorPanel.hh>
#include <Panels/ContentBrowserPanel.hh>
#include <Panels/RuntimeConsolePanel.hh>
#include <Panels/AnimatorTimelinePanel.hh>
#include <Panels/ParticleSimulationPanel.hh>

namespace mikoto::editor {

    // https://traineq.org/imgui_bundle_explorer/
    // https://pthom.github.io/imgui_explorer/

    using namespace mikoto::core;
    using namespace mikoto::scene;
    using namespace mikoto::platform;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;

    auto EditorState::GetPrefab( PrefabModelType model ) -> ModelHandle {
        return mPrefabPaths[model];
    }

    EditorLayer::EditorLayer( Window *window )
        : ILayer{ "EditorLayer" }, mWindow{ window }, mDevice{ RenderSystem::Get()->GetGpuDevice() }
    {}

    auto EditorLayer::OnCreate() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        InitEditorState();

        InitAssets();

        InitDockingSpace();
        InitSceneRenderer();
        InitRenderGraphEditor();

        InitActionCallbacks();

        // These are part of a project settings
        InitEmptyScene();
        InitEditorCamera();
        InitEditorPanels();

        InitLastProject();
    }

    auto EditorLayer::OnDestroy() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Ensure GPU is done
        mDevice->WaitIdle();

        mPanelRegistry.Clear();

        mEditorState.reset();
        mEditorCamera.reset();

        mSceneRenderer->Shutdown();
        mSceneRenderer.reset();

        mThumbnailRenderer->Shutdown();
        mThumbnailRenderer.reset();

        mDevice = nullptr;
        mWindow = nullptr;

        mActionManager->Shutdown();
        mActionManager.reset();
    }

    auto EditorLayer::OnUpdate( float timeStep ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        UpdateCameraState( timeStep );
        UpdateSceneState( timeStep );
        UpdateRendererState( timeStep );

        RenderScene( timeStep );

        if (mScreenPresentTarget == ScreenPresentTarget::ePanels) {
            RenderSystem::Get()->SetPresentTarget( ImGuiService::Get()->GetFinalComposition() );

            UpdateDockSpace();
            UpdateDockSpacePanels( timeStep );

            RenderFrameGraphEditor();

            UpdateViewportState( timeStep );
        } else {
            RenderSystem::Get()->SetPresentTarget( mEditorState->mFinalComposition );
        }

        if (mShowImGuiDebugInfo) {
            ImGui::ShowMetricsWindow();
        }
    }

    auto EditorLayer::OnEvent( IEvent &event ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (event.IsType( EventType::MOUSE_BUTTON_PRESSED_EVENT )) {
            auto* scenePanel{ mPanelRegistry.Get<ScenePanel>() };
            auto* mouseButtongEvent{ as<MouseButtonPressedEvent*>(MKT_ADDRESSOF( event )) };

            if (mouseButtongEvent->GetMouseButton() == Mouse_Button_Right && scenePanel &&
                (scenePanel->IsHovered() || mScreenPresentTarget == ScreenPresentTarget::eFinalImage) ) {
                if (!mWindow->IsCursorMode( CursorMode::eDisabled )) {
                    mWindow->SetCursorMode( CursorMode::eDisabled );
                    mEditorCamera->EnableCamera( true );
                }
            }
        }

        if (event.IsType( EventType::MOUSE_BUTTON_RELEASED_EVENT )) {
            auto* mouseButtongEvent{ checked_cast<MouseButtonReleasedEvent*>(MKT_ADDRESSOF( event )) };

            if (mouseButtongEvent->GetMouseButton() == Mouse_Button_Right) {
                mWindow->SetCursorMode( CursorMode::eNormal );
                mEditorCamera->EnableCamera( false );
            }
        }

        if (event.IsType( EventType::CONTENT_DROPPED_EVENT )) {
            auto* contentDropped{ checked_cast<ContentDroppedEvent*>(MKT_ADDRESSOF( event )) };
            for (const auto& c : contentDropped->GetContents() ) {
                MKT_CORE_LOGGER_DEBUG( "User dropper {}", c.c_str() );
                Path path{ c };

                if ( asset::IsFileImage( path ) ) {
                    // Ideally
                    // Dropped on mesh -> 2D texture
                    // Dropped on sky -> cube-map
                    asset::AssetsService::Get()->LoadAssetAsync<ITexture>( path, TextureDimension::eTexture2D );
                } else if ( asset::IsFileModel( path ) ) {
                    threading::TaskService::Get()->Submit( [this, path]() -> void {
                        ModelLoadDescription description{
                            .mFile = FileService::Get()->LoadFile( path ),
                            .mExtractTextures = true };
                        const ModelHandle model{ AssetsService::Get()->LoadAsset<Model>( description ) };

                        const EntityCreateInfo entityCreateInfo{
                            .mRoot = nullptr,
                            .mName{ description.mFile->GetName() },
                            .mModel = model };
                        mEditorState->mActiveScene->PushEntity( entityCreateInfo );
                    } );
                }
            }
        }

        if (event.IsType( EventType::KEY_PRESSED_EVENT )) {
            // To store panels visibility state to restore later, when switching back to panel rendering
            static eastl::hash_map<Panel*, bool> panelsVisibilityState{};

            if (const auto *keyPressed{ dynamic_cast<KeyPressedEvent *>( std::addressof( event ) ) }) {
                if (keyPressed->GetKeyCode() == KeyCode::Key_F11) {
                    if (mScreenPresentTarget == ScreenPresentTarget::ePanels) {
                        mScreenPresentTarget = ScreenPresentTarget::eFinalImage;

                        // Save panel visibility state before hiding them
                        for ( const auto &panel: mPanelRegistry | std::ranges::views::values ) {
                            panelsVisibilityState[panel.get()] = panel->IsVisible();
                        }
                    } else {
                        mScreenPresentTarget = ScreenPresentTarget::ePanels;

                        // Restore panel visibility state
                        for ( const auto &panel: mPanelRegistry | std::ranges::views::values ) {
                            panel->SetVisible( panelsVisibilityState[panel.get()] );
                        }
                    }
                }

                // Handle shortcuts
                core::ModKey activeMod{ ModKey::eNone };
                const i32 mods{ keyPressed->GetModifiers() };

                if (mods & as<i32>(ModKey::eControl)) activeMod = ModKey::eControl;
                else if (mods & as<i32>(ModKey::eShift)) activeMod = ModKey::eShift;
                else if (mods & as<i32>(ModKey::eAlt)) activeMod = ModKey::eAlt;

                (void)mActionManager->Dispatch( as<KeyCode>( keyPressed->GetKeyCode() ), activeMod);
            }
        }
    }
    auto EditorLayer::InitAssets() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        ankerl::unordered_dense::map<PrefabModelType, eastl::string_view> modelPaths{
            { PrefabModelType::eCube, "Resources/Prefabs/cube/gltf/scene.gltf" },
            { PrefabModelType::eCone, "Resources/Prefabs/cone/gltf/scene.gltf" },
            { PrefabModelType::eSphere, "Resources/Prefabs/sphere/gltf/scene.gltf" },
            { PrefabModelType::eCylinder, "Resources/Prefabs/cylinder/gltf/scene.gltf" },
        };

        // So each thread writes to its own slot, need to double-check
        for ( const auto &[type, path]: modelPaths ) {
            ModelHandle model{ AssetsService::Get()->LoadAsset<Model>( path ) };
            mEditorState->mPrefabPaths[type] = model;
        }
    }

    auto EditorLayer::InitEditorState() -> void {
        mEditorState = eastl::make_unique<EditorState>();
    }

    auto EditorLayer::InitSceneRenderer() -> void {
        // Scene renderer
        auto description{ SceneRendererCreateInfo{}
            .SetName( "MainSceneRenderer" )
            .SetRenderResolution( mEditorState->mResolution )
            .SetDevice( RenderSystem::Get()->GetGpuDevice() ) };
        mSceneRenderer = SceneRenderer::Create( description );

        if (mSceneRenderer) {
            mSceneRenderer->Init();
        }

        // Thumbnails renderer for the content browser
        auto thumbnailRendererDesc{ ThumbnailRendererCreateInfo{}
            .SetName( "MainThumbnailRenderer" )
            .SetShaderBasePath( "Resources/Shaders/slang" )
            .SetRenderResolution( mEditorState->mResolution )
            .SetDevice( RenderSystem::Get()->GetGpuDevice() ) };
        mThumbnailRenderer = ThumbnailRenderer::Create( thumbnailRendererDesc );

        if (mThumbnailRenderer) {
            mThumbnailRenderer->Init();
        }

        mEditorState->mSceneRenderer = mSceneRenderer.get();
        mEditorState->mThumbnailRenderer = mThumbnailRenderer.get();

        // Prepare final image
        mEditorState->mFinalComposition = mSceneRenderer->GetFinalImage( FinalImageType::eTonemap_Output );
    }

    auto EditorLayer::InitEditorCamera() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        auto cameraDesc{ SceneCameraDescription{}
            .SetFieldOfView( 45.0f )
            .SetFarPlane( 0.1f )
            .SetNearPlane( 3000.0f )
            .SetTargetWindow( mWindow )
            .SetAspectRatio( as<f32>( mWindow->GetWidth() ), as<f32>( mWindow->GetHeight() ) ) };
        mEditorCamera = eastl::make_unique<SceneCamera>( cameraDesc );

        mEditorState->mActiveCamera = mEditorCamera.get();
    }

    auto EditorLayer::InitEditorPanels() -> void {
        // Stats panel
        StatsPanelCreateInfo statsCreateInfo{
            .mState = mEditorState.get() };
        mPanelRegistry.Register<StatsPanel>( statsCreateInfo );

        // Console runtime panel
        RuntimeConsolePanelCreateInfo consoleCreateInfo{
            .mState = mEditorState.get() };
        mPanelRegistry.Register<RuntimeConsolePanel>( consoleCreateInfo );

        // Scene viewport panel
        ScenePanelCreateInfo scenePanelCreateInfo{
            .mState = mEditorState.get(),
            .mImage = TextureHandle::CreateEmpty() };
        mPanelRegistry.Register<ScenePanel>( scenePanelCreateInfo );

        // Inspector panel
        InspectorPanelCreateInfo inspectorPanelCreateInfo{};
        inspectorPanelCreateInfo.mState = mEditorState.get();
        mPanelRegistry.Register<InspectorPanel>( inspectorPanelCreateInfo );

        // Hierarchy panel
        HierarchyPanelCreateInfo hierarchyPanelCreateInfo{};
        hierarchyPanelCreateInfo.mState = mEditorState.get();
        mPanelRegistry.Register<HierarchyPanel>( hierarchyPanelCreateInfo );

        // Settings panel
        SettingsPanelCreateInfo settingsPanelCreateInfo{};
        settingsPanelCreateInfo.mState = mEditorState.get();

        mPanelRegistry.Register<SettingsPanel>( settingsPanelCreateInfo );

        // Content browser
        ContentBrowserPanelDescription contentsBrowserPanelCreateInfo{};
        contentsBrowserPanelCreateInfo.mDevice = RenderSystem::Get()->GetGpuDevice();
        contentsBrowserPanelCreateInfo.mState = mEditorState.get();
        contentsBrowserPanelCreateInfo.mProjectBasePath = ".";
        contentsBrowserPanelCreateInfo.mResourcesBasePath = "Resources";
        mPanelRegistry.Register<ContentBrowserPanel>( contentsBrowserPanelCreateInfo );

        // Renderer panel
        RendererPanelCreateInfo rendererPanelCreateInfo{};
        rendererPanelCreateInfo.mState = mEditorState.get();
        //mPanelRegistry.Register<RendererPanel>( rendererPanelCreateInfo );

        LightingPanelCreateInfo lightingPanelCreateInfo{};
        lightingPanelCreateInfo.mState = mEditorState.get();
        mPanelRegistry.Register<LightingPanel>( lightingPanelCreateInfo );
    }

    auto EditorLayer::InitDockingSpace() -> void {
        //https://github.com/ocornut/imgui/wiki/Docking
    }

    auto EditorLayer::InitActionCallbacks() -> void {
        mActionManager = eastl::make_unique<ActionManager>( ActionManagerCreateInfo{} );
        mActionManager->Initialize();

        mActionManager->Bind(core::KeyCode::Key_P, core::ModKey::eControl, []() {
            MKT_CORE_LOGGER_DEBUG( "You pressed Ctrl + P (Create New project)" );
        });

        mActionManager->Bind(core::KeyCode::Key_A, core::ModKey::eControl, []() {
            MKT_CORE_LOGGER_DEBUG( "You pressed Ctrl + A (Select all entities)" );
        });

        mActionManager->Bind(core::KeyCode::Key_I, core::ModKey::eControl, [this]() {
            MKT_CORE_LOGGER_DEBUG( "You pressed Ctrl + I (ImguiDebugInfo)" );

            mShowImGuiDebugInfo = !mShowImGuiDebugInfo;
        });

        mActionManager->Bind(core::KeyCode::Key_A, core::ModKey::eControl, []() {
            MKT_CORE_LOGGER_DEBUG( "You pressed Ctrl + A (Select all entities)" );
        });

        mActionManager->Bind(core::KeyCode::Key_N, core::ModKey::eControl | core::ModKey::eAlt, []() {
            MKT_CORE_LOGGER_DEBUG( "You pressed Ctrl + Shift + N (New entity)" );
        });

        mActionManager->Bind(core::KeyCode::Key_C, core::ModKey::eControl | core::ModKey::eAlt, []() {
            MKT_CORE_LOGGER_DEBUG( "You pressed Ctrl + Shift + C (Open Console)" );
        });

        // Guizmos
        mActionManager->Bind(core::KeyCode::Key_1, core::ModKey::eControl, [this]() {
            MKT_CORE_LOGGER_DEBUG( "You pressed Ctrl + 1 (Translation Guizmo)" );
            ScenePanel* scenePanel{ mPanelRegistry.Get<ScenePanel>() };
            if (scenePanel) {
                scenePanel->SetManipulation( GuizmoType::eTranslation );
            }
        });
        mActionManager->Bind(core::KeyCode::Key_2, core::ModKey::eControl, [this]() {
            MKT_CORE_LOGGER_DEBUG( "You pressed Ctrl + 1 (Rotation Guizmo)" );
            ScenePanel* scenePanel{ mPanelRegistry.Get<ScenePanel>() };
            if (scenePanel) {
                scenePanel->SetManipulation( GuizmoType::eRotation );
            }
        });
        mActionManager->Bind(core::KeyCode::Key_3, core::ModKey::eControl, [this]() {
            MKT_CORE_LOGGER_DEBUG( "You pressed Ctrl + 1 (Scale Guizmo)" );
            ScenePanel* scenePanel{ mPanelRegistry.Get<ScenePanel>() };
            if (scenePanel) {
                scenePanel->SetManipulation( GuizmoType::eScale );
            }
        });
    }

    auto EditorLayer::InitRenderGraphEditor() -> void {
        // Renderer does not create passes yet for D3D11
        if (mDevice->IsGraphicsApi( GraphicsAPI::eD3D11 ) || mDevice->IsGraphicsApi( GraphicsAPI::eD3D12 )) {
            return;
        }

        GraphEditorBuilder builder{};

        auto& passNodes{ mSceneRenderer->GetPassList() };

        for ( const auto& [passName, pass]: passNodes ) {
            builder.PushNode( passName );
        }

        mGraphEditor.Build( builder );
    }

    auto EditorLayer::InitLastProject() -> void {

    }

    auto EditorLayer::InitEmptyScene() -> void {
        mEditorState->mActiveScene = SceneManager::Get()->CreateScene( "Scene" );

        // Camera
        Entity *camera{ mEditorState->mActiveScene->CreateEntity( "Camera" ) };
        auto& cameraComponent{ camera->AddComponent<CameraComponent>( mWindow ) };
        cameraComponent.SetClearFlags( CameraClearFlags::eClearColor );

        mSceneRenderer->SetClearColor( cameraComponent.GetClearColor() );
        mSceneRenderer->SetRenderBackground( SceneBackgroundType::eClearColor );

        // Directional light
        const EntityCreateInfo lightCreateDesc{
            .mName = "Directional light",
            .mIsLight = true,
            .mLightType = LightType::eDirectional };
        //Entity *light{ mEditorState->mActiveScene->CreateEntity( lightCreateDesc ) };


        InitSphereMaterialsScene();
        //InitInstancingTestScene();
    }

    auto EditorLayer::InitInstancingTestScene() -> void {
        Entity *root{ mEditorState->mActiveScene->CreateEntity( "Instancing Grid" ) };

        EntityCreateInfo info{
            .mRoot = root,
            .mModel = mEditorState->GetPrefab( PrefabModelType::eCube ) };

        constexpr u32 gridSize{ 40 };    // gridSize * gridSize * gridSize boxes
        constexpr f32 spacing{ 30.0f }; // Distance between boxes

        for ( u32 x{}; x < gridSize; ++x ) {
            for ( u32 y{}; y < gridSize; ++y ) {
                for ( u32 z{}; z < gridSize; ++z ) {

                    info.mName = string::Format( "Cube_{}_{}_{}", x, y, z );

                    if ( Entity * e{ mEditorState->mActiveScene->CreateEntity( info ) } ) {
                        auto &t{ e->GetComponent<TransformComponent>() };
                        t.SetTranslation(
                                { as<f32>( x ) * spacing,
                                  as<f32>( y ) * spacing,
                                  as<f32>( z ) * spacing } );

                        auto &pbr{ e->GetComponent<MaterialComponent>() };
                        PhysicalMaterial *pbrMat{ pbr.GetMaterial().Dynamic<PhysicalMaterial>() };
                        if ( pbrMat ) {
                            // Randomize color
                        }
                    }

                }
            }
        }
    }

    auto EditorLayer::InitSphereMaterialsScene() -> void {
        Entity *root{ mEditorState->mActiveScene->CreateEntity( "Spheres Grid" ) };

        EntityCreateInfo info{
            .mRoot = root,
            .mModel = mEditorState->GetPrefab( PrefabModelType::eSphere ) };
        constexpr u32 gridSize{ 5 };    // gridSize * gridSize spheres
        constexpr f32 spacing{ 30.0f }; // Distance between spheres

        for (u32 x{}; x < gridSize; ++x) {
            for ( u32 y{}; y < gridSize; ++y ) {
                info.mName = string::Format( "Sphere_{}_{}", x, y );

                if ( Entity * e{ mEditorState->mActiveScene->CreateEntity( info ) } ) {
                    auto &t{ e->GetComponent<TransformComponent>() };
                    t.SetTranslation( { as<f32>( x ) * spacing, as<f32>( y ) * spacing, 0.0f } );

                    // TODO:
                    // ENTT_ASSERT(contains(entt), "Set does not contain entity");

                    auto& materialComponent{ e->GetComponent<MaterialComponent>() };
                    auto* material{ checked_cast<PhysicalMaterial*>( materialComponent.GetMaterial().GetRaw() ) };
                    if (material) {
                        material->SetAlphaMaskCutoff( 1.0f );
                        material->SetMetallicFactor( as<f32>( x ) / as<f32>( gridSize - 1 ) );
                        material->SetRoughnessFactor( as<f32>( y ) / as<f32>( gridSize - 1 ) );
                    }
                }
            }
        }
    }

    auto EditorLayer::UpdateDockSpace() -> void {
        MKT_BEGIN_PROFILER_NAMED();

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

        ImGui::Begin( "EditorDockSpace", nullptr, windowFlags );

        if constexpr (!optPadding) {
            ImGui::PopStyleVar();
        }

        // DockSpace is always fullscreen
        ImGui::PopStyleVar( 2 );

        // Submit the DockSpace
        ImGuiIO &io{ ImGui::GetIO() };
        ImGuiStyle &style{ ImGui::GetStyle() };
        style.WindowMinSize.x = 450;

        // minimum imgui windows width to avoid making them flat
        const float minimumPanelsWidth{ style.WindowMinSize.x };
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
            // https://github.com/ocornut/imgui/wiki/Docking
            ImGuiID dockSpaceId{ ImGui::GetID( "MikotoDockEditor" ) };
            ImGui::DockSpace( dockSpaceId, ImVec2( 0.0f, 0.0f ), dockSpaceConfigFlags );
        }

        style.WindowMinSize.x = minimumPanelsWidth;

        ImGuiScopedStyleVar popupBorder{ ImGuiStyleVar_PopupBorderSize, 1.0f };
        ImGuiScopedStyleVar itemSpacing{ ImGuiStyleVar_ItemSpacing, ImVec2{ 11.0f, 11.0f } };
        ImGuiScopedStyleVar windowPadding{ ImGuiStyleVar_WindowPadding, ImVec2{ 12.0f, 12.0f } };

        if (ImGui::BeginMenuBar()) {
            ImGui::PushStyleVar( ImGuiStyleVar_PopupBorderSize, 1.0f );

            if (ImGui::BeginMenu( MKT_LOC( "menu_file" ).c_str() )) {
                // Disabling fullscreen would allow the window to be moved to the front of other windows,
                // which we can't undo at the moment without finer window depth/z control.

                if (ImGui::MenuItem( MKT_LOC( "new_scene" ).c_str(), "Ctrl + N" )) {}
                if (ImGui::MenuItem( MKT_LOC( "open_scene" ).c_str(), "Ctrl + L" )) {}
                if (ImGui::MenuItem( MKT_LOC( "save_scene" ).c_str(), "Ctrl + S" )) {}

                ImGui::Separator();
                if (ImGui::MenuItem( MKT_LOC( "new_project" ).c_str(), "Ctrl + P" )) {}
                if (ImGui::MenuItem( MKT_LOC( "open_project" ).c_str(), "Ctrl + U" )) {}
                if (ImGui::MenuItem( MKT_LOC( "save_project" ).c_str(), "Ctrl + G" )) {}

                ImGui::Separator();

                // Screen mode
                static std::string screenMode{};

                screenMode = mWindow->IsMaximized() ? "Windowed" : "Fullscreen";
                if (ImGui::MenuItem( screenMode.c_str(), "Windows + H" )) {

                }

                ImGui::Separator();

                if (ImGui::MenuItem( MKT_LOC( "menu_close" ).c_str(), nullptr, false )) {

                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu( MKT_LOC( "menu_edit" ).c_str() )) {
                if (ImGui::MenuItem( "Undo", "Ctrl+Z" )) { /* Undo action */ }
                if (ImGui::MenuItem( "Redo", "Ctrl+Y" )) { /* Redo action */ }

                ImGui::Separator();

                if (ImGui::MenuItem( "Cut", "Ctrl+X" )) { /* Cut action */ }
                if (ImGui::MenuItem( "Copy", "Ctrl+C" )) { /* Copy action */ }
                if (ImGui::MenuItem( "Paste", "Ctrl+V" )) { /* Paste action */ }
                if (ImGui::MenuItem( "Duplicate", "Ctrl+D" )) { /* Duplicate action */ }
                if (ImGui::MenuItem( "Delete", "Delete" )) { /* Delete action */ }
                ImGui::Separator();
                if (ImGui::MenuItem( "Select All", "Ctrl+A" )) { /* Select all action */ }
                if (ImGui::MenuItem( "Preferences..." )) { /* Open preferences window */ }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu( MKT_LOC( "menu_game_object" ).c_str() )) {
                if (ImGui::MenuItem( "Create Empty", "Ctrl+Shift+N" )) {
                    EntityCreateInfo createInfo{
                        .mRoot = nullptr,
                        .mName = "Empty Object" };
                    mEditorState->mActiveScene->PushEntity( createInfo );
                }

                ImGui::Separator();

                if (ImGui::BeginMenu( "3D Object" )) {
                    if (ImGui::MenuItem( "Cube" )) {
                        const EntityCreateInfo entityCreateInfo{
                            .mRoot = nullptr,
                            .mName{ "Cube" },
                            .mModel = mEditorState->GetPrefab( PrefabModelType::eCube ) };
                        mEditorState->mActiveScene->PushEntity( entityCreateInfo );
                    }

                    if (ImGui::MenuItem( "Sphere" )) {
                        const EntityCreateInfo entityCreateInfo{
                            .mRoot = nullptr,
                            .mName{ "Sphere" },
                            .mModel = mEditorState->GetPrefab( PrefabModelType::eSphere ) };
                        mEditorState->mActiveScene->PushEntity( entityCreateInfo );
                    }

                    if (ImGui::MenuItem( "Capsule" )) {
                        const EntityCreateInfo entityCreateInfo{
                            .mRoot = nullptr,
                            .mName{ "Capsule" },
                            .mModel = mEditorState->GetPrefab( PrefabModelType::eSphere ) };
                        mEditorState->mActiveScene->PushEntity( entityCreateInfo );
                    }

                    if (ImGui::MenuItem( "Plane" )) {
                        const EntityCreateInfo entityCreateInfo{
                            .mRoot = nullptr,
                            .mName{ "Plane" },
                            .mModel = mEditorState->GetPrefab( PrefabModelType::eCube ) };
                        mEditorState->mActiveScene->PushEntity( entityCreateInfo );
                    }

                    if (ImGui::MenuItem( "Cylinder" )) {
                        const EntityCreateInfo entityCreateInfo{
                            .mRoot = nullptr,
                            .mName{ "Cylinder" },
                            .mModel = mEditorState->GetPrefab( PrefabModelType::eCylinder ) };
                        mEditorState->mActiveScene->PushEntity( entityCreateInfo );
                    }

                    if (ImGui::MenuItem( "Cone" )) {
                        const EntityCreateInfo entityCreateInfo{
                            .mRoot = nullptr,
                            .mName{ "Cone" },
                            .mModel = mEditorState->GetPrefab( PrefabModelType::eCone ) };
                        mEditorState->mActiveScene->PushEntity( entityCreateInfo );
                    }

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu( "2D Object" )) {
                    if (ImGui::MenuItem( "Sprite" )) { /* Spawn Sprite */ }
                    if (ImGui::MenuItem( "Tilemap" )) { /* Spawn Tilemap */ }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu( "Effects" )) {
                    if (ImGui::MenuItem( "Particle System" )) { /* Spawn Particles */ }
                    if (ImGui::MenuItem( "Line" )) { /* Spawn Line Renderer */ }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu( "Light" )) {
                    if (ImGui::MenuItem( "Directional Light" )) { /* Spawn Directional Light */ }
                    if (ImGui::MenuItem( "Point Light" )) { /* Spawn Point Light */ }
                    if (ImGui::MenuItem( "Spot Light" )) { /* Spawn Spot Light */ }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu( "Audio" )) {
                    if (ImGui::MenuItem( "Audio Source" )) { /* Spawn Audio Source */ }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu( "UI" )) {
                    if (ImGui::MenuItem( "Canvas" )) { /* Spawn Canvas */ }
                    if (ImGui::MenuItem( "Text" )) { /* Spawn Text */ }
                    if (ImGui::MenuItem( "Button" )) { /* Spawn Button */ }
                    if (ImGui::MenuItem( "Image" )) { /* Spawn Image */ }
                    ImGui::EndMenu();
                }

                ImGui::Separator();

                if (ImGui::MenuItem( "Camera" )) { /* Spawn Camera */ }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu( MKT_LOC( "menu_window" ).c_str() )) {
                if (ImGui::BeginMenu( MKT_LOC( "menu_panels" ).c_str() )) {
                    for (auto& panel : mPanelRegistry | std::ranges::views::values) {
                        bool isActive{ panel->IsVisible() };

                        if (ImGui::MenuItem( panel->GetName().data(), nullptr, std::addressof( isActive ) )) {
                            panel->SetVisible( isActive );
                        }
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu( MKT_LOC( "menu_language" ).c_str() )) {
                    static constexpr std::array languages{
                        ISOLanguage::EN_US,
                        ISOLanguage::EN_GB,
                        ISOLanguage::ES_ES,
                        ISOLanguage::JA_JP,
                        ISOLanguage::ZH_CN };
                    const ISOLanguage current{ LocalizationService::Get()->GetCurrentLanguage() };

                    for ( const ISOLanguage lang: languages) {
                        const bool isSelected{ ( lang == current ) };

                        if (ImGui::MenuItem( GetISOName( lang ).data(), nullptr, isSelected )) {
                            LocalizationService::Get()->SetLanguage( lang );
                        }
                    }

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu( MKT_LOC( "menu_theme" ).c_str() )) {
                    if (ImGui::MenuItem( "Classic" )) {
                        ImGui::StyleColorsClassic();
                    }
                    if (ImGui::MenuItem( "Dark Default" )) {
                        ImGui::StyleColorsDark();
                        ImGuiService::Get()->SetThemeDarkModeDefault();
                    }
                    if (ImGui::MenuItem( "Dark Alternative" )) {
                        ImGui::StyleColorsDark();
                        ImGuiService::Get()->SetThemeDarkModeAlt();
                    }
                    if (ImGui::MenuItem( "Focused" )) {
                        ImGui::StyleColorsDark();
                    }
                    if (ImGui::MenuItem( "Blindness" )) {
                        ImGui::StyleColorsLight();
                    }

                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }

            if ( ImGui::BeginMenu( MKT_LOC( "menu_tools" ).c_str() ) ) {
                if (ImGui::BeginMenu( "Physics" )) {
                    if (ImGui::MenuItem( "Physics Debugger" )) { /* Open physics collider visualization window */ }
                    if (ImGui::MenuItem( "Simulation Settings" )) { /* Open gravity, timestep, and layer collision matrix */ }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu( "Audio" )) {
                    if (ImGui::MenuItem( "Audio Mixer" )) { /* Open master, SFX, and music channel controls */ }
                    if (ImGui::MenuItem( "Profiler" )) { /* Open real-time active audio voices monitor */ }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu( "Graphics" )) {
                    if (ImGui::MenuItem( "Render Pipeline Settings" )) { /* Open post-processing, MSAA, and shadow config */ }
                    if (ImGui::MenuItem( "Shader Compiler Status" )) { /* Open window tracking background shader variants compilation */ }

                    if (ImGui::MenuItem( "Display Grid", nullptr, mShowInfiniteGrid )) {
                        mShowInfiniteGrid = !mShowInfiniteGrid;
                    }

                    if (ImGui::MenuItem( "Display ImGui Debug Info", "Ctrl+I", mShowImGuiDebugInfo )) {
                        mShowImGuiDebugInfo = !mShowImGuiDebugInfo;
                    }

                    if (ImGui::BeginMenu( "Output" )) {
                        if (ImGui::MenuItem( "Color" )) {
                            mEditorState->mFinalComposition = mSceneRenderer->GetFinalImage( FinalImageType::eGBuffer_Color );
                        }
                        if (ImGui::MenuItem( "Emission" )) {
                            mEditorState->mFinalComposition = mSceneRenderer->GetFinalImage( FinalImageType::eGBuffer_Emissive );
                        }
                        if (ImGui::MenuItem( "Normals" )) {
                            mEditorState->mFinalComposition = mSceneRenderer->GetFinalImage( FinalImageType::eGBuffer_Normals );
                        }
                        if (ImGui::MenuItem( "Positions" )) {
                            mEditorState->mFinalComposition = mSceneRenderer->GetFinalImage( FinalImageType::eGBuffer_Position );
                        }
                        if (ImGui::MenuItem( "Wireframe" )) {
                            mEditorState->mFinalComposition = mSceneRenderer->GetFinalImage( FinalImageType::eWireframe );
                        }
                        if (ImGui::MenuItem( "Depth" )) {
                            mEditorState->mFinalComposition = mSceneRenderer->GetFinalImage( FinalImageType::eDepthPrepass );
                        }
                        if (ImGui::MenuItem( "Radiance" )) {
                            mEditorState->mFinalComposition = mSceneRenderer->GetFinalImage( FinalImageType::ePBRadiance_Output );
                        }
                        if (ImGui::MenuItem( "Default" )) {
                            mEditorState->mFinalComposition = mSceneRenderer->GetFinalImage( FinalImageType::eTonemap_Output );
                        }
                        if (ImGui::MenuItem( "Chroma" )) {
                            mEditorState->mFinalComposition = mSceneRenderer->GetFinalImage( FinalImageType::eChromaticAberration );
                        }
                        ImGui::EndMenu();
                    }
                    ImGui::EndMenu();
                }

                ImGui::Separator();

                if (ImGui::MenuItem( "Profiler", "Ctrl+7" )) { /* Toggle core CPU/GPU performance profiler timeline */ }
                if (ImGui::MenuItem( "Console", "Ctrl+Shift+C" )) { /* Toggle engine log, warning, and error outputs window */ }

                if (ImGui::MenuItem( "Display RenderGraph", "Ctrl+Shift+G", false, !mShowRenderGraph )) {
                    mShowRenderGraph = !mShowRenderGraph;
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu( MKT_LOC( "menu_help" ).c_str() )) {
                if (ImGui::MenuItem( "About Mikoto" )) { /* Show floating window with credits/version */ }
                ImGui::Separator();

                if (ImGui::MenuItem( "Documentation" )) { /* Open URL for the documentation */ }
                if (ImGui::MenuItem( "Scripting Reference" )) { /* Open URL for the code API */ }

                ImGui::Separator();
                if (ImGui::BeginMenu( "Support" )) {
                    if (ImGui::MenuItem( "Report a Bug" )) { /* Open bug report form or GitHub Issues */ }
                    ImGui::EndMenu();
                }

                ImGui::Separator();
                if (ImGui::MenuItem( "Check for Updates" )) { /* Logic to check for a new engine version */ }

                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
            ImGui::PopStyleVar();
        }

        ImGui::End();
    }

    auto EditorLayer::UpdateDockSpacePanels( float ts ) -> void {
        MKT_BEGIN_PROFILER_NAMED();
        for ( const auto &panel: mPanelRegistry | std::ranges::views::values ) {
            panel->OnUpdate( ts );
        }
    }

    auto EditorLayer::RenderScene( float ) -> void {
        mSceneRenderer->Render( mEditorState->mActiveScene );
    }

    auto EditorLayer::RenderFrameGraphEditor() -> void {
        if (mShowRenderGraph) {
            mGraphEditor.Render( mShowRenderGraph );
        }
    }

    auto EditorLayer::UpdateViewportState( float ) -> void {
        ScenePanel* panel{ mPanelRegistry.Get<ScenePanel>() };
        panel->SetTexture( mEditorState->mFinalComposition );
    }

    auto EditorLayer::UpdateSceneState( float ts  ) -> void {
        mEditorState->mActiveScene->SetState( SceneState::eIdle );
        mEditorState->mActiveScene->Update( ts );
    }

    auto EditorLayer::UpdateCameraState( float ts  ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        SettingsPanel *settingsPanel{ mPanelRegistry.Get<SettingsPanel>() };
        const auto &configuration{ settingsPanel->GetData() };

        mEditorCamera->SetMovementSpeed( configuration.mEditorCameraMovementSpeed );
        mEditorCamera->SetRotationSpeed( configuration.mEditorCameraRotationSpeed );

        mEditorCamera->SetFarPlane( configuration.mFarPlane );
        mEditorCamera->SetNearPlane( configuration.mNearPlane );

        mEditorCamera->WantRotation( configuration.mWantXAxisRotation, configuration.mWantYAxisRotation );

        mEditorCamera->SetFieldOfView( configuration.mFieldOfView );

        // Set viewport to the currently active window we can either expand
        // the final composition to occupy the whole screen or just an ImGui viewport
        if (mScreenPresentTarget == ScreenPresentTarget::ePanels) {
            ScenePanel *scenePanel{ mPanelRegistry.Get<ScenePanel>() };
            mEditorCamera->SetViewportSize( scenePanel->GetWidth(), scenePanel->GetHeight() );
        } else {
            mEditorCamera->SetViewportSize( mWindow->GetWidth(), mWindow->GetHeight() );
        }

        // Camera target
        mEditorCamera->LockCameraToTarget( configuration.mLockCameraToTarget );
        if (configuration.mLockCameraToTarget && mEditorState->mSelectedEntity) {
            auto& transformComp{ mEditorState->mSelectedEntity->GetComponent<TransformComponent>() };
            mEditorCamera->SetCameraTarget( transformComp.GetTranslation() );
        }

        mEditorCamera->Update( ts );
    }

    auto EditorLayer::UpdateRendererState( float ts ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        mSceneRenderer->SetMainCamera( mEditorCamera.get() );
        mSceneRenderer->SetEnableInfiniteGrid( mShowInfiniteGrid );
    }
}// namespace mikoto