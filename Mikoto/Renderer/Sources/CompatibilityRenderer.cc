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

#include <Core/Profiler.hh>

#include <Material/SkyboxMaterial.hh>

#include <Scene/Scene.hh>
#include <Scene/SceneCamera.hh>

#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Rhi/Utility.hh>
#include <Renderer/Core/RenderSystem.hh>
#include <Renderer/CompatibilityRenderer/CompatibilityRenderer.hh>
#include <Renderer/CompatibilityRenderer/CompatibilityPassResources.hh>
#include <Renderer/CompatibilityRenderer/Passes/CompatibilityFrameModule.hh>
#include <Renderer/CompatibilityRenderer/Passes/CompatibilityShadowModule.hh>
#include <Renderer/CompatibilityRenderer/Passes/CompatibilityGeometryModule.hh>
#include <Renderer/CompatibilityRenderer/Passes/CompatibilitySelectionModule.hh>
#include <Renderer/CompatibilityRenderer/Passes/CompatibilityEnvironmentModule.hh>
#include <Renderer/CompatibilityRenderer/Passes/CompatibilityPostProcessModule.hh>
#include <Renderer/CompatibilityRenderer/Passes/CompatibilityDisplayEffectsModule.hh>

namespace mikoto::renderer {
    struct CompatibilityRenderer::Passes {
        CompatibilityFrameModule mFrame{};
        CompatibilityEnvironmentModule mEnvironment{};
        CompatibilityGeometryModule mGeometry{};
        CompatibilityShadowModule mShadows{};
        CompatibilitySelectionModule mSelection{};
        CompatibilityPostProcessModule mPostProcess{};
        CompatibilityDisplayEffectsModule mDisplayEffects{};
    };

    CompatibilityRenderer::CompatibilityRenderer( const SceneRendererCreateInfo& createInfo )
        : mDevice{ createInfo.mDevice } {
        mSettings.mResolution = createInfo.mResolution;
        mSettings.mMultisampling = createInfo.mMultisampling;
    }

    CompatibilityRenderer::~CompatibilityRenderer() = default;

    auto CompatibilityRenderer::Initialize() -> void {
        MKT_BEGIN_PROFILER();

        if ( mPasses ) {
            return;
        }

        auto passes{ eastl::make_unique<Passes>() };
        passes->mFrame.Initialize( mDevice );
        passes->mEnvironment.Initialize( mDevice );
        passes->mGeometry.Initialize( mDevice );
        passes->mPostProcess.Initialize( mDevice );
        passes->mSelection.Initialize( mDevice );

        UpdateTargets();
        mPasses = eastl::move( passes );
    }

    auto CompatibilityRenderer::Shutdown() -> void {
        mPasses.reset();
        mSceneColor.Reset();
        mDepth.Reset();
        mFinalImage.Reset();
        mSkybox.Reset();
        mCamera = nullptr;
        mTextureDropHover = 0;
    }

    auto CompatibilityRenderer::UpdateTargets() -> void {
        const auto [width, height]{ rhi::InferDimensions( mSettings.mResolution, 0 ) };
        if ( width == 0 || height == 0 ) {
            return;
        }

        const rhi::Extent2D size{ .mWidth = width, .mHeight = height };
        auto color{ mSceneColor };
        auto depth{ mDepth };
        ResizeCompatibilityTarget( mDevice, color, size, kCompatibilitySceneFormat,
            rhi::TextureUsageFlagsBits::RenderTarget | rhi::TextureUsageFlagsBits::ShaderResource, "Compatibility HDR scene color" );
        ResizeCompatibilityTarget( mDevice, depth, size, kCompatibilityDepthFormat,
            rhi::TextureUsageFlagsBits::DepthTarget, "Compatibility scene depth" );

        mSceneColor = eastl::move( color );
        mDepth = eastl::move( depth );
    }

