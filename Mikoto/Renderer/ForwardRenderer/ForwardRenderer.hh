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

#ifndef MIKOTO_RENDERER_FORWARD_RENDERER_HH
#define MIKOTO_RENDERER_FORWARD_RENDERER_HH

#include <EASTL/array.h>
#include <EASTL/string.h>
#include <EASTL/optional.h>
#include <EASTL/shared_ptr.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/string_view.h>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Core/Renderer.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/ForwardRenderer/Passes/GuiModule.hh>
#include <Renderer/ForwardRenderer/Passes/DebugModule.hh>
#include <Renderer/ForwardRenderer/Passes/VideoModule.hh>
#include <Renderer/Core/SceneRenderer.hh>
#include <Renderer/ForwardRenderer/Passes/CameraModule.hh>
#include <Renderer/ForwardRenderer/Passes/HelperModule.hh>
#include <Renderer/ForwardRenderer/Passes/VolumeModule.hh>
#include <Renderer/ForwardRenderer/Passes/PrepassModule.hh>
#include <Renderer/ForwardRenderer/Passes/OutlineModule.hh>
#include <Renderer/ForwardRenderer/Passes/WeatherModule.hh>
#include <Renderer/ForwardRenderer/Passes/TonemapModule.hh>
#include <Renderer/ForwardRenderer/Passes/MaterialModule.hh>
#include <Renderer/ForwardRenderer/Passes/ShaderToyModule.hh>
#include <Renderer/ForwardRenderer/Passes/RayTracingModule.hh>
#include <Renderer/ForwardRenderer/Passes/TextRenderModule.hh>
#include <Renderer/ForwardRenderer/Passes/PathTracingModule.hh>
#include <Renderer/ForwardRenderer/Passes/PostProcessModule.hh>
#include <Renderer/ForwardRenderer/Passes/CpuPathTraceModule.hh>
#include <Renderer/ForwardRenderer/Passes/GeometryCullModule.hh>
#include <Renderer/ForwardRenderer/Passes/DebugOverlayModule.hh>
#include <Renderer/ForwardRenderer/Passes/PresentationModule.hh>
#include <Renderer/ForwardRenderer/Passes/MousePickingModule.hh>
#include <Renderer/ForwardRenderer/Passes/ShadowMappingModule.hh>
#include <Renderer/ForwardRenderer/Passes/DisplayEffectsModule.hh>
#include <Renderer/ForwardRenderer/Passes/GeometryShadingModule.hh>
#include <Renderer/ForwardRenderer/Passes/WaterSimulationModule.hh>
#include <Renderer/ForwardRenderer/Passes/IndirectLightingModule.hh>
#include <Renderer/ForwardRenderer/Passes/TerrainGenerationModule.hh>
#include <Renderer/ForwardRenderer/Passes/ParticleSimulationModule.hh>
#include <Renderer/ForwardRenderer/Passes/SubsurfaceScatteringModule.hh>
#include <Renderer/ForwardRenderer/Passes/AtmosphericScatteringModule.hh>

// Frame analysis reference: https://www.adriancourreges.com/blog/
namespace mikoto::renderer {

    class ForwardRenderer final : public ISceneRenderer {
    public:
        explicit ForwardRenderer( const SceneRendererCreateInfo& createInfo );

        auto Initialize() -> void override;
        auto Shutdown() -> void override;

        auto Render( const scene::Scene* scene ) -> void override;

        auto SetMainCamera( const scene::SceneCamera* camera ) -> void override;
        auto SetCullingDebugCamera( const scene::Camera* camera ) -> void override;
        auto SetEditorOverlaysEnabled( bool enabled ) -> void override;
        auto SetInfoOverlayEnabled( bool enabled ) -> void override;
        MKT_NODISCARD auto GetStatistics() const -> FrameGraphStatistics override;
        auto SetClearColor( const rhi::Color& color ) -> void override;

        auto SetTonemapType( ToneMapper type ) -> void override;

        auto SetGamma( core::f32 gamma ) -> void override;
        auto SetExposure( core::f32 exposure ) -> void override;

        auto GetBloomSettings() -> BloomSettings& override;

        auto SetEnableSsao( bool enabled ) -> void override;
        auto SetSsaoIntensity( core::f32 intensity ) -> void;

        auto SetSkyboxBlur( core::f32 blur ) -> void override;
        auto SetAmbientScale( core::f32 ambient ) -> void override;

        auto SetShadowMappDistance( core::f32 distance = 200 ) -> void;

        auto SetEnablePolygonComplexity( bool value ) -> void override;

        auto SetSkyboxMaterial( material::MaterialHandle material ) -> void override;
        auto SetEnvironmentLightingEnabled( bool enabled ) -> void override;

        auto SetRenderBackground( SceneBackgroundType bg ) -> void override;

