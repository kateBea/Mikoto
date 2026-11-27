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

#ifndef MIKOTO_LAYERS_EDITOR_LAYER_HH
#define MIKOTO_LAYERS_EDITOR_LAYER_HH

#include <ankerl/unordered_dense.h>

#include <EASTL/functional.h>
#include <EASTL/memory.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/vector.h>

#include <Theme/Theme.hh>

#include <Application/TerrainTools.hh>
#include <Application/WorkspaceOperation.hh>

#include <Assets/AssetAudioClip.hh>
#include <Assets/AssetMaterial.hh>
#include <Assets/AssetModel.hh>
#include <Assets/AssetRegistryService.hh>
#include <Assets/ImageProcessing.hh>

#include <Core/Core.hh>
#include <Core/Layer.hh>
#include <Core/Registry.hh>

#include <Panels/Panel.hh>
#include <Panels/WorldSelector.hh>

#include <Platform/ActionManager.hh>
#include <Platform/Event.hh>
#include <Platform/Window.hh>

#include <Project/Project.hh>
#include <Project/ProjectSerializer.hh>

#include <Renderer/Core/PhysicsDebugRenderer.hh>
#include <Renderer/Core/SceneRenderer.hh>
#include <Renderer/Core/ThumbnailRenderer.hh>
#include <Renderer/Rhi/Types.hh>

#include <Scene/Entity.hh>
#include <Scene/Scene.hh>
#include <Scene/SceneCamera.hh>

namespace mikoto::platform {
    class KeyPressedEvent;
    class ContentDroppedEvent;
}// namespace mikoto::platform

namespace mikoto::editor {

    enum class PrefabModelType {
        eCube,
        eSphere,
        eCone,
        eCylinder,
        eBunny,
        eMitsuba,
        eTeapot,
        eMissGalaxy,
        eTree01,
        eTree02,
        eTree03,
        eTree04,
        eMedievalDoor,
        eSponza,
    };

    enum class ScreenPresentTarget {
        ePanels,
        eScene,
        eGame
    };

    struct EditorUploadContext {
        bool mUploadOnGoing{ false };
        bool mPopupOpened{};
        asset::ImportConfiguration mSettings{};
        eastl::string mError{};

        eastl::vector<filesystem::Path> mDroppedPaths{};
    };

    // The inherited revision tracks selection identity for picking and gizmo consumers.
    struct EditorState : core::RevisionTracked {
        struct ViewSettings {
            enum class ToolsLayout : core::u8 {
                eDefault,
                eClassic
            };

            enum class OrientationAnchor : core::u8 {
                eBottomLeft,
                eBottomRight,
                eTopLeft,
                eTopRight
            };

            enum class SoftBodyVertexVisibility : core::u8 {
                eHidden,
                eSelected,
                eAll
            };

            SoftBodyVertexVisibility mSoftBodyVertexVisibility{ SoftBodyVertexVisibility::eSelected };
            ToolsLayout mToolsLayout{ ToolsLayout::eDefault };
            OrientationAnchor mOrientationAnchor{ OrientationAnchor::eBottomLeft };
            OrientationAnchor mClassicOrientationAnchor{ OrientationAnchor::eBottomLeft };

            bool mShowIcons{ true };
            bool mShowViewportControls{};
            bool mShowCameraFrustums{};
            bool mShowPointLightRanges{};
            bool mShowSpotLightRanges{};
            bool mCameraLightVolumesSelectedOnly{ true };
            bool mShowGizmos{ true };
            bool mShowGizmosInPlayMode{ true };
            bool mShowGrid{ true };
            bool mShowBounds{};
            renderer::WindDebugView mWindDebugView{ renderer::WindDebugView::eDisabled };
            bool mShowSelectedWireframe{};
            renderer::ArmatureDebugMode mArmatureDebugMode{ renderer::ArmatureDebugMode::eDisabled };
            bool mSelectRoot{};
            bool mExposureOverride{};
            core::f32 mExposure{ 1.0f };
            core::f32 mLineWidth{ 1.0f };
        } mViewSettings{};

        struct ToolbarSettings {
            bool mSave{ true };
            bool mOpen{ true };
            bool mGame{ true };
            bool mRenderer{ true };
            bool mStats{ true };
            bool mPreferences{ true };
            bool mPlay{ true };
            bool mSimulate{ true };
            bool mStop{ true };
        } mToolbarSettings{};

