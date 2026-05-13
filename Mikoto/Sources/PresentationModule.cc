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
#include <Renderer/Passes/PresentationModule.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;

    auto PresentationModule::RegisterPasses( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        RegisterTransition( graph );
        RegisterFullQuadRender( graph );
        //RegisterRenderToSwapchain( graph );
    }

    auto PresentationModule::AddPresentTexture( TextureHandle texture ) -> void {
        mPresentTextures.emplace_back( texture );
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

    auto PresentationModule::RegisterFullQuadRender( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Create the resources
        PresentationPassData& presentationPassData{ graph.GetOrCreate<PresentationPassData>() };

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "ResourceTransition_Pipeline01" )
            .SetPipelineType( PipelineType::eGraphics )
            .SetTopology( PrimitiveTopology::eTriangleStrip )
            .SetDepthFormat( Format::eD32 )
            .AddColorFormat( Format::eBGRA8_UNORM )
            .PushShader( "FullQuad_Vert.slang", FGStageType::eVertex )
            .PushShader( "FullQuad_Frag.slang", FGStageType::eFragment ) };

        presentationPassData.mPipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass(
            "FullQuadRender",
            FGPassType::eGraphics,
            []( FGNodeBuilder &builder, Blackboard &blackboard ) {
                const auto& trianglePassData{ blackboard.Get<TrianglePassData>() };
                builder.Read( trianglePassData.mColorTarget, FGResourceState::eShaderResource );

                const auto& data{ blackboard.Get<PresentationPassData>() };
                for ( const auto& image : data.mPresentTextures ) {
                    builder.Write( image, FGResourceState::eShaderResource );
                }
            },
            []( CommandContext & ctx, Blackboard &b ) {
                const auto& data{ b.Get<PresentationPassData>() };
                for ( const auto& image : data.mPresentTextures ) {
                    struct DrawParams {
                        u32 mTextureIndex{};
                        u32 mSamplerIndex{};
                    } params{
                        .mTextureIndex = ctx.PushTexture_SRV( image ),
                        .mSamplerIndex = ctx.PushSampler( data.mSampler ),
                    };

                    ctx.PushConstants( params );

                    auto graphicsState{ ContextRenderState{}
                        .SetRenderArea( Rect{ 1920, 1080 } )
                        .AddRenderTarget( image, kColorCyan, LoadOp::eClear ) };
                    ctx.BeginRender( graphicsState );

                    ctx.SetViewportState( ViewportState{}
                        .AddViewportAndScissorRect( Viewport( 1920, 1080 ) ) );

                    ctx.BindPipeline( data.mPipeline );
                    ctx.Draw( 3 );

                    ctx.EndRender();
                }
            } );
    }

    auto PresentationModule::RegisterRenderToSwapchain( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

    }
}// namespace mikoto