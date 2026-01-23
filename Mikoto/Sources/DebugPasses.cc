//    Copyright 2025 ケイト
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

#include <Scene/Scene.hh>
#include <Scene/Component.hh>

#include <Renderer/Core/FrameResource.hh>
#include <Renderer/Passes/DebugPasses.hh>

namespace Mikoto {

    auto RegisterHelloTriangle( FrameGraph &graph ) -> void {
        graph.RegisterPass(
                "HelloTriangle",
                []( FramePassBuilder &b ) {
                    b.Create<Texture>( "HelloTriangle_ColorTarget", 1920, 1080, TextureFormat::RGBA8_UNORM );
                    b.Create<Texture>( "HelloTriangle_DepthTarget", 1920, 1080, TextureFormat::D32_FLOAT );

                    PipelineDescription desc{ .Description{ GraphicsPipelineDescription{ .VertexAttributesSpec{} } } };
                    desc.AddShader( "HelloTriangle_Vert.sprv", ShaderStage::VERTEX_STAGE );
                    desc.AddShader( "HelloTriangle_Vert.sprv", ShaderStage::FRAGMENT_STAGE );

                    b.Create<Pipeline>( "HelloTriangle_Pipeline", desc );

                    b.Write( "HelloTriangle_ColorTarget", FrameResourceState::RenderTarget_Color, FrameResourceState::ShaderResource_Read );
                    b.Write( "HelloTriangle_DepthTarget", FrameResourceState::RenderTarget_Depth, FrameResourceState::ShaderResource_Read );
                },
                []( CommandContext &ctx, FrameGraphBlackboard & ) -> void {
                    ctx.BindPipeline( "HelloTriangle_Pipeline" );

                    ctx.SetViewport( 0, 0, 1920, 1080 );
                    ctx.SetScissor( 0, 0, 1920, 1080 );

                    ctx.SetColorRenderTarget( "HelloTriangle_ColorTarget" );
                    ctx.BeginRender();

                    ctx.SetClearColor( { 0.3f, 0.4f, 0.8f, 1.0f } );

                    ctx.Draw( 3, 1, 0, 0 );

                    ctx.EndRender();
                } );
    }
}