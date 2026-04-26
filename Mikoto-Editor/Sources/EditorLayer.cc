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
#include <Core/Types.hh>
#include <Core/Event.hh>
#include <Core/Profiler.hh>
#include <Core/CoreEvents.hh>
#include <Core/Exception.hh>
#include <Core/InputSystem.hh>
#include <Core/LocalizationService.hh>

#include <Memory/Allocator.hh>

#include <Assets/ImageProcessor.hh>

#include <ImGui/ImGuiService.hh>
#include <ImGui/ImGuiUtility.hh>

#include <Layers/EditorLayer.hh>

#include <Filesystem/FileWatcherService.hh>

#include <Renderer/Core/RenderSystem.hh>

#include <Scene/SceneManager.hh>

#include <Panels/StatsPanel.hh>
#include <Panels/ScenePanel.hh>
#include <Panels/InspectorPanel.hh>
#include <Panels/HierarchyPanel.hh>
#include <Panels/SettingsPanel.hh>
#include <Panels/RuntimeConsolePanel.hh>
#include <Panels/ContentBrowserPanel.hh>

namespace mikoto::editor {

    using namespace mikoto::gui;
    using namespace mikoto::core;
    using namespace mikoto::platform;

    auto EditorState::GetPrefab( PrefabModelType model ) -> ModelHandle {
        return mPrefabPaths[model];
    }

    EditorLayer::EditorLayer( Window *window )
        : ILayer{ "EditorLayer" }, mWindow{ window }
    {}

    auto EditorLayer::OnCreate() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        InitEditorState();

        InitEmptyScene();

        InitDockingSpace();

        InitAssets();
        InitSceneRenderer();

        InitEditorCamera();

