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

#include <utility>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <ranges>
#include <exception>
#include <filesystem>

#include <imgui.h>
#include <imgui_internal.h>

#include <EASTL/utility.h>
#include <EASTL/array.h>
#include <EASTL/hash_map.h>
#include <EASTL/algorithm.h>

#include <glm/gtx/compatibility.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <Core/CVar.hh>
#include <Core/Core.hh>
#include <Core/Engine.hh>
#include <Core/DateTime.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>
#include <Core/LocalizationService.hh>

#include <Logging/Logger.hh>

#include <Math/Math.hh>

#include <Filesystem/File.hh>
#include <Filesystem/FileSystem.hh>
#include <Filesystem/FileService.hh>

#include <Platform/CoreEvents.hh>
#include <Platform/InputSystem.hh>
#include <Platform/ActionManager.hh>

#include <Assets/Utility.hh>
#include <Assets/ImageProcessing.hh>
#include <Assets/AssetRegistryService.hh>

#include <Material/SkyboxMaterial.hh>
#include <Material/SurfaceMaterial.hh>

#include <Animation/AnimationSystem.hh>

#include <Physics/PhysicSystem.hh>

#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Rhi/Utility.hh>
#include <Renderer/Core/RenderSystem.hh>

#include <Scene/Scene.hh>
#include <Scene/Component.hh>
#include <Scene/SceneManager.hh>

#include <Scripting/ScriptingService.hh>
#include <Scripting/Dotnet/DotnetScriptingService.hh>

#include <ImGui/Utility.hh>
#include <ImGui/ImGuiWidget.hh>
#include <ImGui/ImGuiService.hh>
#include <ImGui/IconsMaterialDesign.h>

#include <Theme/PopupStyle.hh>

#include <Panels/AboutPanel.hh>
#include <Panels/AudioPanel.hh>
#include <Panels/StatsPanel.hh>
#include <Panels/LayoutsPanel.hh>
#include <Panels/ProjectPanel.hh>
#include <Panels/AnimatorPanel.hh>
#include <Panels/AssetsBrowser.hh>
#include <Panels/GPUStatsPanel.hh>
#include <Panels/LightingPanel.hh>
#include <Panels/RendererPanel.hh>
#include <Panels/SceneControls.hh>
#include <Panels/SettingsPanel.hh>
#include <Panels/ViewportPanel.hh>
#include <Panels/HierarchyPanel.hh>
#include <Panels/InspectorPanel.hh>
#include <Panels/LandscapePanel.hh>
#include <Panels/AudioMixerPanel.hh>
#include <Panels/GamePreviewPanel.hh>
#include <Panels/PreferencesPanel.hh>
#include <Panels/ScriptGraphPanel.hh>
#if defined( MIKOTO_EDITOR_TRACY_VIEWER )
#include <Panels/InstrumentationPanel.hh>
#endif
#include <Panels/AudioProfilerPanel.hh>
#include <Panels/AnimationGraphPanel.hh>
#include <Panels/CompilerStatusPanel.hh>
#include <Panels/ContentBrowserPanel.hh>
#include <Panels/MaterialEditorPanel.hh>
#include <Panels/PassVisualizerPanel.hh>
#include <Panels/RuntimeConsolePanel.hh>
#include <Panels/ParticleSimulationPanel.hh>
#include <Panels/SimulationSettingsPanel.hh>

#include <Layers/EditorLayer.hh>

namespace mikoto::editor {

    using namespace mikoto::core;
    using namespace mikoto::scene;
    using namespace mikoto::platform;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;

    MKT_NODISCARD static auto EditorBeginMenu( const char* label, bool enabled = true ) -> bool {
        // Share a minimum width across dropdowns, including nested menus, while
        // letting ImGui fit longer labels, shortcuts and menu markers automatically.
        const float minimumWidth{ ImGui::GetFontSize() * 13.0f };
        ImGui::SetNextWindowSizeConstraints( ImVec2{ minimumWidth, 0.0f },
            ImVec2{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max() } );
        return ImGui::BeginMenu( label, enabled );
    }

    MKT_NODISCARD static auto EditorMenuItem( const char *label, const char *shortcut = nullptr, bool selected = false, bool enabled = true ) -> bool {
        return ImGui::MenuItem( label, shortcut, selected, enabled );
    }

    MKT_NODISCARD static auto EditorMenuItem( const char *label, const char *shortcut, bool *selected, bool enabled = true ) -> bool {
        return ImGui::MenuItem( label, shortcut, selected, enabled );
    }

    MKT_NODISCARD static auto UtilityBarHeight() -> f32 {
        return ImGui::GetFontSize() + 22.0f;
    }

    static auto DrawStatusDivider() -> void {
        const float scale{ ImGui::GetFontSize() / 17.0f };
        const float height{ ImGui::GetTextLineHeight() };
        const auto position{ ImGui::GetCursorScreenPos() };
        ImGui::GetWindowDrawList()->AddLine( { position.x + 3.0f * scale, position.y + 2.0f * scale },
            { position.x + 3.0f * scale, position.y + height - 2.0f * scale }, IM_COL32( 80, 83, 88, 255 ) );
        ImGui::Dummy( { 6.0f * scale, height } );
        ImGui::SameLine();
    }

    static auto DrawStatusField( eastl::string_view text, float width ) -> void {
        // Reserve the field before drawing so changing names cannot move neighbouring items.
        const auto position{ ImGui::GetCursorScreenPos() };
        const float baseline{ ImGui::GetCurrentWindow()->DC.CurrLineTextBaseOffset };
        const float height{ ImGui::GetTextLineHeight() + baseline };
        const float textWidth{ ImGui::CalcTextSize( text.data(), text.data() + text.size(), false ).x };
        const ImVec4 clip{ position.x, position.y, position.x + width, position.y + height };
        ImGui::Dummy( { width, height } );
        ImGui::GetWindowDrawList()->AddText( ImGui::GetFont(), ImGui::GetFontSize(),
            { position.x, position.y + baseline }, ImGui::GetColorU32( ImGuiCol_Text ),
            text.data(), text.data() + text.size(), 0.0f, &clip );

        if ( textWidth > width ) {
            imgui::widget::TooltipText( text );
        }
    }

    static auto DrawHeaderGlow( ImVec2 origin, float width, float height, core::float4 color ) -> void {
        auto* draw{ ImGui::GetWindowDrawList() };
        const float gradientWidth{ glm::min( width, 360.0f ) };
        const ImU32 left{ ImGui::GetColorU32( ImVec4{ color.r, color.g, color.b, color.a } ) };
        const ImU32 right{ ImGui::GetColorU32( ImVec4{ color.r, color.g, color.b, 0.0f } ) };
        draw->AddRectFilledMultiColor( origin, { origin.x + gradientWidth, origin.y + height }, left, right, right, left );
    }


    auto EditorState::GetPrefab( PrefabModelType model ) -> asset::ModelHandle {
        MKT_BEGIN_PROFILER();
        const auto prefab{ mPrefabs.find( model ) };
        return prefab != mPrefabs.end() ? prefab->second.mRequest.GetIfReady() : asset::ModelHandle{};
    }

    auto EditorState::GetPrefabReference( PrefabModelType model ) const -> asset::AssetRef<asset::Model> {
        return mPrefabs.at( model ).mReference;
    }

    auto EditorState::UpdatePrefabs() -> void {
        for ( auto& [type, prefab]: mPrefabs ) {
            // Import-policy changes invalidate requests; acquire the replacement entry.
            if ( prefab.mRequest.GetState() != asset::AssetState::eUnloaded ) {
                continue;
            }

            // Basic types get priorityzed
            const bool primitive{ 
                type == PrefabModelType::eCube || 
                type == PrefabModelType::eSphere ||
                type == PrefabModelType::eCylinder || 
                type == PrefabModelType::eCone 
            };

            const auto priority{ primitive ? asset::LoadPriority::eCritical : asset::LoadPriority::eBackground };
            prefab.mRequest = asset::AssetRegistryService::Get()->Request<asset::Model>( 
                prefab.mReference, asset::ModelLoadDescription{ 
                    .mPriority = priority 
                } );
        }
    }

    auto EditorState::SelectEntity( scene::Entity* entity ) -> void {
        MKT_BEGIN_PROFILER();
        mSelectedEntityIDs.clear();
        mSelectedEntity = entity;
        mSelectedEntityID = 0;

        BumpRevision();
        mTransformEditRejected = false;

        if ( mEditorRenderer ) {
            mEditorRenderer->CancelPick();
        }

        ValidateSelection();

        if ( mSelectedEntity ) {
            mSelectedEntityID = mSelectedEntity->GetComponent<scene::TagComponent>().GetGuid();
        }

        if ( mSelectedEntityID ) {
            mSelectedEntityIDs.push_back( mSelectedEntityID );
        }
        RefreshHighlights();
    }

    auto EditorState::SelectEntities( const eastl::vector<core::u64>& ids, bool append ) -> void {
        auto selection{ append ? mSelectedEntityIDs : eastl::vector<core::u64>{} };
        for ( const auto id: ids ) {
            if ( mActiveScene && mActiveScene->FindByID( id ) && eastl::find( selection.begin(), selection.end(), id ) == selection.end() ) {
                selection.push_back( id );
            }
        }

        SelectEntity( selection.empty() ? nullptr : mActiveScene->FindByID( selection.front() ) );
        
        mSelectedEntityIDs = eastl::move( selection );

        RefreshHighlights();
    }

    auto EditorState::HasWorkspace() const -> bool {
        MKT_BEGIN_PROFILER();
        return mHasProject || mSandboxMode;
    }

    auto EditorState::ValidateSelection() -> void {
        MKT_BEGIN_PROFILER();

        if ( !mSelectedEntity ) {
            return;
        }

        // After initial ownership validation the GUID is known. Avoid scanning
        // the whole scene every update; compare pointers without dereferencing
        // the selected entity, which may already have been destroyed.
        if ( mActiveScene && mSelectedEntityID && mActiveScene->FindByID( mSelectedEntityID ) == mSelectedEntity ) {
            return;
        }

        bool exists = false;

        if ( mActiveScene ) {
            for ( const auto& [id, entity]: mActiveScene->GetEntities() ) {
                if ( entity.get() == mSelectedEntity && ( mSelectedEntityID == 0 || id == mSelectedEntityID ) ) {
                    exists = true;
                    break;
                }
            }
        }

        // Compare ownership before dereferencing a pointer whose entity may have been destroyed.
        if ( !exists ) {
            mSelectedEntity = nullptr;
            mSelectedEntityID = 0;
            BumpRevision();

            if ( mEditorRenderer ) {
                mEditorRenderer->CancelPick();
            }
        }
    }

    auto EditorState::RefreshHighlights() -> void {
        MKT_BEGIN_PROFILER();

        if ( !mActiveScene ) {
            return;
        }

        eastl::erase_if( mSelectedEntityIDs, [this]( core::u64 id ) {
            return !mActiveScene->FindByID( id );
        } );

        if ( !mSelectedEntity && !mSelectedEntityIDs.empty() ) {
            mSelectedEntityID = mSelectedEntityIDs.front();
            mSelectedEntity = mActiveScene->FindByID( mSelectedEntityID );
        }

        mActiveScene->SetHighlightRoots( mSelectedEntityIDs );
    }

    EditorLayer::EditorLayer( platform::IWindow* window )
        : mName{ "EditorLayer" }, 
        mWindow{ window }, 
        mDevice{ renderer::RenderSystem::Get()->GetGpuDevice() } {
    }

    auto EditorLayer::OnCreate() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        InitEditorState();

        InitAssets();
        InitTitleBar();

        InitSceneRenderers();

        InitActionCallbacks();

