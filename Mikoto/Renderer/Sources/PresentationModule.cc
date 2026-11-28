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


#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Renderer/ForwardRenderer/Passes/DebugModule.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Denoising/Denoiser.hh>
#include <Renderer/ForwardRenderer/Passes/CameraModule.hh>
#include <Renderer/ForwardRenderer/Passes/PrepassModule.hh>
#include <Renderer/ForwardRenderer/Passes/RayTracingModule.hh>
#include <Renderer/ForwardRenderer/Passes/PostProcessModule.hh>
#include <Renderer/ForwardRenderer/Passes/GeometryCullModule.hh>
#include <Renderer/ForwardRenderer/Passes/PresentationModule.hh>
#include <Renderer/ForwardRenderer/Passes/DisplayEffectsModule.hh>
#include <Renderer/ForwardRenderer/Passes/GeometryShadingModule.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::renderer::rhi;

    PresentationModule::PresentationModule( renderer::rhi::RenderResolution resolution )
        : mResolution{ resolution } {
    }

    auto PresentationModule::GetFinalImage( FrameGraph& graph ) -> renderer::rhi::TextureHandle {
        MKT_BEGIN_PROFILER();
        return graph.GetTexture( mFinalImageHandle );
    }

    auto PresentationModule::SetFinalImageTarget( FinalImageType type ) -> void {
        MKT_BEGIN_PROFILER();
        mPresentTarget = type;
    }

    auto PresentationModule::RegisterTransition( FrameGraph& graph, bool rayTracingConsumers ) -> void {
        MKT_BEGIN_PROFILER();

        // The final image can feed an ImGui viewport or another view's surface material.
        graph.RegisterPass(
            "ResourceTransition",
            FGPassType::eGraphics,
            [rayTracingConsumers]( FGNodeBuilder& builder, core::Blackboard& blackboard ) {
                const auto& presentData{ blackboard.Get<PresentationPassData>() };

                builder.UseResource( presentData.mColorImage, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( presentData.mColorImage, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );

                if ( rayTracingConsumers ) {
                    builder.UseResource( presentData.mColorImage, FGPipelineStage::eRayTracingShader, FGResourceAccess::eRead );
                }
            },
            []( CommandContext&, core::Blackboard& ) {
                // Nothing
            } );
    }

    auto PresentationModule::GetTargetImage( const core::Blackboard& b ) -> FGTextureHandle {
        MKT_BEGIN_PROFILER();

        switch ( mPresentTarget ) {
            case FinalImageType::ePostProcessBeforeToneMapping:
                return b.Get<PostProcessModuleInfo>().mBeforeToneMapping.mColorImage;
            case FinalImageType::ePostProcessAfterToneMapping:
                return b.Get<PostProcessModuleInfo>().mAfterToneMapping.mColorImage;
            case FinalImageType::eSsao:
                return b.Get<PostProcessModuleInfo>().mSsaoBlurColorTarget;
            case FinalImageType::eRayTracedAmbientOcclusion:
                return b.Get<RayTracingInfo>().mAmbientOcclusion;
            case FinalImageType::eDlssNoisyColor:
            case FinalImageType::eDlssDiffuseAlbedo:
            case FinalImageType::eDlssSpecularAlbedo:
            case FinalImageType::eDlssNormalRoughness:
            case FinalImageType::eDlssDepth:
            case FinalImageType::eDlssMotion:
            case FinalImageType::eDlssRoughness:
                if ( b.Contains<RayTracingInfo>() ) {
                    const auto &rays{ b.Get<RayTracingInfo>() };
                    if ( rays.mReconstructionReady ) {
                        if ( mPresentTarget == FinalImageType::eDlssRoughness ) {
                            return rays.mReconstruction.mNormalRoughness;
                        }
                        return rays.mReconstruction.GetTextures()[static_cast<core::u32>( mPresentTarget ) -
                            static_cast<core::u32>( FinalImageType::eDlssNoisyColor )];
                    }
                }
                return b.Get<PostProcessModuleInfo>().mAfterToneMapping.mColorImage;
            case FinalImageType::eDenoiserValidation:
                if ( b.Contains<DenoiserModuleInfo>() ) {
                    const auto &denoiser{ b.Get<DenoiserModuleInfo>() };
                    if ( denoiser.mValidationReady ) {
                        return denoiser.mValidation;
                    }
                }
                return b.Get<PostProcessModuleInfo>().mAfterToneMapping.mColorImage;
            case FinalImageType::eGBuffer_Color:
                return b.Get<PrepassModuleInfo>().mGBufferColorTarget;
            case FinalImageType::eGBuffer_Position:
                return b.Get<PrepassModuleInfo>().mGBufferPositionTarget;
            case FinalImageType::eGBuffer_Normals:
                return b.Get<PrepassModuleInfo>().mGBufferNormalTarget;
            case FinalImageType::eGBuffer_Emissive:
                return b.Get<PrepassModuleInfo>().mGBufferEmissiveTarget;
            case FinalImageType::eWireframe:
                return b.Get<WireframeData>().mColorImage;
            case FinalImageType::eDepthPrepass:
                return b.Get<PrepassModuleInfo>().mDepthPrepassColorTarget;
            case FinalImageType::ePBRadiance_Output:
                return b.Get<GeomShadingModuleInfo>().mColorImage;
            case FinalImageType::eTonemap_Output:
                return b.Get<PostProcessModuleInfo>().mAfterToneMapping.mColorImage;

            case FinalImageType::eChromaticAberration: {
                const auto& effects{ b.Get<DisplayEffectsModuleInfo>() };
                return effects.mChromaticAberrationEnabled ? effects.mChromaAbRenderTarget : b.Get<GeomShadingModuleInfo>().mTonemapColor;
            }

            case FinalImageType::eClusterHeatmap:
            case FinalImageType::eClusterHeatmapOverlay:
                return b.Get<GeomShadingModuleInfo>().mTonemapColor;
            default:;
        }

        return { FGResourceManager::kInvalidResourceHandle };
    }

    auto PresentationModule::RegisterFullQuadRender( FrameGraph& graph, FGTextureHandle source ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Create the resources
        auto& info{ graph.GetOrCreate<PresentationPassData>() };

        const auto dimensions{ InferDimensions( mResolution ) };

        auto colorImage{ FGTextureDescription{}
            .SetName( "PresentationPassData_ColorImage01" )
            .SetWidth( core::as<core::i32>( dimensions.first ) )
            .SetHeight( core::as<core::i32>( dimensions.second ) )
            .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
            .SetMultisampling( renderer::rhi::Multisampling::eMsaaX1 )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::RenderTarget | renderer::rhi::TextureUsageFlagsBits::ShaderResource )
            .SetFormat( renderer::rhi::Format::eBGRA8_UNORM ) };
        info.mColorImage = graph.Create( colorImage );
        mFinalImageHandle = info.mColorImage;

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "ResourceTransition_Pipeline01" )
            .SetPipelineType( renderer::rhi::PipelineType::eGraphics )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleStrip )
            .SetDepthTest( false )
            .SetDepthWrite( false )
            .SetBlendEnable( false )
            .AddColorFormat( renderer::rhi::Format::eBGRA8_UNORM )
            .PushShader( "FullQuad_Vert.slang", FGStageType::eVertex )
            .PushShader( "FullQuad_Frag.slang", FGStageType::ePixel ) };
        info.mPipeline = graph.Create( pipelineBuilder );

        info.mClusterHeatmapPipeline = graph.Create( FGPipelineDescription{}
            .SetName( "ClusterHeatmap_Pipeline" )
            .SetPipelineType( renderer::rhi::PipelineType::eGraphics )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList )
            .SetCullMode( renderer::rhi::CullMode::eNone )
            .SetDepthTest( false )
            .SetDepthWrite( false )
            .SetBlendEnable( false )
            .AddColorFormat( renderer::rhi::Format::eBGRA8_UNORM )
            .PushShader( "FullQuad_Vert.slang", FGStageType::eVertex )
            .PushShader( "ClusterHeatmap_Frag.slang", FGStageType::ePixel ) );

        const auto ssaoDebugPipeline{ graph.Create( FGPipelineDescription{}
            .SetName( "SSAODebug_Pipeline" )
            .SetPipelineType( rhi::PipelineType::eGraphics )
            .SetTopology( rhi::PrimitiveTopology::eTriangleList )
            .SetCullMode( rhi::CullMode::eNone )
            .SetDepthTest( false )
            .SetDepthWrite( false )
            .SetBlendEnable( false )
            .AddColorFormat( rhi::Format::eBGRA8_UNORM )
            .PushShader( "FullQuad_Vert.slang", FGStageType::eVertex )
            .PushShader( "SSAODebug_Frag.slang", FGStageType::ePixel ) ) };

        FGPipelineHandle reconstructionDebugPipeline{};
        if ( const auto *rays{ graph.GetBlackBoard().TryGet<RayTracingInfo>() };
             rays && rays->mReconstruction.mNoisyColor.mHandle ) {
            reconstructionDebugPipeline = graph.Create( FGPipelineDescription{}
                .SetName( "DLSS_InputPreviewPipeline" )
                .SetPipelineType( rhi::PipelineType::eGraphics )
                .SetTopology( rhi::PrimitiveTopology::eTriangleList )
                .SetCullMode( rhi::CullMode::eNone )
                .SetDepthTest( false )
                .SetDepthWrite( false )
                .SetBlendEnable( false )
                .AddColorFormat( rhi::Format::eBGRA8_UNORM )
                .PushShader( "FullQuad_Vert.slang", FGStageType::eVertex )
                .PushShader( "DlssDebug_Frag.slang", FGStageType::ePixel ) );
        }

        graph.RegisterPass(
            "FinalImage_Resolve",
            FGPassType::eGraphics,
            [source]( FGNodeBuilder& builder, core::Blackboard& blackboard ) {
                // NRD alone provides a validation attachment. Declare its read
                // when allocated; readiness is updated later during filtering.
                if ( const auto *denoiser{ blackboard.TryGet<DenoiserModuleInfo>() };
                     denoiser && denoiser->mValidation.mHandle != FGResourceManager::kInvalidResourceHandle ) {
                    builder.UseResource( denoiser->mValidation, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                }

                if ( const auto *rays{ blackboard.TryGet<RayTracingInfo>() } ) {
                    for ( auto texture: rays->mReconstruction.GetTextures() ) {
                        if ( texture.mHandle ) {
                            builder.UseResource( texture, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                        }
                    }
                }
                if ( source.mHandle != FGResourceManager::kInvalidResourceHandle ) {
                    builder.UseResource( source, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                    if ( blackboard.Contains<RayTracingInfo>() ) {
                        builder.UseResource( blackboard.Get<RayTracingInfo>().mAmbientOcclusion, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                    }
                    builder.UseResource( blackboard.Get<PresentationPassData>().mColorImage, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                    return;
                }
                // Declare all selectable sources up front. Output selection
                // can then change without rebuilding the graph, while each
                // source still receives the required read synchronization.
                const auto& wireframeData{ blackboard.Get<WireframeData>() };
                const auto& presentData{ blackboard.Get<PresentationPassData>() };
                const auto& prePassData{ blackboard.Get<PrepassModuleInfo>() };
                const auto& shadingPassData{ blackboard.Get<GeomShadingModuleInfo>() };
                const auto& displayEffectsData{ blackboard.Get<DisplayEffectsModuleInfo>() };

                const auto AddPresentImage{ [&]( FGTextureHandle handle ) {
                    if ( handle.mHandle == FGResourceManager::kInvalidResourceHandle ) {
                        return;
                    }

                    builder.UseResource( handle, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                } };

                AddPresentImage( prePassData.mGBufferPositionTarget );
                AddPresentImage( prePassData.mGBufferNormalTarget );
                AddPresentImage( prePassData.mGBufferColorTarget );
                AddPresentImage( prePassData.mGBufferEmissiveTarget );

                AddPresentImage( displayEffectsData.mChromaAbRenderTarget );
                AddPresentImage( displayEffectsData.mVignetteCompositeTarget );
                AddPresentImage( blackboard.Get<PostProcessModuleInfo>().mSsaoBlurColorTarget );

                AddPresentImage( wireframeData.mColorImage );

                AddPresentImage( shadingPassData.mTonemapColor );
                AddPresentImage( shadingPassData.mColorImage );

                const auto& postProcessData{ blackboard.Get<PostProcessModuleInfo>() };
                AddPresentImage( postProcessData.mBeforeToneMapping.mColorImage );
                AddPresentImage( postProcessData.mAfterToneMapping.mColorImage );

                AddPresentImage( prePassData.mDepthPrepassColorTarget );

                if ( blackboard.Contains<RayTracingInfo>() ) {
                    AddPresentImage( blackboard.Get<RayTracingInfo>().mAmbientOcclusion );
                }

                // Debug presentation reads the culling result on the GPU. Declare
                // its compute-to-fragment dependency even though the heatmap
                // pipeline is selected only when that output is requested.
                AddPresentImage( prePassData.mPrepassDepthTarget );
                builder.UseResource( prePassData.mClusterBuffer, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( blackboard.Get<CameraModuleInfo>().mCameraData, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );

                builder.UseResource( presentData.mColorImage, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [this, source, ssaoDebugPipeline, reconstructionDebugPipeline]( CommandContext& ctx, core::Blackboard& b ) {
                const auto& presentData{ b.Get<PresentationPassData>() };
                const auto& geometryData{ b.Get<GeometryCullModuleInfo>() };

                const bool fixedSource{ source.mHandle != FGResourceManager::kInvalidResourceHandle };
                const bool heatmap{ !fixedSource && ( mPresentTarget == FinalImageType::eClusterHeatmap || mPresentTarget == FinalImageType::eClusterHeatmapOverlay ) };
                const bool rayAoDebug{ mPresentTarget == FinalImageType::eRayTracedAmbientOcclusion };
                const bool denoiserDebug{ mPresentTarget == FinalImageType::eDenoiserValidation };
                const bool reconstructionTarget{ mPresentTarget >= FinalImageType::eDlssNoisyColor && mPresentTarget <= FinalImageType::eDlssRoughness };
                const bool reconstructionDebug{ reconstructionTarget && b.Contains<RayTracingInfo>() && b.Get<RayTracingInfo>().mReconstructionReady };
                const bool ssaoDebug{ rayAoDebug || ( !fixedSource && mPresentTarget == FinalImageType::eSsao ) };

                if ( reconstructionDebug ) {
                    struct Parameters {
                        core::u32 mTexture{};
                        core::u32 mSampler{};
                        core::u32 mMode{};
                    } parameters{
                        .mTexture = ctx.PushTexture_SRV( GetTargetImage( b ) ),
                        .mSampler = ctx.PushSampler( geometryData.mBasicSampler ),
                        .mMode = static_cast<core::u32>( mPresentTarget ) - static_cast<core::u32>( FinalImageType::eDlssNoisyColor ),
                    };
                    ctx.PushConstants( parameters );
                }
                else if ( heatmap ) {
                    const auto& prepass{ b.Get<PrepassModuleInfo>() };
                    struct HeatmapParams {
                        SPointer mClusters{};
                        SPointer mCamera{};
                        core::float4 mGridSize{};
                        core::u32 mSceneTexture{};
                        core::u32 mDepthTexture{};
                        core::u32 mSampler{};
                        core::u32 mClusterCount{};
                        core::u32 mOverlay{};
                        core::f32 mOpacity{ 0.55f };
                    } params{
                        .mClusters = ctx.GetDeviceBufferAddress( prepass.mClusterBuffer ),
                        .mCamera = ctx.GetDeviceBufferAddress( b.Get<CameraModuleInfo>().mCameraData ),
                        .mGridSize = prepass.mGridSize,
                        .mSceneTexture = ctx.PushTexture_SRV( GetTargetImage( b ) ),
                        .mDepthTexture = ctx.PushTexture_SRV( prepass.mPrepassDepthTarget ),
                        .mSampler = ctx.PushSampler( geometryData.mBasicSampler ),
                        .mClusterCount = prepass.mClusterCount,
                        .mOverlay = mPresentTarget == FinalImageType::eClusterHeatmapOverlay ? 1u : 0u
                    };
                    ctx.PushConstants( params );
                }
                else {
                    struct DrawParams {
                        core::u32 mTextureIndex{};
                        core::u32 mSamplerIndex{};
                    } params{
                        .mTextureIndex = ctx.PushTexture_SRV( fixedSource && !rayAoDebug && !denoiserDebug && !reconstructionTarget ? source : GetTargetImage( b ) ),
                        .mSamplerIndex = ctx.PushSampler( geometryData.mBasicSampler )
                    };
                    ctx.PushConstants( params );
                }

                const auto dimensions{ InferDimensions( mResolution ) };

                const auto graphicsState{ RenderBeginDescription{}
                    .SetRenderArea( renderer::rhi::Rect{ core::as<core::i32>( dimensions.first ), core::as<core::i32>( dimensions.second ) } )
                    .AddRenderTarget( presentData.mColorImage, renderer::rhi::Color{ .0f }, renderer::rhi::LoadOp::eClear ) };
                ctx.BeginRender( graphicsState );

                ctx.SetViewportState( renderer::rhi::ViewportState{}
                    .AddViewportAndScissorRect( renderer::rhi::Viewport( core::as<core::f32>( dimensions.first ), core::as<core::f32>( dimensions.second ) ) ) );

                // Debug views select their own fragment shaders within the existing presentation draw.
                auto pipeline{ presentData.mPipeline };

                if ( reconstructionDebug ) {
                    pipeline = reconstructionDebugPipeline;
                }
                else if ( heatmap ) {
                    pipeline = presentData.mClusterHeatmapPipeline;
                }
                else if ( ssaoDebug ) {
                    pipeline = ssaoDebugPipeline;
                }

                ctx.BindPipeline( pipeline );
                ctx.Draw( 3 );

                ctx.EndRender();
            } );
    }
}// namespace mikoto::renderer