        bool mShowStatusDateTime{ true };

        static constexpr core::float4 kDefaultHeaderGradientColor{
            16.0f / 255.0f,
            17.0f / 255.0f,
            18.0f / 255.0f,
            140.0f / 255.0f
        };

        core::float4 mHeaderGradientColor{ kDefaultHeaderGradientColor };

        TerrainToolState mTerrainTool{};

        bool mHasProject{};
        bool mSandboxMode{};
        bool mRuntimeActive{};
        bool mCenterPivot{ true };
        bool mTransformChildren{ true };
        bool mTransformEditRejected{};

        filesystem::Path mExternalEditor{};
        filesystem::Path mScriptSolution{};
        filesystem::Path mScriptProject{};

        // Both the Content Browser and entity inspector select this asset for
        // the shared material graph workspace. Its resource is edited and saved directly.
        core::Ref<asset::Material> mEditMaterial{};
        bool mRequestMaterialEditor{};

        // Only the file identity is shared; the Audio panel owns its playback device and listener.
        asset::AssetRef<asset::AudioClip> mPreviewAudio{};
        bool mRequestAudioPanel{};

        eastl::function<void()> mRequestPlay{};
        eastl::function<void()> mRequestStop{};
        eastl::function<void( const filesystem::Path & )> mOpenParticle{};
        eastl::function<void( const asset::ParticleEffectHandle & )> mEditParticleEffect{};
        eastl::function<void( const filesystem::Path & )> mOpenScene{};
        eastl::function<void( const filesystem::Path & )> mOpenProject{};
        eastl::function<void( scene::ProjectionType )> mRequestEditorProjection{};
        eastl::function<bool()> mGetTextureCompression{};
        eastl::function<void( bool )> mSetTextureCompression{};

        scene::Scene *mActiveScene{};
        filesystem::Path mActiveScenePath{};
        scene::Entity *mSelectedEntity{};
        core::u64 mSelectedEntityID{};
        eastl::vector<core::u64> mSelectedEntityIDs{};

        scene::SceneCamera *mActiveCamera{};
        const scene::SceneCamera *mGameCamera{};
        bool mScenePreviewRequested{};
        bool mGamePreviewRequested{};

        // Maximized viewports stay within the editor chrome and keep their docked windows intact.
        ScreenPresentTarget mScreenPresentTarget{ ScreenPresentTarget::ePanels };
        ViewportInfo mMaximizedViewport{};

        core::float2 mGamePreviewSize{ 1920.0f, 1080.0f };
        scene::SceneCamera *mEditorCamera{};
        renderer::ISceneRenderer *mGameRenderer{};
        renderer::ISceneRenderer *mEditorRenderer{};
        renderer::ThumbnailRenderer *mThumbnailRenderer{};
        renderer::PhysicsDebugRenderer *mPhysicsDebugRenderer{};
        renderer::PhysicsDebugRendererSimple *mPhysicsDebugRendererSimple{};

        Theme *mActiveTheme{};

        // Image that will be used for presentation
        renderer::rhi::TextureHandle mFinalComposition{};

        // Current desired resolution. If changed a resize on the final
        // composition should be performed.
        renderer::rhi::RenderResolution mResolution{ renderer::rhi::RenderResolution::e1080P };

        struct Prefab {
            asset::AssetRef<asset::Model> mReference{};
            asset::AssetHandle<asset::Model> mRequest{};
        };

        // Keep residency demands alive while the editor offers these prefabs.
        ankerl::unordered_dense::map<PrefabModelType, Prefab> mPrefabs{};

        // Upload meshes to my project
        EditorUploadContext mUploadContext{};

        auto ValidateSelection() -> void;
        auto RefreshHighlights() -> void;
        auto SelectEntity( scene::Entity *entity ) -> void;
        auto SelectEntities( const eastl::vector<core::u64> &ids, bool append = false ) -> void;

        MKT_NODISCARD auto HasWorkspace() const -> bool;

        /** @brief Gets resident prefab geometry without waiting for streaming.
         * @param model Built-in prefab to look up.
         * @return Loaded model, or an empty handle while unavailable.
         */
        MKT_NODISCARD auto GetPrefab( PrefabModelType model ) -> asset::ModelHandle;

