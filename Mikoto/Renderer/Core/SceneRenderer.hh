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

#ifndef MIKOTO_RENDERER_CORE_SCENE_RENDERER_HH
#define MIKOTO_RENDERER_CORE_SCENE_RENDERER_HH

#include <ankerl/unordered_dense.h>

#include <EASTL/optional.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/vector.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Renderer/Clouds/Clouds.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/InfiniteGrid.hh>
#include <Renderer/Core/Renderer.hh>
#include <Renderer/Core/SparseBufferUpload.hh>
#include <Renderer/Denoising/Denoiser.hh>
#include <Renderer/ForwardRenderer/Passes/DebugModule.hh>
#include <Renderer/ForwardRenderer/Passes/DebugOverlayModule.hh>
#include <Renderer/ForwardRenderer/Passes/DisplayEffectsModule.hh>
#include <Renderer/ForwardRenderer/Passes/GeometryShadingModule.hh>
#include <Renderer/ForwardRenderer/Passes/IndirectLightingModule.hh>
#include <Renderer/ForwardRenderer/Passes/MousePickingModule.hh>
#include <Renderer/ForwardRenderer/Passes/PostProcessModule.hh>
#include <Renderer/ForwardRenderer/Passes/PresentationModule.hh>
#include <Renderer/ForwardRenderer/Passes/ShadowMappingModule.hh>
#include <Renderer/ForwardRenderer/Passes/TonemapModule.hh>
#include <Renderer/Particle/ParticleSystem.hh>
#include <Renderer/RayTracing/RayTracing.hh>
#include <Renderer/Rhi/Texture.hh>
#include <Renderer/Rhi/Types.hh>

namespace mikoto::renderer {

    class ISceneRenderer;

    enum class SceneRendererType : core::u32 {
        eCompatibility,
        eForwardPlus
    };

    struct SceneRendererCreateInfo {
        rhi::IGpuDevice *mDevice{};
        eastl::string_view mName{};
        eastl::string_view mShaderBasePath{};

        SceneRendererType mType{ SceneRendererType::eForwardPlus };

        rhi::Multisampling mMultisampling{ rhi::Multisampling::eMsaaX1 };
        rhi::RenderResolution mResolution{ rhi::RenderResolution::e1080P };
        rhi::ViewportConvention mViewportConvention{ rhi::ViewportConvention::eRightHanded_OriginBottomLeft };

        auto SetName( eastl::string_view name ) -> SceneRendererCreateInfo &;
        auto SetDevice( rhi::IGpuDevice *device ) -> SceneRendererCreateInfo &;
        auto SetRendererType( SceneRendererType type ) -> SceneRendererCreateInfo &;

        auto SetMultisampling( rhi::Multisampling multisampling ) -> SceneRendererCreateInfo &;
        auto SetRenderResolution( rhi::RenderResolution resolution ) -> SceneRendererCreateInfo &;
        auto SetViewportConvention( rhi::ViewportConvention convention ) -> SceneRendererCreateInfo &;

        MKT_NODISCARD auto Build() const -> eastl::unique_ptr<ISceneRenderer>;
    };

    struct SceneRenderSettings {
        RayTracingConfiguration mRayTracing{};
        DenoiserSettings mDenoiser{};
        ScreenSpaceReflectionSettings mScreenSpaceReflections{};
        CloudsSettings mClouds{};
        InfiniteGridSettings mInfiniteGrid{};
        rhi::RenderResolution mResolution{ rhi::RenderResolution::e1080P };
        rhi::Multisampling mMultisampling{ rhi::Multisampling::eMsaaX1 };
        ShadowMapQuality mShadowQuality{ ShadowMapQuality::eMedium };
        core::f32 mShadowDistance{ 200.0f };

        bool mCascadedShadowsEnabled{ true };
        bool mSoftShadowsEnabled{};
        bool mSsaoEnabled{ true };

