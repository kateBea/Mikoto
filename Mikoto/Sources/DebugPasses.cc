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
                    b.Create<Texture>( "HelloTriangle_ColorTarget", RenderResolution::FHD_1080, TextureFormat::RGBA8_UNORM, TextureUsage::COLOR );
                    b.Create<Texture>( "HelloTriangle_DepthTarget", RenderResolution::FHD_1080, TextureFormat::D32_FLOAT_S8_UINT, TextureUsage::DEPTH );

                    b.UseShader( "Resources/Shaders/vulkan-spirv/HelloTriangle_Vert.sprv", ShaderStage::VERTEX );
                    b.UseShader( "Resources/Shaders/vulkan-spirv/HelloTriangle_Frag.sprv", ShaderStage::FRAGMENT );

                    b.Create<Pipeline>( "HelloTriangle_Pipeline", GraphicsPipelineDescription{ .VertexAttributesSpec{} } );

                    b.Write( "HelloTriangle_ColorTarget", FrameResourceState::ShaderResource_Read );
                    b.Write( "HelloTriangle_DepthTarget", FrameResourceState::ShaderResource_Read );
                },
                []( CommandContext &ctx, FrameGraphBlackboard & ) -> void {
                    ctx.BindPipeline( "HelloTriangle_Pipeline" );

                    ctx.SetViewport( 0, 0, 1920, 1080 );
                    ctx.SetScissor( 0, 0, 1920, 1080 );

                    ctx.SetClearColor( { 0.3f, 0.4f, 0.8f, 1.0f } );
                    ctx.SetColorRenderTarget( "HelloTriangle_ColorTarget" );
                    ctx.BeginRender();

                    ctx.Draw( 3, 1, 0, 0 );

                    ctx.EndRender();
                } );
    }

    auto RegisterSimpleCompute( FrameGraph &graph ) -> void {
        graph.RegisterPass(
                "SimpleCompute",
                []( FramePassBuilder &b ) {
                    b.Create<Buffer>( "SimpleCompute_Results", BufferUsage::SSBO, 30 * sizeof( float ) );

                    b.UseShader( "Resources/Shaders/vulkan-spirv/BasicCompute_Comp.sprv", ShaderStage::COMPUTE );

                    b.Create<Pipeline>( "SimpleCompute_Pipeline", ComputePipelineDescription{} );

                    b.Write( "SimpleCompute_Results", FrameResourceState::Transfer_Src );

                    // Shader resources
                    b.SetBufferSR( SRGType::SRG_PerPass, "SimpleCompute_Results", 0 );
                },
                []( CommandContext &ctx, FrameGraphBlackboard & ) -> void {
                    ctx.BindPipeline( "SimpleCompute_Pipeline" );

                    // Prime numbers up until this value
                    constexpr UInt32 limitNumbers{ 30 };

                    // matches shader's local_size_x
                    constexpr UInt32 localSize{ 64 };
                    constexpr UInt32 groupCount{ ( limitNumbers + localSize - 1 ) / localSize };

                    ctx.Dispatch( groupCount, 1, 1 );
                } );
    }
}