        /** @brief Gets the asset identity for deferred scene creation.
         * @param model Registered built-in prefab to look up.
         * @return Reference valid even before the model finishes loading.
         */
        MKT_NODISCARD auto GetPrefabReference( PrefabModelType model ) const -> asset::AssetRef<asset::Model>;

        /** @brief Requests unloaded prefabs and renews demands after cache invalidation. */
        auto UpdatePrefabs() -> void;
    };

    class EditorLayer final : public core::ILayer {
    public:
        explicit EditorLayer( platform::IWindow *window );

        auto OnCreate() -> void override;
        auto OnDestroy() -> void override;
        auto OnUpdate( float timeStep ) -> void override;
        auto OnRender( float deltaTime ) -> void override;

        auto OnEvent( platform::Event &event ) -> void override;

        MKT_NODISCARD auto GetName() const -> eastl::string_view override;

    private:
        static constexpr float kTitleBarVerticalPadding{ 8.0f };

        enum class WorkspaceAction : core::u8 {
            eNone,
            eCreateProject,
            eOpenProject,
            eCreateScene,
            eOpenScene,
            eCloseEditor
        };

        // Application setup.
        auto InitAssets() -> void;
        auto InitEditorState() -> void;
        auto InitSceneRenderers() -> void;
        auto InitEditorCamera() -> void;
        auto InitEditorPanels() -> void;
        auto InitActionCallbacks() -> void;
        auto InitEmptyScene() -> void;

        // Frame update and rendering.
        /**
         * @brief Draws disabled workspace panels and operation progress while loading.
         * @param timeStep Elapsed frame time in seconds passed to panels.
         */
        auto DrawLoadingWorkspace( float timeStep ) -> void;

        /**
         * @brief Applies pending preferences, import policy and script build updates.
         */
        auto UpdateEditorServices() -> void;

        /**
         * @brief Presents the launcher without submitting scene or viewport work.
         */
        auto DrawEmptyWorkspace() -> void;

        /**
         * @brief Applies queued Stop and Play requests before updating the scene.
         */
        auto UpdatePlayRequests() -> void;

        /**
         * @brief Draws docked panels and import controls, or updates fullscreen picking.
         * @param timeStep Elapsed frame time in seconds passed to panels.
         */
        auto UpdateWorkspaceViews( float timeStep ) -> void;

        /**
         * @brief Renders measured scene and game viewports and publishes the final image.
         * @param timeStep Elapsed frame time in seconds passed to scene rendering.
         */
        auto RenderWorkspace( float timeStep ) -> void;

        /**
         * @brief Draws ImGui diagnostics, fullscreen workspace dialogs and operation progress.
         */
        auto DrawEditorOverlays() -> void;

        auto DrawRenderingUnavailableNotice() -> void;
        auto UpdateDockSpacePanels( float ts ) -> void;
        auto RenderScene( float ts ) -> void;
        auto RenderGamePreview() -> void;
        auto UpdateGameCamera() -> void;
        auto UpdateGameViewport() -> void;
        auto UpdateViewportState( float ts ) -> void;
        auto UpdateSceneState( float ts ) -> void;
        auto UpdateCameraState( float ts ) -> void;
        auto UpdateRendererState( float ts ) -> void;
        auto SetCameraCapture( bool enabled ) -> void;

        // Input routing.
        /**
         * @brief Checks whether a workspace operation or confirmation owns editor input.
         * @returns True while loading, saving or awaiting a workspace decision.
         */
        MKT_NODISCARD auto IsWorkspaceBusy() const -> bool;

        /**
         * @brief Defers closing while work is pending and prompts for unsaved project settings.
         * @param event Close event marked handled when the editor must remain open.
         */
        auto HandleWindowClose( platform::Event &event ) -> void;

        /**
         * @brief Routes diagnostic and authoring shortcuts while respecting text input.
         * @param event Key press to route.
         */
        auto HandleKeyPressed( const platform::KeyPressedEvent &event ) -> void;

        /**
         * @brief Dispatches console and profiler shortcuts even without an editable workspace.
         * @param event Key press to inspect.
         * @returns True when a diagnostic shortcut was dispatched.
         */
        MKT_NODISCARD auto HandleDiagnosticShortcut( const platform::KeyPressedEvent &event ) -> bool;

