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

#include <Core/Profiler.hh>

#include <Scene/Scene.hh>
#include <Scene/Component.hh>

#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Core/FramePassResource.hh>
#include <Renderer/Passes/DebugPasses.hh>


namespace Mikoto {

    DebugPasses::DebugPasses( RenderResolution resolution )
        : m_Resolution{ resolution }
    {

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

    auto DebugPasses::SetMeshCulling( MeshCulling &culling ) -> void {
        m_Culling = std::addressof( culling );
    }

    auto DebugPasses::SetWireframeEnable( bool enable ) -> void {
        m_RunWireframe = enable;
    }

    auto DebugPasses::RegisterPasses( FrameGraph &graph ) -> void {
        RegisterSimpleCompute( graph );
        RegisterHelloTriangle( graph );
        RegisterHelloTexture( graph );

        RegisterWireFrame( graph );
        RegisterInfiniteGrid( graph );

        RegisterDebugViewsPass( graph );
    }

    auto DebugPasses::SetWireframeLineLineWidth( float value ) -> void {
        m_WireframeLineWidth = value;
    }

    auto DebugPasses::SetWireframeLineColor( const Vec4F &color ) -> void {
        m_WireframeParams.WireframeLineColor = color;
    }

    auto DebugPasses::SetWireframeLineLineClearColor( const Vec4F &color ) -> void {
        m_WireframeClearColor = color;
    }

    auto DebugPasses::RegisterWireFrame( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass(
            "Wireframe",
            [this]( FramePassBuilder &b ) {
                MKT_BEGIN_PROFILER_NAMED();

                b.CreateTexture( "Wireframe_ColorTarget", m_Resolution, TextureFormat::RGBA8_UNORM, TextureUsage::COLOR );
                b.CreateTexture( "Wireframe_DepthTarget", m_Resolution, TextureFormat::D32_FLOAT_S8_UINT, TextureUsage::DEPTH );

                b.UseShader( "Resources/Shaders/slang/Wireframe_Vert.slang", ShaderStage::VERTEX );
                b.UseShader( "Resources/Shaders/slang/Wireframe_Frag.slang", ShaderStage::FRAGMENT );

                GraphicsPipelineDescription graphicsDesc{
                    .DepthTest{ true },
                    .DepthWrite{ true },
                    .AlphaBlending{ true },
                    .Wireframe{ true },
                    .PipelineCullMode{ CullMode::NONE },
                    .PipelinePolygonMode{ PolygonMode::LINES },
                };
                b.CreatePipeline( "Wireframe_Pipeline", graphicsDesc );

                b.Write( "Wireframe_ColorTarget", FrameResourceState::RenderTarget );
                b.Write( "Wireframe_DepthTarget", FrameResourceState::DepthWrite );

                b.Read( "CameraInfoPass_CameraData", FrameResourceState::UniformBuffer );
                b.Read( "FinalBuffer_ObjectInfo", FrameResourceState::UnorderedAccessView );
            },
            [this]( CommandContext &ctx, FrameGraphBlackboard & ) -> void {
                MKT_BEGIN_PROFILER_NAMED();
                if ( !m_RunWireframe ) {
                    return;
                }

                ctx.BindBuffer( ResourceGroup::BufferViews, "CameraInfoPass_CameraData", ResourceSlot::Slot_0 );
                ctx.BindBuffer( ResourceGroup::UnorderedAccessViews, "MeshCulling_GeometryInfo", ResourceSlot::Slot_0 );

                const auto dimensions{ InferDimensions( m_Resolution ) };

                ctx.SetViewport( 0, 0, dimensions.first, dimensions.second );
                ctx.SetScissor( 0, 0, dimensions.first, dimensions.second );

                ctx.SetClearColor( m_WireframeClearColor );
                ctx.SetColorRenderTarget( "Wireframe_ColorTarget" );
                ctx.SetDepthRenderTarget( "Wireframe_DepthTarget" );

                ctx.PushConstants( std::addressof( m_WireframeParams ), sizeof( m_WireframeParams ) );

                ctx.BeginRender();

                ctx.BindPipeline( "Wireframe_Pipeline" );

                ctx.SetPolygonLineWidth( m_WireframeLineWidth );

                m_Culling->DrawInstances( ctx );

                ctx.EndRender();
            } );
    }

    auto DebugPasses::RegisterHelloTriangle( FrameGraph &graph ) -> void {
        graph.RegisterPass(
            "HelloTriangle",
            [this]( FramePassBuilder &b ) {
                b.CreateTexture( "HelloTriangle_ColorTarget", m_Resolution, TextureFormat::RGBA8_UNORM, TextureUsage::COLOR );
                b.CreateTexture( "HelloTriangle_DepthTarget", m_Resolution, TextureFormat::D32_FLOAT_S8_UINT, TextureUsage::DEPTH );

                b.UseShader( "Resources/Shaders/slang/HelloTriangle_Vert.slang", ShaderStage::VERTEX );
                b.UseShader( "Resources/Shaders/slang/HelloTriangle_Frag.slang", ShaderStage::FRAGMENT );

                b.CreatePipeline( "HelloTriangle_Pipeline", GraphicsPipelineDescription{ .VertexAttributesSpec{} } );

                b.Write( "HelloTriangle_ColorTarget", FrameResourceState::RenderTarget );
                b.Write( "HelloTriangle_DepthTarget", FrameResourceState::DepthWrite );
            },
            [this]( CommandContext &ctx, FrameGraphBlackboard & ) -> void {
                const auto dimensions{ InferDimensions( m_Resolution ) };

                ctx.SetViewport( 0, 0, dimensions.first, dimensions.second );
                ctx.SetScissor( 0, 0, dimensions.first, dimensions.second );

                ctx.SetClearColor( { 0.3f, 0.4f, 0.8f, 1.0f } );
                ctx.SetColorRenderTarget( "HelloTriangle_ColorTarget" );
                ctx.SetDepthRenderTarget( "HelloTriangle_DepthTarget" );
                
                ctx.BeginRender();

                ctx.BindPipeline( "HelloTriangle_Pipeline" );
                ctx.Draw( 3 );

                ctx.EndRender();
            }, FramePassNodeType::GRAPHICS );
    }

    auto DebugPasses::RegisterSimpleCompute( FrameGraph &graph ) -> void {
        struct SimpleCompute {
            // Prime numbers up until this value
            UInt32 LimitNumbers{ 30 };

            // matches shader's local_size_x
            UInt32 LocalSize{ 64 };
            UInt32 GroupCount{ ( LimitNumbers + LocalSize - 1 ) / LocalSize };
        };

        graph.RegisterPass<SimpleCompute>(
            "SimpleCompute",
            []( FramePassBuilder &b, SimpleCompute& data ) {
                b.CreateBuffer( "SimpleCompute_Results", BufferUsage::SHADER_STORAGE,
                    MKT_SIZEOF( UInt32 ), data.LocalSize, ResourceUsageType::RESOURCE_USAGE_STREAMING );

                b.UseShader( "Resources/Shaders/slang/BasicCompute_Comp.slang", ShaderStage::COMPUTE );
                b.CreatePipeline( "SimpleCompute_Pipeline", ComputePipelineDescription{} );

                b.Write( "SimpleCompute_Results", FrameResourceState::UnorderedAccessView );
            },
            []( CommandContext &ctx, FrameGraphBlackboard& blackboard ) -> void {

                ctx.BindBuffer( ResourceGroup::BufferViews, "SimpleCompute_Results", ResourceSlot::Slot_0 );

                const auto & data{ blackboard.Get<SimpleCompute>() };
                ctx.BindPipeline( "SimpleCompute_Pipeline" );
                ctx.Dispatch( data.GroupCount, 1, 1 );
            }, FramePassNodeType::COMPUTE );
    }

    auto DebugPasses::RegisterInfiniteGrid( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass(
            "InfiniteGrid",
            [this]( FramePassBuilder &b ) {
                MKT_BEGIN_PROFILER_NAMED();

                b.Create<Texture>( "InfiniteGrid_ColorTarget", m_Resolution, TextureFormat::RGBA8_UNORM, TextureUsage::COLOR );

                b.UseShader( "Resources/Shaders/slang/InfiniteGrid_Vert.slang", ShaderStage::VERTEX );
                b.UseShader( "Resources/Shaders/slang/InfiniteGrid_Frag.slang", ShaderStage::FRAGMENT );

                b.Create<Pipeline>( "InfiniteGrid_Pipeline", 
                    GraphicsPipelineDescription{
                        .DepthTest{ true },
                        .DepthWrite{ false },
                        .PipelinePolygonMode{ PolygonMode::LINES },
                        .PrimitiveTopology{ Topology::TRIANGLE_LIST },
                        .VertexAttributesSpec{} } );

                b.Write( "InfiniteGrid_ColorTarget", FrameResourceState::RenderTarget );
                b.Read( "CameraInfoPass_CameraData", FrameResourceState::UniformBuffer );

                b.Use( ResourceGroup::Dynamic, "CameraInfoPass_CameraData", 0 );
            },
            [this]( CommandContext &ctx, FrameGraphBlackboard & ) -> void {
                MKT_BEGIN_PROFILER_NAMED();

                ctx.BindBuffer( ResourceGroup::BufferViews, "CameraInfoPass_CameraData", ResourceSlot::Slot_0 );

                const auto dimensions{ InferDimensions( m_Resolution ) };

                ctx.SetViewport( 0, 0, dimensions.first, dimensions.second );
                ctx.SetScissor( 0, 0, dimensions.first, dimensions.second );

                ctx.SetClearColor( { 1.0f, 1.0f, 1.0f, 1.0f } );

                ctx.SetColorRenderTarget( "InfiniteGrid_ColorTarget" );

                PassRenderInfo renderInfo{
                    .ColorLoadOp{ LoadOp::CLEAR },
                    .DephtLoadOp{ LoadOp::CLEAR },
                };
                ctx.BeginRender( renderInfo );

                ctx.BindPipeline( "InfiniteGrid_Pipeline" );

                ctx.Draw( 4 );

                ctx.EndRender();
            } );
    }

    auto DebugPasses::RegisterHelloCube( FrameGraph &graph ) -> void {

    }

    auto DebugPasses::RegisterHelloTexture( FrameGraph &graph ) -> void {
        struct HelloTextureData {
            TextureHandle TargetTexture{};

            struct PushConstantData {
                Int32 TextureIndex{ GlobalTextures::INVALID_TEXTURE_INDEX };
            } PushConstants{};
        };

        // Example with pass data
        graph.RegisterPass<HelloTextureData>(
            "HelloTexture",
            [this]( FramePassBuilder &b, HelloTextureData& data ) -> void {
                b.CreateTexture( "HelloTexture_ColorTarget", m_Resolution, TextureFormat::RGBA8_UNORM, TextureUsage::COLOR );
                b.CreateTexture( "HelloTexture_DepthTarget", m_Resolution, TextureFormat::D32_FLOAT_S8_UINT, TextureUsage::DEPTH );

                b.UseShader( "Resources/Shaders/slang/HelloTexture_Vert.slang", ShaderStage::VERTEX );
                b.UseShader( "Resources/Shaders/slang/HelloTexture_Frag.slang", ShaderStage::FRAGMENT );

                b.CreatePipeline( "HelloTexture_Pipeline", GraphicsPipelineDescription{
                                        .PrimitiveTopology{ Topology::TRIANGLE_STRIP },
                                        .VertexAttributesSpec{} } );

                b.Write( "HelloTexture_ColorTarget", FrameResourceState::RenderTarget );
                b.Write( "HelloTexture_DepthTarget", FrameResourceState::DepthWrite );

                // Load example texture
                data.TargetTexture = AssetsService::Get()->LoadAsset<Texture>( 
                    Path{ "Resources/Models/1 - Box texture/CatStare.png" } 
                );
            },
            [this]( CommandContext &ctx, FrameGraphBlackboard & blackboard) -> void {

                auto& data{ blackboard.Get<HelloTextureData>() };
                data.PushConstants.TextureIndex = ctx.BindImageSampler( ResourceGroup::UnboundedImageViews, "Texture2D_List", data.TargetTexture );

                ctx.PushConstants( std::addressof( data.PushConstants ), sizeof( data.PushConstants ) );

                const auto flip{ false };
                const auto dimensions{ InferDimensions( m_Resolution ) };
                ctx.SetViewport( 0, 0, dimensions.first, dimensions.second, flip );
                ctx.SetScissor( 0, 0, dimensions.first, dimensions.second );

                ctx.SetClearColor( { 0.5f, 0.2f, 0.3f, 1.0f } );
                ctx.SetColorRenderTarget( "HelloTexture_ColorTarget" );
                ctx.SetDepthRenderTarget( "HelloTexture_DepthTarget" );
                ctx.BeginRender();

                ctx.BindPipeline( "HelloTexture_Pipeline" );

                ctx.Draw( 4 );

                ctx.EndRender();
            } );
    }

    auto DebugPasses::RegisterDebugViewsPass( FrameGraph &graph ) -> void {
        graph.RegisterPass(
            "DebugViewsForDebugPasses",
            []( FramePassBuilder &b ) -> void {
                MKT_BEGIN_PROFILER_NAMED();
                b.Read( "Wireframe_ColorTarget", FrameResourceState::ShaderRead_GraphicsPipeline );
                b.Read( "InfiniteGrid_ColorTarget", FrameResourceState::ShaderRead_GraphicsPipeline );
            },
            []( CommandContext &, FrameGraphBlackboard & ) -> void {
                MKT_BEGIN_PROFILER_NAMED();
            } );
    }
}