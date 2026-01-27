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

#include <Renderer/Core/FramePassResource.hh>
#include <Renderer/Passes/DebugPasses.hh>
#include <Renderer/Core/CommandContext.hh>

namespace Mikoto {

    DebugPasses::DebugPasses( RenderResolution resolution )
        : m_Resolution{ resolution }
    {

    }

    auto DebugPasses::SetScene( const Scene *scene ) -> void {
        m_Scene = scene;
    }

    auto DebugPasses::SetClearColor( const Vec4F &vec ) -> void {
        m_ClearColor = vec;
    }

    auto DebugPasses::SetLinesColor( const Vec4F &color ) -> void {
        m_LinesColor = color;
    }

    auto DebugPasses::ShowColorImage( bool value ) -> void {
        m_ShowColorImageWireframe = value;
    }

    auto DebugPasses::RegisterPasses( FrameGraph &graph ) -> void {
        RegisterHelloTexture( graph );
        RegisterSimpleCompute( graph );
        RegisterHelloTriangle( graph );
        RegisterWireFrame( graph );
    }

    auto DebugPasses::RegisterObjectOutline( FrameGraph &graph ) -> void {

    }

    auto DebugPasses::RegisterWireFrame( FrameGraph &graph ) -> void {

    }

    auto DebugPasses::RegisterMaterialPreview( FrameGraph &graph ) -> void {

    }

    auto DebugPasses::RegisterHelloTriangle( FrameGraph &graph ) -> void {
        graph.RegisterPass(
                "HelloTriangle",
                [this]( FramePassBuilder &b ) {
                    b.Create<Texture>( "HelloTriangle_ColorTarget", m_Resolution, TextureFormat::RGBA8_UNORM, TextureUsage::COLOR );
                    b.Create<Texture>( "HelloTriangle_DepthTarget", m_Resolution, TextureFormat::D32_FLOAT_S8_UINT, TextureUsage::DEPTH );

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
                    ctx.SetDepthRenderTarget( "HelloTriangle_DepthTarget" );
                    ctx.BeginRender();

                    ctx.Draw( 3, 1, 0, 0 );

                    ctx.EndRender();
                } );
    }

    auto DebugPasses::RegisterSimpleCompute( FrameGraph &graph ) -> void {

        graph.RegisterPass(
                "SimpleCompute",
                []( FramePassBuilder &b ) {
                    b.Create<Buffer>( "SimpleCompute_Results", BufferUsage::SSBO, 30 * sizeof( float ) );

                    b.UseShader( "Resources/Shaders/vulkan-spirv/BasicCompute_Comp.sprv", ShaderStage::COMPUTE );
                    b.Create<Pipeline>( "SimpleCompute_Pipeline", ComputePipelineDescription{} );

                    b.Write( "SimpleCompute_Results", FrameResourceState::Transfer_Src );

                    b.Use( SRGType::SRG_PerPass, "SimpleCompute_Results", 0 );
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

    auto DebugPasses::RegisterInfiniteGrid( FrameGraph &graph ) -> void {

    }

    auto DebugPasses::RegisterHelloCube( FrameGraph &graph ) -> void {

    }

    auto DebugPasses::RegisterHelloTexture( FrameGraph &graph ) -> void {
        struct HelloTextureData {
            Int32 TextureIndex{ SRGTextures::INVALID_TEXTURE_INDEX };
        };

        // Example with pass data
        graph.RegisterPass<HelloTextureData>(
                "HelloTexture",
                [this]( FramePassBuilder &b, HelloTextureData& ) -> void {
                    b.Create<Buffer>( "HelloTexture_TexturesBuffer", BufferUsage::UNIFORM, sizeof( HelloTextureData ) );
                    b.Create<Texture>( "HelloTexture_ColorTarget", m_Resolution, TextureFormat::RGBA8_UNORM, TextureUsage::COLOR );
                    b.Create<Texture>( "HelloTexture_DepthTarget", m_Resolution, TextureFormat::D32_FLOAT_S8_UINT, TextureUsage::DEPTH );

                    b.UseShader( "Resources/Shaders/vulkan-spirv/FullscreenTriangle_Vert.sprv", ShaderStage::VERTEX );
                    b.UseShader( "Resources/Shaders/vulkan-spirv/FullscreenTriangle_Frag.sprv", ShaderStage::FRAGMENT );

                    b.Create<Pipeline>( "HelloTexture_Pipeline", GraphicsPipelineDescription{
                                            .PrimitiveTopology{ Topology::TRIANGLE_STRIP },
                                            .VertexAttributesSpec{},} );

                    b.Write( "HelloTexture_ColorTarget", FrameResourceState::ShaderResource_Read );
                    b.Write( "HelloTexture_DepthTarget", FrameResourceState::ShaderResource_Read );

                    b.Write( "HelloTexture_TexturesBuffer", FrameResourceState::ShaderResource_Read );
                    b.Use( SRGType::SRG_PerPass, "HelloTexture_TexturesBuffer", 0 );
                    b.Use( SRGType::SRG_Textures );
                },
                [this]( CommandContext &ctx, FrameGraphBlackboard & blackboard) -> void {
                    m_TextureHandle = AssetsService::Get()->LoadAsset<Texture>( Path{ "Resources/Models/1 - Box texture/CatStare.png" } );

                    auto& data{ blackboard.Get<HelloTextureData>() };
                    data.TextureIndex = ctx.PushTexture( m_TextureHandle );

                    ctx.UploadBuffer<HelloTextureData>( "HelloTexture_TexturesBuffer", std::addressof( data ) );

                    ctx.BindPipeline( "HelloTexture_Pipeline" );

                    ctx.SetViewport( 0, 0, 1920, 1080 );
                    ctx.SetScissor( 0, 0, 1920, 1080 );

                    ctx.SetClearColor( { 0.5f, 0.2f, 0.3f, 1.0f } );
                    ctx.SetColorRenderTarget( "HelloTexture_ColorTarget" );
                    ctx.SetDepthRenderTarget( "HelloTexture_DepthTarget" );
                    ctx.BeginRender();

                    ctx.Draw( 4, 1, 0, 0 );

                    ctx.EndRender();
                } );
    }
}