        /**
         * @brief Opens a dropped workspace or queues the files for import in Edit mode.
         * @param event Batch of dropped paths.
         */
        auto HandleContentDropped( const platform::ContentDroppedEvent &event ) -> void;

        /**
         * @brief Opens the first project or scene in a drop batch.
         * @param event Batch of dropped paths.
         * @returns True when the drop requested a workspace change.
         */
        MKT_NODISCARD auto OpenDroppedWorkspace( const platform::ContentDroppedEvent &event ) -> bool;

        /**
         * @brief Starts camera capture in the fullscreen editor view and releases it on button-up.
         * @param button Mouse button that changed state.
         * @param pressed True when the button was pressed.
         */
        auto HandleCameraButton( platform::MouseButton button, bool pressed ) -> void;

        // Docking, menus and editor chrome.
        /**
         * @brief Discovers bundled project manifests and draws the Default Projects submenu.
         */
        auto DrawDefaultProjectsMenu() -> void;

        /**
         * @brief Draws physics debug options and simulation settings.
         */
        auto DrawPhysicsMenu() -> void;

        /**
         * @brief Draws audio mixer and profiler commands.
         */
        auto DrawAudioMenu() -> void;

        /**
         * @brief Draws renderer diagnostics, overlays and output selection.
         */
        auto DrawGraphicsMenu() -> void;

        /**
         * @brief Selects the renderer image displayed in the Scene viewport.
         */
        auto DrawRenderOutputMenu() -> void;

        /** @brief Toggles one viewport inside the workspace without changing the saved dock layout. */
        auto ToggleViewportPresentation( ScreenPresentTarget target ) -> void;

        auto UpdateDockSpace() -> void;
        auto DrawMainMenuBar() -> void;
        auto DrawFileMenu() -> void;
        auto DrawEditMenu() -> void;
        auto DrawGameObjectMenu() -> void;
        auto DrawWindowMenu() -> void;
        auto DrawToolsMenu() -> void;
        auto DrawPlatformMenu() -> void;
        auto DrawHelpMenu() -> void;
        auto InitTitleBar() -> void;
        auto DrawTitleBarIcon() -> void;
        auto DrawTitleBarControls() -> void;
        auto DrawUtilityBar() -> void;

        /** @brief Draws message counts, the optional clock, background work, project status and version. */
        auto DrawStatusBar() -> void;

        /** @brief Draws progress and its divider before the reserved project/version space; returns their width. */
        auto DrawBackgroundActivity( core::f32 trailingWidth ) -> core::f32;

        /** @brief Draws a short hover preview or the full list in the scrollable task popup. */
        auto DrawBackgroundActivityDetails( bool expanded ) -> void;

        /** @brief Rebuilds the default dock arrangement and restores its visible panels. */
        auto ResetPanelLayout() -> void;

        // Projects, scene files and workspace transitions.
        /**
         * @brief Draws Save, Discard and Cancel and dispatches an approved workspace action.
         */
        auto DrawWorkspaceConfirmation() -> void;

        /**
         * @brief Executes a workspace action after its save or discard decision.
         * @param action Approved workspace operation.
         * @param path Destination used when opening a project or scene.
         */
        auto ExecuteWorkspaceAction( WorkspaceAction action, const filesystem::Path &path ) -> void;

        /**
         * @brief Draws project creation, project opening, Sandbox and World selector on the empty workspace.
         */
        auto DrawWelcomeWindow() -> void;

        /**
         * @brief Shows the Sandbox explanation and the location of the standalone demo projects.
         */
        auto DrawSandboxNotice() -> void;

        /**
         * @brief Displays a workspace failure until the user acknowledges its diagnostic.
         */
        auto DrawWorkflowError() -> void;

        /**
         * @brief Validates dropped assets, saves import policies and queues models or images.
         */
        auto ImportDroppedAssets() -> void;

        auto OpenProject( filesystem::Path path = filesystem::Path{}, bool sandbox = false ) -> void;
        auto SaveProject() -> void;
        auto PersistEditorCamera() -> void;
        auto CreateProject() -> void;
        auto EnterSandbox() -> void;
        auto MatchEditorCameraToScene() -> void;
        MKT_NODISCARD auto GetWorkspaceCacheDirectory() const -> filesystem::Path;

