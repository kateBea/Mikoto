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

        RegisterObjectOutline( graph );

        RegisterDepthOfField( graph );

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

    auto PostEffectsPass::RegisterDepthOfField( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Uses the scene main camera

        // https://photographylife.com/what-is-depth-of-field
        // https://dev.epicgames.com/documentation/en-us/unreal-engine/depth-of-field-in-unreal-engine
        // https://developer.nvidia.com/gpugems/gpugems3/part-iv-image-effects/chapter-28-practical-post-process-depth-field
        graph.RegisterPass(
            "DepthOfField_InitializeCoC",
            FGPassType::eGraphics,
            []( FGNodeBuilder&, Blackboard & ) {

            },
            []( CommandContext&, Blackboard & ) {
                // Nothing
            } );

        graph.RegisterPass(
            "DepthOfField_ComputeNearCoC",
            FGPassType::eGraphics,
            []( FGNodeBuilder&, Blackboard & ) {

            },
            []( CommandContext&, Blackboard & ) {
                // Nothing
            } );

        graph.RegisterPass(
            "DepthOfField_BlurNearCoC",
            FGPassType::eGraphics,
            []( FGNodeBuilder&, Blackboard & ) {

            },
            []( CommandContext&, Blackboard & ) {
                // Nothing
            } );

        graph.RegisterPass(
            "DepthOfField_DebugPass",
            FGPassType::eGraphics,
            []( FGNodeBuilder&, Blackboard & ) {

            },
            []( CommandContext&, Blackboard & ) {
                // Green close blur
                // Black semi transparent not blurred
                // Blue far blur
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
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetUsage( TextureUsageFlagsBits::kRenderTarget | TextureUsageFlagsBits::kShaderResource )
            .SetFormat( Format::eR8_UNORM ) };
        info.mSsaoColorTarget = graph.Create( colorImage );

        const auto blurColorImage{ FGTextureDescription{}
            .SetName( "SsaoBlur_ColorImage01" )
            .SetWidth( as<i32>( dimensions.first ) )
            .SetHeight( as<i32>(  dimensions.second ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetMultisampling( Multisampling::eMsaaX1 )
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

        graph.SetExecutionPolicy( "SSAO_DataUpload", FGExecutionPolicy::eOnChange );

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
                      .AddRenderTarget( ssaoData.mSsaoColorTarget, kColorGreen, LoadOp::eLoad ) };
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
                      .AddRenderTarget( ssaoData.mSsaoBlurColorTarget, kColorGreen, LoadOp::eLoad ) };
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

#if false

        graph.RegisterPass(
            "SSAOBlur",
            FGPassType::eGraphics,
            [this]( FGNodeBuilder& builder, Blackboard& blackboard ) {
                b.CreateTexture( "SSAOBlur_ColorTarget", mResolution, TextureFormat::R8_UNORM, TextureUsage::COLOR );

                GraphicsPipelineDescription graphicsDesc{
                    .DepthTest{ false },
                    .DepthWrite{ false },
                    .ColorAttachmentFormats{ TextureFormat::R8_UNORM },
                };

                b.UseShader( "Resources/Shaders/slang/SSAOBlur_Vert.slang", ShaderStage::eVertex );
                b.UseShader( "Resources/Shaders/slang/SSAOBlur_Frag.slang", ShaderStage::eFragment );
                b.CreatePipeline( "SSAOBlur_Pipeline", graphicsDesc );

                b.Write( "SSAOBlur_ColorTarget", FrameResourceState::RenderTarget );
                b.Read( "SSAO_ColorTarget", FrameResourceState::ShaderRead_GraphicsPipeline );
            },
            [this]( CommandContext& ctx, Blackboard& blackboard ) -> void {
                auto& constants{ blackboard.Get<FinalShadingConstants>() };
                if ( !constants.EnableSSAO ) {
                    return;
                }

                if ( m_Sampler.IsEmpty() ) {
                    m_Sampler = ctx.CreateSampler( SamplerDescription{} );
                }

                ctx.BindImageSampler( ResourceGroup::StaticSamplers, "SSAO_ColorTarget", m_Sampler, ResourceSlot::Slot_0 );

                ctx.SetColorRenderTarget( "SSAOBlur_ColorTarget" );

                ctx.BeginRender();

                const auto dimensions{ InferDimensions( mResolution ) };
                ctx.SetViewport( 0, 0, dimensions.first, dimensions.second, false );
                ctx.SetScissor( 0, 0, dimensions.first, dimensions.second );

                ctx.BindPipeline( "SSAOBlur_Pipeline" );
                ctx.Draw( 3 );

                ctx.EndRender();
            } );
#endif

    }

    auto PostEffectsPass::RegisterBloom( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        auto& info{ graph.GetOrCreate<PostProcessModuleInfo>() };

        // one image per mip level
        auto dimensions{ InferDimensions( mResolution ) };
        for (i32 mipLevel{}; mipLevel < kMaxBloomChainImages; ++mipLevel) {
            f32 width{ as<f32>(dimensions.first * std::pow(0.5f, mipLevel)) };
            f32 heigh{ as<f32>(dimensions.second * std::pow(0.5f, mipLevel)) };

            auto colorImage{ FGTextureDescription{}
                .SetName( string::Format( "BloomChainImage Mip {}", mipLevel ) )
                .SetWidth( width )
                .SetHeight( heigh )
                .SetDimensions( TextureDimension::eTexture2D )
                .SetMultisampling( Multisampling::eMsaaX1 )
                .SetUsage( TextureUsageFlagsBits::kRenderTarget | TextureUsageFlagsBits::kShaderResource )
                .SetFormat( Format::eRGBA16_FLOAT ) };
            info.mBloomChainImages.emplace_back( graph.Create( colorImage ) );
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
                     builder.UseResource( postprocess.mBloomChainImages[mipLevel], FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                 } else {
                     // I read from previous mip and write to this mip
                     builder.UseResource( postprocess.mBloomChainImages[mipLevel - 1], FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                     builder.UseResource( postprocess.mBloomChainImages[mipLevel], FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                 }
             },
             []( CommandContext&, Blackboard& ) -> void {
                 // fullscreen additive blur
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

                    builder.UseResource( postprocess.mBloomChainImages[srcMip], FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                    builder.UseResource( postprocess.mBloomChainImages[dstMip], FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                },

                []( CommandContext&, Blackboard& ) -> void {
                    // fullscreen additive blur
                } );
        }

#if false
        auto depthImage{ FGTextureDescription{}
            .SetName( "ModelPass_DepthImage01" )
            .SetWidth( as<i32>( 1920 ) )
            .SetHeight( as<i32>( 1080 ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetUsage( TextureUsageFlagsBits::kDepthTarget )
            .SetFormat( Format::eD32 ) };

        renderModelPass.mDepthTarget = graph.Create( depthImage );

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "ModelPass_Pipeline01" )
            .SetPipelineType( PipelineType::eGraphics )
            .SetTopology( PrimitiveTopology::eTriangleList )
            .SetDepthFormat( Format::eD32 )
            .AddColorFormat( Format::eRGBA8_UNORM )
            .PushShader( "HelloModel_Vert.slang", FGStageType::eVertex )
            .PushShader( "HelloModel_Frag.slang", FGStageType::ePixel ) };

        renderModelPass.mPipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass(
            "BloomDownSampling",
            FGPassType::eGraphics,
            [this]( FGNodeBuilder& builder, Blackboard& blackboard ) {
                b.CreateTexture( "BloomDownSampling_ColorTarget", mResolution, TextureFormat::RGBA16_FLOAT, Multisampling::MSAA_X1, TextureUsage::COLOR, data.MipCount );

                b.CreateBuffer( "Bloom_Parameters", BufferUsage::eUniform, sizeof( BloomParameters ), 1, ResourceUsageType::eStreaming );

                GraphicsPipelineDescription graphicsDesc{
                    .DepthTest{ true },
                    .DepthWrite{ true },
                    .ColorAttachmentFormats{ TextureFormat::RGBA16_FLOAT }
                };

                b.UseShader( "Resources/Shaders/slang/BloomDown_Vert.slang", ShaderStage::eVertex );
                b.UseShader( "Resources/Shaders/slang/BloomDown_Frag.slang", ShaderStage::eFragment );
                b.CreatePipeline( "BloomDownSampling_Pipeline", graphicsDesc );

                b.Write( "Bloom_Parameters", FrameResourceState::UniformBuffer );
                b.Write( "BloomDownSampling_ColorTarget", FrameResourceState::RenderTarget );

                b.Read( "CameraInfoPass_CameraData", FrameResourceState::UniformBuffer );
                b.Read( "GBuffer_Emissive", FrameResourceState::ShaderRead_GraphicsPipeline );
            },
            [this]( CommandContext& ctx, Blackboard& blackboard ) -> void {
                auto& data{ blackboard.Get<BloomParameters>() };

                if (data.GBufferEmissiveSampler.IsEmpty() ) {
                    SamplerDescription description{
                        .MinFilter{ SamplerFilter::eLinear },
                        .MagFilter{ SamplerFilter::eLinear },
                        .WrapU{ SamplerWrapMode::eClampToEdge },
                        .WrapV{ SamplerWrapMode::eClampToEdge },
                        .WrapW{ SamplerWrapMode::eClampToEdge },
                    };

                    data.GBufferEmissiveSampler = ctx.CreateSampler( description );
                }

                ctx.BindBuffer( ResourceGroup::BufferViews, "CameraInfoPass_CameraData", ResourceSlot::Slot_0 );
                ctx.BindImageSampler( ResourceGroup::StaticSamplers, "GBuffer_Emissive", data.GBufferEmissiveSampler, ResourceSlot::Slot_0 );

                struct BloomDownParameters {
                    Vec2F Resolution{};
                } drawData;

                for (UInt32 mipLevel{ 0 }; mipLevel < data.MipCount; ++mipLevel ) {
                    ctx.SetColorRenderTarget( "BloomDownSampling_ColorTarget", mipLevel );

                    PassRenderInfo info{ .ColorLoadOp{ LoadOp::LOAD } };
                    ctx.BeginRender(info);

                    const auto dimensions{ InferDimensions( mResolution, mipLevel ) };
                    ctx.SetViewport( 0, 0, dimensions.first, dimensions.second, false );
                    ctx.SetScissor( 0, 0, dimensions.first, dimensions.second );

                    drawData.Resolution = Vec2F{ dimensions.first, dimensions.second };

                    ctx.PushConstants( MKT_ADDRESSOF( drawData ), MKT_SIZEOF( drawData ) );
                    ctx.BindPipeline( "BloomDownSampling_Pipeline" );
                    ctx.Draw( 3 );

                    ctx.EndRender();
                }
            } );

        graph.RegisterPass(
            "BloomUpSampling",
            FGPassType::eGraphics,
            []( FGNodeBuilder& builder, Blackboard& blackboard ) {
                MKT_BEGIN_PROFILER_NAMED();

                GraphicsPipelineDescription graphicsDesc{
                    .DepthTest{ true },
                    .DepthWrite{ true },
                    .ColorAttachmentFormats{ TextureFormat::RGBA16_FLOAT }
                };

                b.UseShader( "Resources/Shaders/slang/BloomUp_Vert.slang", ShaderStage::eVertex );
                b.UseShader( "Resources/Shaders/slang/BloomUp_Frag.slang", ShaderStage::eFragment );
                b.CreatePipeline( "BloomUpSampling_Pipeline", graphicsDesc );

                b.Read( "Bloom_Parameters", FrameResourceState::UniformBuffer );
                b.Read( "BloomDownSampling_ColorTarget", FrameResourceState::RenderTarget );

                b.Read( "GBuffer_Emissive", FrameResourceState::ShaderRead_GraphicsPipeline );
            },
            [this]( CommandContext& ctx, Blackboard& blackboard ) -> void {
                MKT_BEGIN_PROFILER_NAMED();

                auto& data{ blackboard.Get<BloomParameters>() };
                ctx.BindImageSampler( ResourceGroup::StaticSamplers, "GBuffer_Emissive", data.GBufferEmissiveSampler, ResourceSlot::Slot_0 );

                for (UInt32 mipLevel{ 0 }; mipLevel < data.MipCount; ++mipLevel ) {
                    ctx.SetColorRenderTarget( "BloomDownSampling_ColorTarget", mipLevel );

                    PassRenderInfo info{ .ColorLoadOp{LoadOp::LOAD} };
                    ctx.BeginRender(info);

                    const auto dimensions{ InferDimensions( mResolution, mipLevel ) };
                    ctx.SetViewport( 0, 0, dimensions.first, dimensions.second, false );
                    ctx.SetScissor( 0, 0, dimensions.first, dimensions.second );

                    ctx.BindPipeline( "BloomUpSampling_Pipeline" );
                    ctx.Draw( 3 );

                    ctx.EndRender();
                }
            } );

        // Final bloom composition pass
        graph.RegisterPass(
            "BloomBlend",
            FGPassType::eGraphics,
            [this]( FGNodeBuilder &builder, Blackboard& blackboard ) -> void {
                b.CreateTexture( "BloomBlend_ColorTarget", mResolution, TextureFormat::RGBA8_UNORM, Multisampling::MSAA_X1, TextureUsage::COLOR );

                GraphicsPipelineDescription graphicsDesc{
                    .DepthTest{ true },
                    .DepthWrite{ true },
                    .ColorAttachmentFormats{ TextureFormat::RGBA8_UNORM }
                };

                b.UseShader( "Resources/Shaders/slang/Bloom_Vert.slang", ShaderStage::eVertex );
                b.UseShader( "Resources/Shaders/slang/Bloom_Frag.slang", ShaderStage::eFragment );
                b.CreatePipeline( "BloomBlend_Pipeline", graphicsDesc );

                b.Read( "BloomDownSampling_ColorTarget", FrameResourceState::ShaderRead_GraphicsPipeline );
                b.Read( "FinalShadingPass_ColorTarget", FrameResourceState::ShaderRead_GraphicsPipeline );

                b.Write( "BloomBlend_ColorTarget", FrameResourceState::RenderTarget );
            },
            [this]( CommandContext &ctx, Blackboard& blackboard ) -> void {
                ctx.BindImageSampler( ResourceGroup::StaticSamplers, "BloomDownSampling_ColorTarget", ResourceSlot::Slot_0 );
                ctx.BindImageSampler( ResourceGroup::StaticSamplers, "FinalShadingPass_ColorTarget", ResourceSlot::Slot_1 );

                const auto dimensions{ InferDimensions( mResolution ) };
                ctx.SetViewport( 0, 0, dimensions.first, dimensions.second, false );
                ctx.SetScissor( 0, 0, dimensions.first, dimensions.second );

                ctx.SetColorRenderTarget( "BloomBlend_ColorTarget" );

                ctx.BeginRender();

                ctx.BindPipeline( "BloomBlend_Pipeline" );

                ctx.Draw( 3 );

                ctx.EndRender();
            } );
#endif
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
}