        auto ResetPathAccumulation() -> void override;
        auto ApplySettings( SceneRenderSettings settings ) -> void override;

        /** @brief Selects the highest supported scene sample count at or below the request.
         * Construction and settings changes use the same device capability check.
         */
        auto SetMultisampling( rhi::Multisampling multisampling ) -> void;
        auto SetRenderResolution( rhi::RenderResolution resolution ) -> void;

        auto SetEnableInfiniteGrid( bool value ) -> void override;

        /** Overrides the global particle backend for an isolated scene/preview.
         * The caller must advance that scene's emitter instances using the same mode.
         * Pass nullopt to follow ParticleSystem again. Changing mode rebuilds the graph.
         */
        auto SetParticleSimulationMode( eastl::optional<ParticleSimulationMode> mode ) -> void override;

        auto GetPhysicsDebugSettings() -> physics::PhysicsDebugDrawSettings& override;
        auto GetSceneVolumeSettings() -> SceneVolumeSettings& override;
        auto SetArmatureDebugMode( ArmatureDebugMode mode ) -> void override;

        auto DisablePass( eastl::string_view passName ) -> void;
        auto EnablePass( eastl::string_view passName ) -> void;

        auto CancelPick() -> void override;
        auto SetOutlineStyle( const core::float4& color, core::f32 width ) -> void;

        MKT_NODISCARD auto GetSettings() const -> SceneRenderSettings override;
        MKT_NODISCARD auto GetPathSampleCount() const -> core::u32 override;
        MKT_NODISCARD auto GetRayTracingError() const -> const eastl::string& override;

        MKT_NODISCARD auto IsSsaoEnabled() const -> bool;
        MKT_NODISCARD auto GetSsaoIntensity() const -> core::f32;

        MKT_NODISCARD auto GetFinalImage( FinalImageType type ) -> rhi::TextureHandle override;
        MKT_NODISCARD auto GetFinalImage() -> rhi::TextureHandle override;

        MKT_NODISCARD auto RequestPick( const ReadPixelViewportInfo& viewport ) -> bool override;

        MKT_NODISCARD auto GetRenderResolution() const -> rhi::RenderResolution override;

        MKT_NODISCARD auto GetPickResult() -> eastl::optional<core::u32> override;
        MKT_NODISCARD auto ReadSelectionRectangle( const ReadPixelViewportInfo& first, const ReadPixelViewportInfo& last ) const -> eastl::vector<core::u32> override;
        auto SetTextureDropHover( core::u32 id ) -> void override;

        MKT_NODISCARD auto GetPhysicsDebugLineCount() const -> core::u32 override;
        MKT_NODISCARD auto GetDroppedPhysicsDebugLineCount() const -> core::usize override;

        MKT_NODISCARD auto GetInstanceUploadStatistics() const -> const SceneUploadStatistics& override;
        MKT_NODISCARD auto GetMaterialUploadStatistics() const -> const SceneUploadStatistics& override;
        MKT_NODISCARD auto GetSharedMaterialCount() const -> core::usize override;

        MKT_NODISCARD auto GetShadowDiagnostics() const -> const ShadowDiagnostics& override;

        MKT_NODISCARD auto ReadPixel( core::u32 x, core::u32 y) const -> core::u32;

        MKT_NODISCARD auto ReadPixel( const ReadPixelViewportInfo& ínfo ) const -> core::u32;

        MKT_NODISCARD auto GetNodeControl() const -> const FGNodeControl& override;
        MKT_NODISCARD auto HasFrameGraph() const -> bool override;

        MKT_NODISCARD auto GetPassList() const -> const ankerl::unordered_dense::map<eastl::string, FrameGraphPass>&;
        MKT_NODISCARD auto GetPassStatistics() const -> const ankerl::unordered_dense::map<eastl::string, FGNodeStatistics>& override;
        MKT_NODISCARD auto GetGpuProfiler() const -> GpuQueryProfiler* override;

    private:
        // Graph construction: configure modules, register one rendering path, then bind resources.
        auto ConfigureRenderState() -> void;

        auto RegisterPathTracingPasses() -> void;

        auto RegisterRasterPasses() -> void;

        auto BindGeometryManagers() -> void;

        auto ConfigureRasterExecutionPolicies() -> void;

        auto UpdateRenderRequirements( const scene::Scene* scene ) -> void;
        auto RebuildRenderState() -> void;
        auto BindSceneToPasses( const scene::Scene* scene ) -> bool;
        auto GetEnvironmentFaces() const -> eastl::array<rhi::TextureHandle, 6>;
        auto PreparePathTracingFrame( const scene::Scene* scene, const rhi::TextureHandle& environment, core::f32 environmentScale ) -> bool;
        auto PrepareRasterFrame( const scene::Scene* scene, const rhi::TextureHandle& environment, core::f32 environmentScale ) -> void;

        // Settings that can change without rebuilding graph resources.
        auto ApplyFrameSettings() -> void;