        core::f32 mSsaoIntensity{ 1.0f };
        BloomSettings mBloom{};
        DepthOfFieldQuality mDepthOfFieldQuality{ DepthOfFieldQuality::eHigh };

        bool mVisualizeDepthOfField{};

        ChromaticAberrationSettings mChromaticAberration{};

        bool mVisualizeCascades{};
        bool mVisualizeFrustum{};
        bool mFrustumCullingEnabled{ true };

        core::f32 mSelectionOutlineWidth{ 3.0f };
        core::float4 mSelectionOutlineColor{ 1.0f, 0.55f, 0.08f, 1.0f };

        auto Sanitize() -> void;
    };

    /**
     * @brief Scene rendering and inspection operations shared by editor, runtime and captures.
     * @details Support depends on the concrete renderer. CompatibilityRenderer provides
     * scene rendering with fixed bindings. Frame graph diagnostics require ForwardRenderer.
     * Returned references and pointers are borrowed; reacquire state-owned views after
     * graph replacement, reinitialization or shutdown.
     * @see ForwardRenderer
     * @see CompatibilityRenderer
     */
    class ISceneRenderer : public IRenderer {
    public:
        /**
         * @brief Starts a fluent scene-renderer description without initializing a renderer.
         * @return Builder defaulting to ForwardPlus, 1080p and single-sample rendering.
         * Set the device and call SceneRendererCreateInfo::Build() to create an implementation.
         */
        MKT_NODISCARD static auto Create() -> SceneRendererCreateInfo;


        /**
         * @brief Creates the passes and resources needed by the configured rendering path.
         * Call after construction and before Render().
         */
        virtual auto Initialize() -> void override = 0;


        /**
         * @brief Releases the active rendering state and ends this renderer's use of its device.
         * The device itself remains owned by the caller.
         */
        virtual auto Shutdown() -> void override = 0;


        /**
         * @brief Renders the scene's synchronized state using the selected camera and settings.
         * @param scene Borrowed scene to render; keep it alive while used by this renderer.
         * Configure the main camera first; the caller remains responsible for Scene::Update().
         */
        virtual auto Render( const scene::Scene *scene ) -> void = 0;


        /**
         * @brief Selects the camera used for scene rendering, picking and camera-dependent passes.
         * @param camera Borrowed camera that must remain valid while used; nullptr clears it.
         */
        virtual auto SetMainCamera( const scene::SceneCamera *camera ) -> void = 0;


        /**
         * @brief Selects an additional camera for frustum-classification visualization.
         * @param camera Borrowed debug camera, or nullptr to clear it.
         * ForwardRenderer uses it when SceneRenderSettings::mVisualizeFrustum is enabled;
         * it does not replace the main rendering camera.
         */
        virtual auto SetCullingDebugCamera( const scene::Camera *camera ) -> void = 0;


        /**
         * @brief Controls selection outlines and armature visualization for this view.
         * @param enabled Whether these editor overlays may be drawn.
         */
        virtual auto SetEditorOverlaysEnabled( bool enabled ) -> void = 0;
        virtual auto SetInfoOverlayEnabled( bool enabled ) -> void = 0;

        /** Selects the image orientation used when sampling the output in an editor preview. */
        virtual auto SetOutputForPreview( bool ) -> void {
        }


        /**
         * @brief Sets the color used when the scene background is a clear color.
         * @param color Background clear color.
         */
        virtual auto SetClearColor( const rhi::Color &color ) -> void = 0;


        /**
         * @brief Selects the operator that maps scene-linear HDR color to display color.
         * @param type Tone-mapping operator.
         */
        virtual auto SetTonemapType( ToneMapper type ) -> void = 0;


        /**
         * @brief Sets the gamma adjustment used by display-color processing.
         * @param gamma Gamma value; 1.0 disables the gamma adjustment.
         */
        virtual auto SetGamma( core::f32 gamma ) -> void = 0;


        /**
         * @brief Sets the brightness scale used before tone mapping.
         * @param exposure Linear exposure multiplier.
         */
        virtual auto SetExposure( core::f32 exposure ) -> void = 0;


