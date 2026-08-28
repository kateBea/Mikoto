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

namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::scene;
    using namespace mikoto::renderer::rhi;

    PostEffectsPass::PostEffectsPass( RenderResolution resolution )
        : mResolution{ resolution } {
    }

    auto PostEffectsPass::SetScene( const Scene* scene ) -> void {
        MKT_BEGIN_PROFILER_NAMED();
        mScene = scene;
    }

    auto PostEffectsPass::SetCamera( const Camera* camera ) -> void {
        MKT_BEGIN_PROFILER_NAMED();
        mCamera = camera;
    }

    auto PostEffectsPass::RegisterPasses( FrameGraph& graph) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        RegisterBloom( graph );
        RegisterSsao( graph );

        RegisterEyeAdaptationPass( graph );

        RegisterObjectOutline( graph );

        RegisterPostProcess( graph );

        RegisterInfiniteGrid( graph );
    }

    auto PostEffectsPass::SetEnableBloom( bool value ) -> void {
        mEnableBloom = value;
    }

    auto PostEffectsPass::SetGamma( f32 gamma ) -> void {
        mGamma = gamma;
    }

    auto PostEffectsPass::SetExposure( f32 exposure ) -> void {
        mExposure = exposure;
    }

    auto PostEffectsPass::RegisterInfiniteGrid( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        auto& info{ graph.GetOrCreate<PostProcessModuleInfo>() };

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "InfiniteGrid_Pipeline" )
            .SetPipelineType( PipelineType::eGraphics )
            .SetTopology( PrimitiveTopology::eTriangleList )
            .SetPolygonMode( PolygonMode::eFill )
            .SetDepthFormat( Format::eD32 )
            .AddColorFormat( Format::eRGBA16_FLOAT )
            .PushShader( "InfiniteGrid_Vert.slang", FGStageType::eVertex )
            .PushShader( "InfiniteGrid_Frag.slang", FGStageType::ePixel ) };
        info.mInfiniteGridPipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass(
            "InfiniteGrid",
            FGPassType::eGraphics,
            []( FGNodeBuilder& builder, Blackboard& blackboard ) {
                const auto& cameraData{ blackboard.Get<CameraModuleInfo>() };
                const auto& finalCompData{ blackboard.Get<GeomShadingModuleInfo>() };
                const auto& prePassData{ blackboard.Get<PrepassModuleInfo>() };

                builder.UseResource( cameraData.mCameraData, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( prePassData.mPrepassDepthTarget, FGPipelineStage::eDepthTarget, FGResourceAccess::eRead );
                builder.UseResource( finalCompData.mColorImage, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [this]( CommandContext& ctx, Blackboard& b ) {
                const auto& cameraData{ b.Get<CameraModuleInfo>() };
                const auto& prePassData{ b.Get<PrepassModuleInfo>() };
                const auto& finalCompData{ b.Get<GeomShadingModuleInfo>() };
                const auto& postProcessData{ b.Get<PostProcessModuleInfo>() };

                const auto dimensions{ InferDimensions( mResolution ) };
                const auto graphicsState{ ContextRenderState{}
                    .SetRenderArea( Rect{ as<i32>( dimensions.first ), as<i32>( dimensions.second ) } )
                    .AddDepthTarget( prePassData.mPrepassDepthTarget, LoadOp::eLoad )
                    .AddRenderTarget( finalCompData.mColorImage, kColorGreen, LoadOp::eLoad ) };
                ctx.BeginRender( graphicsState );

                struct DrawParams {
                    u32 mCameraBufferID{};

                    float4 mOuterSquareColor{ 0.8f, 0.8f, 0.8f, 0.8f };
                    float4 mInnerSquareColor{ 0.8f, 0.8f, 0.8f, 0.8f };

                    float4 mXAxisColor{ 0.0, 0.0, 1.0, 1.0 };
                    float4 mZAxisColor{ 1.0, 0.0, 0.0, 1.0 };

                    f32 mOuterSquareWidth{ 0.5f };
                    f32 mInnerSquareWidth{ 1.0f };

                    f32 mXAxisWidth{ 6.0f };
                    f32 mZAxisWidth{ 6.0f };
                } params{
                    .mCameraBufferID = ctx.PushBuffer_SRV( cameraData.mCameraData ) };
                ctx.PushConstants( params );

                ctx.SetViewportState( ViewportState{}
                    .AddViewportAndScissorRect( Viewport( as<f32>( dimensions.first ), as<f32>( dimensions.second ) ) ) );

                ctx.BindPipeline( postProcessData.mInfiniteGridPipeline );
                ctx.Draw( 3 );

                ctx.EndRender();
            } );
    }

    auto PostEffectsPass::RegisterObjectOutline( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // First pass here will require the positions
        // So it will probably depend on GBuffer_Positions

        // Filter objects that mush appear outlined
        graph.RegisterPass(
            "OutlineObjectsFilter",
            FGPassType::eTransfer,
            []( FGNodeBuilder&, Blackboard & ) {

            },
            []( CommandContext&, Blackboard & ) {
                // Nothing
            } );

        graph.RegisterPass(
            "OutlineMask",
            FGPassType::eGraphics,
            []( FGNodeBuilder&, Blackboard & ) {

            },
            []( CommandContext&, Blackboard & ) {
                // Nothing
            } );

        graph.RegisterPass(
            "OutlineSeedInit",
            FGPassType::eGraphics,
            []( FGNodeBuilder&, Blackboard & ) {

            },
            []( CommandContext&, Blackboard & ) {
                // Nothing
            } );

        graph.RegisterPass(
            "JFA_Resolution",
            FGPassType::eGraphics,
            []( FGNodeBuilder&, Blackboard & ) {

            },
            []( CommandContext&, Blackboard & ) {
                // Nothing
            } );

        graph.RegisterPass(
            "CompositeOutline",
            FGPassType::eGraphics,
            []( FGNodeBuilder&, Blackboard & ) {

            },
            []( CommandContext&, Blackboard & ) {
                // Nothing
            } );
    }

    auto PostEffectsPass::RegisterSsao( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        auto& info{ graph.GetOrCreate<PostProcessModuleInfo>() };

        auto ssaoSamplerDesc{ FGSamplerDescription{}
            .SetName( "Ssao_Sampler01" )
            .SetFilter( rhi::SamplerFilter::eNearest )
            .SetWrap( SamplerWrapMode::eRepeat )
            .SetBorderColor( kColorBlack ) };
        info.mSsaoSampler = graph.Create( ssaoSamplerDesc );

        const auto dimensions{ InferDimensions( mResolution ) };
        const auto colorImage{ FGTextureDescription{}
            .SetName( "Ssao_ColorImage01" )
            .SetWidth( as<i32>( dimensions.first ) )
            .SetHeight( as<i32>(  dimensions.second ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetUsage( TextureUsageFlagsBits::kRenderTarget | TextureUsageFlagsBits::kShaderResource )
            .SetFormat( Format::eR8_UNORM ) };
        info.mSsaoColorTarget = graph.Create( colorImage );

        const auto blurColorImage{ FGTextureDescription{}
            .SetName( "SsaoBlur_ColorImage01" )
            .SetWidth( as<i32>( dimensions.first ) )
            .SetHeight( as<i32>(  dimensions.second ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetUsage( TextureUsageFlagsBits::kRenderTarget | TextureUsageFlagsBits::kShaderResource )
            .SetFormat( Format::eR8_UNORM ) };
        info.mSsaoBlurColorTarget = graph.Create( blurColorImage );

        const auto noiseTexture{ FGTextureDescription{}
            .SetName( "SSAO_NoiseImage01" )
            .SetWidth( as<i32>( kSsaoNoiseDimensions ) )
            .SetHeight( as<i32>( kSsaoNoiseDimensions ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetUsage( TextureUsageFlagsBits::kRenderTarget | TextureUsageFlagsBits::kShaderResource | TextureUsageFlagsBits::kCopyDst )
            .SetFormat( Format::eRGBA32_FLOAT ) };
        info.mSsaoNoiseTexture = graph.Create( noiseTexture );

        const auto kernelBufferDesc{ FGBufferDescription{}
            .SetName( "SSAO_KernelBuffer" )
            .SetUsage( BufferUsageFlagsBits::kStorage | BufferUsageFlagsBits::kCopyDst )
            .SetSizeBytes( MKT_VECTOR_SIZE_BYTES( mSsaoKernelSamples ) )
            .SetHeapType( HeapType::eDeviceLocal ) };
        info.mSsaoKernelBuffer = graph.Create( kernelBufferDesc );

        // Generate noise
        for ( u32 i{}; i < kSsaoNoiseDimensions * kSsaoNoiseDimensions; i++ ) {
            // rotate around z-axis (in tangent space)
            mSsaoNoiseData.emplace_back( math::random::GetRandomReal( 0.0, 1.0 ) * 2.0f - 1.0f,
                math::random::GetRandomReal( 0.0, 1.0 ) * 2.0f - 1.0f, 0.0f, 0.0f );
        }

        // Generate samples
        for ( u32 i{}; i < kSsaoKernelSize; ++i ) {
            float4 sample{
                as<f32>( math::random::GetRandomReal( 0.0, 1.0 ) ) * 2.0 - 1.0,
                as<f32>( math::random::GetRandomReal( 0.0, 1.0 ) ) * 2.0 - 1.0,
                as<f32>( math::random::GetRandomReal( 0.0, 1.0 ) ), 1.0 };

            sample = glm::normalize( sample );
            sample *= math::random::GetRandomReal( 0.0, 1.0 );

            f32 scale{ as<f32>( i ) / 64.0f };

            // scale samples s.t. they're more aligned to center of kernel
            scale = as<f32>( math::Lerp( 0.1f, 1.0f, scale * scale ) );
            sample *= scale;

            mSsaoKernelSamples[i] = sample;
        }

        // Pass to upload Data for SSAO
        graph.RegisterPass<PostProcessModuleInfo>(
            "SSAO_DataUpload",
            FGPassType::eTransfer,
            []( FGNodeBuilder &b, PostProcessModuleInfo &data ) {
                b.UseResource( data.mSsaoNoiseTexture, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                b.UseResource( data.mSsaoKernelBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this]( CommandContext &ctx, Blackboard &blackboard ) {
                const auto &data{ blackboard.Get<PostProcessModuleInfo>() };

                ctx.CopyTexture( data.mSsaoNoiseTexture, mSsaoNoiseData.data(), MKT_VECTOR_SIZE_BYTES( mSsaoNoiseData ) );
                ctx.CopyBuffer( data.mSsaoKernelBuffer, 0, mSsaoKernelSamples.data(), MKT_VECTOR_SIZE_BYTES( mSsaoKernelSamples ) );
            } );

        graph.SetExecutionPolicy( "SSAO_DataUpload", FGExecutionPolicy::eOnWake );

        const auto ssaoRenderPipelineDesc{ FGPipelineDescription{}
            .SetName( "SsaoRender_Pipeline01" )
            .SetPipelineType( PipelineType::eGraphics )
            .SetTopology( PrimitiveTopology::eTriangleList )
            .AddColorFormat( Format::eR8_UNORM )
            .PushShader( "SSAO_Vert.slang", FGStageType::eVertex )
            .PushShader( "SSAO_Frag.slang", FGStageType::ePixel ) };
        info.mSsaoRenderPipeline = graph.Create( ssaoRenderPipelineDesc );

        graph.RegisterPass(
            "Ssao_Render",
            FGPassType::eGraphics,
            []( FGNodeBuilder &builder, Blackboard& blackboard ) {
                const auto& ssaoData{ blackboard.Get<PostProcessModuleInfo>() };
                const auto& prePassData{ blackboard.Get<PrepassModuleInfo>() };

                builder.UseResource( ssaoData.mSsaoColorTarget, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                builder.UseResource( ssaoData.mSsaoNoiseTexture, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( ssaoData.mSsaoKernelBuffer, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );

                builder.UseResource( prePassData.mGBufferPositionTarget, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( prePassData.mGBufferNormalTarget, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
            },
            [this]( CommandContext &ctx, Blackboard &blackboard ) {
                const auto &ssaoData{ blackboard.Get<PostProcessModuleInfo>() };
                const auto &prePassData{ blackboard.Get<PrepassModuleInfo>() };

                const auto renderDimensions{ InferDimensions( mResolution ) };
                const auto graphicsState{ ContextRenderState{}
                      .SetRenderArea( Rect{ as<i32>( renderDimensions.first ), as<i32>( renderDimensions.second ) } )
                      .AddRenderTarget( ssaoData.mSsaoColorTarget, kColorGreen, LoadOp::eClear ) };
                ctx.BeginRender( graphicsState );

                struct DrawParams {
                    u32 mSamplerID{};

                    u32 mKernelBufferID{};
                    u32 mNoiseTextureID{};
                    u32 mGBufferNormalsID{};
                    u32 mGBufferPositionsID{};

                    float4x4 mProjection{};
                } params{
                    .mSamplerID = ctx.PushSampler(ssaoData.mSsaoSampler),

                    .mKernelBufferID = ctx.PushBuffer_SRV(ssaoData.mSsaoKernelBuffer),
                    .mNoiseTextureID = ctx.PushTexture_SRV(ssaoData.mSsaoNoiseTexture),
                    .mGBufferNormalsID = ctx.PushTexture_SRV(prePassData.mGBufferNormalTarget),
                    .mGBufferPositionsID = ctx.PushTexture_SRV(prePassData.mGBufferPositionTarget),

                    .mProjection = mCamera->GetProjection() };
                ctx.PushConstants( params );

                ctx.SetViewportState( ViewportState{}
                    .AddViewportAndScissorRect( Viewport( as<f32>( renderDimensions.first ), as<f32>( renderDimensions.second ) ) ) );

                ctx.BindPipeline( ssaoData.mSsaoRenderPipeline );
                ctx.Draw( 3 );

                ctx.EndRender();
            } );

        const auto ssaoBlurPipelineDesc{ FGPipelineDescription{}
            .SetName( "SsaoBlur_Pipeline01" )
            .SetPipelineType( PipelineType::eGraphics )
            .SetTopology( PrimitiveTopology::eTriangleList )
            .AddColorFormat( Format::eR8_UNORM )
            .PushShader( "SSAOBlur_Vert.slang", FGStageType::eVertex )
            .PushShader( "SSAOBlur_Frag.slang", FGStageType::ePixel ) };
        info.mSsaoBlurPipeline = graph.Create( ssaoBlurPipelineDesc );

        graph.RegisterPass(
            "Ssao_Blur",
            FGPassType::eGraphics,
            []( FGNodeBuilder &b, Blackboard& blackboard ) {
                const auto& ssaoData{ blackboard.Get<PostProcessModuleInfo>() };

                b.UseResource( ssaoData.mSsaoColorTarget, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                b.UseResource( ssaoData.mSsaoBlurColorTarget, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [this]( CommandContext &ctx, Blackboard &blackboard ) {
                const auto &ssaoData{ blackboard.Get<PostProcessModuleInfo>() };

                const auto renderDimensions{ InferDimensions( mResolution ) };
                const auto graphicsState{ ContextRenderState{}
                      .SetRenderArea( Rect{ as<i32>( renderDimensions.first ), as<i32>( renderDimensions.second ) } )
                      .AddRenderTarget( ssaoData.mSsaoBlurColorTarget, kColorGreen, LoadOp::eClear ) };
                ctx.BeginRender( graphicsState );

                struct DrawParams {
                    u32 mSamplerID{};
                    u32 mSsaoColorTargetID{};
                } params{
                    .mSamplerID = ctx.PushSampler(ssaoData.mSsaoSampler),
                    .mSsaoColorTargetID = ctx.PushTexture_SRV( ssaoData.mSsaoColorTarget ) };
                ctx.PushConstants( params );

                ctx.SetViewportState( ViewportState{}
                    .AddViewportAndScissorRect( Viewport( as<f32>( renderDimensions.first ), as<f32>( renderDimensions.second ) ) ) );

                ctx.BindPipeline( ssaoData.mSsaoBlurPipeline );
                ctx.Draw( 3 );

                ctx.EndRender();
            } );
    }

    auto PostEffectsPass::RegisterBloom( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        auto& info{ graph.GetOrCreate<PostProcessModuleInfo>() };

        const auto bloomUpSamplePipelineDesc{ FGPipelineDescription{}
            .SetName( "BloomUpSample_Pipeline01" )
            .SetPipelineType( PipelineType::eGraphics )
            .SetTopology( PrimitiveTopology::eTriangleList )
            .AddColorFormat( Format::eRGBA16_FLOAT )
            .PushShader( "BloomUp_Vert.slang", FGStageType::eVertex )
            .PushShader( "BloomUp_Frag.slang", FGStageType::ePixel ) };
        info.mBloomUpSamplePipeline = graph.Create( bloomUpSamplePipelineDesc );

        const auto bloomDownSamplePipelineDesc{ FGPipelineDescription{}
            .SetName( "BloomDownSample_Pipeline01" )
            .SetPipelineType( PipelineType::eGraphics )
            .SetTopology( PrimitiveTopology::eTriangleList )
            .AddColorFormat( Format::eRGBA16_FLOAT )
            .PushShader( "BloomDown_Vert.slang", FGStageType::eVertex )
            .PushShader( "BloomDown_Frag.slang", FGStageType::ePixel ) };
        info.mBloomDownSamplePipeline = graph.Create( bloomDownSamplePipelineDesc );

        // one image per mip level
        auto dimensions{ InferDimensions( mResolution ) };
        for (i32 mipLevel{}; mipLevel < kMaxBloomChainImages; ++mipLevel) {
            f32 width{ as<f32>(dimensions.first * std::pow(0.5f, mipLevel)) };
            f32 height{ as<f32>(dimensions.second * std::pow(0.5f, mipLevel)) };

            auto colorImage{ FGTextureDescription{}
                .SetName( string::Format( "BloomChainImage Mip {}", mipLevel ) )
                .SetWidth( width )
                .SetHeight( height )
                .SetDimensions( TextureDimension::eTexture2D )
                .SetUsage( TextureUsageFlagsBits::kRenderTarget | TextureUsageFlagsBits::kShaderResource )
                .SetFormat( Format::eRGBA16_FLOAT ) };
            info.mBloomChainImages.emplace_back( PostProcessModuleInfo::ImageDescription {
                .mWidth = width,
                .mHeight = height,
                .mImage = graph.Create( colorImage ) });
        }

        // Downsampling chain
        for (i32 mipLevel{}; mipLevel < kMaxBloomChainImages; ++mipLevel) {
            graph.RegisterPass( mipLevel == 0 ?
            string::Format( "BloomDownSample Emissive to Mip 0" ) :
            string::Format( "BloomDownSample Mip{} to Mip {}",  mipLevel - 1, mipLevel ),
            FGPassType::eGraphics,
            [&]( FGNodeBuilder& builder, Blackboard& blackboard ) -> void {
                auto& postprocess{ blackboard.Get<PostProcessModuleInfo>() };

                // first mip is the only one that reads from GBuffer emissive images
                // because bloom is applied on emissive materials
                if (mipLevel == 0) {
                     auto& prepass{ blackboard.Get<PrepassModuleInfo>() };
                     builder.UseResource( prepass.mGBufferEmissiveTarget, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                     builder.UseResource( postprocess.mBloomChainImages[mipLevel].mImage, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                } else {
                     // I read from previous mip and write to this mip
                     builder.UseResource( postprocess.mBloomChainImages[mipLevel - 1].mImage, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                     builder.UseResource( postprocess.mBloomChainImages[mipLevel].mImage, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                }
            },
            [this, mipLevel]( CommandContext& ctx, Blackboard& b ) -> void {
                const auto& prePassData{ b.Get<PrepassModuleInfo>() };
                const auto& finalCompData{ b.Get<GeomShadingModuleInfo>() };
                const auto& postProcessData{ b.Get<PostProcessModuleInfo>() };

                // Emissive texture uses default render resolution specified by renderer
                auto dimensions{ InferDimensions( mResolution ) };

                FGTextureHandle readImage{};
                if (mipLevel == 0) {
                    readImage = prePassData.mGBufferEmissiveTarget;
                } else {
                    // I read from previous mip and write to this mip
                    dimensions.first = postProcessData.mBloomChainImages[mipLevel].mWidth;
                    dimensions.second = postProcessData.mBloomChainImages[mipLevel].mHeight;
                    readImage = postProcessData.mBloomChainImages[mipLevel - 1].mImage;
                }

                const auto graphicsState{ ContextRenderState{}
                    .SetRenderArea( Rect{ as<i32>( dimensions.first ), as<i32>( dimensions.second ) } )
                    .AddRenderTarget( postProcessData.mBloomChainImages[mipLevel].mImage, kColorGreen, LoadOp::eClear ) };
                ctx.BeginRender( graphicsState );

                struct DrawParams {
                    u32 mSampler{};
                    u32 mTargetImage{};
                } params{
                    .mSampler = ctx.PushSampler( finalCompData.mSkyboxCubeSampler ),
                    .mTargetImage = ctx.PushTexture_SRV( readImage ) };
                ctx.PushConstants( params );

                ctx.SetViewportState( ViewportState{}
                    .AddViewportAndScissorRect( Viewport( as<f32>( dimensions.first ), as<f32>( dimensions.second ) ) ) );

                ctx.BindPipeline( postProcessData.mBloomDownSamplePipeline );
                ctx.Draw( 3 );

                ctx.EndRender();
            } );
        }

        // Upsampling chain
        for (i32 mipLevel{ 1 }; mipLevel < kMaxBloomChainImages; ++mipLevel) {
            graph.RegisterPass(
                string::Format( "BloomUpSample Mip{} to Mip {}", kMaxBloomChainImages - mipLevel, kMaxBloomChainImages - mipLevel - 1 ),
                FGPassType::eGraphics,
                [&]( FGNodeBuilder& builder, Blackboard& blackboard ) -> void {
                    auto& postprocess{ blackboard.Get<PostProcessModuleInfo>() };

                    const i32 srcMip{ as<i32>( kMaxBloomChainImages - mipLevel ) };
                    const i32 dstMip{ srcMip - 1 };

                    builder.UseResource( postprocess.mBloomChainImages[srcMip].mImage, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                    builder.UseResource( postprocess.mBloomChainImages[dstMip].mImage, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                },
                [mipLevel]( CommandContext& ctx, Blackboard& b ) -> void {
                    const auto& finalCompData{ b.Get<GeomShadingModuleInfo>() };
                    const auto& postProcessData{ b.Get<PostProcessModuleInfo>() };

                    const i32 srcMip{ as<i32>( kMaxBloomChainImages - mipLevel ) };
                    const i32 dstMip{ srcMip - 1 };

                    const auto dimensions{ eastl::pair<f32, f32>{ postProcessData.mBloomChainImages[dstMip].mWidth,
                        postProcessData.mBloomChainImages[dstMip].mHeight} };

                    const auto graphicsState{ ContextRenderState{}
                        .SetRenderArea( Rect{ as<i32>( dimensions.first ), as<i32>( dimensions.second ) } )
                        .AddRenderTarget( postProcessData.mBloomChainImages[dstMip].mImage, kColorGreen, LoadOp::eClear ) };
                    ctx.BeginRender( graphicsState );

                    struct DrawParams {
                        u32 mSampler{};
                        u32 mTargetImage{};
                    } params{
                        .mSampler = ctx.PushSampler( finalCompData.mSkyboxCubeSampler ),
                        .mTargetImage = ctx.PushTexture_SRV( postProcessData.mBloomChainImages[srcMip].mImage ) };
                    ctx.PushConstants( params );

                    ctx.SetViewportState( ViewportState{}
                        .AddViewportAndScissorRect( Viewport( as<f32>( dimensions.first ), as<f32>( dimensions.second ) ) ) );

                    ctx.BindPipeline( postProcessData.mBloomUpSamplePipeline );
                    ctx.Draw( 3 );

                    ctx.EndRender();
                } );
        }
    }

    auto PostEffectsPass::RegisterPostProcess( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Sort all post process materials and upload them
        // in PostProcessMaterials_Graphics we apply all postprocess materials
        // to the final image
        graph.RegisterPass(
            "PostProcessMaterials_Filter",
            FGPassType::eTransfer,
            []( FGNodeBuilder&, Blackboard& ) -> void {
            },
            [this]( CommandContext& ctx, Blackboard& b ) -> void {
                SetupPostProcessMaterials(ctx, b);
            } );

        // For every post process materials do a full-quad render
        // applying its effects on the final composition image
        graph.RegisterPass(
            "PostProcessMaterials_Graphics",
            FGPassType::eGraphics,
            []( FGNodeBuilder& builder, Blackboard& blackboard) -> void {
                const auto& finalCompData{ blackboard.Get<GeomShadingModuleInfo>() };
                builder.UseResource( finalCompData.mColorImage, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            []( CommandContext&, Blackboard& ) -> void {

            } );
    }

    auto PostEffectsPass::SetupPostProcessMaterials( CommandContext& ctx, Blackboard& b  ) -> void {
        auto &registry{ mScene->GetRegistry() };
        const auto renderables{ registry.view<TagComponent, TransformComponent, PostProcessMaterialComponent>() };

        for ( const auto& [entity,
            tagComponent,
            transformComponent,
            postProcessMaterial ]: renderables.each() ) {

        }
    }

    auto PostEffectsPass::RegisterEyeAdaptationPass( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // I might move this to the PostEffects passes so it happens before tonemap

        graph.RegisterPass(
            "EyeAdaptation_Luminance",
            FGPassType::eCompute,
            []( FGNodeBuilder&, Blackboard& ) {

            },
            []( CommandContext&, Blackboard& ) {
                // Nothing
            } );

        graph.RegisterPass(
            "EyeAdaptation",
            FGPassType::eCompute,
            []( FGNodeBuilder&, Blackboard& ) {

            },
            []( CommandContext&, Blackboard& ) {
                // Nothing
            } );
    }
}