    auto CompatibilityRenderer::RecordScenePass( rhi::ICommandList* commands, const scene::Scene* scene ) -> bool {
        const auto environment{ mPasses->mEnvironment.Prepare( commands, dynamic_cast<const material::SkyboxMaterial*>( mSkybox.GetPtr() ),
            mEnvironmentEnabled ? mEnvironmentScale : 0.0f, mBackground == SceneBackgroundType::ePrefilterMap ? mSkyboxBlur : 0.0f ) };

        bool drawOutline{};
        if ( mCamera && scene ) {
            scene->UpdateStreamingView( this, *mCamera, false );
            mPasses->mGeometry.Prepare( *scene, *mCamera, mTextureDropHover );
            mPasses->mShadows.Record( mDevice, commands, *scene, *mCamera, mPasses->mGeometry, environment, mSettings );
            drawOutline = mEditorOverlaysEnabled && mPasses->mGeometry.HasHighlights();
        }

        const auto shadowMap{ mCamera && scene ? mPasses->mShadows.GetMap() : rhi::TextureHandle{} };
        auto frameUniform{ mPasses->mFrame.Prepare( commands, scene, mCamera, environment.mParameters,
            mPasses->mShadows.GetMatrix(), shadowMap ? mPasses->mShadows.GetLightIndex() : 0u ) };

        const rhi::Extent2D size{ .mWidth = mSceneColor->GetWidth(), .mHeight = mSceneColor->GetHeight() };
        auto scenePass{ rhi::RenderPassDescription{}
            .SetScopeName( "Compatibility Scene" )
            .SetRenderArea( rhi::Rect{ core::as<core::i32>( size.mWidth ), core::as<core::i32>( size.mHeight ) } )
            .AddDepthTarget( mDepth )
            .AddRenderTarget( mSceneColor, {} ) };
        commands->SetTransition( rhi::TransitionDescription{}
            .AddTexture( mSceneColor.GetPtr(), rhi::ResourceStates::eRenderTarget )
            .AddTexture( mDepth.GetPtr(), rhi::ResourceStates::eDepthWrite ) );
        commands->BeginRenderPass( scenePass );
        SetCompatibilityViewport( commands, size );

        // Skybox and geometry share the HDR attachments and clear them once.
        if ( mCamera && mBackground != SceneBackgroundType::eClearColor && environment.mHasSkybox ) {
            mPasses->mEnvironment.RecordSkybox( commands, environment, *frameUniform.GetPtr() );
        }

        if ( mCamera && scene ) {
            mPasses->mGeometry.Record( commands, environment, *frameUniform.GetPtr(), shadowMap );
        }

        commands->EndRenderPass();

        if ( mCamera && scene && ( drawOutline || mPasses->mSelection.NeedsPass() ) ) {
            mPasses->mSelection.Record( commands, size, mPasses->mGeometry, environment, *frameUniform.GetPtr() );
        }

        return drawOutline;
    }

    auto CompatibilityRenderer::RecordOutputPasses( rhi::ICommandList* commands, bool drawOutline ) -> rhi::TextureHandle {
        auto toneMapped{ mPasses->mPostProcess.RecordToneMap( commands, mSceneColor, {
            .mClearColor = mClearColor,
            .mExposure = mExposure,
            .mGamma = mGamma,
            .mToneMapper = core::as<core::i32>( mToneMapper ),
            .mOutputForPreview = mOutputForPreview
        } ) };
        const auto& aberration{ mSettings.mChromaticAberration };
        auto finalImage{ mPasses->mDisplayEffects.RecordChromaticAberration( mDevice, commands, toneMapped,
            aberration.mEnabled ? aberration.mIntensity : 0.0f, aberration.mDirection ) };
        if ( drawOutline ) {
            finalImage = mPasses->mSelection.RecordOutline( commands, finalImage,
                mSettings.mSelectionOutlineColor, mSettings.mSelectionOutlineWidth, mOutputForPreview );
        }

        return finalImage;
    }