        /**
         * @brief Controls the screen-space ambient-occlusion contribution to raster shading.
         * @param enabled Whether SSAO contributes; material occlusion maps remain independent.
         */
        virtual auto SetEnableSsao( bool enabled ) -> void = 0;


        /**
         * @brief Sets the blur used when sampling the environment background.
         * @param blur Background blur amount.
         */
        virtual auto SetSkyboxBlur( core::f32 blur ) -> void = 0;


        /**
         * @brief Scales the configured environment-lighting contribution.
         * @param ambient Environment intensity multiplier.
         */
        virtual auto SetAmbientScale( core::f32 ambient ) -> void = 0;


        /**
         * @brief Selects the skybox material and environment textures.
         * @param material SkyboxMaterial handle, or an empty handle to clear the environment.
         * ForwardRenderer requires an initialized graph and treats missing required textures
         * as an empty environment. Changed inputs schedule derived-map updates.
         */
        virtual auto SetSkyboxMaterial( material::MaterialHandle material ) -> void = 0;


        /**
         * @brief Controls lighting from the configured environment independently of its visibility.
         * @param enabled Whether the configured environment contributes lighting.
         */
        virtual auto SetEnvironmentLightingEnabled( bool enabled ) -> void = 0;


        /**
         * @brief Selects a clear color, skybox or blurred environment as the visible background.
         * @param background Background source; environment-lighting controls are separate.
         */
        virtual auto SetRenderBackground( SceneBackgroundType background ) -> void = 0;


        /**
         * @brief Controls the infinite-grid pass when available in the active rendering path.
         * @param enabled Whether to draw the grid.
         */
        virtual auto SetEnableInfiniteGrid( bool enabled ) -> void = 0;


        /**
         * @brief Controls the wireframe overlay merged into the shaded output.
         * @param enabled Whether to overlay mesh edges; a selected wireframe debug output
         * can still require the wireframe pass when this overlay is disabled.
         */
        virtual auto SetEnablePolygonComplexity( bool enabled ) -> void = 0;


        /**
         * @brief Overrides the particle-simulation backend for this renderer.
         * @param mode CPU/GPU override, or eastl::nullopt to follow ParticleSystem's mode.
         * ForwardRenderer handles any required raster-graph replacement on Render().
         */
        virtual auto SetParticleSimulationMode( eastl::optional<ParticleSimulationMode> mode ) -> void = 0;


        /**
         * @brief Highlights the entity currently targeted by a texture drag-and-drop operation.
         * @param id Picking/entity ID to highlight; zero clears the hover highlight.
         */
        virtual auto SetTextureDropHover( core::u32 id ) -> void = 0;


        /**
         * @brief Selects skeleton visualization and whether ordinary meshes remain visible.
         * @param mode Disabled, armature with mesh, or armature only.
         * ForwardRenderer applies this mode only while editor overlays are enabled.
         */
        virtual auto SetArmatureDebugMode( ArmatureDebugMode mode ) -> void = 0;

        MKT_NODISCARD virtual auto GetStatistics() const -> FrameGraphStatistics = 0;


        /**
         * @brief Replaces the stored scene-rendering configuration.
         * @param settings Requested settings, copied into the renderer.
         * ForwardRenderer sanitizes values, limits features to device support and defers
         * required graph rebuilds until Render().
         */
        virtual auto ApplySettings( SceneRenderSettings settings ) -> void = 0;


        /**
         * @brief Returns a copy of the stored configuration, including pending changes.
         * @return Settings snapshot; apply edits through ApplySettings().
         */
        MKT_NODISCARD virtual auto GetSettings() const -> SceneRenderSettings = 0;


        /**
         * @brief Reports the renderer's output-resolution preset.
         * @return Current resolution; ForwardRenderer reports its active render state,
         * so a requested resize becomes visible here after the graph is rebuilt.
         */
        MKT_NODISCARD virtual auto GetRenderResolution() const -> rhi::RenderResolution = 0;


