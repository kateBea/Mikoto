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

#include <array>
#include <cstddef>
#include <algorithm>

#include <EASTL/functional.h>

#include <glm/glm.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Math/Math.hh>

#include <Material/PostProcessMaterial.hh>

#include <Renderer/Rhi/Utility.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/ForwardRenderer/Passes/PrepassModule.hh>
#include <Renderer/ForwardRenderer/Passes/DisplayEffectsModule.hh>
#include <Renderer/ForwardRenderer/Passes/GeometryShadingModule.hh>

#include <Scene/Scene.hh>
#include <Scene/Component.hh>
#include <Scene/SceneCamera.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::renderer::rhi;

    static auto HasChromaticAberrationMaterial( const scene::Scene* scene ) -> bool {
        if ( !scene ) {
            return false;
        }

        for ( const auto [entity, tag, component]:
            scene->GetRegistry().view<scene::TagComponent, scene::PostProcessMaterialComponent>().each() ) {
            const auto handle{ component.GetMaterial() };
            const auto* material{ dynamic_cast<const material::PostProcessMaterial*>( handle.GetPtr() ) };

            if ( tag.IsActive() && material && material->IsChromaticAberrationEnabled() ) {
                return true;
            }
        }

        return false;
    }

    // Shared push-constant layout.
    // Keep scalar/array offsets identical to DepthOfField_Helpers.slang.
    struct DepthOfFieldParameters {
        eastl::array<core::u32, 8> mInputs{};
        eastl::array<core::u32, 2> mOutputs{};

        core::u32 mSampler{};
        core::u32 mWidth{};
        core::u32 mHeight{};
        core::u32 mTargetWidth{};
        core::u32 mTargetHeight{};
        core::u32 mRingCount{};
        core::f32 mCoCScale{};
        core::f32 mFocusDistance{};
        core::f32 mMaxRadius{};
        core::f32 mBladeCount{};
        core::f32 mBladeCurvature{};
        core::f32 mBladeRotation{};
        core::f32 mNearPlane{};
        core::f32 mFarPlane{};
        core::u32 mMipLevel{};
        core::u32 mEnabled{};
        core::u32 mVisualize{};
        core::u32 mPadding{};
    };

    MKT_NODISCARD static auto CreateDepthOfFieldTexture( FrameGraph& graph, core::cstr name, core::u32 width, core::u32 height ) -> FGTextureHandle {
        return graph.Create( FGTextureDescription{}.SetName( name ).SetWidth( width ).SetHeight( height ).SetFormat( rhi::Format::eRGBA16_FLOAT ).SetUsage( rhi::TextureUsageFlagsBits::ShaderResource | rhi::TextureUsageFlagsBits::UnorderedAccess ) );
    }

    DisplayEffectsModule::DisplayEffectsModule( rhi::RenderResolution resolution )
        : mResolution{ resolution } {
    }

    auto DisplayEffectsModule::SetScene( const scene::Scene* scene ) -> void {
        MKT_BEGIN_PROFILER();
        mScene = scene;
    }

    auto DisplayEffectsModule::SetCamera( const scene::Camera* camera ) -> void {
        MKT_BEGIN_PROFILER();
        mCamera = camera;
    }

    auto DisplayEffectsModule::SetMultisampling( rhi::Multisampling multisampling ) -> void {
    }

    auto DisplayEffectsModule::SetGeometryManager( GeometryCullModule& geom ) -> void {
        MKT_BEGIN_PROFILER();
        mGeometryCullModule = MKT_ADDRESSOF( geom );
    }

    auto DisplayEffectsModule::RegisterDepthOfField( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER();

        // https://advances.realtimerendering.com/
        // https://www.adriancourreges.com/blog/2018/12/02/ue4-optimized-post-effects/

        // A Life of a Bokeh, Guillaume Abadie, SIGGRAPH 2018 (slides 17-126, 135, 161).
        // https://www.advances.realtimerendering.com/s2018/2018-08-13-SIGGRAPH-A-Life-of-a-Bokeh.pptx
        // No temporal AA: all sampling is deterministic. CoC is a signed radius:
        // negative before the focus plane, positive behind it. Half/quarter refer to each dimension.
        auto& info{ graph.GetOrCreate<DisplayEffectsModuleInfo>() };
        const auto [resolutionWidth, resolutionHeight]{ InferDimensions( mResolution ) };

        const core::u32 width{ std::max( 1u, static_cast<core::u32>( resolutionWidth ) ) };
        const core::u32 height{ std::max( 1u, static_cast<core::u32>( resolutionHeight ) ) };
        const core::u32 halfWidth{ ( width + 1 ) / 2 };
        const core::u32 halfHeight{ ( height + 1 ) / 2 };
        const core::u32 quarterWidth{ ( halfWidth + 1 ) / 2 };
        const core::u32 quarterHeight{ ( halfHeight + 1 ) / 2 };
        const core::u32 tileWidth{ ( halfWidth + 7 ) / 8 };
        const core::u32 tileHeight{ ( halfHeight + 7 ) / 8 };

        const auto sceneColor{ graph.GetOrCreate<GeomShadingModuleInfo>().mColorImage };

        // This prepass is single-sampled even when the lighting targets use MSAA.
        const auto depth{ graph.GetOrCreate<PrepassModuleInfo>().mPrepassDepthTarget };
        const auto sampler{ graph.Create( FGSamplerDescription{}.SetName( "DepthOfField_LinearClamp" )
            .SetFilter( rhi::SamplerFilter::eLinear )
            .SetWrap( rhi::SamplerWrapMode::eClampToEdge ) ) };

        const auto fullCoC{ CreateDepthOfFieldTexture( graph, "DepthOfField_FullCoC", width, height ) };
        const auto setup{ CreateDepthOfFieldTexture( graph, "DepthOfField_HalfColorCoC", halfWidth, halfHeight ) };
        const auto flattened{ CreateDepthOfFieldTexture( graph, "DepthOfField_TileCoC", tileWidth, tileHeight ) };
        const auto dilatedMax{ CreateDepthOfFieldTexture( graph, "DepthOfField_TileMaxRadius", tileWidth, tileHeight ) };
        const auto dilated{ CreateDepthOfFieldTexture( graph, "DepthOfField_TileBounds", tileWidth, tileHeight ) };
        const auto quarter{ CreateDepthOfFieldTexture( graph, "DepthOfField_QuarterColorCoC", quarterWidth, quarterHeight ) };
        const auto scatterLUT{ CreateDepthOfFieldTexture( graph, "DepthOfField_ScatterLUT", 64, 64 ) };
        const auto gatherLUT{ CreateDepthOfFieldTexture( graph, "DepthOfField_GatherLUT", 64, 64 ) };
        const auto recombineLUT{ CreateDepthOfFieldTexture( graph, "DepthOfField_RecombineLUT", 64, 64 ) };
        const auto foreground{ CreateDepthOfFieldTexture( graph, "DepthOfField_Foreground", halfWidth, halfHeight ) };
        const auto foregroundFiltered{ CreateDepthOfFieldTexture( graph, "DepthOfField_ForegroundFiltered", halfWidth, halfHeight ) };
        const auto holeFilling{ CreateDepthOfFieldTexture( graph, "DepthOfField_HoleFilling", halfWidth, halfHeight ) };
        const auto background{ CreateDepthOfFieldTexture( graph, "DepthOfField_Background", halfWidth, halfHeight ) };
        const auto backgroundFiltered{ CreateDepthOfFieldTexture( graph, "DepthOfField_BackgroundFiltered", halfWidth, halfHeight ) };
        const auto slightDefocus{ CreateDepthOfFieldTexture( graph, "DepthOfField_SlightDefocus", halfWidth, halfHeight ) };

        info.mDepthOfFieldColor = CreateDepthOfFieldTexture( graph, "DepthOfField_Color", width, height );
        info.mVisualizeDepthOfField = false;

        // Evaluate the camera at execution time; independent LUT passes need no CPU ordering dependency on Setup.
        const eastl::function<DepthOfFieldParameters()> makeParameters{ [this, width, height]() {
            DepthOfFieldParameters parameters{};
            parameters.mWidth = width;
            parameters.mHeight = height;
            static constexpr std::array<core::u32, 5> rings{ 0, 3, 4, 5, 7 };
            const auto quality{ std::min( static_cast<core::usize>( mDepthOfFieldQuality ), rings.size() - 1 ) };
            parameters.mRingCount = rings[quality];

            if ( mCamera ) {
                const auto& lens{ mCamera->GetDepthOfFieldSettings() };
                const float focalLength{ std::clamp( lens.mFocalLength, 1.0f, 300.0f ) * 0.001f };
                const float aperture{ std::max( lens.mAperture, std::max( lens.mMaxAperture, 0.7f ) ) };
                parameters.mFocusDistance = std::max( lens.mFocusDistance, focalLength + 0.001f );
                parameters.mMaxRadius = std::clamp( lens.mMaxBlur, 0.0f, 64.0f );
                // Thin-lens CoC diameter / 2, converted from sensor meters to screen pixels.
                parameters.mCoCScale = focalLength * focalLength * width /
                                       ( 2.0f * aperture * ( parameters.mFocusDistance - focalLength ) * std::max( lens.mSensorWidth * 0.001f, 0.001f ) );
                parameters.mBladeCount = static_cast<float>( std::clamp( lens.mBladeCount, 3u, 16u ) );
                const float opening{ std::clamp( lens.mMaxAperture / aperture, 0.0f, 1.0f ) };
                // The editor defines roundness at maximum aperture; stopping down reveals the blades.
                parameters.mBladeCurvature = std::clamp( lens.mBladeCurvature, 0.0f, 1.0f ) * opening * opening;
                parameters.mBladeRotation = glm::radians( lens.mBladeRotation );
                parameters.mNearPlane = mCamera->GetNearPlane();
                parameters.mFarPlane = mCamera->GetFarPlane();
                parameters.mEnabled = lens.mEnabled && quality != 0 && parameters.mMaxRadius > 0.0f && !mCamera->IsOrthographic();
                parameters.mVisualize = parameters.mEnabled && mVisualizeDepthOfField;
            }

            return parameters;
        } };

        const auto dispatchDepthOfField{ []( CommandContext& ctx, DepthOfFieldParameters parameters,
            const std::array<FGTextureHandle, 8>& inputs, const std::array<FGTextureHandle, 2>& outputs,
            FGSamplerHandle sampler, FGPipelineHandle pipeline, core::u32 width, core::u32 height, core::u32 mipLevel = 0 ) -> void {
            for ( core::usize i{}; i < inputs.size(); ++i ) {
                if ( inputs[i].mHandle ) {
                    parameters.mInputs[i] = ctx.PushSRV( inputs[i] );
                }
            }

            for ( core::usize i{}; i < outputs.size(); ++i ) {
                if ( outputs[i].mHandle ) {
                    parameters.mOutputs[i] = ctx.PushUAV( outputs[i] );
                }
            }

            parameters.mSampler = ctx.PushSampler( sampler );
            parameters.mTargetWidth = width;
            parameters.mTargetHeight = height;
            parameters.mMipLevel = mipLevel;

            ctx.BindPipeline( pipeline );
            ctx.PushConstants( parameters );
            ctx.Dispatch( ( width + 7 ) / 8, ( height + 7 ) / 8, 1 );
        } };

        const auto registerDepthOfFieldCompute{ [&graph, makeParameters, dispatchDepthOfField](
            core::cstr name, core::cstr shader,
            std::array<FGTextureHandle, 8> inputs, std::array<FGTextureHandle, 2> outputs,
            FGSamplerHandle sampler, core::u32 width, core::u32 height, core::u32 mipLevel = 0, bool alwaysRun = false ) -> void {
            const auto pipeline{ graph.Create( FGPipelineDescription{}
                .SetName( name )
                .SetPipelineType( rhi::PipelineType::eCompute )
                .PushShader( shader, FGStageType::eCompute ) ) };

            graph.RegisterPass(
                name,
                FGPassType::eCompute,
                [=]( FGNodeBuilder& builder, core::Blackboard& ) {
                    for ( const auto input: inputs ) {
                        if ( input.mHandle ) {
                            builder.UseResource( input, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                        }
                    }

                    for ( const auto output: outputs ) {
                        if ( output.mHandle ) {
                            builder.UseResource( output, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
                        }
                    }
                },
                [=]( CommandContext& ctx, core::Blackboard& ) {
                    const auto parameters{ makeParameters() };

                    if ( parameters.mEnabled || alwaysRun ) {
                        dispatchDepthOfField( ctx, parameters, inputs, outputs, sampler, pipeline, width, height, mipLevel );
                    }
                } );
        } };

        // Setup: full-resolution HDR color + depth + lens settings -> full CoC and half-resolution RGB/CoC.
        // Bilateral downsampling keeps foreground colors from averaging into the background.
        const auto setupPipeline{ graph.Create( FGPipelineDescription{}
            .SetName( "DepthOfField_Setup" )
            .SetPipelineType( rhi::PipelineType::eCompute )
            .PushShader( "DepthOfFieldSetup_Comp.slang", FGStageType::eCompute ) ) };

        graph.RegisterPass(
            "DepthOfField_Setup",
            FGPassType::eCompute,
            [=]( FGNodeBuilder& builder, core::Blackboard& ) {
                builder.UseResource( sceneColor, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( depth, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( fullCoC, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
                builder.UseResource( setup, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
            },
            [=]( CommandContext& ctx, core::Blackboard& blackboard ) {
                const auto parameters{ makeParameters() };
                blackboard.Get<DisplayEffectsModuleInfo>().mVisualizeDepthOfField = parameters.mVisualize != 0;

                if ( parameters.mEnabled ) {
                    dispatchDepthOfField( ctx, parameters,
                        { sceneColor, depth }, { fullCoC, setup },
                        sampler, setupPipeline, width, height );
                }
            } );

        // FlattenCoC: half-resolution CoC -> per-8x8-tile foreground/background minimum and maximum CoC.
        // These bounds classify tiles and bound the radius of subsequent gathers.
        registerDepthOfFieldCompute( "DepthOfField_FlattenCoC", "DepthOfFieldFlattenCoC_Comp.slang",
            { setup }, { flattened }, sampler, tileWidth * 8, tileHeight * 8 );

        // DilateCoCMinMax: local tile bounds -> largest foreground/background radii that can reach each tile.
        // The shader visits the full required neighborhood, so no fixed iteration count limits large blurs.
        registerDepthOfFieldCompute( "DepthOfField_DilateCoCMinMax", "DepthOfFieldDilateCoCMinMax_Comp.slang",
            { flattened }, { dilatedMax }, sampler, tileWidth, tileHeight );

        // DilateCoCMinAbs: local bounds + expanded radii -> complete bounds including the CoC closest to focus.
        // This distinguishes uniform blur regions from depth discontinuities before choosing a gather mip.
        registerDepthOfFieldCompute( "DepthOfField_DilateCoCMinAbs", "DepthOfFieldDilateCoCMinAbs_Comp.slang",
            { flattened, dilatedMax }, { dilated }, sampler, tileWidth, tileHeight );

        // Downsample: half-resolution RGB/CoC -> quarter-resolution reference for the bilateral mip reduction.
        registerDepthOfFieldCompute( "DepthOfField_Downsample", "DepthOfFieldDownsample_Comp.slang",
            { setup }, { quarter }, sampler, quarterWidth, quarterHeight );

        // Reduce: half-resolution RGB/CoC + quarter reference -> bilateral color/CoC mip levels.
        // TODO: extract highlights only once sprite scattering works; keep their energy in the mips for now.
        // Separate images let the existing RHI bind each level as a UAV without adding mip-view APIs.
        std::array<FGTextureHandle, 4> mips{ setup };

        for ( core::u32 level{ 1 }; level < mips.size(); ++level ) {
            const auto mipWidth{ std::max( 1u, ( halfWidth + ( 1u << level ) - 1 ) >> level ) };
            const auto mipHeight{ std::max( 1u, ( halfHeight + ( 1u << level ) - 1 ) >> level ) };
            const auto name{ string::Format( "DepthOfField_Reduce{}", level ) };
            mips[level] = CreateDepthOfFieldTexture( graph, name.c_str(), mipWidth, mipHeight );
            registerDepthOfFieldCompute( name.c_str(), "DepthOfFieldReduce_Comp.slang",
                { mips[level - 1], quarter }, { mips[level] }, sampler, mipWidth, mipHeight, level );
        }

        // BuildBokehLUTScatter: blade count, curvature and rotation -> directional aperture radius/area lookup.
        // Also used by the half-resolution slight-defocus gather, even while sprite scattering is unavailable.
        registerDepthOfFieldCompute( "DepthOfField_BuildBokehLUTScatter", "DepthOfFieldBokehLUTScatter_Comp.slang",
            {}, { scatterLUT }, sampler, 64, 64 );

        // BuildBokehLUTGather: aperture parameters -> non-circular sample locations for the gather kernels.
        registerDepthOfFieldCompute( "DepthOfField_BuildBokehLUTGather", "DepthOfFieldBokehLUTGather_Comp.slang",
            {}, { gatherLUT }, sampler, 64, 64 );

        // GatherForeground: bilateral mips + dilated CoC + aperture samples -> premultiplied half-resolution foreground RGBA.
        // Mirrored CoC tests fill gaps within the foreground while alpha records blur coverage.
        registerDepthOfFieldCompute( "DepthOfField_GatherForeground", "DepthOfFieldGatherForeground_Comp.slang",
            { mips[0], mips[1], mips[2], mips[3], dilated, gatherLUT }, { foreground }, sampler, halfWidth, halfHeight );

        // PostfilterForeground: noisy foreground RGBA -> component-wise 3x3 median-filtered RGBA.
        registerDepthOfFieldCompute( "DepthOfField_PostfilterForeground", "DepthOfFieldPostfilterForeground_Comp.slang",
            { foreground }, { foregroundFiltered }, sampler, halfWidth, halfHeight );

        // ScatterForeground: would add extracted foreground highlight sprites to the filtered foreground.
        // TODO: the RHI exposes alpha blending, not additive blending. Leave this empty and retain highlights in Reduce.
        graph.RegisterPass(
            "DepthOfField_ScatterForeground",
            FGPassType::eGraphics,
            []( FGNodeBuilder&, core::Blackboard& ) {
            },
            []( CommandContext&, core::Blackboard& ) {
            } );

        // GatherHoleFilling: half-resolution RGB/CoC + bounds + foreground coverage -> separate fill color/opacity.
        // Approximate the background exposed at partially covered foreground edges; hidden geometry is unavailable.
        registerDepthOfFieldCompute( "DepthOfField_GatherHoleFilling", "DepthOfFieldGatherHoleFilling_Comp.slang",
            { setup, {}, {}, {}, dilated, gatherLUT, foregroundFiltered }, { holeFilling }, sampler, halfWidth, halfHeight );

        // GatherBackground: scene mips + CoC bounds + aperture samples -> premultiplied background RGBA.
        // Descending ring buckets let nearer background samples occlude farther ones.
        // TODO: publish CoC mean/variance for scatter occlusion when that pass is available; allocate no unused target yet.
        registerDepthOfFieldCompute( "DepthOfField_GatherBackground", "DepthOfFieldGatherBackground_Comp.slang",
            { mips[0], mips[1], mips[2], mips[3], dilated, gatherLUT }, { background }, sampler, halfWidth, halfHeight );

        // PostfilterBackground: noisy background RGBA -> component-wise 3x3 median-filtered RGBA.
        registerDepthOfFieldCompute( "DepthOfField_PostfilterBackground", "DepthOfFieldPostfilterBackground_Comp.slang",
            { background }, { backgroundFiltered }, sampler, halfWidth, halfHeight );

        // ScatterBackground: would add extracted background sprites, attenuated using the background CoC moments.
        // TODO: requires additive blending. Do not extract or discard highlight energy until this can be implemented.
        graph.RegisterPass(
            "DepthOfField_ScatterBackground",
            FGPassType::eGraphics,
            []( FGNodeBuilder&, core::Blackboard& ) {
            },
            []( CommandContext&, core::Blackboard& ) {
            } );

        // GatherSlightDefocus: original half-resolution RGB/CoC + aperture lookup -> local focus color/coverage.
        // A brute-force radius-three kernel supplies bounds for the full-resolution gather in Recombine.
        registerDepthOfFieldCompute( "DepthOfField_GatherSlightDefocus", "DepthOfFieldGatherSlightDefocus_Comp.slang",
            { setup, dilated, scatterLUT }, { slightDefocus }, sampler, halfWidth, halfHeight );

        // BuildBokehLUTRecombine: aperture parameters -> subpixel aperture distances for the full-resolution focus gather.
        registerDepthOfFieldCompute( "DepthOfField_BuildBokehLUTRecombine", "DepthOfFieldBokehLUTRecombine_Comp.slang",
            {}, { recombineLUT }, sampler, 64, 64 );

        // Recombine: full scene/CoC + foreground/background/fill/focus layers -> full-resolution HDR color.
        // Use fixed spatial samples because there is no TAA; disabling DoF copies the original scene unchanged.
        registerDepthOfFieldCompute( "DepthOfField_Recombine", "DepthOfFieldRecombine_Comp.slang",
            { sceneColor, fullCoC, foregroundFiltered, backgroundFiltered, holeFilling, slightDefocus, recombineLUT, setup },
            { info.mDepthOfFieldColor }, sampler, width, height, 0, true );
    }

    auto DisplayEffectsModule::SetDepthOfFieldQuality( DepthOfFieldQuality quality, bool visualize ) -> void {
        mDepthOfFieldQuality = quality;
        mVisualizeDepthOfField = visualize;
    }

    auto DisplayEffectsModule::SetChromaticAberration( const ChromaticAberrationSettings& settings ) -> void {
        mChromaticAberration = settings;
    }

    auto DisplayEffectsModule::RegisterChromaticAberration( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        auto& info{ graph.GetOrCreate<DisplayEffectsModuleInfo>() };

        const auto dimensions{ InferDimensions( mResolution ) };

        auto colorImage{ FGTextureDescription{}
            .SetName( "ChromaticAberration_ColorImage01" )
            .SetWidth( core::as<core::i32>( dimensions.first ) )
            .SetHeight( core::as<core::i32>( dimensions.second ) )
            .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
            .SetMultisampling( renderer::rhi::Multisampling::eMsaaX1 )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::RenderTarget |
                           renderer::rhi::TextureUsageFlagsBits::ShaderResource )
            .SetFormat( renderer::rhi::Format::eRGBA8_UNORM ) };
        info.mChromaAbRenderTarget = graph.Create( colorImage );

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "ChromaticAberration_Pipeline" )
            .SetPipelineType( renderer::rhi::PipelineType::eGraphics )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList )
            .SetDepthTest( false )
            .SetDepthWrite( false )
            .SetBlendEnable( false )
            .AddColorFormat( renderer::rhi::Format::eRGBA8_UNORM )
            .SetCullMode( renderer::rhi::CullMode::eNone )
            .PushShader( "ChromaticAberration_Vert.slang", FGStageType::eVertex )
            .PushShader( "ChromaticAberration_Frag.slang", FGStageType::ePixel ) };
        info.mChromaAbPipeline = graph.Create( pipelineBuilder );

        const auto sampler{ graph.Create( FGSamplerDescription{}
            .SetFilter( renderer::rhi::SamplerFilter::eLinear )
            .SetWrap( renderer::rhi::SamplerWrapMode::eClampToEdge ) ) };

        graph.RegisterPass(
            "ChromaticAberration",
            FGPassType::eGraphics,
            []( FGNodeBuilder& builder, core::Blackboard& blackboard ) {
                const auto& displayEffectsData{ blackboard.Get<DisplayEffectsModuleInfo>() };

                builder.UseResource( blackboard.Get<GeomShadingModuleInfo>().mTonemapColor, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( displayEffectsData.mChromaAbRenderTarget, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [this, sampler]( CommandContext& ctx, core::Blackboard& blackboard ) {
                auto& displayEffectsData{ blackboard.Get<DisplayEffectsModuleInfo>() };
                // Older projects keep their renderer-wide effect until an active material supplies it.
                displayEffectsData.mChromaticAberrationEnabled = mChromaticAberration.mEnabled &&
                    mChromaticAberration.mIntensity > 0.0f && !displayEffectsData.mVisualizeDepthOfField &&
                    !HasChromaticAberrationMaterial( mScene );

                if ( !displayEffectsData.mChromaticAberrationEnabled ) {
                    return;
                }

                const auto [width, height]{ InferDimensions( mResolution ) };
                struct DrawParams {
                    core::u32 mSamplerID{};
                    core::u32 mTonemapImageID{};
                    core::float2 mTexelSize{};
                    core::f32 mIntensity{};
                    core::f32 mDirection{};
                } params{
                    .mSamplerID = ctx.PushSampler( sampler ),
                    .mTonemapImageID = ctx.PushSRV( blackboard.Get<GeomShadingModuleInfo>().mTonemapColor ),
                    .mTexelSize = { 1.0f / width, 1.0f / height },
                    .mIntensity = mChromaticAberration.mIntensity,
                    .mDirection = glm::radians( mChromaticAberration.mDirection )
                };

                ctx.PushConstants( params );

                const auto dimensions{ InferDimensions( mResolution ) };
                const auto graphicsState{ RenderBeginDescription{}
                    .SetRenderArea( renderer::rhi::Rect{ core::as<core::i32>( dimensions.first ), core::as<core::i32>( dimensions.second ) } )
                    .AddRenderTarget( displayEffectsData.mChromaAbRenderTarget, renderer::rhi::kColorCyan, renderer::rhi::LoadOp::eClear ) };
                ctx.BeginRender( graphicsState );

                ctx.SetViewportState( renderer::rhi::ViewportState{}
                    .AddViewportAndScissorRect( renderer::rhi::Viewport( core::as<core::f32>( dimensions.first ),
                            core::as<core::f32>( dimensions.second ) ) ) );

                ctx.BindPipeline( displayEffectsData.mChromaAbPipeline );

                ctx.Draw( 3 );

                ctx.EndRender();
            } );
    }

    auto DisplayEffectsModule::RegisterLensDirtPass( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass(
            "LensDirt",
            FGPassType::eGraphics,
            []( FGNodeBuilder&, core::Blackboard& ) {

            },
            []( CommandContext&, core::Blackboard& ) {
                // Nothing
            } );
    }

    auto DisplayEffectsModule::RegisterLensFlarePass( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass(
            "LensFlare",
            FGPassType::eGraphics,
            []( FGNodeBuilder&, core::Blackboard& ) {

            },
            []( CommandContext&, core::Blackboard& ) {
                // Nothing
            } );
    }

    auto DisplayEffectsModule::RegisterMotionBlurrPass( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER();

        // https://developer.nvidia.com/gpugems/gpugems3/part-iv-image-effects/chapter-27-motion-blur-post-processing-effect
        graph.RegisterPass(
            "MotionBlurr",
            FGPassType::eGraphics,
            []( FGNodeBuilder&, core::Blackboard& ) {

            },
            []( CommandContext&, core::Blackboard& ) {
                // Nothing
            } );
    }

    auto DisplayEffectsModule::RegisterVignetteShapeRenderPass( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER();
        auto& info{ graph.GetOrCreate<DisplayEffectsModuleInfo>() };

        /*
          * TODO: Everything can be rendered in a single pass. There is no need to generate
          * a separate shape mask beforehand. Instead, the signed distance function (SDF)
          * and smoothstep are evaluated directly in the fragment shader.
          *
          * Example:
          *
          * float4 MainPS(float2 uv : TEXCOORD0) : SV_Target
          * {
          *     float3 sceneColor = sceneTexture.Sample(linearSampler, uv).rgb;
          *
          *     float2 p = (uv - center) * 2.0;
          *     p.x *= aspectRatio;
          *
          *     float distanceToEdge;
          *     if (shape == 0) {
          *         distanceToEdge = length(p) - radius;
          *     } else {
          *         distanceToEdge = SignedDistanceBox(p, halfSize);
          *     }
          *
          *     float mask = smoothstep(-feather, feather, distanceToEdge);
          *     float alpha = saturate(mask * strength);
          *
          *     float3 result = lerp(sceneColor, vignetteColor, alpha);
          *     return float4(result, 1.0);
          * }
          * */

        const auto [width, height]{ InferDimensions( mResolution ) };

        // https://iquilezles.org/articles/distfunctions/
        // https://iquilezles.org/articles/distfunctions2d/
        info.mVignetteShapeTarget = graph.Create( FGTextureDescription{}
            .SetName( "Vignette_ShapeMask" )
            .SetWidth( core::as<core::u32>( width ) )
            .SetHeight( core::as<core::u32>( height ) )
            .SetFormat( rhi::Format::eR16_FLOAT )
            .SetUsage( rhi::TextureUsageFlagsBits::RenderTarget | rhi::TextureUsageFlagsBits::ShaderResource ) );

        info.mVignetteShapePipeline = graph.Create( FGPipelineDescription{}
            .SetName( "Vignette_ShapePipeline" )
            .SetPipelineType( rhi::PipelineType::eGraphics )
            .SetTopology( rhi::PrimitiveTopology::eTriangleList )
            .SetCullMode( rhi::CullMode::eNone )
            .SetDepthTest( false )
            .SetDepthWrite( false )
            .SetBlendEnable( false )
            .AddColorFormat( rhi::Format::eR16_FLOAT )
            .PushShader( "FullQuad_Vert.slang", FGStageType::eVertex )
            .PushShader( "VignetteShape_Frag.slang", FGStageType::ePixel ) );

        graph.RegisterPass(
            "Vignette_RenderShape",
            FGPassType::eGraphics,
            []( FGNodeBuilder& builder, core::Blackboard& blackboard ) {
                builder.UseResource( blackboard.Get<DisplayEffectsModuleInfo>().mVignetteShapeTarget,
                    FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [this]( CommandContext& ctx, core::Blackboard& blackboard ) {
                auto& effects{ blackboard.Get<DisplayEffectsModuleInfo>() };
                const auto* camera{ dynamic_cast<const scene::SceneCamera*>( mCamera ) };

                if ( !camera ) {
                    return;
                }

                effects.mVignetteEnabled = camera->GetVignetteSettings().mEnabled &&
                                           camera->GetVignetteSettings().mStrength > 0.0f;

                if ( !effects.mVignetteEnabled ) {
                    return;
                }

                const auto& settings{ camera->GetVignetteSettings() };
                const auto [width, height]{ InferDimensions( mResolution ) };
                // Keep offsets identical to VignetteShape_Frag.slang.
                struct DrawParams {
                    core::float2 mCenter{};
                    core::float2 mHalfSize{};
                    core::f32 mAspectRatio{};
                    core::f32 mFeather{};
                    core::u32 mShape{};
                    core::u32 mPadding{};
                } params{
                    .mCenter = settings.mCenter,
                    .mHalfSize = settings.mHalfSize,
                    .mAspectRatio = settings.mAspectCorrect ? width / height : 1.0f,
                    .mFeather = settings.mFeather,
                    .mShape = core::as<core::u32>( settings.mShape )
                };

                ctx.PushConstants( params );

                ctx.BeginRender( RenderBeginDescription{}
                    .SetRenderArea( rhi::Rect{ core::as<core::i32>( width ), core::as<core::i32>( height ) } )
                    .AddRenderTarget( effects.mVignetteShapeTarget, rhi::Color{ 0.0f }, rhi::LoadOp::eClear ) );
                ctx.SetViewportState( rhi::ViewportState{}.AddViewportAndScissorRect( rhi::Viewport( width, height ) ) );
                ctx.BindPipeline( effects.mVignetteShapePipeline );
                ctx.Draw( 3 );
                ctx.EndRender();
            } );
    }

    auto DisplayEffectsModule::RegisterVignetteShapeCompositePass( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER();

        auto& info{ graph.GetOrCreate<DisplayEffectsModuleInfo>() };
        const auto [width, height]{ InferDimensions( mResolution ) };

        // Second fullscreen draw: display color + shape mask + camera tint/strength -> vignetted color.
        info.mVignetteCompositeTarget = graph.Create( FGTextureDescription{}
            .SetName( "Vignette_CompositeColor" )
            .SetWidth( core::as<core::u32>( width ) )
            .SetHeight( core::as<core::u32>( height ) )
            .SetFormat( rhi::Format::eRGBA8_UNORM )
            .SetUsage( rhi::TextureUsageFlagsBits::RenderTarget | rhi::TextureUsageFlagsBits::ShaderResource ) );

        info.mVignetteCompositePipeline = graph.Create( FGPipelineDescription{}
            .SetName( "Vignette_CompositePipeline" )
            .SetPipelineType( rhi::PipelineType::eGraphics )
            .SetTopology( rhi::PrimitiveTopology::eTriangleList )
            .SetCullMode( rhi::CullMode::eNone )
            .SetDepthTest( false )
            .SetDepthWrite( false )
            .SetBlendEnable( false )
            .AddColorFormat( rhi::Format::eRGBA8_UNORM )
            .PushShader( "FullQuad_Vert.slang", FGStageType::eVertex )
            .PushShader( "VignetteComposite_Frag.slang", FGStageType::ePixel ) );

        const auto sampler{ graph.Create( FGSamplerDescription{}
            .SetName( "Vignette_LinearClamp" )
            .SetFilter( rhi::SamplerFilter::eLinear )
            .SetWrap( rhi::SamplerWrapMode::eClampToEdge ) ) };

        graph.RegisterPass(
            "Vignette_Composite",
            FGPassType::eGraphics,
            []( FGNodeBuilder& builder, core::Blackboard& blackboard ) {
                const auto& effects{ blackboard.Get<DisplayEffectsModuleInfo>() };
                builder.UseResource( effects.mVignetteShapeTarget, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( blackboard.Get<GeomShadingModuleInfo>().mTonemapColor,
                    FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                // The source is selected at execution time, so declare both possible readers.
                builder.UseResource( effects.mChromaAbRenderTarget, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( effects.mVignetteCompositeTarget, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [this, sampler]( CommandContext& ctx, core::Blackboard& blackboard ) {
                auto& effects{ blackboard.Get<DisplayEffectsModuleInfo>() };
                const auto* camera{ dynamic_cast<const scene::SceneCamera*>( mCamera ) };

                // This pass depends on tone mapping, so the current DoF visualization flag is ready.
                effects.mVignetteEnabled = effects.mVignetteEnabled && !effects.mVisualizeDepthOfField;

                if ( !effects.mVignetteEnabled || !camera ) {
                    return;
                }

                const auto& settings{ camera->GetVignetteSettings() };

                // Keep offsets identical to VignetteComposite_Frag.slang.
                struct DrawParams {
                    core::u32 mSceneImage{};
                    core::u32 mMaskImage{};
                    core::u32 mSampler{};
                    core::f32 mStrength{};
                    core::float3 mColor{};
                    core::f32 mPadding{};
                } params{
                    .mSceneImage = ctx.PushSRV( effects.mChromaticAberrationEnabled
                        ? effects.mChromaAbRenderTarget
                        : blackboard.Get<GeomShadingModuleInfo>().mTonemapColor ),
                    .mMaskImage = ctx.PushSRV( effects.mVignetteShapeTarget ),
                    .mSampler = ctx.PushSampler( sampler ),
                    .mStrength = settings.mStrength,
                    .mColor = settings.mColor
                };

                ctx.PushConstants( params );

                const auto [width, height]{ InferDimensions( mResolution ) };
                ctx.BeginRender( RenderBeginDescription{}
                    .SetRenderArea( rhi::Rect{ core::as<core::i32>( width ), core::as<core::i32>( height ) } )
                    .AddRenderTarget( effects.mVignetteCompositeTarget, rhi::Color{ 0.0f }, rhi::LoadOp::eClear ) );
                ctx.SetViewportState( rhi::ViewportState{}.AddViewportAndScissorRect( rhi::Viewport( width, height ) ) );
                ctx.BindPipeline( effects.mVignetteCompositePipeline );
                ctx.Draw( 3 );
                ctx.EndRender();
            } );
    }

    auto DisplayEffectsModule::RegisterLensDistortionPass( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass(
            "LensDistortion",
            FGPassType::eGraphics,
            []( FGNodeBuilder&, core::Blackboard& ) {

            },
            []( CommandContext&, core::Blackboard& ) {
                // Nothing
            } );
    }
}// namespace mikoto::renderer