        InitEditorPanels();
    }

    auto EditorLayer::OnDestroy() -> void {
        MKT_BEGIN_PROFILER_NAMED();
    }

    auto EditorLayer::OnUpdate( float timeStep ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        UpdateCameraState( timeStep );
        UpdateRendererState( timeStep );
        UpdateSceneState( timeStep );

        RenderScene( timeStep );

        ShowDockSpace();
        ShowDockSpacePanels( timeStep );
    }

    auto EditorLayer::OnEvent( Event &event ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (event.IsType( EventType::MOUSE_BUTTON_PRESSED_EVENT )) {
            auto* scenePanel{ mPanelRegistry.Get<ScenePanel>() };
            auto* mouseButtongEvent{ as<MouseButtonPressedEvent*>(MKT_ADDRESSOF( event )) };

            if (mouseButtongEvent->GetMouseButton() == Mouse_Button_Right && scenePanel && scenePanel->IsHovered() ) {
                if (!mWindow->IsCursorMode( CursorMode::eDisabled )) {
                    mWindow->SetCursorMode( CursorMode::eDisabled );
                }
            }
        }

        if (event.IsType( EventType::MOUSE_BUTTON_RELEASED_EVENT )) {
            auto* mouseButtongEvent{ as<MouseButtonReleasedEvent*>(MKT_ADDRESSOF( event )) };

            if (mouseButtongEvent->GetMouseButton() == Mouse_Button_Right) {
                mWindow->SetCursorMode( CursorMode::eNormal );
            }
        }
    }

    auto EditorLayer::InitAssets() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        ankerl::unordered_dense::map<PrefabModelType, eastl::string_view> modelPaths{
            { PrefabModelType::eCube, "Resources/Models/Prefabs/cube/gltf/scene.gltf" },
            { PrefabModelType::eCone, "Resources/Models/Prefabs/cone/gltf/scene.gltf" },
            { PrefabModelType::eSphere, "Resources/Models/Prefabs/sphere/gltf/scene.gltf" },
            { PrefabModelType::eCylinder, "Resources/Models/Prefabs/cylinder/gltf/scene.gltf" },
        };

        // So each thread writes to its own slot, need to double-check
        for ( const auto &[type, path]: modelPaths ) {
            mEditorState->mPrefabPaths[type] = ModelHandle::CreateEmpty();
        }

        threading::TaskService::Get()->ParallelFor(modelPaths,
            [&](const PrefabModelType& type, const Path& path) -> void {
            mEditorState->mPrefabPaths[type] = AssetsService::Get()->LoadAsset<Model>( path );
        });
    }

    auto EditorLayer::InitEditorState() -> void {
        mEditorState = eastl::make_unique<EditorState>();
    }

    auto EditorLayer::InitSceneRenderer() -> void {
        SceneRendererCreateInfo spec{};
        spec.WithName( "Scene renderer" )
            .WithDevice( RenderSystem::Get()->GetGpuDevice() );

        mSceneRenderer = SceneRenderer::Create( spec );

        if (mSceneRenderer) {
            mSceneRenderer->Init();
        }

        mEditorState->mSceneRenderer = mSceneRenderer.get();
    }

    auto EditorLayer::InitEditorCamera() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        SceneCameraDescription cameraDescription{
            .mFov = 45.0,
            .mAspectRatio = as<float>( mWindow->GetWidth() ) / as<float>( mWindow->GetHeight() ),
            .mNearPlane = 0.1f,
            .mFarPlane = 3000.0f,
            .mWindow = mWindow
        };

        mEditorCamera = eastl::make_unique<SceneCamera>( cameraDescription );
        mEditorState->mActiveCamera = mEditorCamera.get();
    }

    auto EditorLayer::InitEditorPanels() -> void {
        // Stats panel
        StatsPanelCreateInfo statsCreateInfo{
            .mState = mEditorState.get(),
        };
        mPanelRegistry.Register<StatsPanel>( statsCreateInfo );

        // Console runtime panel
        RuntimeConsolePanelCreateInfo consoleCreateInfo{
            .mState = mEditorState.get(),
        };
        mPanelRegistry.Register<RuntimeConsolePanel>( consoleCreateInfo );

        // Scene viewport panel
        ScenePanelCreateInfo scenePanelCreateInfo{
            .mState = mEditorState.get(),
            .mImage = TextureHandle::CreateEmpty(),
        };
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
    }

    auto EditorLayer::InitDockingSpace() -> void {
        //https://github.com/ocornut/imgui/wiki/Docking
    }

    auto EditorLayer::InitEmptyScene() -> void {
        mEditorState->mActiveScene = SceneManager::Get()->CreateScene( "Scene" );
    }

    auto EditorLayer::ShowDockSpace() -> void {
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
                if (ImGui::MenuItem( MKT_LOC( "open_project" ).c_str(), "Ctrl + P" )) {}
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

            if (ImGui::BeginMenu( MKT_LOC( "menu_language" ).c_str() )) {
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

            if ( ImGui::BeginMenu( MKT_LOC( "menu_tools" ).c_str() ) ) {
                // Disabling fullscreen would allow the window to be moved to the front of other windows,
                // which we can't undo at the moment without finer window depth/z control.

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu( MKT_LOC( "menu_about" ).c_str() )) {

                ImGui::EndMenu();
            }

#if !defined(NDEBUG)
            ImGui::TextUnformatted( string::Format(" | Build type [DEBUG]. Framerate: {:.1f}", ImGui::GetIO().Framerate ).c_str() );
#else
            ImGui::TextUnformatted( string::Format( " | Build type [RELEASE]. Framerate: {:.1f}", ImGui::GetIO().Framerate ).c_str() );
#endif

            ImGui::EndMenuBar();
            ImGui::PopStyleVar();
        }

        ImGui::End();
    }

    auto EditorLayer::ShowDockSpacePanels( float ts ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        for (const auto &panel: mPanelRegistry | std::ranges::views::values) {
            panel->OnUpdate( ts );
        }
    }

    auto EditorLayer::RenderScene( float ) -> void {
        mSceneRenderer->Render( mEditorState->mActiveScene );
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
        ScenePanel *scenePanel{ mPanelRegistry.Get<ScenePanel>() };
        if (mScreenPresentTarget == ScreenPresentTarget::ePanels) {
            mEditorCamera->SetViewportSize( scenePanel->GetWidth(), scenePanel->GetHeight() );
        } else {
            mEditorCamera->SetViewportSize( mWindow->GetWidth(), mWindow->GetHeight() );
        }

        if ((InputSystem::Get()->IsMouseKeyPressed( Mouse_Button_Right ) && scenePanel->IsHovered()) ||
            mScreenPresentTarget == ScreenPresentTarget::eFinalImage) {
            mEditorCamera->EnableCamera( true );
        } else {
            mEditorCamera->EnableCamera( false );
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
    }
}// namespace mikoto