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

#include <Renderer/Passes/GeometryShadingModule.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;

    PresentationModule::PresentationModule( RenderResolution resolution )
        : mResolution{ resolution }
    {}

    auto PresentationModule::RegisterPasses( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        RegisterTransition( graph );
        RegisterFullQuadRender( graph );
    }

    auto PresentationModule::SetPresentType( PresentTarget type ) -> void {
        mPresentTarget = type;
    }

    auto PresentationModule::RegisterPresentImage( FrameGraph& graph, TextureHandle texture ) -> void {
        mPresentTexture = graph.ImportTexture( texture );
    }

    auto PresentationModule::RegisterTransition( FrameGraph &graph ) -> void {
        graph.RegisterPass(
            "ResourceTransition",
            FGPassType::eGraphics,
            []( FGNodeBuilder&, Blackboard & ) {

            },
            []( CommandContext&, Blackboard & ) {
                // Nothing
            } );
    }

    auto PresentationModule::GetTargetImage( Blackboard &b ) -> FGTextureHandle {
        switch ( mPresentTarget ) {
            case PresentTarget::eGBuffer_Color: return b.Get<PrepassModuleInfo>().mGBufferColorTarget;
            case PresentTarget::eGBuffer_Position: return b.Get<PrepassModuleInfo>().mGBufferPositionTarget;
            case PresentTarget::eGBuffer_Normals: return b.Get<PrepassModuleInfo>().mGBufferNormalTarget;
            case PresentTarget::eGBuffer_Emissive: return b.Get<PrepassModuleInfo>().mGBufferEmissiveTarget;
            case PresentTarget::eWireframe: return b.Get<WireframeData>().mColorImage;
            case PresentTarget::eDepthPrepass: return b.Get<PrepassModuleInfo>().mDepthPrepassColorTarget;
            case PresentTarget::ePBRadiance_Output: return b.Get<GeomShadingModuleInfo>().mShadingColorImage;
            case PresentTarget::eTonemap_Output: return b.Get<GeomShadingModuleInfo>().mTonemapColor;
            default:;
        }

        return { FGResourceManager::kInvalidResourceHandle };
    }

    auto PresentationModule::RegisterFullQuadRender( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Create the resources
        PresentationPassData& presentationPassData{ graph.GetOrCreate<PresentationPassData>() };

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "ResourceTransition_Pipeline01" )
            .SetPipelineType( PipelineType::eGraphics )
            .SetTopology( PrimitiveTopology::eTriangleStrip )
            .AddColorFormat( Format::eBGRA8_UNORM )
            .PushShader( "FullQuad_Vert.slang", FGStageType::eVertex )
            .PushShader( "FullQuad_Frag.slang", FGStageType::ePixel ) };

        presentationPassData.mPipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass(
            "FullQuadRender",
            FGPassType::eGraphics,
            []( FGNodeBuilder &builder, Blackboard &blackboard ) {

                // Specify the target you want to present
                // The color target used in this pass is externally managed
                // The frame graph does not control access to it nor does it make sure the
                // image is in proper layout
                const auto& wireframe{ blackboard.Get<WireframeData>() };
                const auto& prepass{ blackboard.Get<PrepassModuleInfo>() };
                const auto& trianglePassData{ blackboard.Get<TrianglePassData>() };
                const auto& shading{ blackboard.Get<GeomShadingModuleInfo>() };

                builder.Read( trianglePassData.mColorTarget, FGResourceState::eShaderResource );
                builder.Read( prepass.mGBufferNormalTarget, FGResourceState::eShaderResource );
                builder.Read( prepass.mGBufferColorTarget, FGResourceState::eShaderResource );
                builder.Read( wireframe.mColorImage, FGResourceState::eShaderResource );
                builder.Read( shading.mShadingColorImage, FGResourceState::eShaderResource );
                builder.Read( prepass.mDepthPrepassColorTarget, FGResourceState::eShaderResource );
            },
            [this]( CommandContext &ctx, Blackboard &b ) {
                const auto &data{ b.Get<PresentationPassData>() };
                const auto &geom{ b.Get<GeometryManagementModuleInfo>() };

                struct DrawParams {
                    u32 mTextureIndex{};
                    u32 mSamplerIndex{};
                } params{
                    .mTextureIndex = ctx.PushTexture_SRV( GetTargetImage( b ) ),
                    .mSamplerIndex = ctx.PushSampler( geom.mBasicSampler ),
                };

                ctx.PushConstants( params );

                const auto dimensions{ InferDimensions( mResolution ) };

                auto graphicsState{ ContextRenderState{}
                    .SetRenderArea( Rect{ as<i32>( dimensions.first ), as<i32>( dimensions.second ) } )
                    .AddRenderTarget( mPresentTexture, Color{ .0f }, LoadOp::eClear ) };
                ctx.BeginRender( graphicsState );

                ctx.SetViewportState( ViewportState{}
                    .AddViewportAndScissorRect( Viewport( as<f32>( dimensions.first ), as<f32>( dimensions.second ) ) ) );

                ctx.BindPipeline( data.mPipeline );
                ctx.Draw( 3 );

                ctx.EndRender();
            } );
    }
}// namespace mikoto