    private:
        rhi::IGpuDevice* mDevice{};
        const scene::Scene* mLastRenderedScene{};
        core::u64 mVideoRevision{};
        asset::ShaderLibrary* mShaderLibrary{};

        rhi::RenderResolution mTargetResolution{ rhi::RenderResolution::e1080P };
        rhi::ViewportConvention mViewportConvention{ rhi::ViewportConvention::eRightHanded_OriginBottomLeft };

        struct RenderState {
            explicit RenderState( rhi::RenderResolution resolution );

            rhi::RenderResolution mTargetResolution;
            rhi::Multisampling mMultisampling{ rhi::Multisampling::eMsaaX1 };
            ShadowMapQuality mShadowQuality{ ShadowMapQuality::eMedium };
            bool mCascadedShadowsEnabled{ true };

            // Scene prepass
            GeometryCullModule mGeometryManagement{};
            CameraModule mCameraPass{ mTargetResolution };
            PrepassModule mRenderPrepass{ mTargetResolution };
            ShadowMappingModule mShadowMapping{};
            TerrainGenerationModule mTerrainGenerationModule{ mTargetResolution };
            MousePickingModule mMousePickingModule{ mTargetResolution };
            OutlineModule mOutlineModule{ mTargetResolution };
            IndirectLightingModule mIndirectIlluminationModule{ mTargetResolution };

            // Simulations
            WeatherModule mWeatherModule{ mTargetResolution };
            WaterSimulationModule mWaterSimulation{ mTargetResolution };
            ParticleSimulationModule mParticleSimulation{ mTargetResolution };

            // Scene shading
            AtmosphericScatteringModule mAtmosModule{ mTargetResolution };
            VolumeModule mVolumeModule{ mTargetResolution };
            VideoModule mVideoModule{};
            GeometryShadingModule mGeometryShading{ mTargetResolution };
            SubsurfaceScatteringModule mSubsurfaceScattering{};
            material::MaterialHandle mSkyboxMaterial{};
            eastl::array<rhi::TextureHandle, 6> mSkyboxTextures;
            material::SkyboxType mSkyboxType{ material::SkyboxType::eEquirectangular };
            bool mSkyboxStateInitialized{};

            // Special effects
            GuiModule mGuiModule{ mTargetResolution };
            TonemapModule mTonemapModule{ mTargetResolution };
            TextRenderModule mTextRendering{ mTargetResolution };
            PostEffectsPass mPostEffectsPasses{ mTargetResolution };
            ShaderToyModule mShaderToyModule{ mTargetResolution };
            DisplayEffectsModule mDisplayEffectsModule{ mTargetResolution };

            HelperModule mHelperModule{};
            PresentationModule mPresentationModule{ mTargetResolution };

            // Raytracing
            bool mPathMode{};
            PathTracingModule mPathTracing{ mTargetResolution };
            RayTracingModule mRayTracingPass{ mTargetResolution };
            CpuPathTraceModule mCpuPathTraceModule{ rhi::RenderResolution::e720P }; // For now, it uses its own resolution this is very expensive

            // Debug Passes
            bool mArmatureMeshesHidden{};
            eastl::array<bool, 2> mArmatureHiddenPasses{};
            DebugModule mDebugPasses{ mTargetResolution };
            MaterialModule mMaterialModule{ mTargetResolution };
            DebugOverlayModule mDebugOverlayModule{ mTargetResolution };

            eastl::unique_ptr<FrameGraph> mFrameGraph{};
        };

        rhi::Color mClearColor{ rhi::kColorBlue };

        SceneRenderSettings mSettings{};
        eastl::shared_ptr<RenderState> mState{};

        const scene::SceneCamera* mMainCamera{};
        const scene::Camera* mCullingDebugCamera{};

        core::f32 mGamma{ 1.0f };
        core::f32 mExposure{ 1.0f };
        core::f32 mSkyboxBlur{ 0.05f };
        core::f32 mAmbientScale{ 1.0f };

        eastl::optional<ParticleSimulationMode> mParticleModeOverride{};

        bool mEnvironmentAllocated{};
        bool mSubsurfaceAllocated{};
        bool mFogAllocated{};
        bool mRebuildPending{ false };
        bool mOverlaysEnabled{ true };
        bool mInfoOverlayEnabled{};
        ArmatureDebugMode mArmatureDebugMode{ ArmatureDebugMode::eDisabled };
        bool mEnvironmentEnabled{ true };
        bool mInfiniteGrid{ false };
        bool mPolygonComplexity{ false };

        ToneMapper mToneMapping{ ToneMapper::eAces };
        SceneBackgroundType mBackground{ SceneBackgroundType::eSkybox };
        FinalImageType mFinalImageType{ FinalImageType::eTonemap_Output };
    };
}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_FORWARD_RENDERER_HH