    auto CompatibilityRenderer::Render( const scene::Scene* scene ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !mPasses ) {
            return;
        }

        UpdateTargets();
        if ( !mSceneColor ) {
            return;
        }

        auto commands{ mDevice->CreateCommandList( rhi::QueueType::eGraphics ) };
        commands->SetEnableAutomaticBarriers( true );
        commands->Open( {} );
        const bool drawOutline{ RecordScenePass( commands.GetPtr(), scene ) };
        auto finalImage{ RecordOutputPasses( commands.GetPtr(), drawOutline ) };

        mPasses->mGeometry.ClearDraws();
        commands->Close();

        auto submission{ rhi::SubmitInfo{}.AddCommandList( commands ) };
        RenderSystem::Get()->BatchSubmission( eastl::move( submission ), rhi::QueueType::eGraphics );
        mFinalImage = eastl::move( finalImage );
    }

    auto CompatibilityRenderer::SetMainCamera( const scene::SceneCamera* camera ) -> void {
        mCamera = camera;
    }

    auto CompatibilityRenderer::SetCullingDebugCamera( const scene::Camera* ) -> void {
    }

    auto CompatibilityRenderer::SetEditorOverlaysEnabled( bool enabled ) -> void {
        mEditorOverlaysEnabled = enabled;
    }

    auto CompatibilityRenderer::SetOutputForPreview( bool preview ) -> void {
        mOutputForPreview = preview;
    }

    auto CompatibilityRenderer::SetClearColor( const rhi::Color& color ) -> void {
        mClearColor = color;
    }

    auto CompatibilityRenderer::ApplySettings( SceneRenderSettings settings ) -> void {
        settings.Sanitize();
        if ( settings.mResolution != mSettings.mResolution ) {
            CancelPick();
        }
        mSettings = settings;
    }

    auto CompatibilityRenderer::GetSettings() const -> SceneRenderSettings {
        return mSettings;
    }

    auto CompatibilityRenderer::GetRenderResolution() const -> rhi::RenderResolution {
        return mSettings.mResolution;
    }

    auto CompatibilityRenderer::SetTonemapType( ToneMapper type ) -> void {
        mToneMapper = type;
    }

    auto CompatibilityRenderer::SetGamma( core::f32 gamma ) -> void {
        mGamma = gamma;
    }

    auto CompatibilityRenderer::SetExposure( core::f32 exposure ) -> void {
        mExposure = exposure;
    }

    auto CompatibilityRenderer::SetEnableSsao( bool enabled ) -> void {
        mSettings.mSsaoEnabled = enabled;
    }

    auto CompatibilityRenderer::GetBloomSettings() -> BloomSettings& {
        return mSettings.mBloom;
    }

    auto CompatibilityRenderer::SetSkyboxBlur( core::f32 blur ) -> void {
        mSkyboxBlur = blur;
    }

    auto CompatibilityRenderer::SetAmbientScale( core::f32 ambient ) -> void {
        mEnvironmentScale = ambient;
    }

    auto CompatibilityRenderer::SetSkyboxMaterial( material::MaterialHandle material ) -> void {
        mSkybox = eastl::move( material );
    }

    auto CompatibilityRenderer::SetEnvironmentLightingEnabled( bool enabled ) -> void {
        mEnvironmentEnabled = enabled;
    }

    auto CompatibilityRenderer::SetRenderBackground( SceneBackgroundType background ) -> void {
        mBackground = background;
    }

    auto CompatibilityRenderer::SetEnableInfiniteGrid( bool ) -> void {
    }

    auto CompatibilityRenderer::SetEnablePolygonComplexity( bool ) -> void {
    }

    auto CompatibilityRenderer::SetParticleSimulationMode( eastl::optional<ParticleSimulationMode> ) -> void {
    }

    auto CompatibilityRenderer::ResetPathAccumulation() -> void {
    }

    auto CompatibilityRenderer::GetPathSampleCount() const -> core::u32 {
        return {};
    }

    auto CompatibilityRenderer::GetRayTracingError() const -> const eastl::string& {
        static const eastl::string empty{};
        return empty;
    }

    auto CompatibilityRenderer::GetFinalImage( FinalImageType type ) -> rhi::TextureHandle {
        return mPasses && type == FinalImageType::eTonemap_Output ? mPasses->mPostProcess.GetOutput() : rhi::TextureHandle{};
    }

    auto CompatibilityRenderer::GetFinalImage() -> rhi::TextureHandle {
        return mFinalImage;
    }

    auto CompatibilityRenderer::CancelPick() -> void {
        if ( mPasses ) {
            mPasses->mSelection.Cancel();
        }
    }

    auto CompatibilityRenderer::SetTextureDropHover( core::u32 id ) -> void {
        mTextureDropHover = id;
    }

    auto CompatibilityRenderer::RequestPick( const ReadPixelViewportInfo& viewport ) -> bool {
        if ( !mPasses || !mSceneColor ) {
            return false;
        }

        return mPasses->mSelection.Request( viewport, { .mWidth = mSceneColor->GetWidth(), .mHeight = mSceneColor->GetHeight() } );
    }

    auto CompatibilityRenderer::GetPickResult() -> eastl::optional<core::u32> {
        return mPasses ? mPasses->mSelection.TakeResult() : eastl::optional<core::u32>{};
    }

    auto CompatibilityRenderer::ReadSelectionRectangle( const ReadPixelViewportInfo& first,
        const ReadPixelViewportInfo& last ) const -> eastl::vector<core::u32> {
        return mPasses ? mPasses->mSelection.ReadRectangle( first, last ) : eastl::vector<core::u32>{};
    }

    auto CompatibilityRenderer::SetArmatureDebugMode( ArmatureDebugMode ) -> void {
    }

    auto CompatibilityRenderer::GetPhysicsDebugSettings() -> physics::PhysicsDebugDrawSettings& {
        return mPhysicsDebugSettings;
    }

    auto CompatibilityRenderer::GetSceneVolumeSettings() -> SceneVolumeSettings& {
        return mSceneVolumeSettings;
    }

    auto CompatibilityRenderer::GetPhysicsDebugLineCount() const -> core::u32 {
        return {};
    }

    auto CompatibilityRenderer::GetDroppedPhysicsDebugLineCount() const -> core::usize {
        return {};
    }

    auto CompatibilityRenderer::GetInstanceUploadStatistics() const -> const SceneUploadStatistics& {
        static const SceneUploadStatistics empty{};
        return empty;
    }

    auto CompatibilityRenderer::GetMaterialUploadStatistics() const -> const SceneUploadStatistics& {
        static const SceneUploadStatistics empty{};
        return empty;
    }

    auto CompatibilityRenderer::GetSharedMaterialCount() const -> core::usize {
        return {};
    }

    auto CompatibilityRenderer::GetShadowDiagnostics() const -> const ShadowDiagnostics& {
        static const ShadowDiagnostics empty{};
        return mPasses ? mPasses->mShadows.GetDiagnostics() : empty;
    }

    auto CompatibilityRenderer::HasFrameGraph() const -> bool {
        return {};
    }

    auto CompatibilityRenderer::IsRenderAvailable() const -> bool {
        return mPasses != nullptr;
    }

    auto CompatibilityRenderer::GetNodeControl() const -> const FGNodeControl& {
        static const FGNodeControl empty{};
        return empty;
    }

    auto CompatibilityRenderer::GetPassStatistics() const
        -> const ankerl::unordered_dense::map<eastl::string, FGNodeStatistics>& {
        static const ankerl::unordered_dense::map<eastl::string, FGNodeStatistics> empty{};
        return empty;
    }

    auto CompatibilityRenderer::GetGpuProfiler() const -> GpuQueryProfiler* {
        return nullptr;
    }
}// namespace mikoto::renderer
