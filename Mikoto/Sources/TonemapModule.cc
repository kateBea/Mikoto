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
#include <Core/Profiler.hh>
#include <Core/String.hh>
#include <Core/Types.hh>

#include <Math/Math.hh>
#include <Math/Random.hh>

#include <Scene/Scene.hh>
#include <Scene/Component.hh>

#include <Renderer/Core/CommandContext.hh>

#include <Renderer/Passes/CameraModule.hh>
#include <Renderer/Passes/PrepassModule.hh>
#include <Renderer/Passes/PostProcessModule.hh>
#include <Renderer/Passes/GeometryShadingModule.hh>

#include <Renderer/Passes/TonemapModule.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::renderer::rhi;

    TonemapModule::TonemapModule( RenderResolution resolution )
        : mResolution{ resolution }
    {}

    auto TonemapModule::RegisterPasses( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        RegisterColorGradientPass( graph );
        RegisterTonemapPass( graph );
    }


    auto TonemapModule::SetToneMapping( ToneMappingType type ) -> void {
        mToneMapType = type;
    }

    auto TonemapModule::RegisterTonemapPass( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        auto& info{ graph.GetOrCreate<GeomShadingModuleInfo>() };

        const auto dimensions{ InferDimensions( mResolution ) };

        auto colorImage{ FGTextureDescription{}
            .SetName( "Tonemap_ColorImage01" )
            .SetWidth( as<i32>( dimensions.first ) )
            .SetHeight( as<i32>( dimensions.second ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetUsage( TextureUsageFlagsBits::kRenderTarget | TextureUsageFlagsBits::kShaderResource )
            .SetFormat( Format::eRGBA8_UNORM ) };
        info.mTonemapColor = graph.Create( colorImage );

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "Tonemap_Pipeline" )
            .SetPipelineType( PipelineType::eGraphics )
            .SetTopology( PrimitiveTopology::eTriangleList )
            .AddColorFormat( Format::eRGBA8_UNORM )
            .SetCullMode( CullMode::eNone )
            .PushShader( "Tonemap_Vert.slang", FGStageType::eVertex )
            .PushShader( "Tonemap_Frag.slang", FGStageType::ePixel ) };
        info.mTonemapPipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass(
            "Tonemap",
            FGPassType::eGraphics,
            []( FGNodeBuilder& builder, Blackboard& blackboard ) {
                auto& geometryData{ blackboard.Get<GeomShadingModuleInfo>() };

                builder.UseResource( geometryData.mColorImage, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( geometryData.mTonemapColor, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [this]( CommandContext &ctx, Blackboard& blackboard ) {
                const auto& geometryData{ blackboard.Get<GeomShadingModuleInfo>() };

                struct DrawParams {
                    u32 mFinalImageID{};
                    u32 mBasicSamplerID{};

                    f32 mExposure{};
                    f32 mGamma{};

                    i32 mToneMapType{};
                } params{
                    .mFinalImageID = ctx.PushTexture_SRV( geometryData.mColorImage ),
                    .mBasicSamplerID = ctx.PushSampler( geometryData.mDefaultSampler ),

                    .mExposure = geometryData.mExposure,
                    .mGamma = geometryData.mExposure,

                    .mToneMapType = as<i32>(mToneMapType) };

                ctx.PushConstants( params );

                const auto dimensions{ InferDimensions( mResolution ) };
                const auto graphicsState{ ContextRenderState{}
                    .SetRenderArea( Rect{ as<i32>(dimensions.first), as<i32>(dimensions.second) } )
                    .AddRenderTarget( geometryData.mTonemapColor, kColorCyan, LoadOp::eClear ) };
                ctx.BeginRender( graphicsState );

                ctx.SetViewportState( ViewportState{}
                    .AddViewportAndScissorRect( Viewport( as<f32>(dimensions.first), as<f32>(dimensions.second) ) ) );

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
            .SetWidth( as<i32>( dimensions.first ) )
            .SetHeight( as<i32>( dimensions.second ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetUsage( TextureUsageFlagsBits::kRenderTarget | TextureUsageFlagsBits::kShaderResource )
            .SetFormat( Format::eRGBA16_FLOAT ) };
        info.mColorGradientRenderTarget = graph.Create( colorImage );

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "ColorGradient_Pipeline" )
            .SetPipelineType( PipelineType::eGraphics )
            .SetTopology( PrimitiveTopology::eTriangleList )
            .AddColorFormat( Format::eRGBA16_FLOAT )
            .SetCullMode( CullMode::eNone )
            .PushShader( "ColorGradient_Vert.slang", FGStageType::eVertex )
            .PushShader( "ColorGradient_Frag.slang", FGStageType::ePixel ) };
        info.mColorGradientPipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass(
            "ColorGradient",
            FGPassType::eGraphics,
            []( FGNodeBuilder& builder, Blackboard& blackboard ) {
                auto& geometryData{ blackboard.Get<GeomShadingModuleInfo>() };

                builder.UseResource( geometryData.mColorImage, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( geometryData.mColorGradientRenderTarget, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [this]( CommandContext &ctx, Blackboard& blackboard ) {
                const auto& cameraData{ blackboard.Get<CameraModuleInfo>() };
                const auto& geometryData{ blackboard.Get<GeomShadingModuleInfo>() };

                struct DrawParams {
                    u32 mSamplerID{};
                    u32 mFinalImageID{};

                    u32 mCameraBufferID{};

                    f32 mExposure{};
                    f32 mContrast{};
                    f32 mSaturation{};
                    float4 mColorTint{ 0.2f, 0.3f, 0.5f, 1.0f };
                } params{
                    .mSamplerID = ctx.PushSampler( geometryData.mDefaultSampler ),
                    .mFinalImageID = ctx.PushTexture_SRV( geometryData.mColorImage ),

                    .mCameraBufferID = ctx.PushBuffer_SRV( cameraData.mCameraData ),

                    .mExposure = 2.3f,
                    .mContrast = 3.5f,
                    .mSaturation = 2.1f };

                ctx.PushConstants( params );

                const auto dimensions{ InferDimensions( mResolution ) };
                const auto graphicsState{ ContextRenderState{}
                    .SetRenderArea( Rect{ as<i32>(dimensions.first), as<i32>(dimensions.second) } )
                    .AddRenderTarget( geometryData.mColorGradientRenderTarget, kColorCyan, LoadOp::eClear ) };
                ctx.BeginRender( graphicsState );

                ctx.SetViewportState( ViewportState{}
                    .AddViewportAndScissorRect( Viewport( as<f32>(dimensions.first), as<f32>(dimensions.second) ) ) );

                ctx.BindPipeline( geometryData.mColorGradientPipeline );

                ctx.Draw( 3 );

                ctx.EndRender();
            } );
    }
} // mikoto