        // An empty path opens the scene file picker.
        auto OpenScene( filesystem::Path path = filesystem::Path{} ) -> void;
        auto SaveScene() -> void;
        auto CreateScene() -> void;
        auto SaveSceneAs() -> void;
        auto QueueSceneSave( bool saveAs, bool projectSave = false, WorkspaceAction nextAction = WorkspaceAction::eNone,
            filesystem::Path nextPath = filesystem::Path{} ) -> bool;
        auto DrawWorkspaceProgress() -> void;
        auto DrawProjectLauncher() -> void;
        auto ConfirmWorkspaceChange( WorkspaceAction action, const filesystem::Path &path = filesystem::Path{} ) -> bool;
        auto StartPlay() -> void;
        auto StopPlay() -> void;
        auto ApplyTextureCompression( bool enabled ) -> void;
        auto ActivateProject( project::ProjectHandle project, scene::Scene *scene ) -> void;
        auto ActivateScene( scene::Scene *scene, const filesystem::Path &path ) -> void;
        auto ApplySceneEnvironment( scene::Scene &scene ) -> void;
        auto HandleUploadAsset() -> void;

        // Sandbox showcase and new-project starter scene.
        auto CreateStarterScene( const project::Project &project ) -> scene::Scene *;

    private:
        eastl::string mName{};

        platform::IWindow *mWindow{};
        eastl::string mTitleBarText{};
        double mNextTitleBarRefresh{};
        renderer::rhi::TextureHandle mTitleBarIcon{};
        renderer::rhi::IGpuDevice *mDevice{};

        core::Registry<Panel> mPanelRegistry{};
        eastl::vector<eastl::pair<eastl::string, bool>> mPendingPanelVisibility{};
        bool mResetLayoutRequested{};

        eastl::unique_ptr<EditorState> mEditorState{};
        eastl::unique_ptr<scene::SceneCamera> mEditorCamera{};
        scene::SceneCamera mGameCamera{};
        eastl::unique_ptr<renderer::ISceneRenderer> mGameRenderer{};
        eastl::unique_ptr<renderer::ISceneRenderer> mEditorRenderer{};
        bool mShowRenderingUnavailableNotice{};
        bool mShowSandboxNotice{};
        eastl::unique_ptr<renderer::ThumbnailRenderer> mThumbnailRenderer{};

        eastl::unique_ptr<platform::ActionManager> mActionManager{};

        project::ProjectHandle mProject{};
        project::ProjectSerializer mProjectSerializer{};
        scene::Scene *mEditScene{};
        scene::SceneHandle mRuntimeScene{};
        eastl::string mWorkflowError{};
        WorkspaceOperation mWorkspaceOperation{};
        WorldSelector mWorldSelector{};
        ImVec2 mWorkspacePosition{};
        ImVec2 mWorkspaceSize{};
        WorkspaceAction mDeferredWorkspaceAction{ WorkspaceAction::eNone };
        filesystem::Path mDeferredWorkspacePath{};
        ankerl::unordered_dense::map<core::u64, bool> mEditorAnimatorSuspensions{};
        eastl::vector<audio::AudioSourceHandle> mEditorAudioPreviews{};
        WorkspaceAction mPendingWorkspaceAction{ WorkspaceAction::eNone };
        filesystem::Path mPendingWorkspacePath{};

        bool mPlayRequested{};
        bool mStopRequested{};
        bool mWorkspaceChangeApproved{};
        bool mCloseApproved{};

        // Status-bar text is refreshed once per UTC second.
        core::i64 mStatusClockSecond{ -1 };
        eastl::string mStatusClockText{};

        // Poll worker snapshots at a bounded rate; rendering the footer never waits for jobs.
        core::f64 mStatusWorkRefresh{};
        asset::AssetLoadingProgress mStatusLoadingProgress{};
        eastl::vector<asset::TextureCompressionProgress> mStatusCompressionProgress{};

        // Renderer display settings
        bool mShowImGuiDebugInfo{};
        bool mDisplayPhysicsPanelLines{};
        bool mDisplayPhysicsPanelTriangles{};
        bool mShowPolygonComplexity{};
        bool mShowPerformanceOverlay{};
    };
}// namespace mikoto::editor

#endif// MIKOTO_LAYERS_EDITOR_LAYER_HH