        /**
         * @brief Provides editable bloom configuration.
         * @return Borrowed renderer-owned settings; ForwardRenderer applies edits on Render().
         */
        MKT_NODISCARD virtual auto GetBloomSettings() -> BloomSettings & = 0;


        /**
         * @brief Resets progressive path-tracing accumulation for subsequent rendering.
         */
        virtual auto ResetPathAccumulation() -> void = 0;

        /** Hides the outgoing scene's image until a frame of the new scene is submitted.
         * Keeps its GPU resources alive while background pipeline preparation completes.
         */
        virtual auto InvalidateSceneOutput() -> void {
            mHasSceneOutput = false;
            CancelPick();
            ResetPathAccumulation();
        }


        /**
         * @brief Reports the path tracer's current accumulation counter.
         * @return Samples recorded since the last reset; zero when no samples are accumulated.
         * This counter does not imply that submitted GPU work has completed.
         */
        MKT_NODISCARD virtual auto GetPathSampleCount() const -> core::u32 = 0;
        /** Reports pending renderer preparation; only a frame of the same scene may remain visible. */
        MKT_NODISCARD virtual auto IsPreparing() const -> bool {
            return false;
        }

        MKT_NODISCARD virtual auto IsDenoiserActive() const -> bool {
            return false;
        }


        /**
         * @brief Exposes the diagnostic from the selected ray-tracing module.
         * @return Borrowed error string, empty when no diagnostic is recorded.
         */
        MKT_NODISCARD virtual auto GetRayTracingError() const -> const eastl::string & = 0;


        /**
         * @brief Selects the presented output and returns its current texture.
         * @param type Output/debug image to present; ForwardRenderer remembers this selection.
         * @return Retained texture handle, or empty when no output is available.
         * Selection changes take effect during subsequent rendering. Retrieving a handle
         * does not render the image or wait for GPU completion.
         */
        MKT_NODISCARD virtual auto GetFinalImage( FinalImageType type ) -> rhi::TextureHandle = 0;


        /**
         * @brief Returns the current texture for the remembered presentation selection.
         * @return Retained texture handle, or empty when no output is available.
         * Does not render the image or wait for GPU completion.
         */
        MKT_NODISCARD virtual auto GetFinalImage() -> rhi::TextureHandle = 0;


        /**
         * @brief Cancels the pending pick and discards any unconsumed pick result.
         * Outstanding readbacks may finish, but their cancelled results are not published.
         */
        virtual auto CancelPick() -> void = 0;


        /**
         * @brief Queues an asynchronous entity-ID query against the selection image.
         * @param viewport Pointer position and displayed viewport rectangle in the same
         * coordinate space, with a top-left viewport origin.
         * @return True when queued; false for an invalid position or unavailable picking state.
         * A new accepted request replaces the previous request and clears its result.
         * Rendering/readback processing must continue before GetPickResult() can return it.
         */
        MKT_NODISCARD virtual auto RequestPick( const ReadPixelViewportInfo &viewport ) -> bool = 0;


        /**
         * @brief Takes and clears the latest completed asynchronous pick result.
         * @return Entity ID, zero for a completed background hit, or eastl::nullopt when
         * no unconsumed result is available. Does not wait for a pending readback.
         */
        MKT_NODISCARD virtual auto GetPickResult() -> eastl::optional<core::u32> = 0;


        /**
         * @brief Reads visible entity IDs inside a rectangle from the last completed ID snapshot.
         * @param first First corner and its displayed viewport rectangle.
         * @param last Opposite corner in the same viewport and coordinate space.
         * @return Sorted unique nonzero IDs, or empty for invalid coordinates, unavailable
         * readback data or a rectangle containing no entities. Does not start a readback.
         */
        MKT_NODISCARD virtual auto ReadSelectionRectangle( const ReadPixelViewportInfo &first,
            const ReadPixelViewportInfo &last ) const -> eastl::vector<core::u32> = 0;


