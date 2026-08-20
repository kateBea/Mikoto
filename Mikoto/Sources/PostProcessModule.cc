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

    using namespace mikoto::scene;

    PostEffectsPass::PostEffectsPass( RenderResolution resolution )
        : mResolution{ resolution } {
    }

    auto PostEffectsPass::SetScene( const Scene& scene ) -> void {
        MKT_BEGIN_PROFILER_NAMED();
        mScene = MKT_ADDRESSOF( scene );
    }

    auto PostEffectsPass::SetCamera( const Camera& camera ) -> void {
        mCamera = MKT_ADDRESSOF( camera );;
    }

    auto PostEffectsPass::RegisterPasses( FrameGraph& graph) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        RegisterBloom( graph );
        RegisterSsao( graph );

        RegisterObjectOutline( graph );

        RegisterDepthOfField( graph );

        RegisterPostProcess( graph );

        //RegisterInfiniteGrid( graph );
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

        PostProcessModuleInfo& info{ graph.GetOrCreate<PostProcessModuleInfo>() };

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
                CameraModuleInfo& cam{ blackboard.Get<CameraModuleInfo>() };
                GeomShadingModuleInfo& shading{ blackboard.Get<GeomShadingModuleInfo>() };
                PrepassModuleInfo& prepass{ blackboard.Get<PrepassModuleInfo>() };

                builder.Read( cam.mCameraData, FGPipelineStage::ePixelShader );
                builder.Read( prepass.mDepthPrepassDepthTarget, FGPipelineStage::eDepthTarget );
                builder.Write( shading.mColorImage, FGPipelineStage::eRenderTarget );
            },
            [this]( CommandContext& ctx, Blackboard& b ) {
                CameraModuleInfo& cam{ b.Get<CameraModuleInfo>() };
                PrepassModuleInfo& prepass{ b.Get<PrepassModuleInfo>() };
                GeomShadingModuleInfo& shading{ b.Get<GeomShadingModuleInfo>() };
                PostProcessModuleInfo& info{ b.Get<PostProcessModuleInfo>() };

                const auto dimensions{ InferDimensions( mResolution ) };

                const auto graphicsState{ ContextRenderState{}
                    .SetRenderArea( Rect{ as<i32>( dimensions.first ), as<i32>( dimensions.second ) } )
                    .AddDepthTarget( prepass.mDepthPrepassDepthTarget, LoadOp::eLoad )
                    .AddRenderTarget( shading.mColorImage, kColorGreen, LoadOp::eLoad ) };
                ctx.BeginRender( graphicsState );

                struct DrawParams {
                    u32 mCameraBufferID{};

                    float4 OuterSquareColor{ 0.8f, 0.8f, 0.8f, 0.8f };
                    float4 InnerSquareColor{ 0.8f, 0.8f, 0.8f, 0.8f };

                    float4 XAxisColor{ 0.0, 0.0, 1.0, 1.0 };
                    float4 ZAxisColor{ 1.0, 0.0, 0.0, 1.0 };

                    f32 OuterSquareWidth{ 0.5f };
                    f32 InnerSquareWidth{ 1.0f };

                    f32 XAxisWidth{ 6.0f };
                    f32 ZAxisWidth{ 6.0f };
                } params{
                    .mCameraBufferID = ctx.PushBuffer_SRV( cam.mCameraData ),
                };

                ctx.PushConstants( params );

                ctx.SetViewportState( ViewportState{}
                    .AddViewportAndScissorRect( Viewport( as<f32>( dimensions.first ), as<f32>( dimensions.second ) ) ) );

                ctx.BindPipeline( info.mInfiniteGridPipeline );
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

#if false
        auto colorImage{ FGTextureDescription{}
            .SetName( "ModelPass_ColorImage01" )
            .SetWidth( as<i32>( 1920 ) )
            .SetHeight( as<i32>( 1080 ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetUsage( TextureUsageFlagsBits::kRenderTarget | TextureUsageFlagsBits::kShaderResource )
            .SetFormat( Format::eRGBA8_UNORM ) };

        renderModelPass.mColorTarget = graph.Create( colorImage );

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

        // Generate noise
        constexpr u32 ssaoNoiseDimensions{ 8 };
        eastl::fixed_vector<float4, ssaoNoiseDimensions * ssaoNoiseDimensions> ssaoNoise{};
        for ( u32 i{}; i < ssaoNoiseDimensions * ssaoNoiseDimensions; i++ ) {
            // rotate around z-axis (in tangent space)
            ssaoNoise.emplace_back( GetRandomReal( 0.0, 1.0 ) * 2.0f - 1.0f,
                GetRandomReal( 0.0, 1.0 ) * 2.0f - 1.0f, 0.0f, 0.0f );
        }

        // Generate samples
        eastl::array<float4, kSsaoKernelSize> samples{};
        for ( u32 i{}; i < kSsaoKernelSize; ++i ) {
            float4 sample{
                as<f32>( GetRandomReal( 0.0, 1.0 ) ) * 2.0 - 1.0,
                as<f32>( GetRandomReal( 0.0, 1.0 ) ) * 2.0 - 1.0,
                as<f32>( GetRandomReal( 0.0, 1.0 ) ), 1.0 };

            sample = glm::normalize( sample );
            sample *= GetRandomReal( 0.0, 1.0 );

            f32 scale{ as<f32>( i ) / 64.0f };

            // scale samples s.t. they're more aligned to center of kernel
            scale = as<f32>( math::Lerp( 0.1f, 1.0f, scale * scale ) );
            sample *= scale;

            samples[i] = sample;
        }

        graph.RegisterPass(
            "SSAO",
            FGPassType::eGraphics,
            [this]( FGNodeBuilder& builder, Blackboard& blackboard  ) {
                b.CreateBuffer( "SSAO_Parameters", BufferUsage::eUniform, sizeof( m_SSAOParameters ), 1, ResourceUsageType::eStreaming );

                b.CreateTexture( "SSAO_ColorTarget", mResolution, TextureFormat::R8_UNORM, TextureUsage::COLOR );
                b.CreateTexture( "SSAO_NoiseTexture", ssaoNoiseDimensions, ssaoNoiseDimensions, // SSAO_NOISE_DIM is constexpr, capturing it is not required
                                 TextureFormat::RGBA32_FLOAT, ssaoNoise.data(), MKT_SIZEOF( Vec4F ) * ssaoNoise.size() );

                GraphicsPipelineDescription graphicsDesc{
                    .DepthTest{ true },
                    .DepthWrite{ true },
                    .ColorAttachmentFormats{ TextureFormat::R8_UNORM }
                };

                b.UseShader( "Resources/Shaders/slang/SSAO_Vert.slang", ShaderStage::eVertex );
                b.UseShader( "Resources/Shaders/slang/SSAO_Frag.slang", ShaderStage::eFragment );
                b.CreatePipeline( "SSAO_Pipeline", graphicsDesc );

                b.Write( "SSAO_ColorTarget", FrameResourceState::RenderTarget );
                b.Write( "SSAO_NoiseTexture", FrameResourceState::ShaderRead_GraphicsPipeline );

                b.Read( "SSAO_Parameters", FrameResourceState::UniformBuffer );
                b.Read( "GBuffer_Position", FrameResourceState::ShaderRead_GraphicsPipeline );
                b.Read( "GBuffer_Normal", FrameResourceState::ShaderRead_GraphicsPipeline );
            },
            [this]( CommandContext& ctx, Blackboard& blackboard ) -> void {
                auto& constants{ blackboard.Get<FinalShadingConstants>() };
                if (!constants.EnableSSAO) {
                    return;
                }

                if ( m_Sampler.IsEmpty() || m_SamplerNoise.IsEmpty() ) {
                    m_Sampler = ctx.CreateSampler( SamplerDescription{} );

                    m_SamplerNoise = ctx.CreateSampler( SamplerDescription{
                            .MipLevels{ 0 },
                            .MinFilter{ SamplerFilter::eNearest },
                            .MagFilter{ SamplerFilter::eNearest },

                            .WrapU{ SamplerWrapMode::eRepeat },
                            .WrapV{ SamplerWrapMode::eRepeat },
                            .WrapW{ SamplerWrapMode::eRepeat },
                    } );
                }

                ctx.BindBuffer( ResourceGroup::BufferViews, "SSAO_Parameters", ResourceSlot::Slot_0 );
                ctx.BindImageSampler( ResourceGroup::StaticSamplers, "GBuffer_Position", m_Sampler, ResourceSlot::Slot_0 );
                ctx.BindImageSampler( ResourceGroup::StaticSamplers, "GBuffer_Normal", m_Sampler, ResourceSlot::Slot_1 );
                ctx.BindImageSampler( ResourceGroup::StaticSamplers, "SSAO_NoiseTexture", m_SamplerNoise, ResourceSlot::Slot_2 );

                m_SSAOParameters.Projection = mCamera->GetProjection();

                ctx.UploadBuffer( "SSAO_Parameters", m_SSAOParameters );

                ctx.SetColorRenderTarget( "SSAO_ColorTarget" );

                ctx.BeginRender();

                const auto dimensions{ InferDimensions( mResolution ) };
                ctx.SetViewport( 0, 0, dimensions.first, dimensions.second, false );
                ctx.SetScissor( 0, 0, dimensions.first, dimensions.second );

                ctx.BindPipeline( "SSAO_Pipeline" );
                ctx.Draw( 3 );

                ctx.EndRender();
            } );

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
                     builder.Read( prepass.mGBufferEmissiveTarget, FGPipelineStage::ePixelShader );
                     builder.Write( postprocess.mBloomChainImages[mipLevel], FGPipelineStage::eRenderTarget );
                 } else {
                     // I read from previous mip and write to this mip
                     builder.Read( postprocess.mBloomChainImages[mipLevel - 1], FGPipelineStage::ePixelShader );
                     builder.Write( postprocess.mBloomChainImages[mipLevel], FGPipelineStage::eRenderTarget );
                 }
             },
             []( CommandContext&, Blackboard& ) -> void {
                 // fullscreen additive blur
             } );
        }

        for (i32 mipLevel{ 1 }; mipLevel < kMaxBloomChainImages; ++mipLevel) {
            graph.RegisterPass(
                string::Format( "BloomUpSample Mip{} to Mip {}", kMaxBloomChainImages - mipLevel, kMaxBloomChainImages - mipLevel - 1 ),
                FGPassType::eGraphics,
                [&]( FGNodeBuilder& builder, Blackboard& blackboard ) -> void {
                    auto& postprocess{ blackboard.Get<PostProcessModuleInfo>() };

                    const i32 srcMip{ as<i32>( kMaxBloomChainImages - mipLevel ) };
                    const i32 dstMip{ srcMip - 1 };

                    builder.Read(
                        postprocess.mBloomChainImages[srcMip],
                        FGPipelineStage::ePixelShader );

                    builder.Write(
                        postprocess.mBloomChainImages[dstMip],
                        FGPipelineStage::eRenderTarget );
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
        // applying its effects on the given color image
        graph.RegisterPass(
            "PostProcessMaterials_Graphics",
            FGPassType::eGraphics,
            []( FGNodeBuilder&, Blackboard& ) -> void {
            },
            []( CommandContext&, Blackboard& ) -> void {
            } );
    }

    auto PostEffectsPass::SetupPostProcessMaterials( CommandContext& ctx, Blackboard& b  ) -> void {

    }
}