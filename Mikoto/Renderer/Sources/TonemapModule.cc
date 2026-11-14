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

#include <EASTL/array.h>
#include <EASTL/fixed_vector.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Math/Math.hh>
#include <Math/Random.hh>

#include <Renderer/Core/CommandContext.hh>
#include <Renderer/ForwardRenderer/Passes/CameraModule.hh>
#include <Renderer/ForwardRenderer/Passes/PrepassModule.hh>
#include <Renderer/ForwardRenderer/Passes/TonemapModule.hh>
#include <Renderer/ForwardRenderer/Passes/PostProcessModule.hh>
#include <Renderer/ForwardRenderer/Passes/DisplayEffectsModule.hh>
#include <Renderer/ForwardRenderer/Passes/GeometryShadingModule.hh>

#include <Scene/Scene.hh>
#include <Scene/Component.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::renderer::rhi;

    TonemapModule::TonemapModule( renderer::rhi::RenderResolution resolution )
        : mResolution{ resolution } {
    }

    auto TonemapModule::SetToneMapping( ToneMapper type ) -> void {
        MKT_BEGIN_PROFILER();
        mToneMapType = type;
    }

    auto TonemapModule::SetExposure( core::f32 exposure ) -> void {
        mExposure = exposure;
    }

    auto TonemapModule::SetGamma( core::f32 gamma ) -> void {
        mGamma = gamma;
    }

    auto TonemapModule::RegisterTonemapPass( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        auto& info{ graph.GetOrCreate<GeomShadingModuleInfo>() };
        const auto source{ graph.GetData<PostProcessModuleInfo>().mBeforeToneMapping.mColorImage };
        const auto sampler{ graph.Create( FGSamplerDescription{}
            .SetName( "Tonemap_LinearClamp" )
            .SetFilter( SamplerFilter::eLinear )
            .SetWrap( SamplerWrapMode::eClampToEdge ) ) };

        const auto dimensions{ InferDimensions( mResolution ) };

        auto colorImage{ FGTextureDescription{}
            .SetName( "Tonemap_ColorImage01" )
            .SetWidth( core::as<core::i32>( dimensions.first ) )
            .SetHeight( core::as<core::i32>( dimensions.second ) )
            .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::RenderTarget |
                           renderer::rhi::TextureUsageFlagsBits::ShaderResource )
            .SetFormat( renderer::rhi::Format::eRGBA8_UNORM ) };
        info.mTonemapColor = graph.Create( colorImage );

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "Tonemap_Pipeline" )
            .SetPipelineType( renderer::rhi::PipelineType::eGraphics )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList )
            .SetBlendEnable( false )
            .SetDepthTest( false )
            .SetDepthWrite( false )
            .AddColorFormat( renderer::rhi::Format::eRGBA8_UNORM )
            .SetCullMode( renderer::rhi::CullMode::eNone )
            .PushShader( "Tonemap_Vert.slang", FGStageType::eVertex )
            .PushShader( "Tonemap_Frag.slang", FGStageType::ePixel ) };
        info.mTonemapPipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass(
            "Tonemap",
            FGPassType::eGraphics,
            [source]( FGNodeBuilder& builder, core::Blackboard& blackboard ) {
                auto& geometryData{ blackboard.Get<GeomShadingModuleInfo>() };

                // The before-stage input can be raster camera effects or accumulated path-traced HDR.
                builder.UseResource( source, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( blackboard.Get<PostProcessModuleInfo>().mBloomResult, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );

                builder.UseResource( geometryData.mTonemapColor, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [this, source, sampler]( CommandContext& ctx, core::Blackboard& blackboard ) {
                const auto& geometryData{ blackboard.Get<GeomShadingModuleInfo>() };
                const auto& post{ blackboard.Get<PostProcessModuleInfo>() };

                struct DrawParams {
                    core::u32 mFinalImageID{};
                    core::u32 mBasicSamplerID{};

                    core::f32 mExposure{};
                    core::f32 mGamma{};

                    core::i32 mToneMapType{};
                    core::u32 mBloomImageID{};
                    core::u32 mBloomSamplerID{};
                    core::f32 mBloomIntensity{};
                    core::u32 mBloomEnabled{};
                    core::u32 mVisualizeDepthOfField{};
                } params{
                    .mFinalImageID = ctx.PushTexture_SRV( source ),
                    .mBasicSamplerID = ctx.PushSampler( sampler ),
                    .mExposure = mExposure,
                    .mGamma = mGamma,
                    .mToneMapType = core::as<core::i32>( mToneMapType ),
                    .mBloomImageID = ctx.PushTexture_SRV( post.mBloomResult ),
                    .mBloomSamplerID = ctx.PushSampler( post.mBloomSampler ),
                    .mBloomIntensity = post.mBloomSettings.mIntensity,
                    .mBloomEnabled = post.mBloomSettings.mEnabled && post.mBloomSettings.mIntensity > 0.0f ? 1u : 0u,
                    .mVisualizeDepthOfField = blackboard.Get<DisplayEffectsModuleInfo>().mVisualizeDepthOfField ? 1u : 0u
                };

                ctx.PushConstants( params );

                const auto dimensions{ InferDimensions( mResolution ) };
                const auto graphicsState{ RenderBeginDescription{}
                    .SetRenderArea( renderer::rhi::Rect{ core::as<core::i32>( dimensions.first ), core::as<core::i32>( dimensions.second ) } )
                    .AddRenderTarget( geometryData.mTonemapColor, renderer::rhi::kColorCyan, renderer::rhi::LoadOp::eClear ) };
                ctx.BeginRender( graphicsState );

                ctx.SetViewportState( renderer::rhi::ViewportState{}
                    .AddViewportAndScissorRect( renderer::rhi::Viewport( core::as<core::f32>( dimensions.first ), core::as<core::f32>( dimensions.second ) ) ) );

                ctx.BindPipeline( geometryData.mTonemapPipeline );

                ctx.Draw( 3 );

                ctx.EndRender();
            } );
    }

    auto TonemapModule::RegisterColorGradientPass( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        auto& info{ graph.GetOrCreate<GeomShadingModuleInfo>() };

        const auto dimensions{ InferDimensions( mResolution ) };

        auto colorImage{ FGTextureDescription{}
            .SetName( "ColorGradient_ColorImage01" )
            .SetWidth( core::as<core::i32>( dimensions.first ) )
            .SetHeight( core::as<core::i32>( dimensions.second ) )
            .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::RenderTarget | renderer::rhi::TextureUsageFlagsBits::ShaderResource )
            .SetFormat( renderer::rhi::Format::eRGBA16_FLOAT ) };
        info.mColorGradientRenderTarget = graph.Create( colorImage );

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "ColorGradient_Pipeline" )
            .SetPipelineType( renderer::rhi::PipelineType::eGraphics )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList )
            .SetBlendEnable( false )
            .AddColorFormat( renderer::rhi::Format::eRGBA16_FLOAT )
            .SetCullMode( renderer::rhi::CullMode::eNone )
            .PushShader( "ColorGradient_Vert.slang", FGStageType::eVertex )
            .PushShader( "ColorGradient_Frag.slang", FGStageType::ePixel ) };
        info.mColorGradientPipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass(
            "ColorGradient",
            FGPassType::eGraphics,
            []( FGNodeBuilder& builder, core::Blackboard& blackboard ) {
                auto& geometryData{ blackboard.Get<GeomShadingModuleInfo>() };

                builder.UseResource( geometryData.mColorImage, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( geometryData.mColorGradientRenderTarget, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [this]( CommandContext& ctx, core::Blackboard& blackboard ) {
                const auto& cameraData{ blackboard.Get<CameraModuleInfo>() };
                const auto& geometryData{ blackboard.Get<GeomShadingModuleInfo>() };

                struct DrawParams {
                    core::u32 mSamplerID{};
                    core::u32 mFinalImageID{};

                    core::u32 mCameraBufferID{};

                    core::f32 mExposure{};
                    core::f32 mContrast{};
                    core::f32 mSaturation{};
                    core::float4 mColorTint{ 0.2f, 0.3f, 0.5f, 1.0f };
                } params{
                    .mSamplerID = ctx.PushSampler( geometryData.mDefaultSampler ),
                    .mFinalImageID = ctx.PushTexture_SRV( geometryData.mColorImage ),

                    .mCameraBufferID = ctx.PushBuffer_SRV( cameraData.mCameraData ),

                    .mExposure = 2.3f,
                    .mContrast = 3.5f,
                    .mSaturation = 2.1f
                };

                ctx.PushConstants( params );

                const auto dimensions{ InferDimensions( mResolution ) };
                const auto graphicsState{ RenderBeginDescription{}
                    .SetRenderArea( renderer::rhi::Rect{ core::as<core::i32>( dimensions.first ), core::as<core::i32>( dimensions.second ) } )
                    .AddRenderTarget( geometryData.mColorGradientRenderTarget, renderer::rhi::kColorCyan, renderer::rhi::LoadOp::eClear ) };
                ctx.BeginRender( graphicsState );

                ctx.SetViewportState( renderer::rhi::ViewportState{}
                    .AddViewportAndScissorRect( renderer::rhi::Viewport( core::as<core::f32>( dimensions.first ), core::as<core::f32>( dimensions.second ) ) ) );

                ctx.BindPipeline( geometryData.mColorGradientPipeline );

                ctx.Draw( 3 );

                ctx.EndRender();
            } );
    }
}// mikoto
