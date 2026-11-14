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

#ifndef MIKOTO_RENDERER_COMPATIBILITY_RENDERER_HH
#define MIKOTO_RENDERER_COMPATIBILITY_RENDERER_HH

#include <Renderer/Core/SceneRenderer.hh>

namespace mikoto::renderer {

    /** Renders the scene with conventional vertex input and fixed resource bindings. */
    class CompatibilityRenderer final : public ISceneRenderer {
    public:
        explicit CompatibilityRenderer( const SceneRendererCreateInfo& createInfo );
        ~CompatibilityRenderer() override;

        auto Initialize() -> void override;
        auto Shutdown() -> void override;
        auto Render( const scene::Scene* scene ) -> void override;

        auto SetMainCamera( const scene::SceneCamera* camera ) -> void override;
        auto SetCullingDebugCamera( const scene::Camera* camera ) -> void override;
        auto SetEditorOverlaysEnabled( bool enabled ) -> void override;
        auto SetOutputForPreview( bool preview ) -> void override;
        auto SetClearColor( const rhi::Color& color ) -> void override;

        auto ApplySettings( SceneRenderSettings settings ) -> void override;
        MKT_NODISCARD auto GetSettings() const -> SceneRenderSettings override;
        MKT_NODISCARD auto GetRenderResolution() const -> rhi::RenderResolution override;

        auto SetTonemapType( ToneMapper type ) -> void override;
        auto SetGamma( core::f32 gamma ) -> void override;
        auto SetExposure( core::f32 exposure ) -> void override;
        auto SetEnableSsao( bool enabled ) -> void override;
        MKT_NODISCARD auto GetBloomSettings() -> BloomSettings& override;

        auto SetSkyboxBlur( core::f32 blur ) -> void override;
        auto SetAmbientScale( core::f32 ambient ) -> void override;
        auto SetSkyboxMaterial( material::MaterialHandle material ) -> void override;
        auto SetEnvironmentLightingEnabled( bool enabled ) -> void override;
        auto SetRenderBackground( SceneBackgroundType background ) -> void override;

        auto SetEnableInfiniteGrid( bool enabled ) -> void override;
        auto SetEnablePolygonComplexity( bool enabled ) -> void override;
        auto SetParticleSimulationMode( eastl::optional<ParticleSimulationMode> mode ) -> void override;

        auto ResetPathAccumulation() -> void override;
        MKT_NODISCARD auto GetPathSampleCount() const -> core::u32 override;
        MKT_NODISCARD auto GetRayTracingError() const -> const eastl::string& override;

        MKT_NODISCARD auto GetFinalImage( FinalImageType type ) -> rhi::TextureHandle override;
        MKT_NODISCARD auto GetFinalImage() -> rhi::TextureHandle override;

        auto CancelPick() -> void override;
        auto SetTextureDropHover( core::u32 id ) -> void override;
        MKT_NODISCARD auto RequestPick( const ReadPixelViewportInfo& viewport ) -> bool override;
        MKT_NODISCARD auto GetPickResult() -> eastl::optional<core::u32> override;
        MKT_NODISCARD auto ReadSelectionRectangle( const ReadPixelViewportInfo& first,
            const ReadPixelViewportInfo& last ) const -> eastl::vector<core::u32> override;

        auto SetArmatureDebugMode( ArmatureDebugMode mode ) -> void override;
        MKT_NODISCARD auto GetPhysicsDebugSettings() -> physics::PhysicsDebugDrawSettings& override;
        MKT_NODISCARD auto GetSceneVolumeSettings() -> SceneVolumeSettings& override;
        MKT_NODISCARD auto GetPhysicsDebugLineCount() const -> core::u32 override;
        MKT_NODISCARD auto GetDroppedPhysicsDebugLineCount() const -> core::usize override;

        MKT_NODISCARD auto GetInstanceUploadStatistics() const -> const SceneUploadStatistics& override;
        MKT_NODISCARD auto GetMaterialUploadStatistics() const -> const SceneUploadStatistics& override;
        MKT_NODISCARD auto GetSharedMaterialCount() const -> core::usize override;
        MKT_NODISCARD auto GetShadowDiagnostics() const -> const ShadowDiagnostics& override;

        MKT_NODISCARD auto HasFrameGraph() const -> bool override;
        MKT_NODISCARD auto IsRenderAvailable() const -> bool override;
        MKT_NODISCARD auto GetNodeControl() const -> const FGNodeControl& override;
        MKT_NODISCARD auto GetPassStatistics() const
            -> const ankerl::unordered_dense::map<eastl::string, FGNodeStatistics>& override;
        MKT_NODISCARD auto GetGpuProfiler() const -> GpuQueryProfiler* override;

    private:
        struct Passes;

        auto UpdateTargets() -> void;
        auto RecordScenePass( rhi::ICommandList& commands, const scene::Scene* scene ) -> void;

    private:

        rhi::IGpuDevice* mDevice{};
        const scene::SceneCamera* mCamera{};
        eastl::unique_ptr<Passes> mPasses{};
        material::MaterialHandle mSkybox{};
        rhi::TextureHandle mSceneColor{};
        rhi::TextureHandle mDepth{};
        rhi::TextureHandle mFinalImage{};

        SceneRenderSettings mSettings{};
        SceneVolumeSettings mSceneVolumeSettings{};
        physics::PhysicsDebugDrawSettings mPhysicsDebugSettings{};

        rhi::Color mClearColor{};
        
        ToneMapper mToneMapper{ ToneMapper::eAces };
        SceneBackgroundType mBackground{ SceneBackgroundType::eSkybox };
        
        core::f32 mSkyboxBlur{};
        core::f32 mEnvironmentScale{ 1.0f };
        core::f32 mGamma{ 2.2f };
        core::f32 mExposure{ 1.0f };
        core::u32 mTextureDropHover{};

        bool mEnvironmentEnabled{ true };
        bool mEditorOverlaysEnabled{ true };
        bool mOutputForPreview{};
    };
}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_COMPATIBILITY_RENDERER_HH