        /**
         * @brief Provides editable physics-debug drawing controls.
         * @return Borrowed settings for the physics overlay; reacquire after state replacement.
         */
        MKT_NODISCARD virtual auto GetPhysicsDebugSettings() -> physics::PhysicsDebugDrawSettings & = 0;


        /**
         * @brief Provides debug drawing controls for bounds, camera frustums and light ranges.
         * @return Borrowed overlay settings for diagnostic scene geometry.
         */
        MKT_NODISCARD virtual auto GetSceneVolumeSettings() -> SceneVolumeSettings & = 0;


        /**
         * @brief Reports physics-debug lines retained by the latest executed gather pass.
         * @return Line count, or zero when no lines were gathered.
         */
        MKT_NODISCARD virtual auto GetPhysicsDebugLineCount() const -> core::u32 = 0;


        /**
         * @brief Reports physics-debug lines omitted because the debug renderer reached capacity.
         * @return Dropped-line count from the latest executed gather pass.
         */
        MKT_NODISCARD virtual auto GetDroppedPhysicsDebugLineCount() const -> core::usize = 0;


        /**
         * @brief Exposes the latest staged instance-buffer upload statistics.
         * @return Borrowed byte, changed-record, copy-call and scatter-dispatch counters.
         */
        MKT_NODISCARD virtual auto GetInstanceUploadStatistics() const -> const SceneUploadStatistics & = 0;


        /**
         * @brief Exposes the latest staged material-buffer upload statistics.
         * @return Borrowed byte, changed-record, copy-call and scatter-dispatch counters.
         */
        MKT_NODISCARD virtual auto GetMaterialUploadStatistics() const -> const SceneUploadStatistics & = 0;


        /**
         * @brief Reports distinct material entries retained by this renderer's geometry cache.
         * @return Distinct cached materials within this renderer; instances can share entries.
         */
        MKT_NODISCARD virtual auto GetSharedMaterialCount() const -> core::usize = 0;


        /**
         * @brief Exposes shadow-light, map-submission and caster-culling counters.
         * @return Borrowed diagnostics for the most recently updated shadow state.
         */
        MKT_NODISCARD virtual auto GetShadowDiagnostics() const -> const ShadowDiagnostics & = 0;


        /**
         * @brief Indicates whether this renderer currently owns a frame graph.
         * @return True when a graph exists; this does not guarantee an available output image.
         */
        MKT_NODISCARD virtual auto HasFrameGraph() const -> bool = 0;

        /** @brief Returns whether the renderer is initialized and can produce a scene image. */
        MKT_NODISCARD virtual auto IsRenderAvailable() const -> bool {
            return HasFrameGraph();
        }


        /**
         * @brief Exposes frame-graph node descriptions and execution-control state for inspection.
         * @return Borrowed node control, or an empty control when no graph is available.
         */
        MKT_NODISCARD virtual auto GetNodeControl() const -> const FGNodeControl & = 0;


        /**
         * @brief Exposes the current frame-graph statistics indexed by pass name.
         * @return Borrowed statistics map, empty when no graph is available.
         */
        MKT_NODISCARD virtual auto GetPassStatistics() const
            -> const ankerl::unordered_dense::map<eastl::string, FGNodeStatistics> & = 0;


        /**
         * @brief Provides access to the active frame graph's GPU query profiler.
         * @return Borrowed profiler, or nullptr when unavailable; reacquire after graph replacement.
         * Query results may describe earlier submissions because GPU collection is asynchronous.
         */
        MKT_NODISCARD virtual auto GetGpuProfiler() const -> GpuQueryProfiler * = 0;

    protected:
        // Presentation belongs to a scene as well as to a render target. A
        // completed graph can still contain the outgoing project's last frame.
        const scene::Scene *mRequestedScene{};
        bool mHasSceneOutput{};
    };
}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_CORE_SCENE_RENDERER_HH