        InitEditorCamera();
        InitEmptyScene();
        InitEditorPanels();
    }

    auto EditorLayer::OnDestroy() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Ensure GPU is done
        if ( mDevice ) {
            mDevice->WaitIdle();
        }

        mWorkspaceOperation.Reset();
        StopPlay();

        mPanelRegistry.Clear();
        mTitleBarIcon.Reset();

        if ( mWindow->HasCustomTitleBar() ) {
            // The native window owns the hit-test state and releases it on destruction.
            // Restoring decorations here briefly exposes a native caption during shutdown.
            mWindow->SetTitleBarDragArea( 0.0f, 0.0f, 0.0f, 0.0f );
        }

        mEditorState->mGameCamera = nullptr;
        mEditorState->mGameRenderer = nullptr;
        mEditorState->mEditorRenderer = nullptr;

        if ( mGameRenderer ) {
            mGameRenderer->Shutdown();
        }

        mGameRenderer.reset();

        if ( mEditorRenderer ) {
            mEditorRenderer->Shutdown();
        }

        mEditorRenderer.reset();
        mEditorCamera.reset();
        mEditorState.reset();

        if ( mThumbnailRenderer ) {
            mThumbnailRenderer->Shutdown();
        }

        mThumbnailRenderer.reset();

        mDevice = nullptr;
        mWindow = nullptr;

        if ( mActionManager ) {
            mActionManager->Shutdown();
        }

        mActionManager.reset();
    }

    auto EditorLayer::OnUpdate( float timeStep ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Might not need to update every frame
        mEditorState->UpdatePrefabs();

        // A panel requests a render only when it submits a visible image this frame.
        mEditorState->mScenePreviewRequested = false;
        mEditorState->mGamePreviewRequested = false;
        scripting::ScriptingService::Get()->SetInputFocus( false, false );

        mWorkspaceOperation.Update();

        if ( IsWorkspaceBusy() || !mEditorState->HasWorkspace() || ImGui::GetIO().AppFocusLost ) {
            SetCameraCapture( false );

            if ( auto* panel{ mPanelRegistry.Get<ViewportPanel>() } ) {
                panel->SuspendInput();
            }
        }

        if ( mWorkspaceOperation.IsVisible() ) {
            DrawLoadingWorkspace( timeStep );
            return;
        }

        UpdateEditorServices();
        DrawRenderingUnavailableNotice();

        if ( !mEditorState->HasWorkspace() ) {
            DrawEmptyWorkspace();
            return;
        }

        UpdatePlayRequests();
        UpdateCameraState( timeStep );

        if ( !mEditorState->mRuntimeActive ) {
            UpdateSceneState( timeStep );
            UpdateRendererState( timeStep );
        }

        UpdateGameCamera();
        mEditorRenderer->SetCullingDebugCamera( mEditorState->mGameCamera );

        if ( mEditorState->mRuntimeActive && mScreenPresentTarget != ScreenPresentTarget::ePanels ) {
            scripting::ScriptingService::Get()->SetInputFocus( !ImGui::GetIO().WantTextInput, true );
        }

        UpdateWorkspaceViews( timeStep );

        if ( mScreenPresentTarget == ScreenPresentTarget::ePanels ) {
            const auto* panel{ mPanelRegistry.Get<ViewportPanel>() };
            SetCameraCapture( panel && panel->IsCameraNavigating() && !IsWorkspaceBusy() );
        }

        if ( mEditorState->mRuntimeActive ) {
            // Let the Game panel route this frame's input before scripts run.
            // Publish the scripted camera pose before either viewport renders.
            UpdateSceneState( timeStep );
            UpdateRendererState( timeStep );
            UpdateGameCamera();
        }

        RenderWorkspace( timeStep );
        DrawEditorOverlays();
    }

    auto EditorLayer::DrawLoadingWorkspace( float timeStep ) -> void {
        MKT_BEGIN_PROFILER();

        renderer::RenderSystem::Get()->SetPresentTarget( imgui::ImGuiService::Get()->GetFinalComposition() );
        ImGui::PushStyleVar( ImGuiStyleVar_DisabledAlpha, 1.0f );
        ImGui::BeginDisabled();

        UpdateDockSpace();

        if ( mEditorState->HasWorkspace() ) {
            UpdateDockSpacePanels( timeStep );
        }

        ImGui::EndDisabled();
        ImGui::PopStyleVar();

        DrawWorkspaceProgress();
    }

    auto EditorLayer::UpdateEditorServices() -> void {
        MKT_BEGIN_PROFILER();
        mPanelRegistry.Get<PreferencesPanel>()->ApplyPending();

        for ( const auto& [name, visible]: mPendingPanelVisibility ) {
            for ( const auto& panel: mPanelRegistry | std::ranges::views::values ) {
                if ( panel->GetName() == name ) {
                    panel->SetVisible( visible );
                }
            }
        }

        mPendingPanelVisibility.clear();

        // Preferences edits the project value. Apply cache-changing import
        // policy here, before panels record previews or scene rendering.
        if ( mProject && asset::AssetRegistryService::Get()->IsTextureCompressionEnabled() != mProject->GetCompressTextures() ) {
            ApplyTextureCompression( mProject->GetCompressTextures() );
        }

        mPanelRegistry.Get<CompilerStatusPanel>()->UpdateBuilds( mProject ? mProject->GetAssetsDirectory() : filesystem::Path{ "Assets" }.ToAbsolute(),
            mEditorState->mScriptProject );
    }

    auto EditorLayer::DrawEmptyWorkspace() -> void {
        MKT_BEGIN_PROFILER();

        mEditorState->mGameCamera = nullptr;
        mEditorRenderer->SetCullingDebugCamera( nullptr );
        
        // The launcher is the entire workspace until a project is opened.
        // Keep docking settings alive, but submit no panels or scene work.
        renderer::RenderSystem::Get()->SetPresentTarget( imgui::ImGuiService::Get()->GetFinalComposition() );

        UpdateDockSpace();
        DrawProjectLauncher();
        DrawWorkspaceProgress();
    }

    auto EditorLayer::UpdatePlayRequests() -> void {
        MKT_BEGIN_PROFILER();

        if ( mStopRequested ) {
            mStopRequested = false;
            StopPlay();
        }

        if ( mPlayRequested ) {
            mPlayRequested = false;
            StartPlay();
        }
    }

    auto EditorLayer::UpdateWorkspaceViews( float timeStep ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mScreenPresentTarget == ScreenPresentTarget::ePanels || !mEditorRenderer->IsRenderAvailable() ) {
            renderer::RenderSystem::Get()->SetPresentTarget( imgui::ImGuiService::Get()->GetFinalComposition() );

            UpdateDockSpace();
            UpdateDockSpacePanels( timeStep );

            HandleUploadAsset();

            UpdateViewportState( timeStep );
        }
        else {
            if ( auto* panel{ mPanelRegistry.Get<ViewportPanel>() } ) {
                panel->UpdateFullscreenPicking();
            }
        }
    }

    auto EditorLayer::RenderWorkspace( float timeStep ) -> void {
        MKT_BEGIN_PROFILER();

        // The Scene panel measures its image area while building the UI and
        // updates the active camera's aspect ratio. Record scene rendering
        // afterwards so opening a workspace or resizing uses this frame's size.
        UpdateGameViewport();
        RenderScene( timeStep );

        // Publish the current final image after rendering; resizing may replace its graph texture.
        if ( mScreenPresentTarget != ScreenPresentTarget::ePanels && mEditorRenderer->IsRenderAvailable() ) {
            renderer::RenderSystem::Get()->SetPresentTarget( mEditorState->mFinalComposition );
        }

        RenderGamePreview();
    }

    auto EditorLayer::DrawEditorOverlays() -> void {
        MKT_BEGIN_PROFILER();

        if ( mShowImGuiDebugInfo ) {
            ImGui::ShowMetricsWindow();
        }

        // Fullscreen presentation skips the dock space, which normally draws confirmations.
        if ( mScreenPresentTarget != ScreenPresentTarget::ePanels && mEditorRenderer->IsRenderAvailable() ) {
            DrawProjectLauncher();
        }

        DrawWorkspaceProgress();
    }

    auto EditorLayer::OnRender( float ) -> void {
        // TODO: Refactor
    }

    auto EditorLayer::GetName() const -> eastl::string_view {
        return mName;
    }

    auto EditorLayer::OnEvent( platform::Event& event ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        const auto type{ event.GetType() };

        // A workspace operation can veto a close even while it owns input.
        if ( type == EventType::WINDOW_CLOSE_EVENT ) {
            HandleWindowClose( event );
            return;
        }

        if ( IsWorkspaceBusy() ) {
            return;
        }

        switch ( type ) {
            case EventType::KEY_PRESSED_EVENT:
                if ( const auto* key{ dynamic_cast<const KeyPressedEvent*>( MKT_ADDRESSOF( event ) ) } ) {
                    HandleKeyPressed( *key );
                }
                break;

            case EventType::CONTENT_DROPPED_EVENT:
                HandleContentDropped( *core::checked_cast<const ContentDroppedEvent*>( MKT_ADDRESSOF( event ) ) );
                break;

            case EventType::MOUSE_BUTTON_PRESSED_EVENT:
                HandleCameraButton( core::checked_cast<const MouseButtonPressedEvent*>( MKT_ADDRESSOF( event ) )->GetMouseButton(), true );
                break;

            case EventType::MOUSE_BUTTON_RELEASED_EVENT:
                HandleCameraButton( core::checked_cast<const MouseButtonReleasedEvent*>( MKT_ADDRESSOF( event ) )->GetMouseButton(), false );
                break;

            default:
                break;
        }
    }

    auto EditorLayer::IsWorkspaceBusy() const -> bool {
        MKT_BEGIN_PROFILER();
        return mWorkspaceOperation.IsVisible() || mPendingWorkspaceAction != WorkspaceAction::eNone ||
               mDeferredWorkspaceAction != WorkspaceAction::eNone;
    }

    auto EditorLayer::HandleWindowClose( platform::Event& event ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mCloseApproved ) {
            return;
        }
        // Keep the app alive while a save or confirmation owns the workspace.
        if ( IsWorkspaceBusy() ) {
            event.SetHandled( true );
            return;
        }

        if ( mProject && !mEditorState->mSandboxMode ) {
            try {
                mProject->SetSceneRendererSettings( mEditorRenderer->GetSettings() );

                if ( mProject->HasUnsavedChanges() ) {
                    mPendingWorkspaceAction = WorkspaceAction::eCloseEditor;
                    event.SetHandled( true );
                }
            } catch ( const std::exception& error ) {
                mWorkflowError = error.what();
                event.SetHandled( true );
            }
        }
    }

    auto EditorLayer::HandleKeyPressed( const KeyPressedEvent& event ) -> void {
        MKT_BEGIN_PROFILER();

        if ( HandleDiagnosticShortcut( event ) ) {
            return;
        }

        if ( !mEditorState->HasWorkspace() || mEditorState->mRuntimeActive ) {
            return;
        }

        const auto key{ event.GetKeyCode() };
        const bool navigationShortcut{ key == KeyCode::Key_F11 || key == KeyCode::Key_F4 || key == KeyCode::Key_F5 };

        // Function keys remain available while a text widget owns editing shortcuts.
        if ( navigationShortcut || !ImGui::GetIO().WantTextInput ) {
            const auto modifiers{ navigationShortcut ? ModKey::eNone : event.GetModifiers() };
            ( void )mActionManager->Dispatch( key, modifiers );
        }
    }

    auto EditorLayer::HandleDiagnosticShortcut( const KeyPressedEvent& event ) -> bool {
        MKT_BEGIN_PROFILER();

        if ( !mActionManager || !ImGui::GetCurrentContext() || ImGui::GetIO().WantTextInput ) {
            return false;
        }

        const auto key{ event.GetKeyCode() };
        const auto modifiers{ event.GetModifiers() & ( ModKey::eControl | ModKey::eShift | ModKey::eAlt | ModKey::eSuper ) };
        const bool openConsole{ key == KeyCode::Key_C && modifiers == ( ModKey::eControl | ModKey::eShift ) };
        const bool openProfiler{ key == KeyCode::Key_7 && modifiers == ModKey::eControl };

        if ( openConsole || openProfiler ) {
            ( void )mActionManager->Dispatch( key, modifiers );
            return true;
        }

        return false;
    }

    auto EditorLayer::HandleContentDropped( const ContentDroppedEvent& event ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mEditorState->mRuntimeActive ) {
            return;
        }

        // Projects and scenes can open from the launcher and take priority over asset imports.
        if ( OpenDroppedWorkspace( event ) || !mEditorState->HasWorkspace() ) {
            return;
        }

        auto& upload{ mEditorState->mUploadContext };
        upload.mUploadOnGoing = true;

        for ( const auto& content: event.GetContents() ) {
            MKT_CORE_LOGGER_DEBUG( "User dropped {}", content.c_str() );
            upload.mDroppedPaths.emplace_back( content );
        }
    }

    auto EditorLayer::OpenDroppedWorkspace( const ContentDroppedEvent& event ) -> bool {
        MKT_BEGIN_PROFILER();

        for ( const auto& content: event.GetContents() ) {
            const filesystem::Path path{ content };
            const auto type{ filesystem::InferFileTypeFromExtension( path.GetNormalizedExtension() ) };

            if ( type == filesystem::FileType::eMikoto_Project ) {
                OpenProject( path );
                return true;
            }

            if ( type == filesystem::FileType::eMikoto_Scene ) {
                OpenScene( path );
                return true;
            }
        }

        return false;
    }

    auto EditorLayer::HandleCameraButton( MouseButton button, bool pressed ) -> void {
        MKT_BEGIN_PROFILER();

        if ( button != Mouse_Button_Right || !mEditorState->HasWorkspace() ||
             ( mEditorState->mRuntimeActive && mScreenPresentTarget != ScreenPresentTarget::ePanels ) ) {
            return;
        }

        // The docked viewport decides when a right-button gesture starts camera capture.
        if ( !pressed || mScreenPresentTarget == ScreenPresentTarget::eFinalImage ) {
            SetCameraCapture( pressed );
        }
    }

    auto EditorLayer::SetCameraCapture( bool enabled ) -> void {
        if ( enabled != mWindow->IsCursorMode( platform::CursorMode::eDisabled ) ) {
            mWindow->SetCursorMode( enabled ? platform::CursorMode::eDisabled : platform::CursorMode::eNormal );
        }

        mEditorCamera->EnableCamera( enabled );
    }

    // Editor setup.

    auto EditorLayer::InitAssets() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        auto* assets = asset::AssetRegistryService::GetPtr();

        if ( !assets || !assets->IsInitialized() ) {
            MKT_THROW_RUNTIME_ERROR( "Prefab loading requires initialized AssetRegistryService. Check the service startup error above." );
        }

        static constexpr std::array<std::pair<PrefabModelType, eastl::string_view>, 17> modelPaths{ {
            { PrefabModelType::eCube, "Resources/Prefabs/cube/gltf/scene.gltf" },
            { PrefabModelType::eCone, "Resources/Prefabs/cone/gltf/scene.gltf" },
            { PrefabModelType::eSphere, "Resources/Prefabs/sphere/gltf/scene.gltf" },
            { PrefabModelType::eCylinder, "Resources/Prefabs/cylinder/gltf/scene.gltf" },
            { PrefabModelType::eBunny, "Resources/Prefabs/bunny/bunny.obj" },
            { PrefabModelType::eMitsuba, "Resources/Prefabs/mitsuba/mitsuba.obj" },
            { PrefabModelType::eTeapot, "Resources/Prefabs/teapot/teapot.obj" },
            { PrefabModelType::eMissGalaxy, "Resources/Prefabs/miss_galaxy/scene.gltf" },
            { PrefabModelType::eTree01, "Resources/Prefabs/jungle_tree/scene.gltf" },
            { PrefabModelType::eTree02, "Resources/Prefabs/low_poly_tree_scene_free/scene.gltf" },
            { PrefabModelType::eTree03, "Resources/Prefabs/tree/scene.gltf" },
            { PrefabModelType::eTree04, "Resources/Prefabs/tree_animate/scene.gltf" },
            { PrefabModelType::eMedievalDoor, "Resources/Prefabs/medieval_door/scene.gltf" },
            { PrefabModelType::eSponza, "Resources/Prefabs/sponza/sponza-atrium-3/source/glTF/glTF/Sponza.gltf" },
        } };

        for ( const auto& [type, path]: modelPaths ) {
            const auto reference{ asset::AssetRef<asset::Model>{ filesystem::Path{ path } } };
            mEditorState->mPrefabs.emplace( type, EditorState::Prefab{ .mReference = reference } );
        }

        mEditorState->UpdatePrefabs();
    }

    auto EditorLayer::InitTitleBar() -> void {
        try {
            auto titleBarIconImage{ asset::ProcessImage2D( filesystem::Path{ "Resources/Editor/MikotoEditor.png" } ) };

            if ( titleBarIconImage ) {
                if ( !mWindow->SetIcon( 
                    as<const core::ubyte *>( titleBarIconImage->mBufferSpan->GetData() ), 
                    as<core::i32>( titleBarIconImage->mWidth ), 
                    as<core::i32>( titleBarIconImage->mHeight ) ) ) {
                    MKT_CORE_LOGGER_WARN( "Cannot set the editor window icon." );
                }

                if ( !mWindow->HasCustomTitleBar() ) {
                    return;
                }

                mTitleBarIcon = mDevice->CreateTexture( renderer::rhi::TextureCreateDescription{}
                    .SetName( "Editor title bar icon" )
                    .SetImageData( titleBarIconImage )
                    .SetWidth( titleBarIconImage->mWidth )
                    .SetHeight( titleBarIconImage->mHeight )
                    .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
                    .SetUsage( renderer::rhi::TextureUsageFlagsBits::ShaderResource )
                    .SetFormat( renderer::rhi::Format::eRGBA8_UNORM ) );
            }
        } catch ( const std::exception& error ) {
            MKT_CORE_LOGGER_WARN( "Cannot load the editor title bar icon: {}", error.what() );
        }
    }

    auto EditorLayer::InitEditorState() -> void {
        MKT_BEGIN_PROFILER();
        mEditorState = eastl::make_unique<EditorState>();

        mEditorState->mOpenParticle = [this]( const filesystem::Path& path ) {
            mPanelRegistry.Get<ParticleSimulationPanel>()->OpenAsset( path );
        };

        mEditorState->mOpenScene = [this]( const filesystem::Path& path ) {
            OpenScene( path );
        };

        mEditorState->mOpenProject = [this]( const filesystem::Path& path ) {
            OpenProject( path );
        };

        mEditorState->mGetTextureCompression = [this] {
            return mProject && mProject->GetCompressTextures();
        };

        mEditorState->mSetTextureCompression = [this]( bool enabled ) {
            if ( mProject && !mEditorState->mRuntimeActive ) {
                mProject->SetCompressTextures( enabled );
            }
        };

        mEditorState->mRequestEditorProjection = [this]( scene::ProjectionType projection ) {
            mEditorCamera->TransitionProjectionType( projection );
        };
    }

    auto EditorLayer::InitSceneRenderers() -> void {
        MKT_BEGIN_PROFILER();

        // Editor renderer
        mEditorRenderer = ISceneRenderer::Create()
            .SetName( "MainSceneRenderer" )
            .SetRenderResolution( mEditorState->mResolution )
            .SetDevice( renderer::RenderSystem::Get()->GetGpuDevice() )
            .Build();
        if ( mEditorRenderer ) {
            mEditorRenderer->Initialize();
        }
        mShowRenderingUnavailableNotice = !mEditorRenderer || !mEditorRenderer->IsRenderAvailable();

        // Game renderer
        const auto settings{ mEditorRenderer->GetSettings() };
        mGameRenderer = ISceneRenderer::Create()
            .SetName( "GameSceneRenderer" )
            .SetDevice( renderer::RenderSystem::Get()->GetGpuDevice() )
            .SetRenderResolution( settings.mResolution )
            .SetMultisampling( settings.mMultisampling )
            .Build();

        if ( mGameRenderer ) {
            // The hidden game view needs settings, not a second scene graph.
            mGameRenderer->SetEditorOverlaysEnabled( false );
            mGameRenderer->SetOutputForPreview( true );
        }

        // Thumbnails renderer
        auto thumbnailRendererDesc{ ThumbnailRendererCreateInfo{}
            .SetName( "MainThumbnailRenderer" )
            .SetShaderBasePath( "Resources/Shaders/slang" )
            .SetRenderResolution( mEditorState->mResolution )
            .SetDevice( renderer::RenderSystem::Get()->GetGpuDevice() ) };
        mThumbnailRenderer = renderer::ThumbnailRenderer::Create( thumbnailRendererDesc );

        if ( mThumbnailRenderer ) {
            mThumbnailRenderer->Initialize();
        }

        mEditorState->mGameRenderer = mGameRenderer.get();
        mEditorState->mEditorRenderer = mEditorRenderer.get();
        mEditorState->mThumbnailRenderer = mThumbnailRenderer.get();
        mEditorState->mPhysicsDebugRenderer = physics::PhysicSystem::Get()->GetDebugRenderer();
        mEditorState->mPhysicsDebugRendererSimple = physics::PhysicSystem::Get()->GetDebugRendererSimple();

        // Prepare final image
        mEditorState->mFinalComposition = mEditorRenderer->GetFinalImage( renderer::FinalImageType::eTonemap_Output );
    }

    auto EditorLayer::DrawRenderingUnavailableNotice() -> void {
        MKT_BEGIN_PROFILER();

        const PopupStyle popupStyle{};
        const auto localizedTitle{ imgui::widget::LocalizedLabel( 
            "editor_layer_scene_rendering_unavailable",
            "Scene rendering unavailable" ) };

        const core::cstr title{ localizedTitle.c_str() };

        if ( mShowRenderingUnavailableNotice ) {
            ImGui::OpenPopup( title );
            mShowRenderingUnavailableNotice = false;
        }

        ImGui::SetNextWindowSize( ImVec2{ 440.0f, 0.0f }, ImGuiCond_Appearing );

        if ( ImGui::BeginPopupModal( title, nullptr, ImGuiWindowFlags_AlwaysAutoResize ) ) {
            const auto* device{ renderer::RenderSystem::Get()->GetGpuDevice() };
            const core::cstr backend{ device->IsGraphicsApi( renderer::rhi::GraphicsAPI::eOpenGL ) ? "OpenGL" : "D3D11" };
            ImGui::TextWrapped( MKT_LOC( "editor_layer_scene_rendering_is_not_available_with_yet" ).c_str(), backend );
            ImGui::Spacing();
            ImGui::TextWrapped( "%s", MKT_LOC( "editor_layer_you_can_continue_browsing_and_editing_the_project_the_scene" ).c_str() );
            ImGui::Spacing();

            if ( imgui::widget::AccentButton( MKT_LOC( "editor_layer_continue" ).c_str(), ImVec2{ 120.0f, 0.0f } ) ) {
                ImGui::CloseCurrentPopup();
            }

            ImGui::SetItemDefaultFocus();
            ImGui::EndPopup();
        }
    }

    auto EditorLayer::InitEditorCamera() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        auto cameraDesc{ scene::SceneCameraDescription{}
            .SetFieldOfView( 45.0f )
            .SetNearPlane( 0.1f )
            .SetFarPlane( 3000.0f )
            .SetTargetWindow( mWindow )
            .SetAspectRatio( core::as<core::f32>( mWindow->GetWidth() ), core::as<core::f32>( mWindow->GetHeight() ) ) };
        mEditorCamera = eastl::make_unique<scene::SceneCamera>( cameraDesc );
        mEditorCamera->ResetSettings();
        mEditorCamera->ResetView();

        mEditorState->mEditorCamera = mEditorCamera.get();
        mEditorState->mActiveCamera = mEditorCamera.get();
    }

    auto EditorLayer::InitEditorPanels() -> void {
        MKT_BEGIN_PROFILER();
        mPanelRegistry.Register<LayoutsPanel>( LayoutsPanelCreateInfo{
            .mCaptureVisibility = [this]() {
                LayoutPanelVisibility visibility{};

                for ( const auto& panel: mPanelRegistry | std::ranges::views::values ) {
                    visibility.emplace_back( eastl::string{ panel->GetName() }, panel->IsVisible() );
                }

                return visibility;
            },
            .mRestoreVisibility = [this]( LayoutPanelVisibility visibility ) {
                mPendingPanelVisibility = eastl::move( visibility );
            },
            .mResetLayout = [this]() {
                mResetLayoutRequested = true;
            } } );

        mPanelRegistry.Register<StatsPanel>( StatsPanelCreateInfo{
            .mState = mEditorState.get(),
            .mGroup = "Diagnostics",
            .mIsToggleable = true } );

        mPanelRegistry.Register<GPUStatsPanel>( GPUStatsPanelCreateInfo{
            .mState = mEditorState.get(),
            .mGpuDevice = mDevice,
            .mGroup = "Diagnostics",
            .mIsToggleable = true } );

#if defined( MIKOTO_EDITOR_TRACY_VIEWER )
        mPanelRegistry.Register<InstrumentationPanel>();
#endif

        mPanelRegistry.Register<CompilerStatusPanel>( CompilerStatusPanelCreateInfo{
            .mState = mEditorState.get(),
            .mGroup = "Diagnostics",
            .mIsToggleable = true } );

        mPanelRegistry.Register<SimulationSettingsPanel>( SimulationSettingsPanelCreateInfo{
            .mState = mEditorState.get(),
            .mGroup = "Settings",
            .mIsToggleable = true } );

        mPanelRegistry.Register<AudioPanel>( AudioPanelCreateInfo{
            .mState = mEditorState.get(),
            .mDevice = mDevice,
            .mGroup = "Audio",
            .mIsToggleable = true } );

        mPanelRegistry.Register<AudioMixerPanel>( AudioMixerPanelCreateInfo{
            .mState = mEditorState.get(),
            .mGroup = "Audio",
            .mIsToggleable = true } );

        mPanelRegistry.Register<AudioProfilerPanel>( AudioProfilerPanelCreateInfo{
            .mState = mEditorState.get(),
            .mGroup = "Audio",
            .mIsToggleable = true } );

        mPanelRegistry.Register<AboutPanel>( AboutPanelCreateInfo{
            .mGroup = "Settings",
            .mIsToggleable = true } );

        mPanelRegistry.Register<MaterialEditorPanel>( MaterialEditorPanelDescription{
            .mState = mEditorState.get(),
            .mDevice = mDevice,
            .mGroup = "Rendering",
            .mIsToggleable = true } );

        mPanelRegistry.Register<ParticleSimulationPanel>( ParticleSimulationPanelCreateInfo{
            .mState = mEditorState.get(),
            .mGroup = "Rendering",
            .mIsToggleable = true } );

        mPanelRegistry.Register<RuntimeConsolePanel>( RuntimeConsolePanelCreateInfo{
            .mState = mEditorState.get(),
            .mGroup = "Diagnostics",
            .mIsToggleable = true } );

        mPanelRegistry.Register<ViewportPanel>( ViewportPanelCreateInfo{
            .mState = mEditorState.get(),
            .mGpuDevice = mDevice,
            .mGroup = "Scene",
            .mIsToggleable = true } );

        mPanelRegistry.Register<GamePreviewPanel>( GamePreviewPanelCreateInfo{
            .mState = mEditorState.get(),
            .mGroup = "Scene",
            .mIsToggleable = true } );

        mPanelRegistry.Register<InspectorPanel>( InspectorPanelCreateInfo{
            .mState = mEditorState.get(),
            .mGroup = "Scene",
            .mIsToggleable = true } );

        mPanelRegistry.Register<HierarchyPanel>( HierarchyPanelCreateInfo{
            .mState = mEditorState.get(),
            .mGroup = "Scene",
            .mIsToggleable = true } );

        mPanelRegistry.Register<SettingsPanel>( SettingsPanelCreateInfo{
            .mState = mEditorState.get(),
            .mGroup = "Settings",
            .mIsToggleable = true } );

        mPanelRegistry.Register<PreferencesPanel>( PreferencesPanelCreateInfo{
            .mState = mEditorState.get(),
            .mViewportPanel = mPanelRegistry.Get<ViewportPanel>(),
            .mCameraPanel = mPanelRegistry.Get<SettingsPanel>(),
            .mActions = mActionManager.get(),
            .mGroup = "Settings",
            .mIsToggleable = true } );

        mPanelRegistry.Register<ContentBrowserPanel>( ContentBrowserPanelDescription{
            .mDevice = renderer::RenderSystem::Get()->GetGpuDevice(),
            .mState = mEditorState.get(),
            .mProjectBasePath = ".",
            .mResourcesBasePath = "Resources",
            .mGroup = "Assets",
            .mIsToggleable = true } );

        mPanelRegistry.Register<RendererPanel>( RendererPanelCreateInfo{
            .mState = mEditorState.get(),
            .mGroup = "Rendering",
            .mIsToggleable = true } );

        mPanelRegistry.Register<LightingPanel>( LightingPanelCreateInfo{
            .mState = mEditorState.get(),
            .mGroup = "Rendering",
            .mIsToggleable = true } );

        mPanelRegistry.Register<PassVisualizerPanel>( PassVisualizerDescription{
            .mState = mEditorState.get(),
            .mGroup = "Rendering",
            .mIsToggleable = true } );

        mPanelRegistry.Register<AnimationGraphPanel>( AnimationGraphPanelCreateInfo{
            .mState = mEditorState.get(),
            .mGroup = "Animation",
            .mIsToggleable = true } );

        mPanelRegistry.Register<ScriptGraphPanel>( ScriptGraphPanelCreateInfo{
            .mGroup = "Scripting",
            .mIsToggleable = true } );

        mPanelRegistry.Register<AnimatorPanel>( AnimatorPanelCreateInfo{
            .mState = mEditorState.get(),
            .mGroup = "Animation",
            .mIsToggleable = true } );

        mPanelRegistry.Register<PrefabsBrowserPanel>( PrefabsBrowserPanelCreateInfo{
            .mState = mEditorState.get(),
            .mGroup = "Assets",
            .mIsToggleable = true } );

        mPanelRegistry.Register<MaterialsBrowserPanel>( MaterialsBrowserPanelCreateInfo{
            .mState = mEditorState.get(),
            .mGroup = "Assets",
            .mIsToggleable = true } );

        mPanelRegistry.Register<AnimationsBrowserPanel>( AnimationsBrowserPanelCreateInfo{
            .mState = mEditorState.get(),
            .mGroup = "Assets",
            .mIsToggleable = true } );

        mPanelRegistry.Register<LandscapePanel>( LandscapePanelCreateInfo{
            .mState = mEditorState.get(),
            .mGroup = "Rendering",
            .mIsToggleable = true } );
    }

    auto EditorLayer::InitActionCallbacks() -> void {
        MKT_BEGIN_PROFILER();
        mActionManager = eastl::make_unique<platform::ActionManager>( platform::ActionManagerCreateInfo{} );
        mActionManager->Initialize();
        mActionManager->Bind( platform::KeyCode::Key_N, platform::ModKey::eControl,
            [this]() {
            CreateScene();
        }, "editor_layer_new_scene" );

        mActionManager->Bind( platform::KeyCode::Key_L, platform::ModKey::eControl,
            [this]() {
            OpenScene();
        }, "editor_layer_open_scene" );

        mActionManager->Bind( platform::KeyCode::Key_S, platform::ModKey::eControl,
            [this]() {
            SaveScene();
        }, "editor_layer_save_scene" );

        mActionManager->Bind( platform::KeyCode::Key_S, platform::ModKey::eControl | platform::ModKey::eShift,
            [this]() {
            SaveSceneAs();
        }, "editor_layer_save_scene_as" );

        mActionManager->Bind( platform::KeyCode::Key_D, platform::ModKey::eControl,
            [this]() {
            MKT_CORE_LOGGER_DEBUG( "You pressed Ctrl + D (Duplicate Entity)" );
            ( void )mEditorState->mActiveScene->DuplicateEntity( mEditorState->mSelectedEntity );
        }, "shortcut_duplicate_selection" );

        mActionManager->Bind( platform::KeyCode::Key_P, platform::ModKey::eControl, 
            [this]() {
            MKT_CORE_LOGGER_DEBUG( "You pressed Ctrl + P (Create project)" );
            CreateProject();
        }, "editor_layer_new_project" );

        mActionManager->Bind( platform::KeyCode::Key_U, platform::ModKey::eControl, 
            [this]() {
            MKT_CORE_LOGGER_DEBUG( "You pressed Ctrl + U (Open project)" );
            OpenProject();
        }, "editor_layer_open_project" );

        mActionManager->Bind( platform::KeyCode::Key_G, platform::ModKey::eControl, 
            [this]() {
            SaveProject();
            MKT_CORE_LOGGER_DEBUG( "You pressed Ctrl + G (Save project)" );
        }, "editor_layer_save_project" );

        mActionManager->Bind( platform::KeyCode::Key_A, platform::ModKey::eControl, 
            []() {
            MKT_CORE_LOGGER_DEBUG( "You pressed Ctrl + A (Select all entities)" );
        }, "shortcut_select_all_placeholder" );

        mActionManager->Bind( platform::KeyCode::Key_I, platform::ModKey::eControl, 
            [this]() {
            MKT_CORE_LOGGER_DEBUG( "You pressed Ctrl + I (ImguiDebugInfo)" );

            mShowImGuiDebugInfo = !mShowImGuiDebugInfo;
        }, "editor_layer_display_imgui_debug_info" );

        mActionManager->Bind( platform::KeyCode::Key_N, platform::ModKey::eControl | platform::ModKey::eAlt, 
            []() {
            MKT_CORE_LOGGER_DEBUG( "You pressed Ctrl + Alt + N (New entity)" );
        }, "shortcut_new_entity_placeholder" );

        mActionManager->Bind( platform::KeyCode::Key_C, platform::ModKey::eControl | platform::ModKey::eShift, 
            [this]() {
            mPanelRegistry.Get<RuntimeConsolePanel>()->FocusCommandInput();
        }, "editor_layer_console" );

        mActionManager->Bind( platform::KeyCode::Key_7, platform::ModKey::eControl, 
            [this]() {
            mPanelRegistry.Get<StatsPanel>()->SetVisible( true );
            mPanelRegistry.Get<GPUStatsPanel>()->SetVisible( true );
        }, "editor_layer_stats" );

        // Guizmos
        mActionManager->Bind( platform::KeyCode::Key_1, platform::ModKey::eControl, 
            [this]() {
            MKT_CORE_LOGGER_DEBUG( "You pressed Ctrl + 1 (Translation Guizmo)" );
            ViewportPanel* scenePanel{ mPanelRegistry.Get<ViewportPanel>() };

            if ( scenePanel ) {
                scenePanel->SetGizmoType( imgui::GizmoType::eTranslation );
            }
        }, "shortcut_gizmo_translate" );

        mActionManager->Bind( platform::KeyCode::Key_2, platform::ModKey::eControl, 
            [this]() {
            MKT_CORE_LOGGER_DEBUG( "You pressed Ctrl + 2 (Rotation Guizmo)" );
            ViewportPanel* scenePanel{ mPanelRegistry.Get<ViewportPanel>() };

            if ( scenePanel ) {
                scenePanel->SetGizmoType( imgui::GizmoType::eRotation );
            }
        }, "shortcut_gizmo_rotate" );

        mActionManager->Bind( platform::KeyCode::Key_3, platform::ModKey::eControl, 
            [this]() {
            MKT_CORE_LOGGER_DEBUG( "You pressed Ctrl + 3 (Scale Guizmo)" );
            ViewportPanel* scenePanel{ mPanelRegistry.Get<ViewportPanel>() };

            if ( scenePanel ) {
                scenePanel->SetGizmoType( imgui::GizmoType::eScale );
            }
        }, "shortcut_gizmo_scale" );

        mActionManager->Bind( platform::KeyCode::Key_Delete, platform::ModKey::eNone, 
            [this]() {
            MKT_CORE_LOGGER_DEBUG( "You pressed Delete (Delete current entity)" );

            if ( mEditorState->mSelectedEntity ) {
                // Store the ID because we will lose
                // the entity after SelectEntity( nullptr ) call
                core::u32 entityID{ mEditorState->mSelectedEntity->GetComponent<scene::TagComponent>().GetGuid() };

                mEditorState->SelectEntity( nullptr );
                mEditorState->mActiveScene->RemoveEntity( entityID );
            }
        }, "shortcut_delete_selection" );

        mActionManager->Bind( platform::KeyCode::Key_F11, platform::ModKey::eNone, 
            [this]() {
            static eastl::hash_map<Panel*, bool> panelsVisibilityState{};

            SetCameraCapture( false );
            if ( auto* panel{ mPanelRegistry.Get<ViewportPanel>() } ) {
                panel->SuspendInput();
                panel->CancelPicking();
            }

            if ( mScreenPresentTarget == ScreenPresentTarget::ePanels ) {
                mScreenPresentTarget = ScreenPresentTarget::eFinalImage;

                // Save panel visibility state before hiding them
                for ( const auto& panel: mPanelRegistry | std::ranges::views::values ) {
                    panelsVisibilityState[panel.get()] = panel->IsVisible();
                }
            }
            else {
                mScreenPresentTarget = ScreenPresentTarget::ePanels;

                // Restore panel visibility state
                for ( const auto& panel: mPanelRegistry | std::ranges::views::values ) {
                    panel->SetVisible( panelsVisibilityState[panel.get()] );
                }
            }
        }, "shortcut_toggle_viewport_fullscreen" );

        mActionManager->Bind( platform::KeyCode::Key_F4, platform::ModKey::eNone, 
            [this]() {
            mDisplayPhysicsPanelLines = !mDisplayPhysicsPanelLines;
        }, "shortcut_toggle_physics_lines" );

        mActionManager->Bind( platform::KeyCode::Key_F5, platform::ModKey::eNone, 
            [this]() {
            mDisplayPhysicsPanelTriangles = !mDisplayPhysicsPanelTriangles;
        }, "shortcut_toggle_physics_triangles" );
    }

    auto EditorLayer::InitEmptyScene() -> void {
        MKT_BEGIN_PROFILER();

        // An empty scratch scene keeps renderer/panel contracts valid before
        // a project is selected. It contains no demo assets or entities.
        mEditorState->mActiveScene = scene::SceneManager::Get()->CreateScene( "Untitled" );

        mEditorState->mRequestPlay = [this] {
            mPlayRequested = true;
        };

        mEditorState->mRequestStop = [this] {
            mStopRequested = true;
        };
    }

    auto EditorLayer::UpdateDockSpace() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        const auto& background{ ImGui::GetStyleColorVec4( ImGuiCol_WindowBg ) };
        imgui::ImGuiService::Get()->SetImGuiBackGroundClearColor( { background.x, background.y, background.z, 1.0f } );

        constexpr ImGuiWindowFlags windowFlags{
            ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus
        };

        const auto* viewport{ ImGui::GetMainViewport() };
        const float statusHeight{ ImGui::GetTextLineHeight() + 12.0f };
        ImGui::SetNextWindowPos( viewport->WorkPos );
        ImGui::SetNextWindowSize( { viewport->WorkSize.x, glm::max( 1.0f, viewport->WorkSize.y - statusHeight ) } );
        ImGui::SetNextWindowViewport( viewport->ID );
        ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, 0.0f );
        ImGui::PushStyleVar( ImGuiStyleVar_WindowBorderSize, 0.0f );
        ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f } );

        if ( mWindow->HasCustomTitleBar() ) {
            ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, { ImGui::GetStyle().FramePadding.x, kTitleBarVerticalPadding } );
        }

        // Submit even when Begin returns false, keeping docked panels attached.
        imgui::BeginWindow( "EditorDockSpace", nullptr, windowFlags );

        if ( mWindow->HasCustomTitleBar() ) {
            ImGui::PopStyleVar();
        }

        ImGui::PopStyleVar( 3 );

        DrawUtilityBar();

        // Keep startup windows within the area between the utility bar and status bar.
        mWorkspacePosition = { viewport->WorkPos.x, ImGui::GetCursorScreenPos().y };
        mWorkspaceSize = { viewport->WorkSize.x, viewport->WorkPos.y + viewport->WorkSize.y - statusHeight - mWorkspacePosition.y };

        // Panels share this minimum width, including those opened by a menu.
        ImGui::GetStyle().WindowMinSize.x = 300.0f;

        if ( ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable ) {
            if ( mResetLayoutRequested ) {
                ResetPanelLayout();
                mResetLayoutRequested = false;
            }

            const ImGuiDockNodeFlags flags{ mEditorState->HasWorkspace() ? ImGuiDockNodeFlags_None : ImGuiDockNodeFlags_KeepAliveOnly };
            // DockSpace lays out its tabs before the individual panels begin.
            const imgui::ImGuiScopedStyleVar tabPadding{ ImGuiStyleVar_FramePadding, imgui::kWindowTabPadding };
            ImGui::DockSpace( ImGui::GetID( "MikotoDockEditor" ), { 0.0f, 0.0f }, flags );
        }

        DrawMainMenuBar();

        ImGui::End();
        
        DrawStatusBar();
    }

    auto EditorLayer::DrawStatusBar() -> void {
        MKT_BEGIN_PROFILER();

        const auto* viewport{ ImGui::GetMainViewport() };
        const float height{ ImGui::GetTextLineHeight() + 12.0f };
        
        ImGui::SetNextWindowPos( { viewport->WorkPos.x, viewport->WorkPos.y + viewport->WorkSize.y - height } );
        ImGui::SetNextWindowSize( { viewport->WorkSize.x, height } );
        ImGui::SetNextWindowViewport( viewport->ID );

        const imgui::ImGuiScopedStyleVar padding{ ImGuiStyleVar_WindowPadding, ImVec2{ 10.0f, 5.0f } };
        const imgui::ImGuiScopedStyleVar itemSpacing{ ImGuiStyleVar_ItemSpacing, ImVec2{ 4.0f, ImGui::GetStyle().ItemSpacing.y } };
        const imgui::ImGuiScopedStyleVar rounding{ ImGuiStyleVar_WindowRounding, 0.0f };
        const imgui::ImGuiScopedStyleVar border{ ImGuiStyleVar_WindowBorderSize, 0.0f };
        const imgui::ImGuiScopedStyleVar minimumSize{ ImGuiStyleVar_WindowMinSize, ImVec2{ 1.0f, 1.0f } };

        ImGui::PushStyleColor( ImGuiCol_WindowBg, ImGui::GetStyleColorVec4( ImGuiCol_MenuBarBg ) );
        
        constexpr ImGuiWindowFlags flags{
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoFocusOnAppearing
        };

        if ( imgui::BeginWindow( "##EditorStatusBar", nullptr, flags ) ) {
            const auto position{ ImGui::GetWindowPos() };
            ImGui::GetWindowDrawList()->AddLine( position, { position.x + viewport->WorkSize.x, position.y }, ImGui::GetColorU32( ImGuiCol_Border ) );

            const float rowWidth{ glm::max( 0.0f, ImGui::GetContentRegionAvail().x ) };
            const float scale{ ImGui::GetFontSize() / 17.0f };
            const float selectionWidth{ glm::min( 220.0f * scale, rowWidth * 0.2f ) };
            const auto* activeScene{ mEditorState->HasWorkspace() ? mEditorState->mActiveScene : nullptr };

            const eastl::array<const char*, 3> states{
                MKT_LOC( "hierarchy_ready" ).c_str(), MKT_LOC( "stats_playing" ).c_str(), MKT_LOC( "stats_simulating" ).c_str()
            };

            constexpr eastl::array<const char*, 3> stateIcons{ ICON_MD_CHECK_CIRCLE, ICON_MD_PLAY_ARROW, ICON_MD_BOLT };
            constexpr eastl::array<ImVec4, 3> stateColors{
                ImVec4{ 0.55f, 0.76f, 0.48f, 1.0f }, ImVec4{ 0.35f, 0.82f, 0.46f, 1.0f }, ImVec4{ 0.30f, 0.64f, 0.96f, 1.0f }
            };

            const SceneState state{ mEditorState->mRuntimeActive || 
                ( activeScene && activeScene->IsPlaying() ) ? SceneState::ePlaying :
                activeScene && activeScene->IsSimulating() ? 
                SceneState::eSimulating : 
                SceneState::eIdle };

            float stateWidth{};
            const auto stateIndex{ core::as<core::usize>( state ) };

            for ( core::usize index{}; index < states.size(); ++index ) {
                stateWidth = glm::max( stateWidth, ImGui::CalcTextSize( states[index] ).x +
                    ImGui::CalcTextSize( stateIcons[index] ).x + ImGui::CalcTextSize( " " ).x );
            }

            // Reserve the longest state label so playback changes cannot shift the next field.
            {
                const imgui::ImGuiScopedColor color{ ImGuiCol_Text, stateColors[stateIndex] };
                DrawStatusField( string::Format( "{} {}", stateIcons[stateIndex], states[stateIndex] ), stateWidth );
            }

            ImGui::SameLine();

            DrawStatusDivider();
            
            const eastl::string_view sceneName{ activeScene ? activeScene->GetName() : eastl::string_view{ MKT_LOC( "editor_status_no_scene" ) } };
            eastl::string sceneLabel{ MKT_LOC( "editor_status_scene" ) };
            sceneLabel += ": ";
            sceneLabel.append( sceneName.data(), sceneName.size() );
            
            DrawStatusField( sceneLabel, glm::min( ImGui::CalcTextSize( sceneLabel.c_str() ).x, 240.0f * scale ) );
            
            ImGui::SameLine();
            
            DrawStatusDivider();

            const auto counts{ core::RuntimeConsole::Get()->GetLogCounts() };
            constexpr eastl::array levels{ core::ConsoleLogLevel::eInfo, core::ConsoleLogLevel::eWarning, core::ConsoleLogLevel::eError };
            constexpr eastl::array icons{ ICON_MD_INFO, ICON_MD_WARNING, ICON_MD_ERROR };

            constexpr eastl::array keys{ 
                "runtime_console_info", 
                "runtime_console_warn", 
                "runtime_console_error" 
            };

            constexpr eastl::array colors{
                ImVec4{ 0.35f, 0.72f, 0.92f, 1.0f }, 
                ImVec4{ 0.95f, 0.74f, 0.32f, 1.0f }, 
                ImVec4{ 0.95f, 0.35f, 0.35f, 1.0f }
            };

            for ( core::usize index{}; index < levels.size(); ++index ) {
                if ( index != 0 ) {
                    ImGui::SameLine();
                }

                const auto count{ counts[core::as<core::usize>( levels[index] )] };
                const auto label{ string::Format( "{} {}", icons[index], count ) };
                ImGui::TextColored( colors[index], "%s", label.c_str() );
                imgui::widget::ItemTooltip( "%s: %zu", MKT_LOC( keys[index] ).c_str(), count );
            }

            if ( mEditorState->mShowStatusDateTime ) {
                // Refresh local conversion/formatting once per second, independent of frame rate.
                const auto now{ core::DateTime::UtcNow() };
                const auto second{ now.GetTicks() / core::DateTime::kTicksPerSecond };

                if ( second != mStatusClockSecond ) {
                    mStatusClockText = now.ToLocalTime().ToString( "yyyy-MM-dd HH:mm:ss" );
                    mStatusClockSecond = second;
                }

                ImGui::SameLine();
                
                DrawStatusDivider();

                // Reserve the widest possible digits, independent of the current time.
                float digitWidth{};
                for ( char digit{ '0' }; digit <= '9'; ++digit ) {
                    const eastl::array<char, 2> glyph{ digit, '\0' };
                    digitWidth = glm::max( digitWidth, ImGui::CalcTextSize( glyph.data() ).x );
                }

                const float clockWidth{ digitWidth * 14.0f + ImGui::CalcTextSize( "-- ::" ).x };
                {
                    const imgui::ImGuiScopedColor color{ ImGuiCol_Text, ImGui::GetStyleColorVec4( ImGuiCol_TextDisabled ) };
                    DrawStatusField( mStatusClockText, clockWidth );
                }

                imgui::widget::ItemTooltip( "%s", MKT_LOC( "editor_status_local_time" ).c_str() );
            }

            // Selection ends the left-hand information group without shifting the project/task group.
            eastl::string selection{ MKT_LOC( "editor_status_no_selection" ) };

            if ( activeScene && mEditorState->mSelectedEntityIDs.size() > 1 ) {
                selection = string::Format( "{} {}", mEditorState->mSelectedEntityIDs.size(), MKT_LOC( "editor_status_selected_entities" ) );
            }
            else if ( const auto* entity{ activeScene ? activeScene->FindByID( mEditorState->mSelectedEntityID ) : nullptr } ) {
                selection = entity->GetComponent<scene::TagComponent>().GetTag();
            }

            ImGui::SameLine();

            DrawStatusDivider();
            DrawStatusField( selection, selectionWidth );

            eastl::string status{};

            if ( mWorkspaceOperation.IsBusy() ) {
                status = string::Format( "{} {}", ICON_MD_HOURGLASS_EMPTY, mWorkspaceOperation.GetTitle() );
            }
            else if ( mEditorState->mSandboxMode ) {
                status = MKT_LOC( "editor_status_sandbox" );
            }
            else if ( mProject ) {
                // Renderer edits join camera and project revisions in the existing save tracking.
                mProject->SetSceneRendererSettings( mEditorRenderer->GetSettings() );
                const bool dirty{ mProject->HasUnsavedChanges() };
                status = string::Format( "{} {}", dirty ? ICON_MD_SAVE : ICON_MD_CHECK_CIRCLE,
                    MKT_LOC( dirty ? "editor_status_unsaved_changes" : "editor_status_project_saved" ) );
            }
            else {
                status = MKT_LOC( mEditorState->mSandboxMode ? "editor_status_sandbox" : "editor_status_no_project" );
            }

            const auto version{ string::Format( "{} {}", MKT_LOC( "editor_status_version" ),
                core::CVarRegistry::Get()->GetValue<eastl::string>( "engine.version", "?" ) ) };
            const auto* apiName{ renderer::rhi::GetGraphicsApiName( renderer::RenderSystem::Get()->GetActiveGraphicsApi() ) };
            const eastl::string api{ apiName ? apiName : MKT_LOC( "stats_unknown" ).c_str() };
            const float spacing{ ImGui::GetStyle().ItemSpacing.x * 2.0f };
            const float statusWidth{ ImGui::CalcTextSize( status.c_str() ).x };
            const float versionWidth{ ImGui::CalcTextSize( version.c_str() ).x };
            const float apiWidth{ ImGui::CalcTextSize( api.c_str() ).x };

            ImGui::SameLine();
            
            const auto rowStart{ ImGui::GetCursorPos() };
            const float remaining{ glm::max( 0.0f, ImGui::GetContentRegionAvail().x ) };
            const bool showApi{ remaining >= apiWidth };
            const float apiSpace{ showApi ? apiWidth + spacing : 0.0f };
            const bool showVersion{ remaining >= statusWidth + apiSpace + versionWidth + spacing * 2.0f };
            const float versionSpace{ showVersion ? versionWidth + spacing : 0.0f };
            const float trailingSpace{ versionSpace + apiSpace };
            const float activityWidth{ DrawBackgroundActivity( statusWidth + trailingSpace ) };
            const float statusSpace{ glm::max( 0.0f, remaining - activityWidth - trailingSpace ) };

            if ( statusSpace >= statusWidth ) {
                ImGui::SetCursorPos( { rowStart.x + remaining - statusWidth - trailingSpace, rowStart.y } );
                ImGui::TextUnformatted( status.c_str() );

                if ( mProject ) {
                    imgui::widget::ItemTooltip( "%s", MKT_LOC( "editor_status_project_settings_hint" ).c_str() );
                }
            }

            if ( showVersion && statusSpace >= statusWidth ) {
                ImGui::SetCursorPos( { rowStart.x + remaining - versionWidth - apiSpace, rowStart.y } );
                ImGui::TextDisabled( "%s", version.c_str() );
            }

            if ( showApi ) {
                ImGui::SetCursorPos( { rowStart.x + remaining - apiWidth, rowStart.y } );
                ImGui::TextDisabled( "%s", api.c_str() );
                imgui::widget::ItemTooltip( "%s", MKT_LOC( "stats_graphics_api" ).c_str() );
            }

        }

        ImGui::End();
        ImGui::PopStyleColor();
    }

    auto EditorLayer::DrawBackgroundActivity( core::f32 trailingWidth ) -> core::f32 {
        auto* assets{ asset::AssetRegistryService::GetPtr() };
        const auto now{ ImGui::GetTime() };

        // Query only queue IDs/counters, not the resident asset inventory or GPU memory statistics.
        if ( now >= mStatusWorkRefresh ) {
            mStatusLoadingProgress = assets ? assets->GetLoadingProgress() : asset::AssetLoadingProgress{};
            mStatusCompressionProgress = asset::GetTextureCompressionProgress();
            mStatusWorkRefresh = now + 0.15;
        }

        const auto& loading{ mStatusLoadingProgress };
        const usize pending{ loading.mQueued.size() + loading.mLoading.size() };
        const bool compressing{ !mStatusCompressionProgress.empty() };
        const bool workspace{ mWorkspaceOperation.IsBusy() };
        const bool busy{ pending != 0 || compressing || workspace };
        constexpr const char* popupID{ "##BackgroundTasks" };
        const float scale{ ImGui::GetFontSize() / 17.0f };

        // A clicked task list stays available if its final job finishes while it is open.
        // The indicator and divider still disappear as soon as the queue becomes idle.
        if ( !busy && !ImGui::IsPopupOpen( popupID ) ) {
            return 0.0f;
        }

        float occupiedWidth{};
        bool hovered{};

        if ( busy ) {
            eastl::string label{};
            eastl::string number{};
            f32 progress{};

            // Show measured block progress during encoding, otherwise finished requests in the batch.
            if ( compressing ) {
                usize completed{}, total{};

                for ( const auto& texture: mStatusCompressionProgress ) {
                    completed += texture.mCompletedBlocks;
                    total += texture.mTotalBlocks;
                }

                progress = total ? as<f32>( completed ) / as<f32>( total ) : 0.0f;
                label = string::Format( "{} ({})", MKT_LOC( "editor_status_compressing" ), mStatusCompressionProgress.size() );
                number = string::Format( "{}%", as<u32>( progress * 100.0f ) );
            }
            else if ( pending ) {
                progress = loading.mTotal ? as<f32>( loading.mCompleted ) / as<f32>( loading.mTotal ) : 0.0f;
                label = MKT_LOC( "editor_status_loading" );
                number = string::Format( "{}/{}", loading.mCompleted, loading.mTotal );
            }
            else if ( workspace ) {
                label = MKT_LOC( "editor_status_working" );
                number = "1";
            }

            const float height{ ImGui::GetTextLineHeight() };
            const float gap{ 7.0f * scale };
            const float barWidth{ 64.0f * scale };
            const auto textSize{ ImGui::CalcTextSize( label.c_str() ) };
            const float width{ textSize.x + barWidth + ImGui::CalcTextSize( number.c_str() ).x + gap * 2.0f };
            const float dividerWidth{ 6.0f * scale + ImGui::GetStyle().ItemSpacing.x * 2.0f };
            occupiedWidth = width + dividerWidth;

            ImGui::SetCursorPosX( ImGui::GetCursorPosX() + glm::max( 0.0f,
                ImGui::GetContentRegionAvail().x - width - dividerWidth - trailingWidth ) );
            
            const auto position{ ImGui::GetCursorScreenPos() };
            const bool clicked{ ImGui::InvisibleButton( "##BackgroundActivity", { width, height } ) };
            hovered = ImGui::IsItemHovered( ImGuiHoveredFlags_DelayShort );
            auto* draw{ ImGui::GetWindowDrawList() };
            const auto textColor{ ImGui::GetColorU32( ImGuiCol_Text ) };
            draw->AddText( position, textColor, label.c_str() );

            const ImVec2 minimum{ position.x + textSize.x + gap, position.y + ( height - 6.0f * scale ) * 0.5f };
            const ImVec2 maximum{ minimum.x + barWidth, minimum.y + 6.0f * scale };
            draw->AddRectFilled( minimum, maximum, ImGui::GetColorU32( ImGuiCol_FrameBg ), 3.0f * scale );

            // Imports report completed requests, not progress inside a request.
            // Keep the first long load visibly active without inventing a percentage.
            const bool indeterminate{ !compressing && ( !pending || loading.mCompleted == 0 ) };
            const float start{ indeterminate ? glm::fract( as<f32>( now * 0.6 ) ) * 0.7f : 0.0f };
            const float end{ indeterminate ? start + 0.3f : glm::clamp( progress, 0.0f, 1.0f ) };

            if ( end > start ) {
                draw->AddRectFilled( { minimum.x + start * barWidth, minimum.y },
                    { minimum.x + end * barWidth, maximum.y }, ImGui::GetColorU32( ImGuiCol_PlotHistogram ), 3.0f * scale );
            }

            draw->AddText( { maximum.x + gap, position.y }, textColor, number.c_str() );

            ImGui::SameLine();
            DrawStatusDivider();

            if ( clicked ) {
                // Set the anchor once, above the footer. Mouse movement never repositions this popup.
                ImGui::OpenPopup( popupID );
                ImGui::SetNextWindowPos( { position.x + width, position.y - 8.0f * scale },
                    ImGuiCond_Appearing, { 1.0f, 1.0f } );
            }
        }

        // Native popups keep their position and provide wheel/scrollbar interaction.
        // https://github.com/ocornut/imgui/blob/v1.92.9b-docking/imgui_demo.cpp
        const bool popupOpen{ ImGui::IsPopupOpen( popupID ) };

        if ( !popupOpen && !hovered ) {
            return occupiedWidth;
        }

        const PopupStyle popupStyle{};
        const float maxHeight{ glm::min( 600.0f * scale, ImGui::GetMainViewport()->WorkSize.y * 0.75f ) };

        if ( popupOpen ) {
            ImGui::SetNextWindowSizeConstraints( { 460.0f * scale, 0.0f }, { 460.0f * scale, maxHeight } );

            if ( ImGui::BeginPopup( popupID, ImGuiWindowFlags_NoMove ) ) {
                DrawBackgroundActivityDetails( true );
                ImGui::EndPopup();
            }
        }
        else if ( hovered ) {
            ImGui::SetNextWindowSizeConstraints( { 360.0f * scale, 0.0f }, { 460.0f * scale, maxHeight } );

            if ( ImGui::BeginTooltip() ) {
                DrawBackgroundActivityDetails( false );
                ImGui::Separator();
                ImGui::TextDisabled( "%s", MKT_LOC( "editor_status_background_details_hint" ).c_str() );
                ImGui::EndTooltip();
            }
        }

        return occupiedWidth;
    }

    auto EditorLayer::DrawBackgroundActivityDetails( bool expanded ) -> void {
        auto* assets{ asset::AssetRegistryService::GetPtr() };
        const auto& loading{ mStatusLoadingProgress };
        const bool pending{ !loading.mLoading.empty() || !loading.mQueued.empty() };
        const bool compressing{ !mStatusCompressionProgress.empty() };
        const bool workspace{ mWorkspaceOperation.IsBusy() };

        ImGui::PushTextWrapPos( 0.0f );
        ImGui::TextUnformatted( MKT_LOC( "editor_status_background_tasks" ).c_str() );

        if ( !pending && !compressing && !workspace ) {
            ImGui::TextDisabled( "%s", MKT_LOC( "editor_status_no_background_tasks" ).c_str() );
        }

        if ( workspace ) {
            ImGui::Separator();
            ImGui::TextUnformatted( mWorkspaceOperation.GetTitle().c_str() );
            ImGui::TextWrapped( "%s", mWorkspaceOperation.GetStage().c_str() );
        }

        if ( pending ) {
            ImGui::Separator();
            ImGui::Text( MKT_LOC( "editor_status_load_counts" ).c_str(), loading.mLoading.size(), loading.mQueued.size(),
                loading.mCompleted, loading.mTotal );

            const auto drawAssets{ [&]( const eastl::vector<asset::AssetID>& ids, const char* state ) {
                const usize limit{ expanded ? ids.size() : eastl::min<usize>( ids.size(), 3 ) };

                for ( usize index{}; index < eastl::min( ids.size(), limit ); ++index ) {
                    const auto metadata{ assets ? assets->FindMetadata( ids[index] ) : eastl::optional<asset::AssetMetadata>{} };
                    const eastl::string name{ metadata ? eastl::string{ metadata->GetPath().GetFilename() } : string::Format( "#{}", ids[index] ) };
                    ImGui::TextWrapped( "%s: %s", state, name.c_str() );
                }

                if ( ids.size() > limit ) {
                    ImGui::TextDisabled( MKT_LOC( "editor_status_more_tasks" ).c_str(), ids.size() - limit );
                }
            } };

            drawAssets( loading.mLoading, MKT_LOC( "editor_status_loading" ).c_str() );
            drawAssets( loading.mQueued, MKT_LOC( "editor_status_queued" ).c_str() );
        }

        if ( compressing ) {
            ImGui::SeparatorText( MKT_LOC( "editor_status_compressing" ).c_str() );

            const usize count{ expanded ? mStatusCompressionProgress.size() : eastl::min<usize>( mStatusCompressionProgress.size(), 3 ) };

            for ( usize index{}; index < count; ++index ) {
                const auto& texture{ mStatusCompressionProgress[index] };
                const eastl::string name{ texture.mPath.IsEmpty() ? eastl::string{ MKT_LOC( "editor_status_generated_texture" ) }
                    : eastl::string{ texture.mPath.GetFilename() } };
                const float fraction{ as<f32>( texture.mCompletedBlocks ) / as<f32>( texture.mTotalBlocks ) };
                const auto percent{ string::Format( "{}%", as<u32>( fraction * 100.0f ) ) };
                ImGui::TextWrapped( "%s", name.c_str() );
                ImGui::TextDisabled( "%s | %u x %u", texture.mFormat == asset::ImageFormat::eCompressed_BC6 ? "BC6H" : "BC7",
                    texture.mWidth, texture.mHeight );
                ImGui::ProgressBar( fraction, { -1.0f, 0.0f }, percent.c_str() );
            }

            if ( mStatusCompressionProgress.size() > count ) {
                ImGui::TextDisabled( MKT_LOC( "editor_status_more_tasks" ).c_str(), mStatusCompressionProgress.size() - count );
            }
        }

        ImGui::PopTextWrapPos();
    }

    auto EditorLayer::ResetPanelLayout() -> void {
        const ImGuiID dock{ ImGui::GetID( "MikotoDockEditor" ) };
        ImGui::DockBuilderRemoveNode( dock );
        ImGui::DockBuilderAddNode( dock, ImGuiDockNodeFlags_DockSpace );
        ImGui::DockBuilderSetNodeSize( dock, ImGui::GetContentRegionAvail() );
        ImGuiID center{ dock };
        const ImGuiID left{ ImGui::DockBuilderSplitNode( center, ImGuiDir_Left, 0.18f, nullptr, &center ) };
        const ImGuiID right{ ImGui::DockBuilderSplitNode( center, ImGuiDir_Right, 0.25f, nullptr, &center ) };
        const ImGuiID bottom{ ImGui::DockBuilderSplitNode( center, ImGuiDir_Down, 0.25f, nullptr, &center ) };

        for ( const auto& panel: mPanelRegistry | std::ranges::views::values ) {
            if ( panel->IsToggleable() ) {
                panel->SetVisible( false );
            }
        }

        const auto place{ []( Panel* panel, ImGuiID node ) {
            panel->SetVisible( true );
            ImGui::DockBuilderDockWindow( panel->GetHeaderName().data(), node );
        } };
        place( mPanelRegistry.Get<HierarchyPanel>(), left );
        place( mPanelRegistry.Get<ViewportPanel>(), center );
        place( mPanelRegistry.Get<GamePreviewPanel>(), center );
        place( mPanelRegistry.Get<InspectorPanel>(), right );
        place( mPanelRegistry.Get<RendererPanel>(), right );
        place( mPanelRegistry.Get<SettingsPanel>(), right );
        place( mPanelRegistry.Get<LayoutsPanel>(), right );
        place( mPanelRegistry.Get<ContentBrowserPanel>(), bottom );
        place( mPanelRegistry.Get<RuntimeConsolePanel>(), bottom );
        ImGui::DockBuilderFinish( dock );
    }

    auto EditorLayer::DrawMainMenuBar() -> void {
        MKT_BEGIN_PROFILER();

        imgui::ImGuiScopedStyleVar popupBorder{ ImGuiStyleVar_PopupBorderSize, 1.0f };
        imgui::ImGuiScopedStyleVar itemSpacing{ ImGuiStyleVar_ItemSpacing, ImVec2{ 10.0f, 8.0f } };
        imgui::ImGuiScopedStyleVar windowPadding{ ImGuiStyleVar_WindowPadding, ImVec2{ 14.0f, 14.0f } };

        // Compact menus use the theme's selection accent. Keep these overrides
        // local so panel widgets and the custom title-bar geometry are unaffected.
        // The menu bar must use the same vertical padding used to reserve its height
        // at Begin(). Popup row height is controlled by the compact ItemSpacing above.
        imgui::ImGuiScopedStyleVar menuPadding{ ImGuiStyleVar_FramePadding,
            ImVec2{ 4.0f, mWindow->HasCustomTitleBar() ? kTitleBarVerticalPadding : ImGui::GetStyle().FramePadding.y } };
        imgui::ImGuiScopedStyleVar menuRounding{ ImGuiStyleVar_PopupRounding, 4.0f };
        imgui::ImGuiScopedStyleVar menuItemRounding{ ImGuiStyleVar_MenuItemRounding, 4.0f };
        const ImVec4 selection{ ImGui::GetStyleColorVec4( ImGuiCol_HeaderActive ) };

        ImGui::PushStyleColor( ImGuiCol_Header, selection );
        ImGui::PushStyleColor( ImGuiCol_HeaderHovered, selection );
        ImGui::PushStyleColor( ImGuiCol_HeaderActive, selection );
        ImGui::PushStyleColor( ImGuiCol_Border, ImVec4{ 0.07f, 0.07f, 0.07f, 1.0f } );

        if ( ImGui::BeginMenuBar() ) {
            ImGui::PushStyleVar( ImGuiStyleVar_PopupBorderSize, 1.0f );
            DrawTitleBarIcon();

            if ( mEditorState->mSandboxMode ) {
                ImGui::TextUnformatted( MKT_LOC( "editor_layer_sandbox" ).c_str() );
            }

            ImGui::BeginDisabled( mEditorState->mRuntimeActive );

            DrawFileMenu();

            DrawEditMenu();

            DrawGameObjectMenu();

            DrawWindowMenu();

            DrawToolsMenu();

            DrawPlatformMenu();

            DrawHelpMenu();

            ImGui::EndDisabled();

            DrawTitleBarControls();
            
            ImGui::EndMenuBar();
            ImGui::PopStyleVar();
        }

        ImGui::PopStyleColor( 4 );
    }

    auto EditorLayer::DrawFileMenu() -> void {
        MKT_BEGIN_PROFILER();

        if ( EditorBeginMenu( MKT_LOC( "editor_layer_menu_file" ).c_str() ) ) {
            ImGui::BeginDisabled( !mEditorState->HasWorkspace() || mEditorState->mSandboxMode );

            if ( EditorMenuItem( MKT_LOC( "editor_layer_new_scene" ).c_str(), "Ctrl + N" ) ) {
                CreateScene();
            }

            if ( EditorMenuItem( MKT_LOC( "editor_layer_open_scene" ).c_str(), "Ctrl + L" ) ) {
                OpenScene();
            }

            if ( EditorMenuItem( MKT_LOC( "editor_layer_save_scene" ).c_str(), "Ctrl + S" ) ) {
                SaveScene();
            }

            if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_save_scene_as", "Save Scene As..." ).c_str(), "Ctrl+Shift+S" ) ) {
                SaveSceneAs();
            }

            ImGui::EndDisabled();

            ImGui::Separator();

            if ( EditorMenuItem( MKT_LOC( "editor_layer_new_project" ).c_str(), "Ctrl + P", false, !mEditorState->mSandboxMode ) ) {
                CreateProject();
            }

            if ( EditorMenuItem( MKT_LOC( "editor_layer_open_project" ).c_str(), "Ctrl + U", false, !mEditorState->mSandboxMode ) ) {
                OpenProject();
            }

            if ( EditorMenuItem( MKT_LOC( "world_selector_button" ).c_str(), nullptr, false,
                 !mEditorState->mRuntimeActive && !IsWorkspaceBusy() ) ) {
                mWorldSelector.Open();
            }

            DrawDefaultProjectsMenu();

            if ( EditorMenuItem( MKT_LOC( "editor_layer_save_project" ).c_str(), "Ctrl + G", false, mEditorState->mHasProject && !mEditorState->mSandboxMode ) ) {
                SaveProject();
            }

            ImGui::Separator();

            // Screen mode
            const auto& screenMode{ MKT_LOC(
                mWindow->IsMaximized() ? "editor_layer_windowed" : "editor_layer_fullscreen" ) };

            if ( EditorMenuItem( screenMode.c_str(), "Windows + H" ) ) {
                try {
                    if ( !mWindow->IsScreenMode( platform::ScreenMode::eBorderless ) ) {
                        mWindow->SetScreenMode( platform::ScreenMode::eBorderless );
                    }
                    else {
                        mWindow->SetScreenMode( platform::ScreenMode::eWindowed );
                    }
                } catch ( const std::exception& e ) {
                    MKT_CORE_LOGGER_ERROR( "Error while changing screen modes. e.what() {}", e.what() );
                }
            }

            ImGui::Separator();

            if ( EditorMenuItem( MKT_LOC( "editor_layer_menu_close" ).c_str(), nullptr, false ) ) {
                platform::EventSystem::Get()->Queue<platform::WindowCloseEvent>();
            }

            ImGui::EndMenu();
        }
    }

    auto EditorLayer::DrawDefaultProjectsMenu() -> void {
        MKT_BEGIN_PROFILER();

        if ( EditorBeginMenu( imgui::widget::LocalizedLabel( "editor_layer_default_projects", "Default Projects" ).c_str() ) ) {
            for ( const auto& path: WorldSelector::FindDemoProjects() ) {
                const eastl::string name{ path.GetStem() };

                if ( EditorMenuItem( name.c_str() ) ) {
                    OpenProject( path );
                }
            }

            ImGui::EndMenu();
        }
    }

    auto EditorLayer::DrawEditMenu() -> void {
        MKT_BEGIN_PROFILER();

        if ( EditorBeginMenu( MKT_LOC( "editor_layer_menu_edit" ).c_str(), mEditorState->HasWorkspace() ) ) {
            if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_undo", "Undo" ).c_str(), "Ctrl+Z" ) ) { 
                // TODO
            }

            if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_redo", "Redo" ).c_str(), "Ctrl+Y" ) ) {
                // TODO
            }

            ImGui::Separator();

            if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_cut", "Cut" ).c_str(), "Ctrl+X" ) ) {
                // TODO
            }

            if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_copy", "Copy" ).c_str(), "Ctrl+C" ) ) {
                // TODO
            }

            if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_paste", "Paste" ).c_str(), "Ctrl+V" ) ) {
                // TODO
            }

            if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_duplicate", "Duplicate" ).c_str(), "Ctrl+D" ) ) {
                // TODO
            }

            if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_delete", "Delete" ).c_str(), MKT_LOC( "editor_layer_delete" ).c_str() ) ) {
                // TODO
            }

            ImGui::Separator();
            if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_select_all", "Select All" ).c_str(), "Ctrl+A" ) ) {
                // TODO
            }

            if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_preferences", "Preferences..." ).c_str() ) ) {
                mPanelRegistry.Get<PreferencesPanel>()->SetVisible( true );
            }

            ImGui::EndMenu();
        }
    }

    auto EditorLayer::DrawGameObjectMenu() -> void {
        MKT_BEGIN_PROFILER();

        if ( EditorBeginMenu( MKT_LOC( "editor_layer_menu_game_object" ).c_str(), mEditorState->HasWorkspace() ) ) {
            if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_create_empty", "Create Empty" ).c_str(), "Ctrl+Shift+N" ) ) {
                scene::EntityCreateInfo createInfo{
                    .mRoot = nullptr,
                    .mName = "Empty Object"
                };
                mEditorState->mActiveScene->PushEntity( createInfo );
            }

            ImGui::Separator();

            if ( EditorBeginMenu( imgui::widget::LocalizedLabel( "editor_layer_3d_object", "3D Object" ).c_str() ) ) {
                if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_cube", "Cube" ).c_str() ) ) {
                    const scene::EntityCreateInfo entityCreateInfo{
                        .mRoot = nullptr,
                            .mName{ "Cube" },
                        .mModelAsset = mEditorState->GetPrefabReference( PrefabModelType::eCube )
                    };
                    mEditorState->mActiveScene->PushEntity( entityCreateInfo );
                }

                if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_sphere", "Sphere" ).c_str() ) ) {
                    const scene::EntityCreateInfo entityCreateInfo{
                        .mRoot = nullptr,
                            .mName{ "Sphere" },
                        .mModelAsset = mEditorState->GetPrefabReference( PrefabModelType::eSphere )
                    };
                    mEditorState->mActiveScene->PushEntity( entityCreateInfo );
                }

                if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_capsule", "Capsule" ).c_str() ) ) {
                    const scene::EntityCreateInfo entityCreateInfo{
                        .mRoot = nullptr,
                            .mName{ "Capsule" },
                        .mModelAsset = mEditorState->GetPrefabReference( PrefabModelType::eSphere )
                    };
                    mEditorState->mActiveScene->PushEntity( entityCreateInfo );
                }

                if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_plane", "Plane" ).c_str() ) ) {
                    const scene::EntityCreateInfo entityCreateInfo{
                        .mRoot = nullptr,
                            .mName{ "Plane" },
                        .mModelAsset = mEditorState->GetPrefabReference( PrefabModelType::eCube )
                    };
                    mEditorState->mActiveScene->PushEntity( entityCreateInfo );
                }

                if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_cylinder", "Cylinder" ).c_str() ) ) {
                    const scene::EntityCreateInfo entityCreateInfo{
                        .mRoot = nullptr,
                            .mName{ "Cylinder" },
                        .mModelAsset = mEditorState->GetPrefabReference( PrefabModelType::eCylinder )
                    };
                    mEditorState->mActiveScene->PushEntity( entityCreateInfo );
                }

                if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_cone", "Cone" ).c_str() ) ) {
                    const scene::EntityCreateInfo entityCreateInfo{
                        .mRoot = nullptr,
                            .mName{ "Cone" },
                        .mModelAsset = mEditorState->GetPrefabReference( PrefabModelType::eCone )
                    };
                    mEditorState->mActiveScene->PushEntity( entityCreateInfo );
                }

                ImGui::EndMenu();
            }

            if ( EditorBeginMenu( imgui::widget::LocalizedLabel( "editor_layer_2d_object", "2D Object" ).c_str() ) ) {
                if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_sprite", "Sprite" ).c_str() ) ) { /* Spawn Sprite */
                }

                if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_tilemap", "Tilemap" ).c_str() ) ) { /* Spawn Tilemap */
                }

                ImGui::EndMenu();
            }

            if ( EditorBeginMenu( imgui::widget::LocalizedLabel( "editor_layer_effects", "Effects" ).c_str() ) ) {
                if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_particle_system", "Particle System" ).c_str() ) ) { /* Spawn Particles */
                }

                if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_line", "Line" ).c_str() ) ) { /* Spawn Line Renderer */
                }

                ImGui::EndMenu();
            }

            if ( EditorBeginMenu( imgui::widget::LocalizedLabel( "editor_layer_light", "Light" ).c_str() ) ) {
                if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_directional_light", "Directional Light" ).c_str() ) ) { /* Spawn Directional Light */
                }

                if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_point_light", "Point Light" ).c_str() ) ) { /* Spawn Point Light */
                }

                if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_spot_light", "Spot Light" ).c_str() ) ) { /* Spawn Spot Light */
                }

                ImGui::EndMenu();
            }

            if ( EditorBeginMenu( imgui::widget::LocalizedLabel( "editor_layer_audio", "Audio" ).c_str() ) ) {
                if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_audio_source", "Audio Source" ).c_str() ) ) { /* Spawn Audio Source */
                }

                ImGui::EndMenu();
            }

            if ( EditorBeginMenu( "UI" ) ) {
                if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_canvas", "Canvas" ).c_str() ) ) { /* Spawn Canvas */
                }

                if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_text", "Text" ).c_str() ) ) { /* Spawn Text */
                }

                if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_button", "Button" ).c_str() ) ) { /* Spawn Button */
                }

                if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_image", "Image" ).c_str() ) ) { /* Spawn Image */
                }

                ImGui::EndMenu();
            }

            ImGui::Separator();

            if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_camera", "Camera" ).c_str() ) ) { /* Spawn Camera */
            }

            ImGui::EndMenu();
        }
    }

    auto EditorLayer::DrawWindowMenu() -> void {
        MKT_BEGIN_PROFILER();

        if ( EditorBeginMenu( MKT_LOC( "editor_layer_menu_window" ).c_str() ) ) {
            if ( EditorBeginMenu( MKT_LOC( "editor_layer_menu_panels" ).c_str() ) ) {
                static constexpr std::array<std::pair<eastl::string_view, core::cstr>, 9> groupTitles{ { { "Animation", "panel_group_animation" },
                    { "Assets", "panel_group_assets" },
                    { "Audio", "panel_group_audio" },
                    { "Diagnostics", "panel_group_diagnostics" },
                    { "Rendering", "panel_group_rendering" },
                    { "Scene", "panel_group_scene" },
                    { "Scripting", "panel_group_scripting" },
                    { "Settings", "panel_group_settings" },
                    { "Other", "panel_group_other" } } };
                const auto panelGroup{ []( const Panel& panel ) -> eastl::string_view {
                    return panel.GetGroup().empty() ? eastl::string_view{ "Other" } : eastl::string_view{ panel.GetGroup() };
                } };
                eastl::vector<eastl::string_view> groups{};

                for ( const auto& panel: mPanelRegistry | std::ranges::views::values ) {
                    if ( !panel->IsToggleable() ) {
                        continue;
                    }

                    const auto group{ panelGroup( *panel ) };

                    if ( eastl::find( groups.begin(), groups.end(), group ) == groups.end() ) {
                        groups.push_back( group );
                    }
                }

                eastl::sort( groups.begin(), groups.end() );

                for ( const auto group: groups ) {
                    eastl::string label{ group };

                    for ( const auto& [name, key]: groupTitles ) {
                        if ( group == name ) {
                            label = MKT_LOC( key );
                            break;
                        }
                    }

                    label += "###PanelGroup:";
                    label.append( group.data(), group.size() );

                    if ( EditorBeginMenu( label.c_str() ) ) {
                        for ( const auto& panel: mPanelRegistry | std::ranges::views::values ) {
                            if ( !panel->IsToggleable() || panelGroup( *panel ) != group ) {
                                continue;
                            }

                            ImGui::PushID( panel.get() );
                            bool isActive{ panel->IsVisible() };

                            if ( EditorMenuItem( panel->GetDisplayName().data(), nullptr, std::addressof( isActive ) ) ) {
                                panel->SetVisible( isActive );
                            }

                            ImGui::PopID();
                        }

                        ImGui::EndMenu();
                    }
                }

                ImGui::EndMenu();
            }

            if ( EditorBeginMenu( MKT_LOC( "editor_layer_menu_language" ).c_str() ) ) {
                static constexpr std::array languages{
                    core::ISOLanguage::EN_US,
                    core::ISOLanguage::EN_GB,
                    core::ISOLanguage::ES_ES,
                    core::ISOLanguage::JA_JP,
                    core::ISOLanguage::ZH_CN
                };
                const core::ISOLanguage current{ LocalizationService::Get()->GetCurrentLanguage() };

                for ( const core::ISOLanguage lang: languages ) {
                    const bool isSelected{ ( lang == current ) };

                    if ( EditorMenuItem( GetIsoName( lang ).data(), nullptr, isSelected ) ) {
                        LocalizationService::Get()->SetLanguage( lang );
                    }
                }

                ImGui::EndMenu();
            }


            ImGui::EndMenu();
        }
    }

    auto EditorLayer::DrawToolsMenu() -> void {
        MKT_BEGIN_PROFILER();

        if ( EditorBeginMenu( MKT_LOC( "editor_layer_menu_tools" ).c_str(), mEditorState->HasWorkspace() ) ) {
            DrawPhysicsMenu();

            DrawAudioMenu();

            DrawGraphicsMenu();

            ImGui::Separator();

            if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_profiler", "Profiler" ).c_str(), "Ctrl+7" ) ) {
                mPanelRegistry.Get<StatsPanel>()->SetVisible( true );
                mPanelRegistry.Get<GPUStatsPanel>()->SetVisible( true );
            }

            if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_console", "Console" ).c_str(), "Ctrl+Shift+C" ) ) {
                mPanelRegistry.Get<RuntimeConsolePanel>()->FocusCommandInput();
            }

            if ( auto* panel{ mPanelRegistry.Get<GamePreviewPanel>() } ) {
                bool visible{ panel->IsVisible() };

                if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_game", "Game" ).c_str(), nullptr, &visible ) ) {
                    panel->SetVisible( visible );
                }
            }

            if ( auto* panel = mPanelRegistry.Get<PassVisualizerPanel>() ) {
                bool visible{ panel->IsVisible() };

                if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_pass_visualizer", "Pass Visualizer" ).c_str(), nullptr, &visible ) ) {
                    panel->SetVisible( visible );
                }
            }

            if ( auto* panel{ mPanelRegistry.Get<AnimationGraphPanel>() } ) {
                bool visible{ panel->IsVisible() };

                if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_animation_graph", "Animation Graph" ).c_str(), nullptr, &visible ) ) {
                    panel->SetVisible( visible );
                }
            }

            if ( auto* panel{ mPanelRegistry.Get<ScriptGraphPanel>() } ) {
                bool visible{ panel->IsVisible() };

                if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_script_graph", "Script Graph" ).c_str(), nullptr, &visible ) ) {
                    panel->SetVisible( visible );
                }
            }

            ImGui::EndMenu();
        }
    }

    auto EditorLayer::DrawPhysicsMenu() -> void {
        MKT_BEGIN_PROFILER();

        if ( EditorBeginMenu( imgui::widget::LocalizedLabel( "editor_layer_physics", "Physics" ).c_str() ) ) {
            auto& debug{ mEditorRenderer->GetPhysicsDebugSettings() };
            const bool available{ mEditorState->mPhysicsDebugRendererSimple != nullptr };
            ImGui::BeginDisabled( !available );
            EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_show_physics_debug", "Show Physics Debug" ).c_str(), nullptr, &debug.mEnabled );
            ImGui::Separator();
            EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_collider_wireframes", "Collider Wireframes" ).c_str(), nullptr, &debug.mShapes );
            EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_bounding_boxes", "Bounding Boxes" ).c_str(), nullptr, &debug.mBoundingBoxes );
            EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_body_axes", "Body Axes" ).c_str(), nullptr, &debug.mBodyAxes );
            EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_center_of_mass", "Center of Mass" ).c_str(), nullptr, &debug.mCenterOfMass );
            EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_velocity_vectors", "Velocity Vectors" ).c_str(), nullptr, &debug.mVelocities );
            EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_constraints", "Constraints" ).c_str(), nullptr, &debug.mConstraints );
            EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_constraint_limits", "Constraint Limits" ).c_str(), nullptr, &debug.mConstraintLimits );
            EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_constraint_frames", "Constraint Frames" ).c_str(), nullptr, &debug.mConstraintFrames );
            ImGui::Separator();
            EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_show_through_geometry", "Show Through Geometry" ).c_str(), nullptr, &debug.mShowThroughGeometry );
            ImGui::TextDisabled( MKT_LOC( "editor_layer_lines" ).c_str(), mEditorRenderer->GetPhysicsDebugLineCount() );

            if ( const auto dropped{ mEditorRenderer->GetDroppedPhysicsDebugLineCount() }; dropped > 0 ) {
                ImGui::TextDisabled( MKT_LOC( "editor_layer_line_limit_reached_omitted" ).c_str(), dropped );
            }

            ImGui::TextDisabled( MKT_LOC( "editor_layer_static_gray_dynamic_yellow_sleeping_red" ).c_str() );
            ImGui::TextDisabled( MKT_LOC( "editor_layer_kinematic_green" ).c_str() );
            ImGui::EndDisabled();

            if ( !available ) {
                ImGui::TextDisabled( MKT_LOC( "editor_layer_physics_debug_drawing_requires_a_debug_build" ).c_str() );
            }

            ImGui::Separator();

            if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_simulation_settings", "Simulation Settings" ).c_str() ) ) {
                mPanelRegistry.Get<SimulationSettingsPanel>()->SetVisible( true );
            }

            ImGui::EndMenu();
        }
    }

    auto EditorLayer::DrawAudioMenu() -> void {
        MKT_BEGIN_PROFILER();

        if ( EditorBeginMenu( imgui::widget::LocalizedLabel( "editor_layer_audio", "Audio" ).c_str() ) ) {
            if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_audio_mixer", "Audio Mixer" ).c_str() ) ) {
                mPanelRegistry.Get<AudioMixerPanel>()->SetVisible( true );
            }

            if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_profiler", "Profiler" ).c_str() ) ) {
                mPanelRegistry.Get<AudioProfilerPanel>()->SetVisible( true );
            }

            ImGui::EndMenu();
        }
    }

    auto EditorLayer::DrawGraphicsMenu() -> void {
        MKT_BEGIN_PROFILER();

        if ( EditorBeginMenu( imgui::widget::LocalizedLabel( "editor_layer_graphics", "Graphics" ).c_str() ) ) {
            if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_render_pipeline_settings", "Render Pipeline Settings" ).c_str() ) ) {
                mPanelRegistry.Get<RendererPanel>()->SetVisible( true );
            }

            if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_compiler_status_c_shaders", "Compiler Status (C# / Shaders)" ).c_str() ) ) {
                mPanelRegistry.Get<CompilerStatusPanel>()->SetVisible( true );
            }

            if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_display_grid", "Display Grid" ).c_str(), nullptr, mEditorState->mViewSettings.mShowGrid ) ) {
                mEditorState->mViewSettings.mShowGrid = !mEditorState->mViewSettings.mShowGrid;
            }

            if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_display_imgui_debug_info", "Display ImGui Debug Info" ).c_str(), "Ctrl+I", mShowImGuiDebugInfo ) ) {
                mShowImGuiDebugInfo = !mShowImGuiDebugInfo;
            }

            if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_display_polygon_complexity", "Display Polygon Complexity" ).c_str(), nullptr, mShowPolygonComplexity ) ) {
                mShowPolygonComplexity = !mShowPolygonComplexity;
            }

            if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_display_performance_overlay", "Display Performance Overlay" ).c_str(), nullptr, mShowPerformanceOverlay ) ) {
                mShowPerformanceOverlay = !mShowPerformanceOverlay;

                ViewportPanel* panel{ mPanelRegistry.Get<ViewportPanel>() };
                panel->SetEnablePerformanceOverlay( mShowPerformanceOverlay );
            }

            DrawRenderOutputMenu();
            ImGui::EndMenu();
        }
    }

    auto EditorLayer::DrawRenderOutputMenu() -> void {
        MKT_BEGIN_PROFILER();

        if ( EditorBeginMenu( imgui::widget::LocalizedLabel( "editor_layer_output", "Output" ).c_str(), mEditorRenderer->HasFrameGraph() ) ) {
            if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_ssao", "SSAO" ).c_str() ) ) {
                mEditorState->mFinalComposition = mEditorRenderer->GetFinalImage( renderer::FinalImageType::eSsao );
            }

            ImGui::BeginDisabled( !mDevice->SupportsRayTracing() );
            if ( EditorMenuItem( imgui::widget::LocalizedLabel( "renderer_ray_traced_ao", "Ray-traced AO (PBRT)" ).c_str() ) ) {
                mEditorState->mFinalComposition = mEditorRenderer->GetFinalImage( renderer::FinalImageType::eRayTracedAmbientOcclusion );
            }
            ImGui::EndDisabled();

            if ( EditorMenuItem( imgui::widget::LocalizedLabel( "post_process_debug_before", "Post-process before tone mapping" ).c_str() ) ) {
                mEditorState->mFinalComposition = mEditorRenderer->GetFinalImage( renderer::FinalImageType::ePostProcessBeforeToneMapping );
            }

            if ( EditorMenuItem( imgui::widget::LocalizedLabel( "post_process_debug_after", "Post-process after tone mapping" ).c_str() ) ) {
                mEditorState->mFinalComposition = mEditorRenderer->GetFinalImage( renderer::FinalImageType::ePostProcessAfterToneMapping );
            }

            if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_color", "Color" ).c_str() ) ) {
                mEditorState->mFinalComposition = mEditorRenderer->GetFinalImage( renderer::FinalImageType::eGBuffer_Color );
            }

            if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_emission", "Emission" ).c_str() ) ) {
                mEditorState->mFinalComposition = mEditorRenderer->GetFinalImage( renderer::FinalImageType::eGBuffer_Emissive );
            }

            if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_normals", "Normals" ).c_str() ) ) {
                mEditorState->mFinalComposition = mEditorRenderer->GetFinalImage( renderer::FinalImageType::eGBuffer_Normals );
            }

            if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_positions", "Positions" ).c_str() ) ) {
                mEditorState->mFinalComposition = mEditorRenderer->GetFinalImage( renderer::FinalImageType::eGBuffer_Position );
            }

            if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_wireframe", "Wireframe" ).c_str() ) ) {
                mEditorState->mFinalComposition = mEditorRenderer->GetFinalImage( renderer::FinalImageType::eWireframe );
            }

            if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_depth", "Depth" ).c_str() ) ) {
                mEditorState->mFinalComposition = mEditorRenderer->GetFinalImage( renderer::FinalImageType::eDepthPrepass );
            }

            if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_radiance", "Radiance" ).c_str() ) ) {
                mEditorState->mFinalComposition = mEditorRenderer->GetFinalImage( renderer::FinalImageType::ePBRadiance_Output );
            }

            if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_default", "Default" ).c_str() ) ) {
                mEditorState->mFinalComposition = mEditorRenderer->GetFinalImage( renderer::FinalImageType::eTonemap_Output );
            }

            ImGui::Separator();

            if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_light_clusters_heatmap", "Light clusters: Heatmap" ).c_str() ) ) {
                mEditorState->mFinalComposition = mEditorRenderer->GetFinalImage( renderer::FinalImageType::eClusterHeatmap );
            }

            mikoto::imgui::widget::ItemTooltip( MKT_LOC( "editor_layer_lights_per_cluster_at_the_visible_surface_blue_0_green" ).c_str() );

            if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_light_clusters_overlay", "Light clusters: Overlay" ).c_str() ) ) {
                mEditorState->mFinalComposition = mEditorRenderer->GetFinalImage( renderer::FinalImageType::eClusterHeatmapOverlay );
            }

            mikoto::imgui::widget::ItemTooltip( MKT_LOC( "editor_layer_blend_the_cluster_heatmap_over_the_final_image_uses_opaque" ).c_str() );
            ImGui::EndMenu();
        }
    }

    auto EditorLayer::DrawPlatformMenu() -> void {
        MKT_BEGIN_PROFILER();

        if ( EditorBeginMenu( imgui::widget::LocalizedLabel( "platform_title" ).c_str() ) ) {
            if ( EditorMenuItem( imgui::widget::LocalizedLabel( "platform_settings" ).c_str() ) ) {
                mPanelRegistry.Get<PreferencesPanel>()->OpenPlatformSettings();
            }

            // Reserve build actions without implying that packaging or deployment is available.
            ImGui::Separator();
            EditorMenuItem( imgui::widget::LocalizedLabel( "platform_build" ).c_str(), nullptr, false, false );
            EditorMenuItem( imgui::widget::LocalizedLabel( "platform_package" ).c_str(), nullptr, false, false );
            EditorMenuItem( imgui::widget::LocalizedLabel( "platform_deploy" ).c_str(), nullptr, false, false );
            ImGui::Separator();
            ImGui::TextDisabled( "%s", MKT_LOC( "platform_coming_soon" ).c_str() );
            ImGui::EndMenu();
        }
    }

    auto EditorLayer::DrawHelpMenu() -> void {
        MKT_BEGIN_PROFILER();

        if ( EditorBeginMenu( MKT_LOC( "editor_layer_menu_help" ).c_str() ) ) {
            if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_about_mikoto", "About Mikoto" ).c_str() ) ) {
                mPanelRegistry.Get<AboutPanel>()->SetVisible( true );
            }

            ImGui::Separator();

            if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_documentation", "Documentation" ).c_str() ) ) {
                filesystem::OpenInBrowser( "https://katebea.github.io/Mikoto-Website/" );
            }

            if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_scripting_reference", "Scripting Reference" ).c_str() ) ) {
                filesystem::OpenInBrowser( "https://github.com/kateBea/Mikoto/tree/develop/Mikoto/Scripting" );
            }

            ImGui::Separator();

            if ( EditorBeginMenu( imgui::widget::LocalizedLabel( "editor_layer_support", "Support" ).c_str() ) ) {
                if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_report_a_bug", "Report a Bug" ).c_str() ) ) {
                    filesystem::OpenInBrowser( "https://github.com/kateBea/Mikoto/issues/new" );
                }

                ImGui::EndMenu();
            }

            ImGui::Separator();

            if ( EditorMenuItem( imgui::widget::LocalizedLabel( "editor_layer_check_for_updates", "Check for Updates" ).c_str() ) ) {
                filesystem::OpenInBrowser( "https://github.com/kateBea/Mikoto/releases" );
            }

            ImGui::EndMenu();
        }
    }

    auto EditorLayer::DrawTitleBarIcon() -> void {
        if ( !mWindow->HasCustomTitleBar() ) {
            return;
        }

        // Draw Gradient
        const float height{ ImGui::GetFontSize() + kTitleBarVerticalPadding * 2.0f };
        const float size{ height - 6.0f };
        constexpr float sidePadding{ 12.0f };
        const ImVec2 origin{ ImGui::GetWindowPos() };
        const ImVec2 min{ origin.x + sidePadding, origin.y + ( height - size ) * 0.5f };
        auto* draw{ ImGui::GetWindowDrawList() };
        DrawHeaderGlow( origin, ImGui::GetWindowSize().x, height, mEditorState->mHeaderGradientColor );

        if ( mTitleBarIcon ) {
            draw->AddImage( imgui::ImGuiService::Get()->GetTextureID( mTitleBarIcon ), min, { min.x + size, min.y + size } );
        }
        else {
            const ImVec2 textSize{ ImGui::CalcTextSize( "M" ) };
            draw->AddText( { min.x + ( size - textSize.x ) * 0.5f, origin.y + ( height - textSize.y ) * 0.5f },
                ImGui::GetColorU32( ImGuiCol_Text ), "M" );
        }
        // Reserve horizontal space without changing the menu's text baseline.
        ImGui::Dummy( { size + sidePadding * 2.0f, 0.0f } );
    }

    auto EditorLayer::DrawTitleBarControls() -> void {
        if ( !mWindow->HasCustomTitleBar() ) {
            return;
        }

        const auto* viewport{ ImGui::GetMainViewport() };
        const ImVec2 origin{ ImGui::GetWindowPos() };
        const float height{ ImGui::GetFontSize() + kTitleBarVerticalPadding * 2.0f };
        const float buttonHeight{ ImGui::GetFontSize() + 12.0f };
        const float buttonWidth{ buttonHeight * 1.25f };
        const float controlsX{ origin.x + ImGui::GetWindowSize().x - buttonWidth * 3.0f };
        const float dragStart{ ImGui::GetCursorScreenPos().x + 12.0f };
        const float dragWidth{ glm::max( 0.0f, controlsX - dragStart ) };
        const bool blocked{ ImGui::IsPopupOpen( nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel ) };

        mWindow->SetTitleBarDragArea(
            ( dragStart - viewport->Pos.x ) / viewport->Size.x,
            ( origin.y - viewport->Pos.y ) / viewport->Size.y,
            blocked ? 0.0f : dragWidth / viewport->Size.x, height / viewport->Size.y );

        auto* draw{ ImGui::GetWindowDrawList() };
#if defined( NDEBUG )
        constexpr auto buildType{ "RELEASE" };
#else
        constexpr auto buildType{ "DEBUG" };
#endif
        // Sample the caption twice a second so the FPS readout stays readable.
        if ( mTitleBarText.empty() || ImGui::GetTime() >= mNextTitleBarRefresh ) {
            mTitleBarText = string::Format( "Mikoto [{}] | FPS: {:.0f}", buildType, ImGui::GetIO().Framerate );
            mNextTitleBarRefresh = ImGui::GetTime() + 0.5;
        }

        const ImVec2 titleSize{ ImGui::CalcTextSize( mTitleBarText.c_str() ) };
        const float titleX{ origin.x + ( ImGui::GetWindowSize().x - titleSize.x ) * 0.5f };
        const bool titleFits{ titleX >= dragStart && titleX + titleSize.x + 12.0f <= controlsX };

        if ( titleFits ) {
            draw->AddText( { titleX, origin.y + ( height - titleSize.y ) * 0.5f },
                ImGui::GetColorU32( ImGuiCol_Text ), mTitleBarText.c_str() );
        }

        const eastl::string projectName{ mProject ? eastl::string{ mProject->GetName() } : mEditorState->mSandboxMode ? MKT_LOC( "editor_layer_sandbox" )
                                                                                                                      : eastl::string{} };

        if ( !projectName.empty() ) {
            const ImVec2 textSize{ ImGui::CalcTextSize( projectName.c_str() ) };
            const float badgeRight{ controlsX - 88.0f };
            const float badgeLeftLimit{ titleFits ? titleX + titleSize.x + 24.0f : dragStart };
            const float badgeWidth{ glm::min( glm::min( textSize.x + 56.0f, 248.0f ), badgeRight - badgeLeftLimit ) };

            if ( badgeWidth >= 64.0f ) {
                // Attach the project outline to the top edge of the title bar.
                const ImVec2 min{ badgeRight - badgeWidth, origin.y };
                const ImVec2 max{ badgeRight, origin.y + height - 4.0f };
                const ImVec4 background{ ImGui::GetStyleColorVec4( ImGuiCol_MenuBarBg ) };
                const ImU32 outline{ ImGui::GetColorU32( ImVec4{ background.x * 0.8f, background.y * 0.8f, background.z * 0.8f, background.w } ) };
                draw->AddRect( min, max, outline, 4.0f, ImDrawFlags_RoundCornersBottom, 1.0f );
                draw->PushClipRect( { min.x + 10.0f, min.y }, { max.x - 10.0f, max.y }, true );
                draw->AddText( { min.x + glm::max( 10.0f, ( badgeWidth - textSize.x ) * 0.5f ), origin.y + ( height - textSize.y ) * 0.5f },
                    ImGui::GetColorU32( ImGuiCol_Text ), projectName.c_str() );
                draw->PopClipRect();
            }
        }

        const std::array labels{ MKT_LOC( "editor_layer_minimize" ).c_str(), MKT_LOC( "editor_layer_maximize_restore" ).c_str(), MKT_LOC( "editor_layer_close_editor" ).c_str() };

        for ( int index{}; index < 3; ++index ) {
            const ImVec2 min{ controlsX + buttonWidth * index, origin.y };
            const ImVec2 max{ min.x + buttonWidth, min.y + buttonHeight };
            ImGui::SetCursorScreenPos( min );
            const bool clicked{ ImGui::InvisibleButton( labels[index], { buttonWidth, buttonHeight } ) };
            const bool hovered{ ImGui::IsItemHovered() };

            if ( hovered ) {
                draw->AddRectFilled( min, max, index == 2 ? IM_COL32( 190, 45, 55, 255 ) : ImGui::GetColorU32( ImGuiCol_ButtonHovered ) );
                mikoto::imgui::widget::ItemTooltip( "%s", labels[index] );
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            const ImU32 color{ hovered && index == 2 ? IM_COL32( 255, 255, 255, 255 ) : ImGui::GetColorU32( ImGuiCol_Text ) };
            const ImVec2 center{ ( min.x + max.x ) * 0.5f, ( min.y + max.y ) * 0.5f };
            const float radius{ ImGui::GetFontSize() * 0.28f };

            if ( index == 0 ) {
                draw->AddLine( { center.x - radius, center.y }, { center.x + radius, center.y }, color, 1.4f );
            }
            else if ( index == 1 ) {
                if ( mWindow->IsMaximized() ) {
                    draw->AddRect( { center.x - radius + 3, center.y - radius - 3 }, { center.x + radius + 3, center.y + radius - 3 }, color );
                }

                draw->AddRect( { center.x - radius, center.y - radius }, { center.x + radius, center.y + radius }, color );
            }
            else {
                draw->AddLine( { center.x - radius, center.y - radius }, { center.x + radius, center.y + radius }, color, 1.4f );
                draw->AddLine( { center.x - radius, center.y + radius }, { center.x + radius, center.y - radius }, color, 1.4f );
            }

            if ( clicked ) {
                if ( index == 0 ) {
                    mWindow->Minimize();
                }
                else if ( index == 1 ) {
                    mWindow->ToggleMaximized();
                }
                else {
                    mWindow->RequestClose();
                }
            }
        }
    }

    auto EditorLayer::DrawUtilityBar() -> void {
        const float height{ UtilityBarHeight() };

        const bool compact{ ImGui::GetContentRegionAvail().x < 900.0f };
        ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, { 12.0f, 6.0f } );
        ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, { 10.0f, 5.0f } );
        ImGui::PushStyleVar( ImGuiStyleVar_FrameRounding, 3.0f );
        ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, { 3.0f, 4.0f } );
        ImGui::PushStyleColor( ImGuiCol_ChildBg, ImGui::GetStyleColorVec4( ImGuiCol_WindowBg ) );

        if ( ImGui::BeginChild( "EditorUtilities", { 0.0f, height }, ImGuiChildFlags_AlwaysUseWindowPadding,
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_HorizontalScrollbar ) ) {
            auto* draw{ ImGui::GetWindowDrawList() };
            const ImVec2 position{ ImGui::GetWindowPos() };
            const ImVec2 size{ ImGui::GetWindowSize() };
            
            draw->AddLine( position, { position.x + size.x, position.y }, ImGui::GetColorU32( ImGuiCol_Border ) );

            if ( !mEditorState->HasWorkspace() ) {
                draw->AddLine( { position.x, position.y + size.y - 1.0f },
                    { position.x + size.x, position.y + size.y - 1.0f }, ImGui::GetColorU32( ImGuiCol_Border ) );
            }

            const auto button{ [compact]( const char* icon, const char* id, const char* labelKey, const char* tooltip ) {
                const char* label{ MKT_LOC( labelKey ).c_str() };
                const auto text{ string::Format( "{}{}{}##{}", icon, compact ? "" : "  ", compact ? "" : label, id ) };
                const bool clicked{ imgui::widget::ToolbarButton( id, text.c_str(), tooltip ) };
                ImGui::SameLine();
                return clicked;
            } };
            const auto divider{ []() {
                const ImVec2 pos{ ImGui::GetCursorScreenPos() };
                const float size{ ImGui::GetFrameHeight() };
                ImGui::GetWindowDrawList()->AddLine( { pos.x + 6.0f, pos.y + 3.0f },
                    { pos.x + 6.0f, pos.y + size - 3.0f }, ImGui::GetColorU32( ImGuiCol_Separator ) );
                ImGui::Dummy( { 12.0f, size } );
                ImGui::SameLine();
            } };

            const bool hasScene{ mEditorState->HasWorkspace() && mEditorState->mActiveScene };
            const bool running{ mEditorState->mRuntimeActive };
            const bool simulating{ hasScene && mEditorState->mActiveScene->IsSimulating() };
            const auto& toolbar{ mEditorState->mToolbarSettings };
            ImGui::BeginDisabled( !hasScene || running || simulating );

            if ( toolbar.mSave && button( ICON_MD_SAVE, "Save", "editor_layer_save", MKT_LOC( "editor_layer_save_scene_ctrl_s" ).c_str() ) ) {
                SaveScene();
            }

            if ( toolbar.mOpen && button( ICON_MD_FOLDER_OPEN, "Open", "editor_layer_open", MKT_LOC( "editor_layer_open_scene_ctrl_l" ).c_str() ) ) {
                OpenScene();
            }

            ImGui::EndDisabled();

            if ( ( toolbar.mSave || toolbar.mOpen ) && ( toolbar.mGame || toolbar.mRenderer || toolbar.mStats || toolbar.mPreferences ) ) {
                divider();
            }

            if ( toolbar.mGame && button( ICON_MD_VIDEOGAME_ASSET, "Game", "editor_layer_game", MKT_LOC( "editor_layer_show_the_main_camera_game_preview" ).c_str() ) ) {
                mPanelRegistry.Get<GamePreviewPanel>()->SetVisible( true );
            }

            if ( toolbar.mRenderer && button( ICON_MD_TUNE, "Renderer", "editor_layer_renderer", MKT_LOC( "editor_layer_open_renderer_settings" ).c_str() ) ) {
                mPanelRegistry.Get<RendererPanel>()->SetVisible( true );
            }

            if ( toolbar.mStats && button( ICON_MD_BAR_CHART, "Stats", "editor_layer_stats", MKT_LOC( "editor_layer_show_performance_statistics" ).c_str() ) ) {
                mPanelRegistry.Get<StatsPanel>()->SetVisible( true );
            }

            if ( toolbar.mPreferences && button( ICON_MD_SETTINGS, "Preferences", "editor_layer_preferences", MKT_LOC( "editor_layer_open_editor_preferences" ).c_str() ) ) {
                mPanelRegistry.Get<PreferencesPanel>()->SetVisible( true );
            }

            if ( mEditorState->HasWorkspace() && ( toolbar.mPlay || toolbar.mSimulate || toolbar.mStop ) ) {
                imgui::ImGuiScopedStyleVar controlSpacing{ ImGuiStyleVar_ItemSpacing, ImVec2{ 8.0f, 4.0f } };
                const SceneControlVisibility controls{ toolbar.mPlay, toolbar.mSimulate, toolbar.mStop };

                const float controlsWidth{ GetSceneControlsWidth( controls ) };
                const float centeredX{ ( ImGui::GetWindowSize().x - controlsWidth ) * 0.5f };

                // Utilities stay anchored left. Keep playback centered where it fits,
                // shifting right on narrow windows so the two groups never overlap.
                ImGui::SetCursorPosX( glm::max( centeredX, ImGui::GetCursorPosX() + 12.0f ) );

                DrawSceneControls( *mEditorState, controls );
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar( 4 );
    }

    // Viewport and rendering updates.

    auto EditorLayer::UpdateDockSpacePanels( float ts ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        for ( const auto& panel: mPanelRegistry | std::ranges::views::values ) {
            // Viewports remain interactive during Play, including Scene's Stop button.
            // ViewportPanel guards authoring actions locally while observing the runtime.
            const bool viewportPanel{ panel.get() == mPanelRegistry.Get<GamePreviewPanel>() ||
                                      panel.get() == mPanelRegistry.Get<ViewportPanel>() };
            const bool independentPanel{ panel.get() == mPanelRegistry.Get<AudioPanel>()
#if defined( MIKOTO_EDITOR_TRACY_VIEWER )
                || panel.get() == mPanelRegistry.Get<InstrumentationPanel>()
#endif
            };
            ImGui::BeginDisabled( !independentPanel && ( !mEditorState->HasWorkspace() ||
                                                       ( mEditorState->mRuntimeActive && !viewportPanel ) ) );
            panel->OnRender( ts );
            ImGui::EndDisabled();
        }

        DrawProjectLauncher();

        // Make physics debug image dockable
        if ( mEditorState->mPhysicsDebugRendererSimple ) {
            if ( mDisplayPhysicsPanelLines ) {
                mEditorState->mPhysicsDebugRendererSimple->DisplayImGuiWindowLines( mDisplayPhysicsPanelLines );
            }

            if ( mDisplayPhysicsPanelTriangles ) {
                mEditorState->mPhysicsDebugRendererSimple->DisplayImGuiWindowTriangles( mDisplayPhysicsPanelTriangles );
            }
        }
    }

    auto EditorLayer::RenderScene( float ) -> void {
        MKT_BEGIN_PROFILER();
        // A closed, collapsed, or background-tab Scene panel must not render
        // another full view behind the Game panel. Direct presentation still
        // needs this renderer even when no editor viewport is submitted.
        const bool requested{ mScreenPresentTarget != ScreenPresentTarget::ePanels ||
                              mEditorState->mScenePreviewRequested };

        if ( requested && mEditorRenderer->IsRenderAvailable() ) {
            mEditorState->ValidateSelection();
            mEditorState->RefreshHighlights();
            const auto& view{ mEditorState->mViewSettings };
            mEditorRenderer->SetArmatureDebugMode( view.mArmatureDebugMode );
            auto& volumes{ mEditorRenderer->GetSceneVolumeSettings() };
            volumes.mShowBounds = view.mShowBounds;
            volumes.mShowCameraFrustums = view.mShowCameraFrustums;
            volumes.mShowPointLightRanges = view.mShowPointLightRanges;
            volumes.mShowSpotLightRanges = view.mShowSpotLightRanges;
            volumes.mSelectedOnly = view.mCameraLightVolumesSelectedOnly;
            volumes.mSelectedEntity = mEditorState->mSelectedEntityID;
            volumes.mGameViewport = mEditorState->mGamePreviewSize;
            volumes.mLineWidth = view.mLineWidth;
            mEditorRenderer->SetOutputForPreview( mScreenPresentTarget == ScreenPresentTarget::ePanels );
            mEditorRenderer->Render( mEditorState->mActiveScene );
            mEditorState->mFinalComposition = mEditorRenderer->GetFinalImage();
        }

        if ( requested && mEditorState->mPhysicsDebugRenderer ) {
            mEditorState->mPhysicsDebugRenderer->Render();
        }

        if ( mEditorState->mPhysicsDebugRendererSimple && ( mDisplayPhysicsPanelLines || mDisplayPhysicsPanelTriangles ) ) {
            mEditorState->mPhysicsDebugRendererSimple->Render();
        }
    }

    auto EditorLayer::UpdateGameCamera() -> void {
        mEditorState->mGameCamera = nullptr;
        mEditorState->mGameCameraFixedAspect = false;

        if ( !mEditorState->mActiveScene ) {
            return;
        }

        for ( auto [entity, camera, transform, tag]:
            mEditorState->mActiveScene->GetRegistry().view<scene::CameraComponent, scene::TransformComponent, scene::TagComponent>().each() ) {
            if ( !tag.IsActive() || !camera.HasCamera() || !camera.IsMainCamera() ) {
                continue;
            }
            // Copy lens/pose state only, never SceneCamera's intrusive reference counter.
            static_cast<scene::Camera&>( mGameCamera ) = camera.GetCamera();
            mEditorState->mGameCameraFixedAspect = camera.IsAspectRatioFixed();

            if ( !mEditorState->mGameCameraFixedAspect ) {
                mGameCamera.SetViewportSize( mEditorState->mGamePreviewSize.x, mEditorState->mGamePreviewSize.y );
            }

            const auto& world{ transform.GetWorldTransform() };
            const auto pose{ scene::Camera::ExtractRenderPose( world ) };
            mGameCamera.SetRenderPose( pose );
            mEditorState->mGameCamera = &mGameCamera;

            if ( mGameRenderer ) {
                mGameRenderer->SetMainCamera( &mGameCamera );
                mGameRenderer->SetGamma( camera.GetGamma() );
                mGameRenderer->SetExposure( camera.GetExposure() );
                mGameRenderer->SetSkyboxBlur( camera.GetSkyboxBlur() );
                mGameRenderer->SetClearColor( camera.GetClearColor() );
                mGameRenderer->SetEnableInfiniteGrid( false );
                const auto flags{ camera.GetClearFlags() };
                mGameRenderer->SetRenderBackground( flags == scene::CameraClearFlags::eSkybox ? renderer::SceneBackgroundType::eSkybox : flags == scene::CameraClearFlags::eBlurredSkybox ? renderer::SceneBackgroundType::ePrefilterMap
                                                                                                                                                                                          : renderer::SceneBackgroundType::eClearColor );
            }

            break;
        }
    }

    auto EditorLayer::UpdateGameViewport() -> void {
        if ( mEditorState->mGameCamera && !mEditorState->mGameCameraFixedAspect ) {
            // The panel measures its content during UI layout. Apply the current
            // size before either renderer consumes the game camera this frame.
            mGameCamera.SetViewportSize( mEditorState->mGamePreviewSize.x, mEditorState->mGamePreviewSize.y );
        }
    }

    auto EditorLayer::RenderGamePreview() -> void {
        const bool requested{ mEditorState->mGamePreviewRequested };
        mEditorState->mGamePreviewRequested = false;

        if ( !requested || !mEditorState->mActiveScene || !mEditorState->mGameCamera ||
             !mGameRenderer || !mEditorRenderer ) {
            return;
        }

        if ( !mGameRenderer->IsRenderAvailable() ) {
            mGameRenderer->ApplySettings( mEditorRenderer->GetSettings() );
            mGameRenderer->Initialize();

            if ( !mGameRenderer->IsRenderAvailable() ) {
                return;
            }
        }

        auto settings{ mEditorRenderer->GetSettings() };
        settings.mVisualizeFrustum = false;
        settings.mVisualizeCascades = false;
        mGameRenderer->ApplySettings( settings );
        // This overload synchronizes renderer environment data, not simulation.
        // Scene simulation advances once in UpdateSceneState().
        mEditorState->mActiveScene->Update( mGameRenderer.get() );
        mGameRenderer->Render( mEditorState->mActiveScene );
    }

    auto EditorLayer::UpdateViewportState( float ) -> void {
        MKT_BEGIN_PROFILER();
        ViewportPanel* panel{ mPanelRegistry.Get<ViewportPanel>() };
        panel->SetTexture( mEditorState->mFinalComposition );
    }

    auto EditorLayer::UpdateSceneState( float ts ) -> void {
        MKT_BEGIN_PROFILER();
        scene::SceneManager::Get()->CollectRetiredScenes();
        mEditorState->mActiveScene->Update( ts );
        mEditorState->ValidateSelection();
    }

    auto EditorLayer::UpdateCameraState( float ts ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( mEditorState->mRuntimeActive && mScreenPresentTarget != ScreenPresentTarget::ePanels ) {
            return;
        }

        // Set viewport to the currently active window we can either expand
        // the final composition to occupy the whole screen or just an ImGui viewport
        if ( mScreenPresentTarget == ScreenPresentTarget::ePanels ) {
            ViewportPanel* scenePanel{ mPanelRegistry.Get<ViewportPanel>() };
            mEditorCamera->SetViewportSize( eastl::max( scenePanel->GetWidth(), 1.0f ), eastl::max( scenePanel->GetHeight(), 1.0f ) );
        }
        else {
            mEditorCamera->SetViewportSize( mWindow->GetWidth(), mWindow->GetHeight() );
        }

        mEditorCamera->Update( ts );

        core::float3 camePos{ mEditorCamera->GetPosition() };
        JPH::RVec3 cameraPositionWorldSpace{ camePos.x, camePos.y, camePos.z };

        if ( mEditorState->mPhysicsDebugRenderer ) {
            mEditorState->mPhysicsDebugRenderer->SetCameraPos( cameraPositionWorldSpace );
        }

        if ( mEditorState->mPhysicsDebugRendererSimple ) {
            mEditorState->mPhysicsDebugRendererSimple->SetCameraPos( cameraPositionWorldSpace );
        }
    }

    auto EditorLayer::UpdateRendererState( float ts ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( mEditorState->mPhysicsDebugRendererSimple ) {
            mEditorState->mPhysicsDebugRendererSimple->SetCamera( mEditorCamera.get() );
        }

        mEditorState->mActiveScene->Update( mEditorRenderer.get() );

        scene::SceneCamera* renderCamera{ mEditorCamera.get() };
        auto& registry{ mEditorState->mActiveScene->GetRegistry() };
        scene::CameraComponent* selectedCamera{};
        scene::TransformComponent* cameraTransform{};

        for ( auto [entity, camera, transform, tag]: registry.view<scene::CameraComponent, scene::TransformComponent, scene::TagComponent>().each() ) {
            if ( !tag.IsActive() || !camera.HasCamera() ) {
                continue;
            }

            if ( !selectedCamera || camera.IsMainCamera() ) {
                selectedCamera = &camera;
                cameraTransform = &transform;
            }

            if ( camera.IsMainCamera() ) {
                break;
            }
        }

        if ( selectedCamera ) {
            mEditorCamera->SetDepthOfFieldSettings( selectedCamera->GetCamera().GetDepthOfFieldSettings() );
            mEditorCamera->SetVignetteSettings( selectedCamera->GetCamera().GetVignetteSettings() );
        }
        else {
            mEditorCamera->SetDepthOfFieldSettings( scene::DepthOfFieldSettings{} );
            mEditorCamera->SetVignetteSettings( scene::VignetteSettings{} );
        }

        if ( mEditorState->mRuntimeActive && selectedCamera && mScreenPresentTarget != ScreenPresentTarget::ePanels ) {
            renderCamera = &selectedCamera->GetCamera();
            mEditorRenderer->SetGamma( selectedCamera->GetGamma() );
            mEditorRenderer->SetExposure( selectedCamera->GetExposure() );
            mEditorRenderer->SetSkyboxBlur( selectedCamera->GetSkyboxBlur() );
            mEditorRenderer->SetClearColor( selectedCamera->GetClearColor() );
            // Clear color alone does not select the background pass. Apply
            // the camera's saved mode as well, including after loading a scene.
            switch ( selectedCamera->GetClearFlags() ) {
                case scene::CameraClearFlags::eSkybox:
                    mEditorRenderer->SetRenderBackground( renderer::SceneBackgroundType::eSkybox );
                    break;
                case scene::CameraClearFlags::eBlurredSkybox:
                    mEditorRenderer->SetRenderBackground( renderer::SceneBackgroundType::ePrefilterMap );
                    break;
                default:
                    mEditorRenderer->SetRenderBackground( renderer::SceneBackgroundType::eClearColor );
                    break;
            }

            const auto dimensions{ InferDimensions( mEditorState->mResolution ) };
            selectedCamera->OnViewportResize( dimensions.first, dimensions.second );
            const auto& world{ cameraTransform->GetWorldTransform() };
            // View uses the entity pose, with scale removed to avoid distorting
            // the camera lens. SceneCamera input smoothing is never run in Play.
            const auto pose{ scene::Camera::ExtractRenderPose( world ) };
            renderCamera->SetRenderPose( pose );
        }

        if ( mScreenPresentTarget == ScreenPresentTarget::ePanels ) {
            ViewportPanel* scenePanel{ mPanelRegistry.Get<ViewportPanel>() };
            renderCamera->SetViewportSize( glm::max( scenePanel->GetWidth(), 1.0f ),
                glm::max( scenePanel->GetHeight(), 1.0f ) );
        }
        else {
            renderCamera->SetViewportSize( mWindow->GetWidth(), mWindow->GetHeight() );
        }

        mEditorState->mActiveCamera = renderCamera;
        mEditorRenderer->SetMainCamera( renderCamera );
        mEditorRenderer->SetEnableInfiniteGrid( mEditorState->mViewSettings.mShowGrid );

        if ( mEditorState->mViewSettings.mExposureOverride ) {
            mEditorRenderer->SetExposure( mEditorState->mViewSettings.mExposure );
        }

        mEditorRenderer->SetEnablePolygonComplexity( mShowPolygonComplexity );
    }

    // Projects, scene files and workspace transitions.

    auto EditorLayer::ApplyTextureCompression( bool enabled ) -> void {
        auto* assets{ asset::AssetRegistryService::GetPtr() };

        if ( assets->IsTextureCompressionEnabled() == enabled ) {
            return;
        }

        core::Engine::Get()->WaitForBackgroundTasks();

        mDevice->WaitIdle();
        assets->SetCompressTextures( enabled );
    }

    auto EditorLayer::ActivateProject( project::ProjectHandle project, scene::Scene* scene ) -> void {
        MKT_BEGIN_PROFILER();
        project->RestoreMainCamera( *mEditorCamera );
        mPanelRegistry.Get<ContentBrowserPanel>()->SetProjectDirectory( project->GetAssetsDirectory(), project->GetCacheDirectory() );

        ActivateScene( scene, project->GetScenePath() );
        mWorldSelector.Close();

        mProject = eastl::move( project );
        scripting::ScriptingService::Get()->SetScriptBasePath(
            filesystem::Path{ mProject->GetAssetsDirectory().GetPathTyped<std::filesystem::path>() / "Scripts" } );
        mEditorState->mHasProject = true;
        mEditorState->mSandboxMode = false;
        mProject->SetMainCamera( mEditorCamera.get() );
        mEditorState->mEditorRenderer->ApplySettings( mProject->GetSceneRendererSettings() );
        mEditorState->mResolution = mProject->GetSceneRendererSettings().mResolution;
        mWorkflowError.clear();

        mEditorState->mScriptSolution = {};
        mEditorState->mScriptProject = {};

        try {
            mEditorState->mScriptSolution = mProject->EnsureScriptProject(
                filesystem::Path{ "Resources/Scripting/Mikoto.ScriptCore.dll" }.ToAbsolute() );
            mEditorState->mScriptProject = mProject->GetScriptProjectPath();
        } catch ( const std::exception& error ) {
            mWorkflowError = error.what();
        }
    }

    auto EditorLayer::PersistEditorCamera() -> void {
        MKT_BEGIN_PROFILER();

        if ( !mProject || !mEditorCamera || mEditorState->mSandboxMode ) {
            return;
        }

        // Called only by explicit saves. Preserve the manifest's last saved
        // scene path when saving settings on exit. Capture the editor camera,
        // never Play's camera.
        mProject->SetMainCamera( mEditorCamera.get() );
        mProject->SetSceneRendererSettings( mEditorState->mEditorRenderer->GetSettings() );
        mProject->Serialize( mProject->GetLocation() );
    }

    auto EditorLayer::ApplySceneEnvironment( scene::Scene& scene ) -> void {
        MKT_BEGIN_PROFILER();
        const auto environment{ scene.GetEnvironmentMaterial() };
        // An empty scene explicitly clears the previous environment. Merely
        // skipping the skybox loop would retain its cached IBL and reflections.
        scene.Update( mEditorRenderer.get() );
        mEditorRenderer->SetRenderBackground( environment ? renderer::SceneBackgroundType::eSkybox : renderer::SceneBackgroundType::eClearColor );
        scene::CameraComponent* primary{};

        for ( auto [entity, camera, tag]: scene.GetRegistry().view<scene::CameraComponent, scene::TagComponent>().each() ) {
            if ( !tag.IsActive() || !camera.HasCamera() ) {
                continue;
            }

            if ( !primary || camera.IsMainCamera() ) {
                primary = &camera;
            }

            if ( camera.IsMainCamera() ) {
                break;
            }
        }

        if ( primary ) {
            // Restore scene-owned display settings when entering or leaving
            // Play, instead of inheriting the previous scene's exposure/color.
            mEditorRenderer->SetGamma( primary->GetGamma() );
            mEditorRenderer->SetExposure( primary->GetExposure() );
            mEditorRenderer->SetClearColor( primary->GetClearColor() );
            mEditorRenderer->SetSkyboxBlur( primary->GetSkyboxBlur() );

            switch ( primary->GetClearFlags() ) {
                case scene::CameraClearFlags::eSkybox:
                    mEditorRenderer->SetRenderBackground( renderer::SceneBackgroundType::eSkybox );
                    break;
                case scene::CameraClearFlags::eBlurredSkybox:
                    mEditorRenderer->SetRenderBackground( renderer::SceneBackgroundType::ePrefilterMap );
                    break;
                default:
                    mEditorRenderer->SetRenderBackground( renderer::SceneBackgroundType::eClearColor );
                    break;
            }
        }
    }

    auto EditorLayer::ActivateScene( scene::Scene* scene, const filesystem::Path& path ) -> void {
        MKT_BEGIN_PROFILER();
        // Complete submissions referencing the outgoing scene before releasing
        // its materials. Imports may defer CPU destruction until a later update.
        mDevice->WaitIdle();
        scene::Scene* previous{ mEditorState->mActiveScene };
        mEditorState->SelectEntity( nullptr );

        if ( previous ) {
            previous->SetState( scene::SceneState::eIdle );
        }

        physics::PhysicSystem::Get()->SetSimulationTarget( scene );
        mEditorState->mActiveScene = scene;
        mEditorState->mActiveScenePath = path;

        ApplySceneEnvironment( *scene );

        if ( previous != scene ) {
            scene::SceneManager::Get()->Unload( previous );
        }
    }

    auto EditorLayer::CreateProject() -> void {
        MKT_BEGIN_PROFILER();

        if ( mEditorState->mSandboxMode ) {
            return;
        }

        if ( !ConfirmWorkspaceChange( WorkspaceAction::eCreateProject ) ) {
            return;
        }

        if ( mEditorState->mRuntimeActive ) {
            return;
        }

        const filesystem::Path chosen{ filesystem::SaveFileDialog( "New Project.mktproj", { { MKT_LOC( "editor_layer_mikoto_project" ).c_str(), "mktproj" } } ) };

        if ( chosen.IsEmpty() ) {
            return;
        }

        try {
            auto target{ chosen.GetPathTyped<std::filesystem::path>() };

            if ( target.extension().empty() ) {
                target.replace_extension( ".mktproj" );
            }

            if ( std::filesystem::exists( target ) ) {
                MKT_THROW_RUNTIME_ERROR( MKT_LOC( "editor_layer_a_project_already_exists_there_choose_open_project_or_a" ).c_str() );
            }

            auto project{ project::ProjectHandle::New( filesystem::Path{ target }.GetStem() ) };
            project->SetLocation( filesystem::Path{ target } );

            std::filesystem::create_directories( project->GetAssetsDirectory().GetPathTyped<std::filesystem::path>() / "Scenes" );
            std::filesystem::create_directories( project->GetCacheDirectory().GetPathTyped<std::filesystem::path>() );

            project->Serialize( project->GetLocation() );

            ApplyTextureCompression( project->GetCompressTextures() );
            ActivateProject( project, CreateStarterScene( *project ) );

            // Used by the APP to display in the window header
            auto& cVars{ core::CVarRegistry::Get() };
            cVars.SetValue<eastl::string>( "project_name", project->GetName().data() );

        } catch ( const std::exception& error ) {
            ApplyTextureCompression( mProject ? mProject->GetCompressTextures() : false );
            mWorkflowError = error.what();
        }
    }

    auto EditorLayer::OpenProject( filesystem::Path path, bool sandbox ) -> void {
        MKT_BEGIN_PROFILER();
        sandbox = sandbox || mEditorState->mSandboxMode;

        if ( sandbox ) {
            if ( path.IsEmpty() ) {
                return;
            }

            const auto root{ std::filesystem::weakly_canonical( "Resources/Demo Projects" ) };
            const auto target{ std::filesystem::weakly_canonical( path.GetPathTyped<std::filesystem::path>() ) };
            const auto relative{ target.lexically_relative( root ) };

            if ( relative.empty() || relative.is_absolute() || *relative.begin() == ".." ) {
                return;
            }
        }

        if ( !ConfirmWorkspaceChange( WorkspaceAction::eOpenProject, path ) ) {
            return;
        }

        if ( mEditorState->mRuntimeActive ) {
            return;
        }

        if ( path.IsEmpty() ) {
            path = filesystem::OpenFileDialog( { { MKT_LOC( "editor_layer_mikoto_project" ).c_str(), "mktproj,mktprj" } } );
        }

        if ( path.IsEmpty() ) {
            return;
        }

        struct LoadState {
            project::ProjectHandle mProject{};
            scene::Scene* mScene{};
        };
        auto state{ std::make_shared<LoadState>() };
        mWorkflowError.clear();
        const bool started{ mWorkspaceOperation.Start( MKT_LOC( "editor_layer_opening_project" ).c_str(), path.GetC_Str(), { { MKT_LOC( "editor_layer_reading_project_configuration" ).c_str(), [this, state, path]() {
            state->mProject = mProjectSerializer.Deserialize( path ).As<project::Project>();

            if ( !state->mProject ) {
                MKT_THROW_RUNTIME_ERROR( MKT_LOC( "editor_layer_could_not_open_project" ).c_str() );
            }

            std::filesystem::create_directories( state->mProject->GetAssetsDirectory().GetPathTyped<std::filesystem::path>() );
            std::filesystem::create_directories( state->mProject->GetCacheDirectory().GetPathTyped<std::filesystem::path>() );
        } },
                                                                                                                               { MKT_LOC( "editor_layer_loading_the_project_scene_and_assets" ).c_str(), [this, state]() {
            const auto& loaded{ state->mProject };
            const bool previous{ asset::AssetRegistryService::Get()->IsTextureCompressionEnabled() };

            try {
                ApplyTextureCompression( loaded->GetCompressTextures() );
                state->mScene = loaded->GetScenePath().IsEmpty() ? CreateStarterScene( *loaded ) : scene::SceneManager::Get()->Load( loaded->GetScenePath() );

                if ( !state->mScene ) {
                    MKT_THROW_RUNTIME_ERROR( MKT_LOC( "editor_layer_could_not_open_the_project_s_scene" ).c_str() );
                }

                // Used by the APP to display in the window header
                auto& cVars{ core::CVarRegistry::Get() };
                cVars.SetValue<eastl::string>( "project_name", loaded->GetName().data() );
            } catch ( ... ) {
                ApplyTextureCompression( previous );
                throw;
            }
        } },
                                                                                                                               { MKT_LOC( "editor_layer_applying_project_and_renderer_settings" ).c_str(), [this, state, sandbox]() {
            ActivateProject( state->mProject, state->mScene );
            mEditorState->mSandboxMode = sandbox;

            if ( sandbox ) {
                MatchEditorCameraToScene();
                mShowSandboxNotice = true;
            }
        } } } ) };

        if ( !started ) {
            mWorkflowError = MKT_LOC( "editor_layer_another_workspace_operation_is_already_active" ).c_str();
        }
    }

    auto EditorLayer::SaveProject() -> void {
        MKT_BEGIN_PROFILER();

        if ( mProject.IsEmpty() || mEditorState->mRuntimeActive || mEditorState->mSandboxMode ) {
            return;
        }

        try {
            if ( !QueueSceneSave( false, true ) ) {
                return;
            }

            mWorkflowError.clear();
        } catch ( const std::exception& error ) {
            mWorkflowError = error.what();
        }
    }

    auto EditorLayer::CreateScene() -> void {
        MKT_BEGIN_PROFILER();

        if ( mEditorState->mSandboxMode ) {
            return;
        }

        if ( !ConfirmWorkspaceChange( WorkspaceAction::eCreateScene ) ) {
            return;
        }

        if ( !mEditorState->HasWorkspace() || mEditorState->mRuntimeActive ) {
            return;
        }

        try {
            ActivateScene( CreateStarterScene( *mProject ), filesystem::Path{} );

            if ( mEditorState->mSandboxMode ) {
                MatchEditorCameraToScene();
            }
            else {
                mEditorCamera->LookAt( { 8.0f, 6.5f, 10.0f }, { 0.0f, 1.0f, 0.0f } );
            }

            mWorkflowError.clear();
        } catch ( const std::exception& error ) {
            mWorkflowError = error.what();
        }
    }

    auto EditorLayer::OpenScene( filesystem::Path path ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mEditorState->mSandboxMode ) {
            return;
        }

        if ( !mEditorState->HasWorkspace() || mEditorState->mRuntimeActive ) {
            return;
        }

        if ( !ConfirmWorkspaceChange( WorkspaceAction::eOpenScene, path ) ) {
            return;
        }

        if ( path.IsEmpty() ) {
            path = filesystem::OpenFileDialog( { { MKT_LOC( "editor_layer_mikoto_scene" ).c_str(), "mktscn" } } );
        }

        if ( path.IsEmpty() ) {
            return;
        }

        // Both deferred steps must share the loaded scene, rather than capture
        // independent copies of an initially null pointer.
        auto loaded{ std::make_shared<scene::Scene*>( nullptr ) };
        mWorkflowError.clear();

        const bool started{ mWorkspaceOperation.Start( MKT_LOC( "editor_layer_opening_scene" ).c_str(), path.GetC_Str(), { { MKT_LOC( "editor_layer_loading_scene_objects_and_assets" ).c_str(), [loaded, path]() {
            *loaded = scene::SceneManager::Get()->Load( path );

            if ( !*loaded ) {
                MKT_THROW_RUNTIME_ERROR( MKT_LOC( "editor_layer_could_not_open_scene" ).c_str() );
            }
        } },
                                                                                                                             { MKT_LOC( "editor_layer_preparing_the_scene_and_editor_view" ).c_str(), [this, loaded, path]() {
            ActivateScene( *loaded, path.ToAbsolute() );
        } } } ) };

        if ( !started ) {
            mWorkflowError = MKT_LOC( "editor_layer_another_workspace_operation_is_already_active" ).c_str();
        }
    }

    auto EditorLayer::QueueSceneSave( bool saveAs, bool projectSave, WorkspaceAction nextAction, filesystem::Path nextPath ) -> bool {
        MKT_BEGIN_PROFILER();

        if ( !mEditorState->HasWorkspace() || mEditorState->mSandboxMode || mEditorState->mRuntimeActive || mWorkspaceOperation.IsVisible() ) {
            return false;
        }

        filesystem::Path target{ mEditorState->mActiveScenePath };

        if ( target.IsEmpty() || saveAs ) {
            target = filesystem::SaveFileDialog( "Scene.mktscn", { { MKT_LOC( "editor_layer_mikoto_scene" ).c_str(), "mktscn" } } );

            if ( target.IsEmpty() ) {
                return false;
            }
        }

        auto native{ target.GetPathTyped<std::filesystem::path>() };

        if ( native.extension().empty() ) {
            native.replace_extension( ".mktscn" );
        }

        target = filesystem::Path{ native }.ToAbsolute();
        auto contents{ std::make_shared<eastl::string>() };
        mWorkflowError.clear();
        return mWorkspaceOperation.Start( projectSave ? MKT_LOC( "editor_layer_saving_project" ).c_str() : MKT_LOC( "editor_layer_saving_scene" ).c_str(), target.GetC_Str(), { { MKT_LOC( "editor_layer_serializing_scene_objects_and_asset_references" ).c_str(), [this, target, contents]() {
            *contents = mEditorState->mActiveScene->SerializeText( target );
        } },
                                                                                                                                                                                  { MKT_LOC( "editor_layer_writing_the_scene_file" ).c_str(), [target, contents]() {
            filesystem::FileHandle file{ filesystem::FileService::Get()->CreateNewFile( target ) };

            if ( file.IsEmpty() ) {
                MKT_THROW_RUNTIME_ERROR( "Cannot open scene file for writing." );
            }

            file->Write( *contents );
        }, true },
                                                                                                                                                                                  { mProject ? MKT_LOC( "editor_layer_saving_project_and_renderer_settings" ).c_str() : MKT_LOC( "editor_layer_finishing_scene_save" ).c_str(), [this, target]() {
            mEditorState->mActiveScenePath = target;

            if ( mProject ) {
                mProject->SetScenePath( target );
                PersistEditorCamera();
            }
        } } },
            [this, nextAction, nextPath]() {
            mDeferredWorkspaceAction = nextAction;
            mDeferredWorkspacePath = nextPath;
        } );
    }

    auto EditorLayer::SaveScene() -> void {
        MKT_BEGIN_PROFILER();

        try {
            if ( QueueSceneSave( false ) ) {
                mWorkflowError.clear();
            }
        } catch ( const std::exception& error ) {
            mWorkflowError = error.what();
        }
    }

    auto EditorLayer::SaveSceneAs() -> void {
        MKT_BEGIN_PROFILER();

        try {
            if ( QueueSceneSave( true ) ) {
                mWorkflowError.clear();
            }
        } catch ( const std::exception& error ) {
            mWorkflowError = error.what();
        }
    }

    auto EditorLayer::ConfirmWorkspaceChange( WorkspaceAction action, const filesystem::Path& path ) -> bool {
        MKT_BEGIN_PROFILER();

        if ( mEditorState->mRuntimeActive || mWorkspaceOperation.IsVisible() ||
             mPendingWorkspaceAction != WorkspaceAction::eNone || mDeferredWorkspaceAction != WorkspaceAction::eNone ) {
            return false;
        }

        if ( mWorkspaceChangeApproved || !mEditorState->HasWorkspace() || mEditorState->mSandboxMode ) {
            mWorkspaceChangeApproved = false;
            return true;
        }
        // Conservative confirmation also covers changes made by component tabs
        // which do not yet publish a common scene-dirty notification.
        mPendingWorkspaceAction = action;
        // Keep the destination through Save/Discard. Empty means the request
        // came from a menu or shortcut and still needs a file picker.
        mPendingWorkspacePath = path.IsEmpty() ? filesystem::Path{} : path.ToAbsolute();
        return false;
    }

    auto EditorLayer::DrawProjectLauncher() -> void {
        MKT_BEGIN_PROFILER();
        const PopupStyle popupStyle{};
        DrawWorkspaceConfirmation();

        if ( mEditorState->mRuntimeActive ) {
            mWorldSelector.Close();
        }
        else if ( !mEditorState->HasWorkspace() && !mWorldSelector.IsOpen() ) {
            DrawWelcomeWindow();
        }

        auto project{ mWorldSelector.Draw( mDevice, mWorkspacePosition, mWorkspaceSize, mEditorState->HasWorkspace() ) };
        if ( !project.IsEmpty() ) {
            OpenProject( eastl::move( project ) );
        }

        DrawSandboxNotice();
        DrawWorkflowError();
    }

    auto EditorLayer::DrawWorkspaceConfirmation() -> void {
        MKT_BEGIN_PROFILER();
        WorkspaceAction approvedAction{ eastl::exchange( mDeferredWorkspaceAction, WorkspaceAction::eNone ) };
        filesystem::Path approvedPath{ eastl::exchange( mDeferredWorkspacePath, filesystem::Path{} ) };
        const bool closing{ mPendingWorkspaceAction == WorkspaceAction::eCloseEditor };
        const auto confirmationTitle{ closing ? imgui::widget::LocalizedLabel( "editor_layer_save_project_changes", "Save project changes?" ) : imgui::widget::LocalizedLabel( "editor_layer_save_current_scene", "Save current scene?" ) };

        if ( mPendingWorkspaceAction != WorkspaceAction::eNone ) {
            ImGui::OpenPopup( confirmationTitle.c_str() );
        }

        const auto* confirmationViewport{ ImGui::GetMainViewport() };
        const ImVec2 confirmationSize{
            eastl::min( 480.0f, eastl::max( 1.0f, confirmationViewport->WorkSize.x - 32.0f ) ),
            eastl::min( 180.0f, eastl::max( 1.0f, confirmationViewport->WorkSize.y - 32.0f ) )
        };
        ImGui::SetNextWindowViewport( confirmationViewport->ID );
        ImGui::SetNextWindowPos( confirmationViewport->GetWorkCenter(), ImGuiCond_Always, { 0.5f, 0.5f } );
        ImGui::SetNextWindowSize( confirmationSize, ImGuiCond_Always );
        constexpr ImGuiWindowFlags confirmationFlags{
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking
        };

        if ( ImGui::BeginPopupModal( confirmationTitle.c_str(), nullptr, confirmationFlags ) ) {
            ImGui::TextWrapped( "%s", MKT_LOC( closing ? "editor_layer_save_project_settings_before_closing" : "editor_layer_save_your_current_scene_before_changing_the_workspace" ).c_str() );
            const ImVec2 buttonSize{
                eastl::max( 1.0f, ( ImGui::GetContentRegionAvail().x - 2.0f * ImGui::GetStyle().ItemSpacing.x ) / 3.0f ),
                ImGui::GetFrameHeight() + 8.0f
            };
            ImGui::SetCursorPosY( ImGui::GetCursorPosY() + eastl::max( 0.0f, ImGui::GetContentRegionAvail().y - buttonSize.y ) );

            if ( imgui::widget::AccentButton( MKT_LOC( "editor_layer_save" ).c_str(), buttonSize ) ) {
                try {
                    if ( closing ) {
                        // Settings-only saves need no scene destination or scene serialization.
                        PersistEditorCamera();
                        approvedAction = WorkspaceAction::eCloseEditor;
                    }
                    else if ( QueueSceneSave( false, false, mPendingWorkspaceAction, mPendingWorkspacePath ) ) {
                        mPendingWorkspaceAction = WorkspaceAction::eNone;
                        mPendingWorkspacePath = filesystem::Path{};
                        ImGui::CloseCurrentPopup();
                    }
                } catch ( const std::exception& error ) {
                    mWorkflowError = error.what();
                }
            }

            ImGui::SameLine();

            if ( imgui::widget::AccentButton( MKT_LOC( "editor_layer_discard" ).c_str(), buttonSize ) ) {
                approvedAction = mPendingWorkspaceAction;
                approvedPath = mPendingWorkspacePath;
            }

            ImGui::SameLine();

            if ( imgui::widget::AccentButton( MKT_LOC( "editor_layer_cancel" ).c_str(), buttonSize ) ) {
                mPendingWorkspaceAction = WorkspaceAction::eNone;
                mPendingWorkspacePath = filesystem::Path{};
                ImGui::CloseCurrentPopup();
            }

            if ( approvedAction != WorkspaceAction::eNone ) {
                mPendingWorkspaceAction = WorkspaceAction::eNone;
                mPendingWorkspacePath = filesystem::Path{};
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        if ( approvedAction != WorkspaceAction::eNone ) {
            ExecuteWorkspaceAction( approvedAction, approvedPath );
        }
    }

    auto EditorLayer::ExecuteWorkspaceAction( WorkspaceAction action, const filesystem::Path& path ) -> void {
        MKT_BEGIN_PROFILER();
        mWorkspaceChangeApproved = true;

        switch ( action ) {
            case WorkspaceAction::eCreateProject:
                CreateProject();
                break;
            case WorkspaceAction::eOpenProject:
                OpenProject( path );
                break;
            case WorkspaceAction::eCreateScene:
                CreateScene();
                break;
            case WorkspaceAction::eOpenScene:
                OpenScene( path );
                break;
            case WorkspaceAction::eCloseEditor:
                mWorkspaceChangeApproved = false;
                mCloseApproved = true;
                platform::EventSystem::Get()->Queue<platform::WindowCloseEvent>();
                break;
            case WorkspaceAction::eNone:
                mWorkspaceChangeApproved = false;
                break;
        }
    }

    auto EditorLayer::DrawWelcomeWindow() -> void {
        MKT_BEGIN_PROFILER();
        const auto* viewport{ ImGui::GetMainViewport() };
        constexpr core::f32 preferredWidth{ 440.0f };
        constexpr core::f32 viewportMargin{ 32.0f };
        const core::f32 width{ eastl::min( preferredWidth, eastl::max( 1.0f, mWorkspaceSize.x - viewportMargin ) ) };
        const core::f32 maxHeight{ eastl::max( 1.0f, mWorkspaceSize.y - viewportMargin ) };
        const ImVec2 center{ mWorkspacePosition.x + mWorkspaceSize.x * 0.5f, mWorkspacePosition.y + mWorkspaceSize.y * 0.5f };

        // Recenter as the workspace or the height of the wrapped text changes.
        ImGui::SetNextWindowViewport( viewport->ID );
        ImGui::SetNextWindowPos( center, ImGuiCond_Always, { 0.5f, 0.5f } );
        ImGui::SetNextWindowSizeConstraints( { width, 0.0f }, { width, maxHeight } );
        constexpr ImGuiWindowFlags windowFlags{
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings
        };

        if ( imgui::BeginWindow( imgui::widget::LocalizedLabel( "editor_layer_welcome_to_mikoto", "Welcome to Mikoto" ).c_str(), nullptr, windowFlags ) ) {
            ImGui::TextWrapped( "%s", MKT_LOC( "editor_layer_open_a_project_create_one_or_enter_sandbox_to_test" ).c_str() );

            constexpr core::f32 buttonHeight{ 36.0f };
            const core::f32 availableWidth{ ImGui::GetContentRegionAvail().x };
            const core::f32 buttonWidth{ eastl::max( 1.0f, ( availableWidth - ImGui::GetStyle().ItemSpacing.x ) * 0.5f ) };
            const ImVec2 fullButtonSize{ eastl::max( 1.0f, availableWidth ), buttonHeight };

            if ( imgui::widget::AccentButton( MKT_LOC( "editor_layer_new_project" ).c_str(), { buttonWidth, buttonHeight } ) ) {
                CreateProject();
            }

            imgui::SetCursorHandOnLastItemHovered();

            ImGui::SameLine();

            if ( imgui::widget::AccentButton( MKT_LOC( "editor_layer_open_project" ).c_str(), { buttonWidth, buttonHeight } ) ) {
                OpenProject();
            }

            imgui::SetCursorHandOnLastItemHovered();

            if ( !mEditorState->HasWorkspace() &&
                 imgui::widget::AccentButton( MKT_LOC( "editor_layer_sandbox" ).c_str(), fullButtonSize ) ) {
                EnterSandbox();
            }

            imgui::SetCursorHandOnLastItemHovered();
            imgui::widget::ItemTooltip( "%s", MKT_LOC( "editor_layer_sandbox_description" ).c_str() );

            if ( imgui::widget::AccentButton( MKT_LOC( "world_selector_button" ).c_str(), fullButtonSize ) ) {
                mWorldSelector.Open();
            }

            imgui::SetCursorHandOnLastItemHovered();
        }

        ImGui::End();
    }

    auto EditorLayer::DrawSandboxNotice() -> void {
        MKT_BEGIN_PROFILER();
        const auto sandboxTitle{ MKT_LOC( "editor_layer_sandbox" ) };

        if ( eastl::exchange( mShowSandboxNotice, false ) ) {
            ImGui::OpenPopup( sandboxTitle.c_str() );
        }

        const auto* sandboxViewport{ ImGui::GetMainViewport() };
        const float sandboxWidth{ eastl::min( 480.0f, eastl::max( 1.0f, sandboxViewport->WorkSize.x - 32.0f ) ) };
        ImGui::SetNextWindowViewport( sandboxViewport->ID );
        ImGui::SetNextWindowPos( sandboxViewport->GetWorkCenter(), ImGuiCond_Always, { 0.5f, 0.5f } );
        ImGui::SetNextWindowSizeConstraints( { sandboxWidth, 0.0f },
            { sandboxWidth, eastl::max( 1.0f, sandboxViewport->WorkSize.y - 32.0f ) } );

        if ( ImGui::BeginPopupModal( sandboxTitle.c_str(), nullptr,
                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking ) ) {
            ImGui::TextWrapped( "%s", MKT_LOC( "editor_layer_sandbox_notice" ).c_str() );
            ImGui::Spacing();

            if ( imgui::widget::AccentButton( "OK", { ImGui::GetContentRegionAvail().x, ImGui::GetFrameHeight() + 8.0f } ) ) {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    auto EditorLayer::DrawWorkflowError() -> void {
        MKT_BEGIN_PROFILER();

        if ( !mWorkflowError.empty() ) {
            ImGui::OpenPopup( imgui::widget::LocalizedLabel( "editor_layer_project_operation_failed", "Project operation failed" ).c_str() );
        }

        const auto* errorViewport{ ImGui::GetMainViewport() };
        const float errorWidth{ eastl::min( 560.0f, eastl::max( 1.0f, errorViewport->WorkSize.x - 32.0f ) ) };
        ImGui::SetNextWindowViewport( errorViewport->ID );
        ImGui::SetNextWindowPos( errorViewport->GetWorkCenter(), ImGuiCond_Always, { 0.5f, 0.5f } );
        // Wrapped text needs a defined width; auto-sizing both axes can collapse it to the OK button's width.
        ImGui::SetNextWindowSizeConstraints( { errorWidth, 0.0f },
            { errorWidth, eastl::max( 1.0f, errorViewport->WorkSize.y - 32.0f ) } );
        constexpr ImGuiWindowFlags errorFlags{
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking
        };

        if ( ImGui::BeginPopupModal( imgui::widget::LocalizedLabel( "editor_layer_project_operation_failed", "Project operation failed" ).c_str(), nullptr, errorFlags ) ) {
            ImGui::TextWrapped( "%s", mWorkflowError.c_str() );

            if ( imgui::widget::AccentButton( "OK" ) ) {
                mWorkflowError.clear();
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    auto EditorLayer::StartPlay() -> void {
        MKT_BEGIN_PROFILER();

        if ( !mEditorState->HasWorkspace() || mEditorState->mRuntimeActive ) {
            return;
        }

        try {
            bool cameraFound{};

            for ( auto [entity, camera, tag]: mEditorState->mActiveScene->GetRegistry().view<scene::CameraComponent, scene::TagComponent>().each() ) {
                cameraFound |= camera.HasCamera() && tag.IsActive();
            }

            if ( !cameraFound ) {
                MKT_THROW_RUNTIME_ERROR( MKT_LOC( "editor_layer_add_an_active_camera_component_before_entering_play_mode" ).c_str() );
            }

            // Fail before changing scenes when the game's script project does
            // not build. Runtime loading still owns per-scene script instances.
            bool hasCSharp{};

            for ( auto [entity, script]: mEditorState->mActiveScene->GetRegistry().view<scene::ScriptComponent>().each() ) {
                hasCSharp |= script.GetLanguage() == scripting::ScriptLanguage::eCSharp;
            }

            if ( hasCSharp && mProject && !mEditorState->mScriptSolution.IsEmpty() ) {
                scripting::DotnetScriptingService compiler{};
                compiler.Initialize();

                if ( !compiler.Compile( mProject->GetScriptProjectPath() ) ) {
                    MKT_THROW_RUNTIME_ERROR( MKT_LOC( "editor_layer_script_project_build_failed" ).c_str() );
                }
            }

            // Play owns a deserialized scene; stopping discards simulation and
            // script edits while retaining the untouched editor scene.
            const filesystem::Path snapshot{ GetWorkspaceCacheDirectory().GetPathTyped<std::filesystem::path>() / "PlaySnapshot.mktscn" };
            auto file{ filesystem::FileService::Get()->CreateNewFile( snapshot ) };

            if ( !file ) {
                MKT_THROW_RUNTIME_ERROR( "Cannot create the Play snapshot." );
            }

            file->Write( mEditorState->mActiveScene->SerializeText( snapshot, true ) );
            auto runtime{ SceneHandle::New( "Play" ) };
            runtime->Deserialize( snapshot );
            mEditScene = mEditorState->mActiveScene;
            mEditScene->SetState( scene::SceneState::eIdle );
            mEditorAnimatorSuspensions.clear();

            for ( auto [entity, component]: mEditScene->GetRegistry().view<scene::AnimatorComponent>().each() ) {
                if ( auto* animator{ animation::AnimationSystem::Get()->GetAnimator( component.GetAnimatorID() ) } ) {
                    mEditorAnimatorSuspensions.emplace( component.GetAnimatorID(), animator->IsSuspended() );
                    animator->SetSuspended( true );
                }
            }

            mEditorState->SelectEntity( nullptr );
            mRuntimeScene = eastl::move( runtime );
            mEditorState->mActiveScene = mRuntimeScene.GetPtr();
            mEditorState->mRuntimeActive = true;
            // The editor copy remains alive during Play. Silence its preview
            // voices and listeners so they cannot mix with the runtime scene.
            mEditorAudioPreviews.clear();

            for ( auto [entity, audio]: mEditScene->GetRegistry().view<scene::AudioSourceComponent>().each() ) {
                if ( auto source{ audio.GetSource() }; source && source->IsPlaying() ) {
                    mEditorAudioPreviews.push_back( source );
                    source->Pause();
                }
            }

            for ( auto [entity, listener]: mEditScene->GetRegistry().view<scene::AudioListenerComponent>().each() ) {
                if ( listener.IsActive() ) {
                    listener.GetListener().SetEnabled( false );
                }
            }

            mRuntimeScene->SetState( scene::SceneState::ePlaying );
            ApplySceneEnvironment( *mRuntimeScene );
            mEditorCamera->EnableCamera( false );
            mWindow->SetCursorMode( platform::CursorMode::eNormal );
        } catch ( const std::exception& error ) {
            StopPlay();
            mWorkflowError = error.what();
        }
    }

    auto EditorLayer::StopPlay() -> void {
        MKT_BEGIN_PROFILER();

        if ( !mEditorState->mRuntimeActive ) {
            return;
        }

        mRuntimeScene->SetState( scene::SceneState::eIdle );
        mWindow->SetCursorMode( platform::CursorMode::eNormal );
        mEditorCamera->EnableCamera( false );
        mDevice->WaitIdle();
        physics::PhysicSystem::Get()->SetSimulationTarget( mEditScene );
        mEditorState->SelectEntity( nullptr );
        mEditorState->mActiveScene = mEditScene;
        mEditorState->mRuntimeActive = false;
        mEditorState->mActiveCamera = mEditorCamera.get();
        mEditorRenderer->SetMainCamera( mEditorCamera.get() );
        ApplySceneEnvironment( *mEditScene );

        for ( const auto& [id, suspended]: mEditorAnimatorSuspensions ) {
            if ( auto* animator{ animation::AnimationSystem::Get()->GetAnimator( id ) } ) {
                animator->SetSuspended( suspended );
            }
        }

        mEditorAnimatorSuspensions.clear();
        mRuntimeScene = SceneHandle::CreateEmpty();

        for ( auto [entity, listener, tag]: mEditScene->GetRegistry().view<scene::AudioListenerComponent, scene::TagComponent>().each() ) {
            if ( listener.IsActive() ) {
                listener.GetListener().SetEnabled( listener.IsEnabled() && tag.IsActive() );
            }
        }

        for ( auto& source: mEditorAudioPreviews ) {
            source->Continue();
        }

        mEditorAudioPreviews.clear();
        mEditScene = nullptr;
    }

    auto EditorLayer::DrawWorkspaceProgress() -> void {
        if ( !mWorkspaceOperation.IsVisible() ) {
            return;
        }

        const auto popupTitle{ imgui::widget::LocalizedLabel( "editor_layer_workspace_progress", "Workspace progress##Operation" ) };
        const char* popup{ popupTitle.c_str() };
        const auto* viewport{ ImGui::GetMainViewport() };

        ImGui::OpenPopup( popup );
        ImGui::SetNextWindowViewport( viewport->ID );
        ImGui::SetNextWindowPos( viewport->GetWorkCenter(), ImGuiCond_Always, { 0.5f, 0.5f } );
        ImGui::SetNextWindowSize( { glm::max( 1.0f, glm::min( 460.0f, viewport->WorkSize.x - 32.0f ) ), 0.0f } );
        const PopupStyle popupStyle{};
        const ImGuiWindowFlags flags{ ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                                      ImGuiWindowFlags_AlwaysAutoResize };

        if ( ImGui::BeginPopupModal( popup, nullptr, flags ) ) {
            const bool busy{ mWorkspaceOperation.IsBusy() };
            const bool failed{ mWorkspaceOperation.HasFailed() };
            ImGui::TextUnformatted( mWorkspaceOperation.GetTitle().c_str() );
            ImGui::Spacing();
            ImGui::TextWrapped( "%s", mWorkspaceOperation.GetPath().c_str() );
            ImGui::Spacing();
            ImGui::TextUnformatted( busy ? mWorkspaceOperation.GetStage().c_str() : failed ? MKT_LOC( "editor_layer_operation_failed" ).c_str()
                                                                                           : MKT_LOC( "editor_layer_completed" ).c_str() );

            const auto position{ ImGui::GetCursorScreenPos() };
            const float width{ ImGui::GetContentRegionAvail().x };
            auto* draw{ ImGui::GetWindowDrawList() };
            draw->AddRectFilled( position, { position.x + width, position.y + 7.0f }, ImGui::GetColorU32( ImGuiCol_FrameBg ), 3.5f );

            if ( busy ) {
                // Stages have different costs; do not imply a measured percentage.
                const double time{ ImGui::GetTime() };
                const float phase{ static_cast<float>( time - glm::floor( time ) ) };
                const float offset{ phase * width * 0.72f };
                draw->AddRectFilled( { position.x + offset, position.y },
                    { position.x + offset + width * 0.28f, position.y + 7.0f },
                    ImGui::GetColorU32( ImGuiCol_PlotHistogram ), 3.5f );
            }
            else {
                draw->AddRectFilled( position, { position.x + width, position.y + 7.0f },
                    failed ? IM_COL32( 204, 62, 62, 255 ) : IM_COL32( 45, 151, 100, 255 ), 3.5f );
            }

            ImGui::Dummy( { width, 13.0f } );

            if ( failed ) {
                ImGui::TextWrapped( "%s", mWorkspaceOperation.GetError().c_str() );
            }

            const bool acknowledgeError{ failed && imgui::widget::AccentButton( "OK" ) };

            if ( acknowledgeError || mWorkspaceOperation.CanAutoDismiss() ) {
                ImGui::CloseCurrentPopup();
                mWorkspaceOperation.Dismiss();
            }

            // ImGui measures a new auto-sized popup during its first frame.
            // Wait for the following visible frame before running a blocking stage.
            if ( !ImGui::IsWindowAppearing() ) {
                mWorkspaceOperation.MarkPresented();
            }

            ImGui::EndPopup();
        }
    }

    auto EditorLayer::HandleUploadAsset() -> void {
        auto& upload{ mEditorState->mUploadContext };

        if ( !upload.mUploadOnGoing ) {
            return;
        }

        if ( !upload.mPopupOpened ) {
            upload.mSettings = {};
            upload.mError.clear();

            if ( upload.mDroppedPaths.size() == 1 && asset::IsFileModel( upload.mDroppedPaths.front() ) ) {
                try {
                    upload.mSettings = asset::ImportConfiguration::Load( upload.mDroppedPaths.front() );
                } catch ( const std::exception& error ) {
                    upload.mError = error.what();
                }
            }

            ImGui::OpenPopup( "Import settings" );
            upload.mPopupOpened = true;
        }

        ImGui::SetNextWindowSize( { 480, 0 }, ImGuiCond_Appearing );

        if ( !ImGui::BeginPopupModal( "Import settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize ) ) {
            return;
        }

        bool models{};

        for ( const auto& path: upload.mDroppedPaths ) {
            ImGui::TextUnformatted( path.GetC_Str() );
            models |= asset::IsFileModel( path );
        }

        auto& settings{ upload.mSettings };

        if ( models ) {
            ImGui::SeparatorText( "Model contents" );
            imgui::CheckBox( "Meshes", settings.mImportMesh );
            imgui::CheckBox( "Skeletons", settings.mImportSkeletons );
            ImGui::BeginDisabled( !settings.mImportSkeletons );
            imgui::CheckBox( "Animations", settings.mImportAnimations );
            ImGui::EndDisabled();
            imgui::CheckBox( "Materials", settings.mImportMaterials );
            ImGui::BeginDisabled( !settings.mImportMaterials );
            imgui::CheckBox( "Textures", settings.mImportTextures );
            ImGui::EndDisabled();
            ImGui::SeparatorText( "Transforms" );

            if ( settings.mImportSkeletons ) {
                settings.mImportTranslation = settings.mImportRotation = settings.mImportScale = true;
                ImGui::TextDisabled( "Skeletons retain authored transforms for skinning." );
            }

            ImGui::BeginDisabled( settings.mImportSkeletons );
            imgui::CheckBox( "Translation", settings.mImportTranslation );
            imgui::CheckBox( "Rotation", settings.mImportRotation );
            imgui::CheckBox( "Scale", settings.mImportScale );
            ImGui::EndDisabled();
        }

        if ( !upload.mError.empty() ) {
            ImGui::TextWrapped( "%s", upload.mError.c_str() );
        }

        ImGui::BeginDisabled( !mEditorState->mActiveScene || ( models && !settings.mImportMesh && !settings.mImportSkeletons ) );

        if ( imgui::widget::AccentButton( "Import" ) ) {
            try {
                ImportDroppedAssets();

                upload.mUploadOnGoing = false;
                ImGui::CloseCurrentPopup();
            } catch ( const std::exception& error ) {
                upload.mError = error.what();
            }
        }

        ImGui::EndDisabled();
        ImGui::SameLine();

        if ( ImGui::Button( "Cancel" ) ) {
            upload.mUploadOnGoing = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();

        if ( !upload.mUploadOnGoing ) {
            upload.mDroppedPaths.clear();
            upload.mPopupOpened = false;
        }
    }

    auto EditorLayer::ImportDroppedAssets() -> void {
        MKT_BEGIN_PROFILER();
        auto& upload{ mEditorState->mUploadContext };
        auto& settings{ upload.mSettings };
        // Validate every input before scheduling. Cancel never creates scene entities.
        for ( const auto& path: upload.mDroppedPaths ) {
            if ( !path.Exists() || ( !asset::IsFileModel( path ) && !asset::IsFileImage( path ) ) ) {
                MKT_THROW_RUNTIME_ERROR( string::Format( "Unsupported import: {}", path.GetC_Str() ) );
            }
        }

        while ( !upload.mDroppedPaths.empty() ) {
            const auto path{ upload.mDroppedPaths.front() };

            if ( asset::IsFileModel( path ) ) {
                auto* assets{ asset::AssetRegistryService::GetPtr() };
                const auto reference{ asset::AssetRef<asset::Model>{ path } };
                // Reuse resident geometry unless the chosen import policy changed.
                if ( settings != asset::ImportConfiguration::Load( path ) ) {
                    settings.Save( path );
                    assets->ReloadModel( reference );
                }

                mEditorState->mActiveScene->PushEntity( { .mName = eastl::string{ path.GetStem() }, .mModelAsset = reference } );
            }
            else {
                asset::AssetRegistryService::Get()->LoadAssetAsync<renderer::rhi::ITexture>( path, renderer::rhi::TextureDimension::eTexture2D );
            }

            upload.mDroppedPaths.erase( upload.mDroppedPaths.begin() );
        }
    }

    auto EditorLayer::GetWorkspaceCacheDirectory() const -> filesystem::Path {
        MKT_BEGIN_PROFILER();

        if ( mProject ) {
            return mProject->GetCacheDirectory();
        }

        // Standalone Play snapshots and thumbnails do not require a project
        // manifest and must not change the destination of a saved scene.
        return filesystem::Path{ "Assets/.cache/Sandbox" }.ToAbsolute();
    }

    auto EditorLayer::EnterSandbox() -> void {
        MKT_BEGIN_PROFILER();

        if ( mEditorState->HasWorkspace() || mEditorState->mRuntimeActive ) {
            return;
        }

        OpenProject( filesystem::Path{ "Resources/Demo Projects/Sandbox/Sandbox.mktproj" }, true );
    }

    auto EditorLayer::MatchEditorCameraToScene() -> void {
        MKT_BEGIN_PROFILER();
        // Match the demo's camera and clipping range in both Edit and Play.
        for ( auto [entity, camera, transform]: mEditorState->mActiveScene->GetRegistry().view<scene::CameraComponent, scene::TransformComponent>().each() ) {
            if ( !camera.IsMainCamera() ) {
                continue;
            }

            const auto world{ transform.GetWorldTransform() };
            const core::float3 position{ world[3] };
            mEditorCamera->SetProjectionType( scene::ProjectionType::ePerspective );
            mEditorCamera->SetClipPlanes( camera.GetCamera().GetNearPlane(), camera.GetCamera().GetFarPlane() );
            mEditorCamera->SetFieldOfView( camera.GetCamera().GetFOV() );
            mEditorCamera->LookAt( position, position - core::float3{ world[2] } );
            break;
        }
    }

    auto EditorLayer::CreateStarterScene( const project::Project& project ) -> scene::Scene* {
        MKT_BEGIN_PROFILER();
        const auto cube{ mEditorState->GetPrefab( PrefabModelType::eCube ) };

        if ( !cube || cube->GetMeshNodeCount() != 1 ) {
            MKT_THROW_RUNTIME_ERROR( "The starter scene requires the built-in cube prefab." );
        }

        const auto& vertices{ cube->GetMeshNode( 0 ).GetCollisionVertices() };

        if ( vertices.empty() ) {
            MKT_THROW_RUNTIME_ERROR( "The cube prefab has no collider geometry." );
        }

        core::float3 minimum{ vertices.front() };
        core::float3 maximum{ vertices.front() };

        for ( const auto& vertex: vertices ) {
            minimum = glm::min( minimum, vertex );
            maximum = glm::max( maximum, vertex );
        }

        const core::float3 size{ maximum - minimum };
        const core::float3 center{ ( minimum + maximum ) * 0.5f };

        if ( !glm::all( glm::greaterThan( size, core::float3{ 0.0001f } ) ) ) {
            MKT_THROW_RUNTIME_ERROR( "The cube prefab has invalid bounds." );
        }

        const filesystem::Path controller{ project.GetAssetsDirectory().GetPathTyped<std::filesystem::path>() / "Scripts/Lua/StarterBoxController.lua" };

        if ( !controller.Exists() ) {
            // Install a normal project asset once; later New Scene operations
            // preserve edits the user has made to this controller.
            const auto source{ filesystem::FileHandle::New( filesystem::Path{ "Resources/Script-Examples/StarterBoxController.lua" } ) };
            filesystem::FileHandle file{ filesystem::FileService::Get()->CreateNewFile( controller ) };

            if ( file.IsEmpty() ) {
                MKT_THROW_RUNTIME_ERROR( "Cannot create the starter controller script." );
            }

            file->Write( source->GetContentsString() );
        }

        scene::Scene* scene{ scene::SceneManager::Get()->CreateScene( "Starter Scene" ) };

        try {
            const auto makeBox = [&]( core::cstr name, const core::float3& position, const core::float3& dimensions,
                                     const core::float4& color, scene::RigidBodyComponent::BodyType type ) {
                scene::Entity* entity{ scene->CreateEntity( scene::EntityCreateInfo{ .mName = name, .mModel = cube } ) };
                const core::float3 scale{ dimensions / size };

                auto& transform{ entity->GetComponent<scene::TransformComponent>() };
                // Normalize the imported prefab to world-space dimensions and
                // keep its mesh-space collider offset aligned with the visual.
                transform.SetTransform( core::float4x4{ 1.0f } );
                transform.SetUniformSale( false );
                transform.SetScale( scale );
                transform.SetTranslation( position - center * scale );

                auto& collider{ entity->AddComponent<scene::BoxColliderComponent>() };
                collider.SetHalfExtents( size * 0.5f );
                collider.SetOffset( center );

                auto& body{ entity->AddComponent<scene::RigidBodyComponent>() };
                body.SetBodyType( type );
                body.SetMass( 1.0f );
                body.SetUseGravity( type == scene::RigidBodyComponent::BodyType::eDynamic );
                body.SetFriction( 0.65f );
                body.SetRestitution( 0.1f );

                auto material{ entity->GetComponent<scene::MaterialComponent>().GetMaterial().As<material::SurfaceMaterial>() };
                material->RemoveTexture( material::MapType::eBaseColor );
                material->SetBaseColorFactor( color );
                material->SetMetallicFactor( 0.0f );
                material->SetRoughnessFactor( 0.75f );
                material->SetAlphaMask( material::AlphaMode::eOpaque );
                return entity;
            };

            makeBox( "Ground (Static)", { 0.0f, -0.5f, 0.0f }, { 16.0f, 1.0f, 16.0f },
                { 0.55f, 0.58f, 0.62f, 1.0f }, scene::RigidBodyComponent::BodyType::eStatic );
            auto* player{ makeBox( "Blue Box (WASD / Space / R)", { 0.0f, 3.0f, 0.0f }, core::float3{ 1.0f },
                { 0.12f, 0.4f, 0.9f, 1.0f }, scene::RigidBodyComponent::BodyType::eDynamic ) };
            player->AddComponent<scene::ScriptComponent>( controller );
            makeBox( "Falling Box A", { 2.0f, 1.5f, -1.0f }, core::float3{ 1.0f },
                { 0.95f, 0.45f, 0.12f, 1.0f }, scene::RigidBodyComponent::BodyType::eDynamic );
            makeBox( "Falling Box B", { 2.0f, 3.0f, -1.0f }, core::float3{ 1.0f },
                { 0.2f, 0.75f, 0.35f, 1.0f }, scene::RigidBodyComponent::BodyType::eDynamic );

            auto* cameraEntity{ scene->CreateEntity( "Camera" ) };
            auto& camera{ cameraEntity->AddComponent<scene::CameraComponent>() };
            camera.SetMainCamera( true );
            // With no environment the renderer uses the clear color. If the
            // user adds a skybox later, the starter camera displays it in Play.
            camera.SetClearFlags( scene::CameraClearFlags::eSkybox );
            camera.SetClearColor( renderer::rhi::Color{ 0.16f, 0.19f, 0.24f, 1.0f } );
            camera.SetExposure( 1.0f );
            camera.GetCamera().SetClipPlanes( 0.1f, 100.0f );
            cameraEntity->GetComponent<scene::TransformComponent>().SetTransform(
                glm::inverse( glm::lookAt( core::float3{ 8.0f, 6.5f, 10.0f }, core::float3{ 0.0f, 1.0f, 0.0f }, core::float3{ 0.0f, 1.0f, 0.0f } ) ) );

            auto* sun{ scene->CreateEntity( "Sun" ) };
            auto& light{ sun->AddComponent<scene::LightComponent>( renderer::LightType::eDirectional ).Get<renderer::DirectionalLight>() };
            light.SetDirection( core::float3{ -0.5f, -1.0f, -0.35f } );
            light.SetColor( core::float3{ 1.0f, 0.95f, 0.88f } );
            light.SetIntensity( 3.0f );
            light.SetIsShadowCaster( true );
            return scene;
        } catch ( ... ) {
            scene::SceneManager::Get()->Unload( scene );
            throw;
        }
    }

}// namespace mikoto::editor
