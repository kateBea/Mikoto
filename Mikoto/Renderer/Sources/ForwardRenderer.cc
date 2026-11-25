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

#include <exception>

#include <EASTL/string.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Logging/Logger.hh>

#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Core/RenderSystem.hh>
#include <Renderer/ForwardRenderer/ForwardRenderer.hh>
#include <Renderer/Particle/ParticleSystem.hh>
#include <Renderer/ForwardRenderer/Passes/PostProcessModule.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::scene;
    using namespace mikoto::material;
    using namespace mikoto::renderer::rhi;

    ForwardRenderer::ForwardRenderer( const SceneRendererCreateInfo& createInfo )
        : mDevice{ createInfo.mDevice },
          mTargetResolution{ createInfo.mResolution },
          mViewportConvention{ createInfo.mViewportConvention },
          mState{ eastl::make_shared<RenderState>( createInfo.mResolution ) } {
        mSettings.mResolution = createInfo.mResolution;
        mSettings.mMultisampling = createInfo.mMultisampling;

        ApplySettings( mSettings );

        mState = eastl::make_shared<RenderState>( mTargetResolution );
        mRebuildPending = false;
    }

    auto ForwardRenderer::Initialize() -> void {
        MKT_BEGIN_PROFILER();

        ConfigureRenderState();
        mState->mVolumeModule.RegisterUpload( *mState->mFrameGraph );

        // Static procedural textures: generate and export once per graph lifetime.
        mState->mRenderPrepass.RegisterPerlinNoise( *mState->mFrameGraph );
        mState->mRenderPrepass.RegisterFractalBrownianMotion( *mState->mFrameGraph );

        if ( mState->mPathMode ) {
            RegisterPathTracingPasses();
        } else {
            RegisterRasterPasses();
        }

        mState->mVideoModule.RegisterPasses( *mState->mFrameGraph );
        mState->mDebugPasses.RegisterInfoOverlayPass( *mState->mFrameGraph );
        mState->mPresentationModule.RegisterTransition( *mState->mFrameGraph, mDevice->SupportsRayTracing() );
        mState->mFrameGraph->Compile();

        BindGeometryManagers();

        if ( !mState->mPathMode ) {
            ConfigureRasterExecutionPolicies();
        }

        mRebuildPending = false;
    }

    auto ForwardRenderer::ConfigureRenderState() -> void {
        mShaderLibrary = asset::AssetRegistryService::Get()->GetShaderLibrary();

        // Settings retained by this graph, including rebuild rollback state.
        mState->mMultisampling = mSettings.mMultisampling;
        mState->mShadowQuality = mSettings.mShadowQuality;
        mState->mCascadedShadowsEnabled = mSettings.mCascadedShadowsEnabled;
        mState->mPathMode = mSettings.mRayTracing.mPathTracingEnabled && mDevice->SupportsRayTracing();
        mState->mSkyboxStateInitialized = false;

        // Shadow resources and editor selection appearance.
        mState->mShadowMapping.SetShadowMapsQuality( mSettings.mShadowQuality );
        mState->mShadowMapping.SetCascadedShadowsEnabled( mSettings.mCascadedShadowsEnabled );
        mState->mShadowMapping.SetShadowMappDistance( mSettings.mShadowDistance );
        SetOutlineStyle( mSettings.mSelectionOutlineColor, mSettings.mSelectionOutlineWidth );

        mState->mShadowMapping.SetSoftShadowsEnabled( mSettings.mSoftShadowsEnabled );

        // Raster builders consume these settings while registering their passes.
        if ( !mState->mPathMode ) {
            mState->mGeometryShading.SetMultisampling( mSettings.mMultisampling );
            mState->mGeometryShading.SetEnvironmentAllocated( mEnvironmentAllocated );
        }

        mState->mFrameGraph = FrameGraph::Create( mDevice, mShaderLibrary );
    }

    auto ForwardRenderer::RegisterPathTracingPasses() -> void {
        // Path tracing replaces scene lighting, not editor interaction.
        mState->mCameraPass.RegisterCameraSetupPass( *mState->mFrameGraph );
        mState->mGeometryManagement.RegisterGeometryFilterPass( *mState->mFrameGraph );
        mState->mGeometryManagement.RegisterSceneUploadPass( *mState->mFrameGraph );
        mState->mGeometryManagement.RegisterSkinningPass( *mState->mFrameGraph );
        mState->mGeometryManagement.RegisterFrustumCullPass( *mState->mFrameGraph );
        mState->mRenderPrepass.RegisterDepthPrepass( *mState->mFrameGraph );
        mState->mTerrainGenerationModule.RegisterTerrainGeneration( *mState->mFrameGraph );
        mState->mMousePickingModule.RegisterSelectionBuffer( *mState->mFrameGraph );

        // Atmosphere and path-traced lighting.
        mState->mAtmosModule.RegisterParameterUpload( *mState->mFrameGraph );
        mState->mAtmosModule.RegisterTransmittance( *mState->mFrameGraph );
        mState->mAtmosModule.RegisterMultipleScattering( *mState->mFrameGraph );
        mState->mAtmosModule.RegisterSkyView( *mState->mFrameGraph );

        auto& pathTracing{ mState->mPathTracing.GetRayTracingModule() };
        pathTracing.RegisterUploadPass( *mState->mFrameGraph );
        pathTracing.RegisterBuildPass( *mState->mFrameGraph );
        pathTracing.RegisterFramePass( *mState->mFrameGraph );
        pathTracing.RegisterTracePass( *mState->mFrameGraph, true, true );
        pathTracing.RegisterHistoryPass( *mState->mFrameGraph );

        // Keep transport history in scene-linear HDR. Shared display passes consume the finished
        // accumulation, so bloom and exposure edits never feed back into subsequent path samples.
        // Pipeline and extension points: Renderer/Documentation/PathTracingDisplay.md.
        mState->mPostEffectsPasses.RegisterPostProcess( *mState->mFrameGraph,
            material::PostProcessStage::eBeforeToneMapping, mState->mFrameGraph->GetData<RayTracingInfo>().mDirect );

        mState->mPostEffectsPasses.RegisterBloom( *mState->mFrameGraph,
            mState->mFrameGraph->GetData<PostProcessModuleInfo>().mBeforeToneMapping.mColorImage );
        
        mState->mTonemapModule.RegisterTonemapPass( *mState->mFrameGraph );
        mState->mPostEffectsPasses.RegisterInfiniteGrid( *mState->mFrameGraph );

        // Display-space materials (including film grain), UI and overlays follow tone mapping.
        mState->mPostEffectsPasses.RegisterPostProcess( *mState->mFrameGraph,
            material::PostProcessStage::eAfterToneMapping,
            mState->mFrameGraph->GetData<GeomShadingModuleInfo>().mTonemapColor );

        mState->mFrameGraph->GetOrCreate<PathTracingInfo>().mOutput =
            mState->mFrameGraph->GetData<PostProcessModuleInfo>().mAfterToneMapping.mColorImage;

        // Selection outline, final output, and editor overlays.
        mState->mOutlineModule.RegisterHighlightFilter( *mState->mFrameGraph );
        mState->mOutlineModule.RegisterInteriorMask( *mState->mFrameGraph, rhi::Multisampling::eMsaaX1 );
        mState->mOutlineModule.RegisterSilhouette( *mState->mFrameGraph, rhi::Multisampling::eMsaaX1 );
        mState->mOutlineModule.RegisterSeedInit( *mState->mFrameGraph );
        mState->mOutlineModule.RegisterJumpFlood( *mState->mFrameGraph );
        mState->mOutlineModule.RegisterOutline( *mState->mFrameGraph, rhi::Multisampling::eMsaaX1 );

        mState->mPresentationModule.RegisterFullQuadRender( *mState->mFrameGraph,
            mState->mFrameGraph->GetOrCreate<PathTracingInfo>().mOutput );
        mState->mOutlineModule.RegisterComposite( *mState->mFrameGraph );
        mState->mTerrainGenerationModule.RegisterTerrainRendering( *mState->mFrameGraph, false );
        mState->mWaterSimulation.RegisterWaterSimulation( *mState->mFrameGraph );

        // Composite game UI and editor diagnostics after the scene image is available.
        mState->mGuiModule.RegisterUploadPass( *mState->mFrameGraph );
        mState->mGuiModule.RegisterRenderPass( *mState->mFrameGraph );
        mState->mDebugOverlayModule.RegisterPhysicsDebugger( *mState->mFrameGraph );
        mState->mDebugOverlayModule.RegisterSceneVolumes( *mState->mFrameGraph );
        mState->mDebugPasses.RegisterArmaturePass( *mState->mFrameGraph );
    }

    auto ForwardRenderer::RegisterRasterPasses() -> void {
        // Material previews and diagnostic targets.
        mState->mDebugPasses.RegisterTrianglePass( *mState->mFrameGraph );
        mState->mDebugPasses.RegisterTexturePass( *mState->mFrameGraph );
        mState->mDebugPasses.RegisterSimpleComputePass( *mState->mFrameGraph );

        // Camera, geometry, simulation, depth, shadows, and picking.
        mState->mCameraPass.RegisterCameraSetupPass( *mState->mFrameGraph );
        mState->mGeometryManagement.RegisterGeometryFilterPass( *mState->mFrameGraph );
        mState->mGeometryManagement.RegisterSceneUploadPass( *mState->mFrameGraph );
        mState->mGeometryManagement.RegisterSkinningPass( *mState->mFrameGraph );
        mState->mGeometryManagement.RegisterFrustumCullPass( *mState->mFrameGraph );
        mState->mRenderPrepass.RegisterAabb( *mState->mFrameGraph );
        mState->mRenderPrepass.RegisterLightCulling( *mState->mFrameGraph );
        mState->mRenderPrepass.RegisterDepthPrepass( *mState->mFrameGraph );
        mState->mRenderPrepass.RegisterGBuffer( *mState->mFrameGraph );
        mState->mShadowMapping.RegisterUploadPass( *mState->mFrameGraph );
        mState->mShadowMapping.RegisterShadowMaps( *mState->mFrameGraph );
        mState->mShadowMapping.RegisterDirectionalVisibility( *mState->mFrameGraph, mTargetResolution );
        mState->mTerrainGenerationModule.RegisterTerrainGeneration( *mState->mFrameGraph );

        const auto* particleSystem{ ParticleSystem::GetPtr() };
        mState->mParticleSimulation.SetSimulationMode(
            mParticleModeOverride.value_or( particleSystem ? particleSystem->GetSimulationMode() : ParticleSimulationMode::eCpu ) );

        mState->mParticleSimulation.RegisterSimulationPasses( *mState->mFrameGraph );

        mState->mMousePickingModule.RegisterSelectionBuffer( *mState->mFrameGraph );

        // Highlighted geometry owns its mask and distances, independently of picking.
        mState->mOutlineModule.RegisterHighlightFilter( *mState->mFrameGraph );
        mState->mOutlineModule.RegisterInteriorMask( *mState->mFrameGraph, mSettings.mMultisampling );
        mState->mOutlineModule.RegisterSilhouette( *mState->mFrameGraph, mSettings.mMultisampling );
        mState->mOutlineModule.RegisterSeedInit( *mState->mFrameGraph );
        mState->mOutlineModule.RegisterJumpFlood( *mState->mFrameGraph );

        // SSAO consumes the G-buffer and must be registered before PBR,
        // whose builder immediately declares its ambient-occlusion input.
        mState->mPostEffectsPasses.RegisterSsao( *mState->mFrameGraph );
        mState->mAtmosModule.RegisterParameterUpload( *mState->mFrameGraph );
        mState->mAtmosModule.RegisterTransmittance( *mState->mFrameGraph );
        mState->mAtmosModule.RegisterMultipleScattering( *mState->mFrameGraph );
        mState->mAtmosModule.RegisterSkyView( *mState->mFrameGraph );
        mState->mAtmosModule.RegisterEnvironment( *mState->mFrameGraph );
        mState->mAtmosModule.RegisterAerialPerspective( *mState->mFrameGraph );

        if ( mDevice->SupportsRayTracing() ) {
            mState->mRayTracingPass.RegisterUploadPass( *mState->mFrameGraph );
            mState->mRayTracingPass.RegisterBuildPass( *mState->mFrameGraph );
            mState->mRayTracingPass.RegisterFramePass( *mState->mFrameGraph );
            mState->mRayTracingPass.RegisterTracePass( *mState->mFrameGraph );
            mState->mRayTracingPass.RegisterHistoryPass( *mState->mFrameGraph );
        }

        // Opaque lighting and material effects.
        mState->mGeometryShading.RegisterSkyboxGeometryUpload( *mState->mFrameGraph );
        mState->mGeometryShading.RegisterBrdfLut( *mState->mFrameGraph );
        mState->mGeometryShading.RegisterSkyboxProjection( *mState->mFrameGraph );
        mState->mGeometryShading.RegisterSkyboxMipmaps( *mState->mFrameGraph );
        mState->mGeometryShading.RegisterPrefilter( *mState->mFrameGraph );
        mState->mGeometryShading.RegisterIrradiance( *mState->mFrameGraph );
        mState->mGeometryShading.RegisterSkyboxRender( *mState->mFrameGraph );
        if ( mSettings.mScreenSpaceReflections.mEnabled ) {
            mState->mIndirectIlluminationModule.SetSettings( mSettings.mScreenSpaceReflections );
            mState->mIndirectIlluminationModule.RegisterSurfaceTargets( *mState->mFrameGraph, mSettings.mMultisampling );
        }
        mState->mGeometryShading.RegisterShading( *mState->mFrameGraph );

        if ( mSubsurfaceAllocated ) {
            mState->mGeometryShading.RegisterSubsurfaceLighting( *mState->mFrameGraph );
            mState->mSubsurfaceScattering.RegisterBlurPass( *mState->mFrameGraph, 0, mTargetResolution );
            mState->mSubsurfaceScattering.RegisterBlurPass( *mState->mFrameGraph, 1, mTargetResolution );
            mState->mSubsurfaceScattering.RegisterCompositePass( *mState->mFrameGraph,
                mState->mGeometryManagement, mTargetResolution, mSettings.mMultisampling );
            mState->mSubsurfaceScattering.RegisterStorePass( *mState->mFrameGraph, mTargetResolution, mSettings.mMultisampling );
        }

        if ( !mSettings.mRayTracing.mPathTracingEnabled ) {
            mState->mGeometryShading.RegisterContours( *mState->mFrameGraph );
        }

        if ( mSettings.mMultisampling != renderer::rhi::Multisampling::eMsaaX1 && !mFogAllocated ) {
            mState->mGeometryShading.RegisterTransparentPass( *mState->mFrameGraph );
        }

        mState->mOutlineModule.RegisterOutline( *mState->mFrameGraph, mSettings.mMultisampling );
        mState->mGeometryShading.RegisterResolve( *mState->mFrameGraph );

        // Composite atmosphere, world text, and HDR post effects.
        mState->mTerrainGenerationModule.RegisterTerrainRendering( *mState->mFrameGraph );
        mState->mGeometryShading.RegisterWireframe( *mState->mFrameGraph );
        mState->mAtmosModule.RegisterSceneCopy( *mState->mFrameGraph );
        mState->mAtmosModule.RegisterComposite( *mState->mFrameGraph );
        mState->mAtmosModule.RegisterSun( *mState->mFrameGraph );
        
        if ( mFogAllocated ) {
            mState->mVolumeModule.RegisterFog( *mState->mFrameGraph );
            if ( mSettings.mMultisampling != renderer::rhi::Multisampling::eMsaaX1 ) {
                mState->mVolumeModule.RegisterFogMultisample( *mState->mFrameGraph, mSettings.mMultisampling );
                mState->mGeometryShading.RegisterTransparentPass( *mState->mFrameGraph );
                mState->mGeometryShading.RegisterResolve( *mState->mFrameGraph, true );
            }
        }

        // The fog/MSAA branch can resolve the opaque target again. Trace only
        // after its final resolve so a later resolve cannot overwrite reflections.
        if ( mSettings.mScreenSpaceReflections.mEnabled ) {
            mState->mIndirectIlluminationModule.RegisterPasses( *mState->mFrameGraph, mSettings.mMultisampling );
        }

        mState->mTextRendering.RegisterTextRender( *mState->mFrameGraph );
        mState->mWaterSimulation.RegisterWaterSimulation( *mState->mFrameGraph, true );
        mState->mPostEffectsPasses.RegisterEyeAdaptationPass( *mState->mFrameGraph );

        // Glass composites over opaque geometry and world text before tone mapping.
        if ( mSettings.mMultisampling == renderer::rhi::Multisampling::eMsaaX1 ) {
            mState->mGeometryShading.RegisterTransparentPass( *mState->mFrameGraph );
        }

        // Particles and camera effects finish the scene before editor presentation.
        mState->mParticleSimulation.RegisterRenderPass( *mState->mFrameGraph );
        mState->mDisplayEffectsModule.RegisterDepthOfField( *mState->mFrameGraph );
        mState->mTonemapModule.RegisterColorGradientPass( *mState->mFrameGraph );

        mState->mPostEffectsPasses.RegisterPostProcess( *mState->mFrameGraph, material::PostProcessStage::eBeforeToneMapping );
        mState->mPostEffectsPasses.RegisterBloom( *mState->mFrameGraph,
            mState->mFrameGraph->GetData<PrepassModuleInfo>().mGBufferEmissiveTarget,
            mState->mFrameGraph->GetData<PostProcessModuleInfo>().mBeforeToneMapping.mColorImage );
        mState->mTonemapModule.RegisterTonemapPass( *mState->mFrameGraph );

        mState->mPostEffectsPasses.RegisterInfiniteGrid( *mState->mFrameGraph );
        mState->mDisplayEffectsModule.RegisterLensDirtPass( *mState->mFrameGraph );
        mState->mDisplayEffectsModule.RegisterLensFlarePass( *mState->mFrameGraph );
        mState->mDisplayEffectsModule.RegisterLensDistortionPass( *mState->mFrameGraph );
        mState->mDisplayEffectsModule.RegisterChromaticAberration( *mState->mFrameGraph );
        mState->mDisplayEffectsModule.RegisterVignetteShapeRenderPass( *mState->mFrameGraph );
        mState->mDisplayEffectsModule.RegisterVignetteShapeCompositePass( *mState->mFrameGraph );

        // Display-space material effects finish the image after camera compositing.
        mState->mPostEffectsPasses.RegisterPostProcess( *mState->mFrameGraph, material::PostProcessStage::eAfterToneMapping );

        // Shared image-copy helpers.
        mState->mHelperModule.RegisterImageBlitPass( *mState->mFrameGraph );

        // Final output, editor overlays, screen text, and presentation.
        mState->mPresentationModule.RegisterFullQuadRender( *mState->mFrameGraph );
        mState->mOutlineModule.RegisterComposite( *mState->mFrameGraph );
        mState->mDebugOverlayModule.RegisterPhysicsDebugger( *mState->mFrameGraph );
        mState->mDebugOverlayModule.RegisterSceneVolumes( *mState->mFrameGraph );
        mState->mDebugPasses.RegisterArmaturePass( *mState->mFrameGraph );
        mState->mGuiModule.RegisterUploadPass( *mState->mFrameGraph );
        mState->mGuiModule.RegisterRenderPass( *mState->mFrameGraph );
        mState->mTextRendering.RegisterScreenPass( *mState->mFrameGraph );
    }

    auto ForwardRenderer::BindGeometryManagers() -> void {
        // Both paths use geometry for depth, picking, and editor overlays.
        mState->mRenderPrepass.SetGeometryManager( mState->mGeometryManagement );
        mState->mMousePickingModule.SetGeometryManager( mState->mGeometryManagement );
        mState->mOutlineModule.SetGeometryManager( mState->mGeometryManagement );
        mState->mDebugOverlayModule.SetGeometryManager( mState->mGeometryManagement );

        if ( mState->mPathMode ) {
            return;
        }

        // Raster modules also consume the geometry manager's GPU scene and indirect draws.
        mState->mGuiModule.SetGeometryManager( mState->mGeometryManagement );
        mState->mShadowMapping.SetGeometryManager( mState->mGeometryManagement );
        mState->mGeometryShading.SetGeometryManager( mState->mGeometryManagement );
        mState->mDisplayEffectsModule.SetGeometryManager( mState->mGeometryManagement );
    }

    auto ForwardRenderer::ConfigureRasterExecutionPolicies() -> void {
        // The BRDF lookup is static; environment products refresh when their source changes.
        mState->mFrameGraph->SetExecutionPolicy( "BRDFLut", FGExecutionPolicy::eOnce );

        mState->mFrameGraph->SetExecutionPolicy( "PrefilterPass", FGExecutionPolicy::eOnWake );
        mState->mFrameGraph->SetExecutionPolicy( "IrradiancePass", FGExecutionPolicy::eOnWake );
        mState->mFrameGraph->SetExecutionPolicy( "SkyboxProjection", FGExecutionPolicy::eOnWake );
        mState->mFrameGraph->SetExecutionPolicy( "SkyboxProjection_Graphics", FGExecutionPolicy::eOnWake );
    }

    auto ForwardRenderer::Shutdown() -> void {
        MKT_BEGIN_PROFILER();
        mState->mFrameGraph = nullptr;
        mState->mGeometryShading.SetSkyboxMaterial( material::MaterialHandle::CreateEmpty() );
        mState->mSkyboxMaterial.Reset();

        for ( auto& texture: mState->mSkyboxTextures ) {
            texture.Reset();
        }

        mState->mSkyboxStateInitialized = false;
        mState->mMousePickingModule.Invalidate();
        mLastRenderedScene = nullptr;
        mDevice = nullptr;
    }

    auto ForwardRenderer::Render( const scene::Scene* scene ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        UpdateRenderRequirements( scene );
        RebuildRenderState();
        ApplyFrameSettings();

        if ( !BindSceneToPasses( scene ) ) {
            return;
        }

        const auto environment{ mState->mSkyboxType == material::SkyboxType::eEquirectangular ? 
            mState->mSkyboxTextures[0] : rhi::TextureHandle::CreateEmpty() };

        const core::f32 environmentScale{ mEnvironmentEnabled ? mAmbientScale : 0.0f };

        if ( mState->mPathMode ) {
            if ( !PreparePathTracingFrame( scene, environment, environmentScale ) ) {
                return;
            }
        }
        else {
            PrepareRasterFrame( scene, environment, environmentScale );
        }

        mState->mFrameGraph->Execute();
        mState->mFrameGraph->ExecuteReadbacks();
    }

    auto ForwardRenderer::UpdateRenderRequirements( const scene::Scene* scene ) -> void {
        MKT_BEGIN_PROFILER();

        const auto revision{ mState->mVideoModule.PrepareFrame( mMainCamera ? scene : nullptr ) };

        if ( revision != mVideoRevision ) {
            ResetPathAccumulation();
            mVideoRevision = revision;
        }

        // Allocate the diffusion working set only for scenes using this model.
        // Distance/strength edits stay in the material buffer and need no rebuild.
        bool needsSubsurface{};

        if ( scene && !mSettings.mRayTracing.mPathTracingEnabled ) {
            for ( const auto [entity, component]: scene->GetRegistry().view<scene::MaterialComponent>().each() ) {
                const auto source{ component.GetMaterial() };
                const auto* surface{ dynamic_cast<const material::SurfaceMaterial*>( source.GetPtr() ) };

                if ( surface && surface->GetShadingModel() == material::ShadingModel::eSubsurfaceScattering &&
                     surface->GetAlphaMask() != material::AlphaMode::eBlend ) {
                    needsSubsurface = true;
                    break;
                }
            }
        }

        if ( mSubsurfaceAllocated != needsSubsurface ) {
            mSubsurfaceAllocated = needsSubsurface;
            mRebuildPending = true;
        }

        const bool needsFog{ scene && !mSettings.mRayTracing.mPathTracingEnabled &&
            ( !scene->GetRegistry().view<scene::FogComponent>().empty() || !scene->GetRegistry().view<scene::CloudComponent>().empty() ) };
        if ( mFogAllocated != needsFog ) {
            mFogAllocated = needsFog;
            mRebuildPending = true;
        }

        if ( scene && mMainCamera ) {
            // Off-screen geometry can still contribute to rays or be bent into
            // view. Keep it resident until those paths provide their own demands.
            scene->UpdateStreamingView( this, *mMainCamera, mState->mPathMode );
        }

        if ( const auto* particles{ ParticleSystem::GetPtr() }; !mState->mPathMode ) {
            // Replacing the graph also fences its old particle buffers. Never mutate
            // buffers still consumed by the previous frame when switching backends.
            mRebuildPending |= mState->mParticleSimulation.GetSimulationMode() !=
                               mParticleModeOverride.value_or( particles ? particles->GetSimulationMode() : ParticleSimulationMode::eCpu );
        }
    }

    auto ForwardRenderer::RebuildRenderState() -> void {
        MKT_BEGIN_PROFILER();

        if ( !mRebuildPending ) {
            return;
        }

        // Keep the previous graph available until initialization succeeds.
        // Submitted command lists retain the resources they still reference.
        auto previous{ mState };
        const auto skybox{ previous->mSkyboxMaterial };
        const auto physicsDebug{ previous->mDebugOverlayModule.GetPhysicsSettings() };
        const auto sceneVolumes{ previous->mDebugOverlayModule.GetSceneVolumeSettings() };
        mState = eastl::make_shared<RenderState>( mTargetResolution );

        try {
            Initialize();

            SetMainCamera( mMainCamera );
            SetClearColor( mClearColor );

            SetGamma( mGamma );
            SetExposure( mExposure );
            SetSkyboxBlur( mSkyboxBlur );
            SetAmbientScale( mAmbientScale );

            SetTonemapType( mToneMapping );
            SetRenderBackground( mBackground );

            SetSkyboxMaterial( skybox );
            SetEnvironmentLightingEnabled( mEnvironmentEnabled );

            SetEnableInfiniteGrid( mInfiniteGrid );
            SetEnablePolygonComplexity( mPolygonComplexity );

            // Update current final target
            // TODO: Use a setter in its place
            ( void )GetFinalImage( mFinalImageType );

            mState->mDebugOverlayModule.GetPhysicsSettings() = physicsDebug;
            mState->mDebugOverlayModule.GetSceneVolumeSettings() = sceneVolumes;
        } catch ( const std::exception& error ) {
            mState = previous;
            mSubsurfaceAllocated = previous->mFrameGraph->GetBlackBoard().Contains<SubsurfaceScatteringInfo>();
            mFogAllocated = previous->mFrameGraph->GetBlackBoard().Contains<FogModuleInfo>();
            mTargetResolution = previous->mTargetResolution;
            mSettings.mMultisampling = previous->mMultisampling;
            mSettings.mShadowQuality = previous->mShadowQuality;
            mSettings.mCascadedShadowsEnabled = previous->mCascadedShadowsEnabled;
            mSettings.mResolution = mTargetResolution;
            mSettings.mRayTracing.mPathTracingEnabled = previous->mPathMode;

            if ( mParticleModeOverride ) {
                mParticleModeOverride = previous->mParticleSimulation.GetSimulationMode();
            }
            else if ( auto* particles{ ParticleSystem::GetPtr() } ) {
                particles->SetSimulationMode( previous->mParticleSimulation.GetSimulationMode() );
            }

            MKT_CORE_LOGGER_ERROR( "Cannot rebuild scene render targets: {}. Keeping the previous targets.", error.what() );
        }

        mRebuildPending = false;
    }

    auto ForwardRenderer::BindSceneToPasses( const scene::Scene* scene ) -> bool {
        MKT_BEGIN_PROFILER();

        if ( !mState->mFrameGraph ) {
            return false;
        }

        if ( scene != mLastRenderedScene ) {
            mState->mMousePickingModule.Invalidate();
            mLastRenderedScene = scene;
        }

        mState->mGuiModule.SetScene( scene );
        mState->mRenderPrepass.SetScene( scene );
        mState->mWeatherModule.SetScene( scene );
        mState->mWaterSimulation.SetScene( scene );
        mState->mPostEffectsPasses.SetScene( scene );
        mState->mGeometryManagement.SetScene( scene );
        mState->mDebugOverlayModule.SetScene( scene );
        mState->mDebugPasses.SetScene( scene );
        mState->mDebugPasses.SetInfoOverlayEnabled( mInfoOverlayEnabled && mOverlaysEnabled );
        mState->mTerrainGenerationModule.SetScene( scene );

        mState->mOutlineModule.SetEnabled( mOverlaysEnabled && scene != nullptr );
        if ( mState->mVolumeModule.Prepare( scene, mMainCamera, mEnvironmentEnabled ? mAmbientScale : 0.0f ) ) {
            mState->mPathTracing.GetRayTracingModule().ResetAccumulation();
            mState->mRayTracingPass.ResetAccumulation();
        }

        return true;
    }

    auto ForwardRenderer::GetEnvironmentFaces() const -> eastl::array<rhi::TextureHandle, 6> {
        MKT_BEGIN_PROFILER();

        eastl::array<renderer::rhi::TextureHandle, 6> textures{};

        if ( mState->mSkyboxType == material::SkyboxType::eCubeFaces ) {
            constexpr material::SkyboxFace faces[]{ material::SkyboxFace::eRight, material::SkyboxFace::eLeft,
                material::SkyboxFace::eTop, material::SkyboxFace::eBottom, material::SkyboxFace::eBack, material::SkyboxFace::eFront };

            for ( core::u32 face{}; face < rhi::kMaxCubeFaces; ++face ) {
                textures[face] = mState->mSkyboxTextures[static_cast<core::usize>( faces[face] )];
            }
        }

        return textures;
    }

    auto ForwardRenderer::PreparePathTracingFrame( const scene::Scene* scene,
        const rhi::TextureHandle& environment, core::f32 environmentScale ) -> bool {
        if ( !mMainCamera ) {
            return false;
        }

        ( void )mState->mAtmosModule.Prepare( scene, *mState->mFrameGraph );

        const auto& atmosphere{ mState->mFrameGraph->GetOrCreate<AtmosphereModuleInfo>() };
        mState->mPathTracing.SetAtmosphere( mState->mAtmosModule.IsEnabled() ? &mState->mAtmosModule.GetData() : nullptr,
            { mState->mFrameGraph->GetTexture( atmosphere.mTransmittance ),
                mState->mFrameGraph->GetTexture( atmosphere.mMultipleScattering ),
                mState->mFrameGraph->GetTexture( atmosphere.mSkyView ) } );
        mState->mPathTracing.SetBackground( mBackground, mSkyboxBlur );
        mState->mPathTracing.SetEnvironmentFaces( GetEnvironmentFaces() );
        mState->mPathTracing.Prepare( *mState->mFrameGraph, scene, mMainCamera, mSettings.mRayTracing, environment, environmentScale, mExposure, mGamma );

        return true;
    }

    auto ForwardRenderer::PrepareRasterFrame( const scene::Scene* scene,
        const rhi::TextureHandle& environment, core::f32 environmentScale ) -> void {
        MKT_BEGIN_PROFILER();

        mState->mShadowMapping.SetScene( scene );
        mState->mTextRendering.SetScene( scene );
        mState->mParticleSimulation.SetScene( scene );
        mState->mDisplayEffectsModule.SetScene( scene );

        mState->mIndirectIlluminationModule.SetSettings( mSettings.mScreenSpaceReflections );
        mState->mIndirectIlluminationModule.Prepare( scene, mMainCamera,
            mDevice->SupportsRayTracing() && mSettings.mRayTracing.mReflectionsEnabled );

        const bool atmosphereChanged{ mState->mAtmosModule.Prepare( scene, *mState->mFrameGraph ) };
        const bool atmosphereEnabled{ mState->mAtmosModule.IsEnabled() };

        if ( mDevice->SupportsRayTracing() ) {
            const auto& atmosphere{ mState->mFrameGraph->GetOrCreate<AtmosphereModuleInfo>() };
            mState->mRayTracingPass.SetAtmosphere( atmosphereEnabled ? &mState->mAtmosModule.GetData() : nullptr,
                { mState->mFrameGraph->GetTexture( atmosphere.mTransmittance ),
                    mState->mFrameGraph->GetTexture( atmosphere.mMultipleScattering ),
                    mState->mFrameGraph->GetTexture( atmosphere.mSkyView ) } );
            mState->mRayTracingPass.SetEnvironmentFaces( GetEnvironmentFaces() );
            mState->mRayTracingPass.Prepare( *mState->mFrameGraph, scene, mMainCamera, mSettings.mRayTracing, environment, environmentScale, mExposure, mGamma );
        }

        mState->mGeometryShading.SetAtmosphereEnabled( atmosphereEnabled );

        if ( atmosphereEnabled ) {
            mState->mGeometryShading.SetEnvironmentLightingEnabled( true );
            mState->mGeometryShading.SetAmbientScale( 1.0f );

            // The atmosphere owns IBL until the lighting source changes back.
            // Do not project an unrelated panorama over its generated cubemap.
            mState->mGeometryShading.SetSkyboxPassesEnabled( *mState->mFrameGraph, false );

            for ( cstr pass: { "PrefilterPass", "IrradiancePass" } ) {
                if ( atmosphereChanged ) {
                    mState->mFrameGraph->EnablePass( pass );
                }
                else {
                    mState->mFrameGraph->DisablePass( pass );
                }
            }
        }
        else if ( atmosphereChanged ) {
            // The two sources share filtered IBL targets. Restore the map
            // after leaving atmosphere mode, even if its asset did not change.
            mState->mSkyboxStateInitialized = false;
            SetSkyboxMaterial( mState->mSkyboxMaterial );
            mState->mGeometryShading.SetEnvironmentLightingEnabled( scene && scene->GetEnvironmentLightingSource() == EnvironmentLightingSource::eEnvironmentMap );
        }
    }

    auto ForwardRenderer::ApplyFrameSettings() -> void {
        MKT_BEGIN_PROFILER();

        mSettings.Sanitize();
        const bool aoDebug{ mFinalImageType == FinalImageType::eRayTracedAmbientOcclusion };
        mState->mRayTracingPass.SetAmbientOcclusionDebug( aoDebug );
        mState->mPathTracing.GetRayTracingModule().SetAmbientOcclusionDebug( aoDebug );

        const auto armatureMode{ mOverlaysEnabled ? mArmatureDebugMode : ArmatureDebugMode::eDisabled };
        const bool meshesVisible{ armatureMode != ArmatureDebugMode::eArmatureOnly };
        mState->mDebugPasses.SetArmatureMode( armatureMode );
        mState->mGeometryManagement.SetMeshesVisible( meshesVisible );
        mState->mPathTracing.GetRayTracingModule().SetMeshesVisible( meshesVisible );
        mState->mRayTracingPass.SetMeshesVisible( meshesVisible );

        // Restore each procedural draw's previous state when leaving armature-only mode.
        // Simulations, the background, grid, lights, and UI continue to run.
        constexpr eastl::array<cstr, 2> surfacePasses{ "Terrain_RenderGeometry", "WaterSimulation_Render" };

        for ( usize index{}; index < surfacePasses.size(); ++index ) {
            const auto name{ surfacePasses[index] };

            if ( !mState->mFrameGraph || !mState->mFrameGraph->IsPassPresent( name ) ) {
                continue;
            }

            if ( !meshesVisible ) {
                if ( !mState->mArmatureMeshesHidden ) {
                    mState->mArmatureHiddenPasses[index] = mState->mFrameGraph->GetNodeControl().mNodes.at( name ).mIsAlive;
                }

                mState->mFrameGraph->DisablePass( name );
            }
            else if ( mState->mArmatureMeshesHidden && mState->mArmatureHiddenPasses[index] ) {
                mState->mFrameGraph->EnablePass( name );
            }
        }

        mState->mArmatureMeshesHidden = !meshesVisible;

        // Display and ambient-occlusion effects.
        mState->mDisplayEffectsModule.SetChromaticAberration( mSettings.mChromaticAberration );
        mState->mDisplayEffectsModule.SetDepthOfFieldQuality( mSettings.mDepthOfFieldQuality, mSettings.mVisualizeDepthOfField );
        mState->mPostEffectsPasses.GetBloomSettings() = mSettings.mBloom;
        mState->mGeometryShading.SetEnableSsao( mSettings.mSsaoEnabled );
        mState->mGeometryShading.SetSsaoIntensity( mSettings.mSsaoIntensity );

        // Geometry visibility and debug visualization.
        mState->mGeometryManagement.SetCullingDebugCamera( mSettings.mVisualizeFrustum ? mCullingDebugCamera : nullptr );
        mState->mGeometryManagement.SetFrustumCullingEnabled( mSettings.mFrustumCullingEnabled );
        mState->mCameraPass.SetVisualizeCascades( mSettings.mVisualizeCascades );
    }

    auto ForwardRenderer::GetFinalImage( FinalImageType type ) -> renderer::rhi::TextureHandle {
        MKT_BEGIN_PROFILER();
        mFinalImageType = type;
        mState->mPresentationModule.SetFinalImageTarget( type );

        if ( mState->mFrameGraph && mState->mPathMode ) {
            if ( !mState->mPathTracing.IsReady() ) {
                return renderer::rhi::TextureHandle::CreateEmpty();
            }

            return mState->mPresentationModule.GetFinalImage( *mState->mFrameGraph );
        }

        if ( !mState->mFrameGraph ) {
            return renderer::rhi::TextureHandle::CreateEmpty();
        }

        // Both consumers share this pass; neither may disable the other's output.
        if ( type == FinalImageType::eWireframe || mPolygonComplexity ) {
            mState->mFrameGraph->EnablePass( "WireframePass" );
        }
        else {
            mState->mFrameGraph->DisablePass( "WireframePass" );
        }

        // This is a deferred operation, we can specify the final image output
        // but it is not until the next call to execute that we render it
        mState->mPresentationModule.SetFinalImageTarget( type );

        return mState->mPresentationModule.GetFinalImage( *mState->mFrameGraph );
    }

    auto ForwardRenderer::SetMainCamera( const scene::SceneCamera* camera ) -> void {
        MKT_BEGIN_PROFILER();
        mMainCamera = camera;

        // Keep world rendering, picking and canvas projection on the same camera snapshot.
        mState->mAtmosModule.SetCamera( camera );
        mState->mGuiModule.SetCamera( camera );
        mState->mCameraPass.SetCamera( camera );
        mState->mWeatherModule.SetCamera( camera );
        mState->mRenderPrepass.SetCamera( camera );
        mState->mTextRendering.SetCamera( camera );
        mState->mParticleSimulation.SetCamera( camera );
        mState->mShadowMapping.SetCamera( camera );
        mState->mDebugOverlayModule.SetCamera( camera );
        mState->mPostEffectsPasses.SetCamera( camera );
        mState->mGeometryManagement.SetCamera( camera );
        mState->mDisplayEffectsModule.SetCamera( camera );
        mState->mWaterSimulation.SetCamera( camera );
        mState->mTerrainGenerationModule.SetCamera( camera );
        mState->mIndirectIlluminationModule.SetCamera( camera );
    }

    auto ForwardRenderer::SetClearColor( const renderer::rhi::Color& color ) -> void {
        MKT_BEGIN_PROFILER();
        mClearColor = color;
        mState->mGeometryShading.SetClearColor( color );
    }

    auto ForwardRenderer::SetTonemapType( ToneMapper type ) -> void {
        MKT_BEGIN_PROFILER();
        mToneMapping = type;
        mState->mTonemapModule.SetToneMapping( type );
    }

    auto ForwardRenderer::SetGamma( core::f32 gamma ) -> void {
        MKT_BEGIN_PROFILER();
        mGamma = gamma;
        mState->mGeometryShading.SetGamma( gamma );
        mState->mCameraPass.SetGamma( gamma );
        mState->mPostEffectsPasses.SetGamma( gamma );
        mState->mTonemapModule.SetGamma( gamma );
    }

    auto ForwardRenderer::SetExposure( core::f32 exposure ) -> void {
        MKT_BEGIN_PROFILER();
        mExposure = exposure;

        mState->mGeometryShading.SetExposure( exposure );
        mState->mCameraPass.SetExposure( exposure );
        mState->mPostEffectsPasses.SetExposure( exposure );
        mState->mTonemapModule.SetExposure( exposure );
    }

    auto ForwardRenderer::SetEnableSsao( bool enabled ) -> void {
        MKT_BEGIN_PROFILER();
        mSettings.mSsaoEnabled = enabled;

        // Bypass screen-space occlusion in PBR while retaining the AO textures
        // for inspection. Material occlusion maps still contribute normally.
        mState->mGeometryShading.SetEnableSsao( enabled );
    }

    auto ForwardRenderer::IsSsaoEnabled() const -> bool {
        return mSettings.mSsaoEnabled;
    }

    auto ForwardRenderer::SetSsaoIntensity( core::f32 intensity ) -> void {
        MKT_BEGIN_PROFILER();

        mSettings.mSsaoIntensity = intensity;
        mState->mGeometryShading.SetSsaoIntensity( intensity );
    }

    auto ForwardRenderer::GetSsaoIntensity() const -> core::f32 {
        return mSettings.mSsaoIntensity;
    }

    auto ForwardRenderer::GetBloomSettings() -> BloomSettings& {
        MKT_BEGIN_PROFILER();
        return mSettings.mBloom;
    }

    auto ForwardRenderer::GetShadowDiagnostics() const -> const ShadowDiagnostics& {
        MKT_BEGIN_PROFILER();
        return mState->mShadowMapping.GetDiagnostics();
    }

    auto ForwardRenderer::SetSkyboxBlur( core::f32 blur ) -> void {
        MKT_BEGIN_PROFILER();
        mSkyboxBlur = blur;
        mState->mGeometryShading.SetSkyboxBlur( blur );
    }

    auto ForwardRenderer::SetAmbientScale( core::f32 ambient ) -> void {
        MKT_BEGIN_PROFILER();
        mAmbientScale = ambient;
        mState->mGeometryShading.SetAmbientScale( ambient );
    }

    auto ForwardRenderer::SetShadowMappDistance( core::f32 distance ) -> void {
        MKT_BEGIN_PROFILER();
        mSettings.mShadowDistance = distance;
        mSettings.Sanitize();
        mState->mShadowMapping.SetShadowMappDistance( mSettings.mShadowDistance );
    }

    auto ForwardRenderer::SetEnablePolygonComplexity( bool value ) -> void {
        MKT_BEGIN_PROFILER();
        mPolygonComplexity = value;

        if ( !mState->mFrameGraph || !mState->mFrameGraph->IsPassPresent( "WireframePass" ) ) {
            return;
        }

        if ( value || mFinalImageType == FinalImageType::eWireframe ) {
            mState->mFrameGraph->EnablePass( "WireframePass" );
        }
        else {
            mState->mFrameGraph->DisablePass( "WireframePass" );
        }

        mState->mGeometryShading.SetMergeWireframeToFinalOutput( value );
    }

    auto ForwardRenderer::SetSkyboxMaterial( material::MaterialHandle material ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !mState->mFrameGraph ) {
            return;
        }

        auto* skybox{ material ? core::checked_cast<material::SkyboxMaterial*>( material.GetPtr() ) : nullptr };

        if ( skybox && !skybox->HasRequiredTextures() ) {
            material = material::MaterialHandle::CreateEmpty();
            skybox = nullptr;
        }

        eastl::array<rhi::TextureHandle, 6> textures{};
        const auto type{ skybox ? skybox->GetType() : material::SkyboxType::eEquirectangular };

        if ( skybox ) {
            if ( type == material::SkyboxType::eEquirectangular ) {
                textures[0] = skybox->GetEquirectangular();
            }
            else {
                for ( core::usize face{}; face < textures.size(); ++face ) {
                    textures[face] = skybox->GetFace( static_cast<material::SkyboxFace>( face ) );
                }
            }
        }

        // Scene updates synchronize every frame. Rebuild the expensive maps
        // only when their inputs change, including edits to the same material.
        // Ambient strength and camera clear flags do not change these textures.
        if ( mState->mSkyboxStateInitialized && material == mState->mSkyboxMaterial &&
             type == mState->mSkyboxType && textures == mState->mSkyboxTextures ) {
            return;
        }

        mState->mSkyboxStateInitialized = true;
        mState->mSkyboxMaterial = material;
        // Rebuild only when crossing between absent and present environments.
        // Old targets remain owned by any outstanding command recordings.
        if ( mEnvironmentAllocated != static_cast<bool>( material ) ) {
            mEnvironmentAllocated = static_cast<bool>( material );
            mRebuildPending = true;
        }

        mState->mSkyboxType = type;
        mState->mSkyboxTextures = textures;

        if ( mState->mPathMode ) {
            return;
        }

        if ( material.IsEmpty() ) {
            mState->mGeometryShading.SetSkyboxMaterial( material::MaterialHandle::CreateEmpty() );

            if ( mState->mFrameGraph ) {
                // Derived maps belong to the previous environment. Keep them
                // dormant until a valid replacement schedules regeneration.
                mState->mGeometryShading.SetSkyboxPassesEnabled( *mState->mFrameGraph, false );

                for ( core::cstr pass: { "PrefilterPass", "IrradiancePass" } ) {
                    mState->mFrameGraph->DisablePass( pass );
                }
            }

            return;
        }

        mState->mGeometryShading.SetSkyboxMaterial( material );

        // These passes sleep after every run,
        // we need to re-enable it again so it runs once again
        // on the next call to execute
        mState->mFrameGraph->EnablePass( "PrefilterPass" );
        mState->mFrameGraph->EnablePass( "IrradiancePass" );
        mState->mGeometryShading.SetSkyboxPassesEnabled( *mState->mFrameGraph, true );
    }

    auto ForwardRenderer::SetEnvironmentLightingEnabled( bool enabled ) -> void {
        MKT_BEGIN_PROFILER();
        mEnvironmentEnabled = enabled;
        mState->mGeometryShading.SetEnvironmentLightingEnabled( enabled );
    }

    auto ForwardRenderer::SetRenderBackground( SceneBackgroundType bg ) -> void {
        MKT_BEGIN_PROFILER();
        mBackground = bg;

        mState->mAtmosModule.SetSkyVisible( bg != SceneBackgroundType::eClearColor );
        mState->mGeometryShading.SetRenderBackground( bg );
    }

    auto ForwardRenderer::GetSettings() const -> SceneRenderSettings {
        return mSettings;
    }

    auto ForwardRenderer::GetPathSampleCount() const -> core::u32 {
        return mState->mPathTracing.GetSampleCount();
    }

    auto ForwardRenderer::ResetPathAccumulation() -> void {
        mState->mPathTracing.ResetAccumulation();
    }

    auto ForwardRenderer::GetRayTracingError() const -> const eastl::string& {
        return mState->mPathMode ? mState->mPathTracing.GetError() : mState->mRayTracingPass.GetError();
    }

    auto ForwardRenderer::ApplySettings( SceneRenderSettings settings ) -> void {
        settings.Sanitize();

        settings.mRayTracing.mPathTracingEnabled &= mDevice->SupportsRayTracing();
        settings.mRayTracing.mShadowsEnabled &= mDevice->SupportsRayTracing();
        settings.mRayTracing.mReflectionsEnabled &= mDevice->SupportsRayTracing();
        settings.mRayTracing.mGlobalIlluminationEnabled &= mDevice->SupportsRayTracing();
        settings.mRayTracing.mAmbientOcclusionEnabled &= mDevice->SupportsRayTracing();

        mRebuildPending |= settings.mRayTracing.mPathTracingEnabled != mSettings.mRayTracing.mPathTracingEnabled;
        mRebuildPending |= settings.mScreenSpaceReflections.mEnabled != mSettings.mScreenSpaceReflections.mEnabled;
        mRebuildPending |= settings.mScreenSpaceReflections.mHalfResolution != mSettings.mScreenSpaceReflections.mHalfResolution;
        mRebuildPending |= settings.mShadowQuality != mSettings.mShadowQuality;
        mRebuildPending |= settings.mCascadedShadowsEnabled != mSettings.mCascadedShadowsEnabled;

        SetRenderResolution( settings.mResolution );
        SetMultisampling( settings.mMultisampling );

        settings.mMultisampling = mSettings.mMultisampling;
        mSettings = settings;
        mState->mShadowMapping.SetSoftShadowsEnabled( mSettings.mSoftShadowsEnabled );
        // Distance only changes cascade fitting; the graph and textures remain valid.
        mState->mShadowMapping.SetShadowMappDistance( mSettings.mShadowDistance );
        SetOutlineStyle( mSettings.mSelectionOutlineColor, mSettings.mSelectionOutlineWidth );
    }

    auto ForwardRenderer::SetMultisampling( rhi::Multisampling multisampling ) -> void {
        MKT_BEGIN_PROFILER();

        if ( core::as<core::u32>( multisampling ) > core::as<core::u32>( rhi::Multisampling::eMsaaX16 ) ) {
            multisampling = rhi::Multisampling::eMsaaX1;
        }

        const auto maximum{ mDevice->GetMaxSceneMultisampling() };

        if ( core::as<core::u32>( multisampling ) > core::as<core::u32>( maximum ) ) {
            multisampling = maximum;
        }
        // Support can have gaps: choose the highest supported level no greater
        // than requested. Construction and ApplySettings both use this setter.
        while ( multisampling != renderer::rhi::Multisampling::eMsaaX1 && !mDevice->SupportsSceneMultisampling( multisampling ) ) {
            multisampling = core::as<renderer::rhi::Multisampling>( core::as<core::u32>( multisampling ) - 1 );
        }

        mRebuildPending |= mSettings.mMultisampling != multisampling;
        mSettings.mMultisampling = multisampling;
    }

    auto ForwardRenderer::SetRenderResolution( rhi::RenderResolution resolution ) -> void {
        MKT_BEGIN_PROFILER();

        if ( core::as<unsigned>( resolution ) > static_cast<unsigned>( renderer::rhi::RenderResolution::e3120P ) ) {
            resolution = renderer::rhi::RenderResolution::e1080P;
        }

        mRebuildPending |= mTargetResolution != resolution;
        mTargetResolution = resolution;
        mSettings.mResolution = resolution;
    }

    auto ForwardRenderer::GetRenderResolution() const -> rhi::RenderResolution {
        MKT_BEGIN_PROFILER();
        return mState->mTargetResolution;
    }

    auto ForwardRenderer::SetParticleSimulationMode( eastl::optional<ParticleSimulationMode> mode ) -> void {
        mParticleModeOverride = mode;
    }

    auto ForwardRenderer::SetEnableInfiniteGrid( bool value ) -> void {
        MKT_BEGIN_PROFILER();
        mInfiniteGrid = value;

        if ( !mState->mFrameGraph || !mState->mFrameGraph->IsPassPresent( "InfiniteGrid" ) ) {
            return;
        }

        if ( value ) {
            mState->mFrameGraph->EnablePass( "InfiniteGrid" );
        }
        else {
            mState->mFrameGraph->DisablePass( "InfiniteGrid" );
        }
    }

    auto ForwardRenderer::GetInstanceUploadStatistics() const -> const SceneUploadStatistics& {
        return mState->mGeometryManagement.GetInstanceUploadStatistics();
    }

    auto ForwardRenderer::GetMaterialUploadStatistics() const -> const SceneUploadStatistics& {
        return mState->mGeometryManagement.GetMaterialUploadStatistics();
    }

    auto ForwardRenderer::GetSharedMaterialCount() const -> core::usize {
        return mState->mGeometryManagement.GetSharedMaterialCount();
    }

    auto ForwardRenderer::GetSceneVolumeSettings() -> SceneVolumeSettings& {
        MKT_BEGIN_PROFILER();
        return mState->mDebugOverlayModule.GetSceneVolumeSettings();
    }

    auto ForwardRenderer::SetArmatureDebugMode( ArmatureDebugMode mode ) -> void {
        mArmatureDebugMode = mode <= ArmatureDebugMode::eArmatureOnly ? mode : ArmatureDebugMode::eDisabled;
    }

    auto ForwardRenderer::GetPhysicsDebugSettings() -> physics::PhysicsDebugDrawSettings& {
        MKT_BEGIN_PROFILER();
        return mState->mDebugOverlayModule.GetPhysicsSettings();
    }

    auto ForwardRenderer::GetPhysicsDebugLineCount() const -> core::u32 {
        MKT_BEGIN_PROFILER();
        return mState->mDebugOverlayModule.GetPhysicsLineCount();
    }

    auto ForwardRenderer::GetDroppedPhysicsDebugLineCount() const -> core::usize {
        MKT_BEGIN_PROFILER();
        return mState->mDebugOverlayModule.GetDroppedPhysicsLineCount();
    }

    auto ForwardRenderer::DisablePass( eastl::string_view passName ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !mState->mFrameGraph ) {
            return;
        }

        mState->mFrameGraph->DisablePass( passName );
    }

    auto ForwardRenderer::EnablePass( eastl::string_view passName ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !mState->mFrameGraph ) {
            return;
        }

        mState->mFrameGraph->EnablePass( passName );
    }

    auto ForwardRenderer::ReadPixel( core::u32 x, core::u32 y ) const -> core::u32 {
        MKT_BEGIN_PROFILER();
        return mState->mMousePickingModule.ReadPixel( x, y );
    }

    auto ForwardRenderer::RequestPick( const ReadPixelViewportInfo& viewport ) -> bool {
        MKT_BEGIN_PROFILER();
        return mState->mFrameGraph && mState->mFrameGraph->IsPassPresent( "ObjectSelection_Render" ) &&
               mLastRenderedScene && mState->mMousePickingModule.RequestPixel( viewport );
    }

    auto ForwardRenderer::CancelPick() -> void {
        MKT_BEGIN_PROFILER();
        mState->mMousePickingModule.CancelPendingPick();
    }

    auto ForwardRenderer::SetOutlineStyle( const core::float4& color, core::f32 width ) -> void {
        MKT_BEGIN_PROFILER();
        // Keep style on the module so graph replacement cannot lose it during setup.
        mState->mOutlineModule.SetStyle( color, width );
    }

    auto ForwardRenderer::SetTextureDropHover( core::u32 id ) -> void {
        mState->mOutlineModule.SetTextureDropHover( id );
    }

    auto ForwardRenderer::ReadSelectionRectangle( const ReadPixelViewportInfo& first, const ReadPixelViewportInfo& last ) const -> eastl::vector<core::u32> {
        return mState->mMousePickingModule.ReadRectangle( first,last );
    }

    auto ForwardRenderer::GetPickResult() -> eastl::optional<core::u32> {
        MKT_BEGIN_PROFILER();
        return mState->mMousePickingModule.TakePickResult();
    }

    auto ForwardRenderer::ReadPixel( const ReadPixelViewportInfo& ínfo ) const -> core::u32 {
        MKT_BEGIN_PROFILER();
        return mState->mMousePickingModule.ReadPixel( ínfo );
    }

    auto ForwardRenderer::GetNodeControl() const -> const FGNodeControl& {
        MKT_BEGIN_PROFILER();

        if ( !mState->mFrameGraph ) {
            static const FGNodeControl empty{};
            return empty;
        }

        return mState->mFrameGraph->GetNodeControl();
    }

    auto ForwardRenderer::HasFrameGraph() const -> bool {
        MKT_BEGIN_PROFILER();
        return mState->mFrameGraph != nullptr;
    }

    auto ForwardRenderer::GetPassList() const -> const ankerl::unordered_dense::map<eastl::string, FrameGraphPass>& {
        MKT_BEGIN_PROFILER();
        return GetNodeControl().mNodes;
    }

    auto ForwardRenderer::GetPassStatistics() const -> const ankerl::unordered_dense::map<eastl::string, FGNodeStatistics>& {
        MKT_BEGIN_PROFILER();

        if ( !mState->mFrameGraph ) {
            static const ankerl::unordered_dense::map<eastl::string, FGNodeStatistics> empty{};
            return empty;
        }

        return mState->mFrameGraph->GetStatisticsManager().GetStatistics();
    }

    auto ForwardRenderer::GetGpuProfiler() const -> GpuQueryProfiler* {
        return mState->mFrameGraph ? MKT_ADDRESSOF( mState->mFrameGraph->GetGpuProfiler() ) : nullptr;
    }

    auto ForwardRenderer::SetCullingDebugCamera( const scene::Camera* camera ) -> void {
        mCullingDebugCamera = camera;
    }

    auto ForwardRenderer::SetInfoOverlayEnabled( bool enabled ) -> void {
        mInfoOverlayEnabled = enabled;

        if ( mState ) {
            mState->mDebugPasses.SetInfoOverlayEnabled( enabled && mOverlaysEnabled );
        }
    }

    auto ForwardRenderer::GetStatistics() const -> FrameGraphStatistics {
        return mState && mState->mFrameGraph ? mState->mFrameGraph->GetStatistics() : FrameGraphStatistics{};
    }

    auto ForwardRenderer::SetEditorOverlaysEnabled( bool enabled ) -> void {
        mOverlaysEnabled = enabled;
    }

    auto ForwardRenderer::GetFinalImage() -> rhi::TextureHandle {
        return GetFinalImage( mFinalImageType );
    }

    ForwardRenderer::RenderState::RenderState( rhi::RenderResolution resolution )
        : mTargetResolution{ resolution } {
    }
}// namespace mikoto::renderer
