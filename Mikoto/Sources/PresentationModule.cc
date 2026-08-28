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
#include <Core/Profiler.hh>
#include <Core/Types.hh>

#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Passes/DebugModule.hh>

#include <Renderer/Passes/PrepassModule.hh>
#include <Renderer/Passes/PresentationModule.hh>
#include <Renderer/Passes/GeometryCullModule.hh>
#include <Renderer/Passes/DisplayEffectsModule.hh>

#include <Renderer/Passes/GeometryShadingModule.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::renderer::rhi;

    PresentationModule::PresentationModule( RenderResolution resolution )
        : mResolution{ resolution }
    {}

    auto PresentationModule::RegisterPasses( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        auto& info{ graph.GetOrCreate<PresentationPassData>() };

        const auto dimensions{ InferDimensions( mResolution ) };

        auto colorImage{ FGTextureDescription{}
            .SetName( "PresentationPassData_ColorImage01" )
            .SetWidth( as<i32>( dimensions.first ) )
            .SetHeight( as<i32>( dimensions.second ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetUsage( TextureUsageFlagsBits::kRenderTarget | TextureUsageFlagsBits::kShaderResource )
            .SetFormat( Format::eBGRA8_UNORM ) };
        info.mColorImage = graph.Create( colorImage );
        mFinalImageHandle = info.mColorImage;

        RegisterFullQuadRender( graph );
        RegisterTransition( graph );
    }

    auto PresentationModule::GetFinalImage( FrameGraph& graph ) -> TextureHandle {
        return graph.GetTexture( mFinalImageHandle );
    }

    auto PresentationModule::SetFinalImageTarget( FinalImageType type ) -> void {
        mPresentTarget = type;
    }

    auto PresentationModule::RegisterTransition( FrameGraph &graph ) -> void {
        // Transition for usage in ImGui viewport
        graph.RegisterPass(
            "ResourceTransition",
            FGPassType::eGraphics,
            []( FGNodeBuilder& builder, Blackboard& blackboard ) {
                const auto& presentData{ blackboard.Get<PresentationPassData>() };

                builder.UseResource( presentData.mColorImage, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
            },
            []( CommandContext&, Blackboard & ) {
                // Nothing
            } );
    }

    auto PresentationModule::GetTargetImage( const Blackboard &b ) -> FGTextureHandle {
        switch ( mPresentTarget ) {
            case FinalImageType::eGBuffer_Color: return b.Get<PrepassModuleInfo>().mGBufferColorTarget;
            case FinalImageType::eGBuffer_Position: return b.Get<PrepassModuleInfo>().mGBufferPositionTarget;
            case FinalImageType::eGBuffer_Normals: return b.Get<PrepassModuleInfo>().mGBufferNormalTarget;
            case FinalImageType::eGBuffer_Emissive: return b.Get<PrepassModuleInfo>().mGBufferEmissiveTarget;
            case FinalImageType::eWireframe: return b.Get<WireframeData>().mColorImage;
            case FinalImageType::eDepthPrepass: return b.Get<PrepassModuleInfo>().mDepthPrepassColorTarget;
            case FinalImageType::ePBRadiance_Output: return b.Get<GeomShadingModuleInfo>().mColorImage;
            case FinalImageType::eTonemap_Output: return b.Get<GeomShadingModuleInfo>().mTonemapColor;
            case FinalImageType::eChromaticAberration: return b.Get<DisplayEffectsModuleInfo>().mChromaAbRenderTarget;
            default:;
        }

        return { FGResourceManager::kInvalidResourceHandle };
    }

    auto PresentationModule::RegisterFullQuadRender( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Create the resources
        auto& info{ graph.GetOrCreate<PresentationPassData>() };

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "ResourceTransition_Pipeline01" )
            .SetPipelineType( PipelineType::eGraphics )
            .SetTopology( PrimitiveTopology::eTriangleStrip )
            .AddColorFormat( Format::eBGRA8_UNORM )
            .PushShader( "FullQuad_Vert.slang", FGStageType::eVertex )
            .PushShader( "FullQuad_Frag.slang", FGStageType::ePixel ) };
        info.mPipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass(
            "FinalImage_Resolve",
            FGPassType::eGraphics,
            []( FGNodeBuilder &builder, Blackboard &blackboard ) {
                // Specify the target you want to present
                // The color target used in this pass is externally managed
                // The frame graph does not control access to it nor does it make sure the
                // image is in proper layout
                const auto& wireframeData{ blackboard.Get<WireframeData>() };
                const auto& presentData{ blackboard.Get<PresentationPassData>() };
                const auto& prePassData{ blackboard.Get<PrepassModuleInfo>() };
                const auto& shadingPassData{ blackboard.Get<GeomShadingModuleInfo>() };
                const auto& displayEffectsData{ blackboard.Get<DisplayEffectsModuleInfo>() };

                const auto AddPresentImage{ [&](FGTextureHandle handle ) {
                    if (handle.mHandle == FGResourceManager::kInvalidResourceHandle) {
                        return;
                    }
                    builder.UseResource( handle, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                } };

                AddPresentImage( prePassData.mGBufferPositionTarget );
                AddPresentImage( prePassData.mGBufferNormalTarget );
                AddPresentImage( prePassData.mGBufferColorTarget );
                AddPresentImage( prePassData.mGBufferEmissiveTarget );

                AddPresentImage( displayEffectsData.mChromaAbRenderTarget );

                AddPresentImage( wireframeData.mColorImage );

                AddPresentImage( shadingPassData.mTonemapColor );
                AddPresentImage( shadingPassData.mColorImage );
                AddPresentImage( prePassData.mDepthPrepassColorTarget );

                builder.UseResource( presentData.mColorImage, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [this]( CommandContext &ctx, Blackboard &b ) {
                const auto &presentData{ b.Get<PresentationPassData>() };
                const auto &geometryData{ b.Get<GeometryCullModuleInfo>() };

                struct DrawParams {
                    u32 mTextureIndex{};
                    u32 mSamplerIndex{};
                } params{
                    .mTextureIndex = ctx.PushTexture_SRV( GetTargetImage( b ) ),
                    .mSamplerIndex = ctx.PushSampler( geometryData.mBasicSampler ) };
                ctx.PushConstants( params );

                const auto dimensions{ InferDimensions( mResolution ) };

                const auto graphicsState{ ContextRenderState{}
                    .SetRenderArea( Rect{ as<i32>( dimensions.first ), as<i32>( dimensions.second ) } )
                    .AddRenderTarget( presentData.mColorImage, Color{ .0f }, LoadOp::eClear ) };
                ctx.BeginRender( graphicsState );

                ctx.SetViewportState( ViewportState{}
                    .AddViewportAndScissorRect( Viewport( as<f32>( dimensions.first ), as<f32>( dimensions.second ) ) ) );

                ctx.BindPipeline( presentData.mPipeline );
                ctx.Draw( 3 );

                ctx.EndRender();
            } );
    }
}// namespace mikoto