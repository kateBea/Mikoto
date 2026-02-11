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

#include <Core/Profiler.hh>

#include <Scene/Scene.hh>
#include <Scene/Component.hh>

#include <Math/Math.hh>

#include <Library/Random/Random.hh>
#include <Library/String/String.hh>

#include <Renderer/Core/FramePassResource.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Passes/ShaderParameteres.hh>
#include <Renderer/Passes/PostEffectsPasses.hh>

namespace Mikoto {

    PostEffectsPass::PostEffectsPass( RenderResolution resolution )
        : m_Resolution{ resolution }
    {

    }

    auto PostEffectsPass::SetScene( const Scene *scene ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_Scene = scene;
    }

    auto PostEffectsPass::SetCamera( const Camera* camera ) -> void {
        m_InfiniteGridParameters.CameraPos = Vec4F{ camera->GetPosition(), 1.0f };
        m_InfiniteGridParameters.CameraView = camera->GetViewMatrix();
        m_InfiniteGridParameters.CameraProj = camera->GetProjection();

        m_SSAOParameters.Projection = camera->GetProjection();

        m_Camera = camera;
    }

    auto PostEffectsPass::RegisterPasses( FrameGraph &graph, GpuDevice* device) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        RegisterTextRender( graph, device );
        RegisterInfiniteGrid( graph );
        RegisterSSAO( graph );
        RegisterBloom( graph );
    }

    auto PostEffectsPass::RegisterTextRender( FrameGraph &graph, GpuDevice* device) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Vertex buffer and index buffer for the quads
        BufferDescription vertexDesc{};
        vertexDesc.WithData( reinterpret_cast<Byte*>( VERTICES.data() ) )
                .WithUsage( BufferUsage::VERTEX )
                .WithSizeBytes( InferSize<FontVertex>( VERTICES.size() ) )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_STATIC );
        m_TextVertexBuffer = device->CreateBuffer( vertexDesc );

        BufferDescription indexDesc{};
        indexDesc.WithData( reinterpret_cast<Byte*>( INDICES.data() ) )
                .WithUsage( BufferUsage::INDEX )
                .WithSizeBytes( InferSize<UInt32>( INDICES.size() ) )
                .WithBufferDataType( BufferDataType::BUFFER_DATA_UINT32 )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_STATIC );
        m_TextIndexBuffer = device->CreateBuffer( indexDesc );

        graph.RegisterPass(
                "3DTextRenderingPass",
                []( FramePassBuilder &b ) {
                    MKT_BEGIN_PROFILER_NAMED();

                    b.Create<Buffer>( "TextRenderPass_TextRenderParams", BufferUsage::SHADER_STORAGE, sizeof(TextRenderParams), MAX_GLYPHS )
                        .Create<Buffer>( "TextRenderPass_FontParams", BufferUsage::UNIFORM, sizeof(TextParamsUBO), 1 );

                    b.UseShader( "Resources/Shaders/vulkan-spirv/Text_Vert.sprv", ShaderStage::VERTEX );
                    b.UseShader( "Resources/Shaders/vulkan-spirv/Text_Frag.sprv", ShaderStage::FRAGMENT );

                    GraphicsPipelineDescription graphicsDesc{};

                    graphicsDesc.DepthTest = true;
                    graphicsDesc.DepthWrite = true;
                    graphicsDesc.AlphaBlending = true;

                    // we're providing none as the shader expects none
                    const BufferLayout layout{
                        { ShaderDataType::FLOAT3_TYPE, "a_Position" },
                        { ShaderDataType::UINT_TYPE, "a_TexCoordIndex" },
                    };
                    graphicsDesc.VertexAttributesSpec = { AttributesSpec{
                        .DefaultVertexLayout{ layout },
                        .InputRateSpec{ .BindingIndex = { 0 }, .AttributeRate{ InputRate::PER_VERTEX } }
                    } };
                    graphicsDesc.PrimitiveTopology = Topology::TRIANGLE_LIST;
                    b.Create<Pipeline>( "TextRenderPass_Pipeline", graphicsDesc );

                    b.Read( "FinalShadingPass_DepthTarget", FrameResourceState::DepthWrite )
                    .Read( "FinalShadingPass_ColorTarget", FrameResourceState::RenderTarget )
                    .Read( "FinalCompositionPass_CameraInfo", FrameResourceState::UniformBuffer );

                    b.Write( "TextRenderPass_FontParams", FrameResourceState::UniformBuffer );
                    b.Write( "TextRenderPass_TextRenderParams", FrameResourceState::UniformBuffer );

                    b.Use( SRGType::SRG_PerPass, "TextRenderPass_FontParams", 0 );
                    b.Use( SRGType::SRG_PerPass, "TextRenderPass_TextRenderParams", 1 );
                    b.Use( SRGType::SRG_Textures);
                },
                [this]( CommandContext &ctx, FrameGraphBlackboard & ) -> void {
                    MKT_BEGIN_PROFILER_NAMED();

                    MKT_ASSERT( m_Scene != nullptr, "Scene cannot be NULL" );

                    ctx.SetColorRenderTarget( "FinalShadingPass_ColorTarget" );
                    ctx.SetDepthRenderTarget( "FinalShadingPass_DepthTarget" );

                    PassRenderInfo renderInfo{
                        .ColorLoadOp{ LoadOp::LOAD },
                        .DephtLoadOp{ LoadOp::LOAD }
                    };

                    ctx.BeginRender( renderInfo );
                    ctx.BindPipeline( "TextRenderPass_Pipeline" );

                    const auto dimensions{ InferDimensions( m_Resolution ) };

                    ctx.SetViewport( 0, 0, dimensions.first, dimensions.second );
                    ctx.SetScissor( 0, 0, dimensions.first, dimensions.second );

                    TraverseTextList( ctx );
                    SetupRenderParams( ctx );

                    DrawIndexedState drawIndexedState{};

                    drawIndexedState.IndexBuffer = m_TextIndexBuffer;
                    drawIndexedState.VertexBuffers.emplace_back( m_TextVertexBuffer, 0 );

                    drawIndexedState.IndicesCount = m_TextIndexBuffer->GetCount();
                    drawIndexedState.InstancesCount = m_TextRenderParams.size();

                    ctx.DrawIndexed( drawIndexedState );

                    ctx.EndRender();
                } );
    }

    auto PostEffectsPass::RegisterObjectOutline( FrameGraph& graph, GpuDevice* device ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass(
                "ObjectOutline",
                []( FramePassBuilder &b ) {
                    MKT_BEGIN_PROFILER_NAMED();
                },
                []( CommandContext &ctx, FrameGraphBlackboard & ) -> void {
                    MKT_BEGIN_PROFILER_NAMED();

                } );
    }

    auto PostEffectsPass::SetMeshCulling( MeshCulling &culling ) -> void {
        m_MeshCullingPass = std::addressof( culling );
    }

    auto PostEffectsPass::RegisterSSAO( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Generate noise
        constexpr UInt32 SSAO_NOISE_DIM{ 8 };
        for ( UInt32 i{}; i < SSAO_NOISE_DIM * SSAO_NOISE_DIM; i++ ) {
            // rotate around z-axis (in tangent space)
            m_SSONoise.emplace_back( GetRandomReal( 0.0, 1.0 ) * 2.0f - 1.0f, GetRandomReal( 0.0, 1.0 ) * 2.0f - 1.0f, 0.0f, 0.0f );
        }

        // Generate samples
        for ( UInt32 i{}; i < m_SSAOParameters.Samples.size(); ++i ) {
            Vec4F sample( GetRandomReal( 0.0, 1.0 ) * 2.0 - 1.0, GetRandomReal( 0.0, 1.0 ) * 2.0 - 1.0, GetRandomReal( 0.0, 1.0 ), 1.0 );
            sample = glm::normalize( sample );
            sample *= GetRandomReal( 0.0, 1.0 );
            float scale{ static_cast<float>( i ) / 64.0f };

            // scale samples s.t. they're more aligned to center of kernel
            scale = Math::Lerp( 0.1f, 1.0f, scale * scale );
            sample *= scale;
            m_SSAOParameters.Samples[i] = sample;
        }

        graph.RegisterPass(
                "SSAO",
                [this, SSAO_NOISE_DIM]( FramePassBuilder& b ) {
                    MKT_BEGIN_PROFILER_NAMED();

                    b.Create<Buffer>( "SSAO_Parameters", BufferUsage::UNIFORM, sizeof( m_SSAOParameters ), 1 );

                    b.Create<Texture>( "SSAO_ColorTarget", m_Resolution, TextureFormat::R8_UNORM, TextureUsage::COLOR );
                    b.Create<Texture>( "SSAO_NoiseTexture", SSAO_NOISE_DIM, SSAO_NOISE_DIM,
                                       TextureFormat::RGBA32_FLOAT, m_SSONoise.data(), m_SSONoise.size() * sizeof( Vec4F ) );

                    GraphicsPipelineDescription graphicsDesc{
                        .DepthTest{ false },
                        .DepthWrite{ false },
                        .AlphaBlending{ false },
                        .ColorAttachmentFormats{ TextureFormat::R8_UNORM }
                    };

                    b.UseShader( "Resources/Shaders/vulkan-spirv/SSAO_Vert.sprv", ShaderStage::VERTEX )
                            .UseShader( "Resources/Shaders/vulkan-spirv/SSAO_Frag.sprv", ShaderStage::FRAGMENT )
                            .Create<Pipeline>( "SSAO_Pipeline", graphicsDesc );

                    b.Write( "SSAO_ColorTarget", FrameResourceState::RenderTarget );
                    b.Write( "SSAO_NoiseTexture", FrameResourceState::ShaderRead_GraphicsPipeline );

                    b.Read( "SSAO_Parameters", FrameResourceState::UniformBuffer );
                    b.Read( "GBuffer_Position", FrameResourceState::ShaderRead_GraphicsPipeline );
                    b.Read( "GBuffer_Normal", FrameResourceState::ShaderRead_GraphicsPipeline );

                    b.Use( SRGType::SRG_PerPass, "SSAO_Parameters", 0 );
                },
                [this]( CommandContext& ctx, FrameGraphBlackboard& ) -> void {
                    MKT_BEGIN_PROFILER_NAMED();

                    if ( m_Sampler.IsEmpty() || m_SamplerNoise.IsEmpty() ) {
                        m_Sampler = ctx.CreateSampler( SamplerDescription{} );

                        m_SamplerNoise = ctx.CreateSampler( SamplerDescription{
                                .MipLevels{ 0 },
                                .MinFilter{ SamplerFilter::FILTER_NEAREST },
                                .MagFilter{ SamplerFilter::FILTER_NEAREST },

                                .WrapU{ SamplerWrapMode::WRAP_REPEAT },
                                .WrapV{ SamplerWrapMode::WRAP_REPEAT },
                                .WrapW{ SamplerWrapMode::WRAP_REPEAT },
                        } );
                    }

                    ctx.BindPipeline( "SSAO_Pipeline" );

                    ctx.BindImage( "GBuffer_Position", m_Sampler, 1 );
                    ctx.BindImage( "GBuffer_Normal", m_Sampler, 2 );
                    ctx.BindImage( "SSAO_NoiseTexture", m_SamplerNoise, 3 );

                    ctx.UploadBuffer( "SSAO_Parameters", m_SSAOParameters );

                    ctx.SetColorRenderTarget( "SSAO_ColorTarget" );

                    ctx.BeginRender();

                    const auto dimensions{ InferDimensions( m_Resolution ) };
                    ctx.SetViewport( 0, 0, dimensions.first, dimensions.second );
                    ctx.SetScissor( 0, 0, dimensions.first, dimensions.second );

                    ctx.Draw( 3 );

                    ctx.EndRender();
                } );

        graph.RegisterPass(
                "SSAOBlur",
                [this]( FramePassBuilder& b ) {
                    MKT_BEGIN_PROFILER_NAMED();

                    b.Create<Texture>( "SSAOBlur_ColorTarget", m_Resolution, TextureFormat::R8_UNORM, TextureUsage::COLOR );
                    b.Create<Texture>( "SSAOBlur_DepthTarget", m_Resolution, TextureFormat::D32_FLOAT_S8_UINT, TextureUsage::DEPTH );

                    GraphicsPipelineDescription graphicsDesc{
                        .DepthTest{ false },
                        .DepthWrite{ false },
                        .AlphaBlending{ false },
                        .ColorAttachmentFormats{ TextureFormat::R8_UNORM },
                    };

                    b.UseShader( "Resources/Shaders/vulkan-spirv/SSAOBlur_Vert.sprv", ShaderStage::VERTEX )
                            .UseShader( "Resources/Shaders/vulkan-spirv/SSAOBlur_Frag.sprv", ShaderStage::FRAGMENT )
                            .Create<Pipeline>( "SSAOBlur_Pipeline", graphicsDesc );

                    b.Write( "SSAOBlur_ColorTarget", FrameResourceState::RenderTarget );
                    b.Write( "SSAOBlur_DepthTarget", FrameResourceState::DepthWrite );

                    b.Read( "SSAO_ColorTarget", FrameResourceState::ShaderRead_GraphicsPipeline );
                },
                [this]( CommandContext& ctx, FrameGraphBlackboard& ) -> void {
                    MKT_BEGIN_PROFILER_NAMED();

                    if ( m_Sampler.IsEmpty() ) {
                        m_Sampler = ctx.CreateSampler( SamplerDescription{} );
                    }

                    ctx.BindPipeline( "SSAOBlur_Pipeline" );

                    //ctx.BindImage( "SSAO_ColorTarget", m_Sampler, 0 );

                    ctx.SetColorRenderTarget( "SSAOBlur_ColorTarget" );
                    ctx.SetDepthRenderTarget( "SSAOBlur_DepthTarget" );

                    ctx.BeginRender();

                    const auto dimensions{ InferDimensions( m_Resolution ) };
                    ctx.SetViewport( 0, 0, dimensions.first, dimensions.second );
                    ctx.SetScissor( 0, 0, dimensions.first, dimensions.second );

                    ctx.Draw( 3 );

                    ctx.EndRender();
                } );
    }

    auto PostEffectsPass::RegisterBloom( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass(
                "Bloom",
                []( FramePassBuilder &b ) {
                    MKT_BEGIN_PROFILER_NAMED();
                },
                []( CommandContext &ctx, FrameGraphBlackboard & ) -> void {
                    MKT_BEGIN_PROFILER_NAMED();

                } );
    }

    auto PostEffectsPass::RegisterInfiniteGrid( FrameGraph& graph) -> void {
        MKT_BEGIN_PROFILER_NAMED();


        graph.RegisterPass(
                "InfiniteGrid",
                [this]( FramePassBuilder &b ) {
                    MKT_BEGIN_PROFILER_NAMED();

                    b.Create<Texture>( "InfiniteGrid_ColorTarget", m_Resolution, TextureFormat::RGBA8_UNORM, TextureUsage::COLOR );

                    b.UseShader( "Resources/Shaders/vulkan-spirv/InfiniteGrid_Vert.sprv", ShaderStage::VERTEX );
                    b.UseShader( "Resources/Shaders/vulkan-spirv/InfiniteGrid_Frag.sprv", ShaderStage::FRAGMENT );

                    b.Create<Pipeline>( "InfiniteGrid_Pipeline", GraphicsPipelineDescription{
                        .DepthTest{ true },
                        .DepthWrite{ false },
                        .PipelinePolygonMode{ PolygonMode::LINES },
                        .PrimitiveTopology{ Topology::TRIANGLE_LIST },
                        .VertexAttributesSpec{} } );

                    b.Write( "InfiniteGrid_ColorTarget", FrameResourceState::RenderTarget );

                    b.Read( "FinalShadingPass_ColorTarget", FrameResourceState::RenderTarget );
                    b.Read( "FinalShadingPass_DepthTarget", FrameResourceState::DepthRead );

                    b.Read( "CameraInfoPass_CameraData", FrameResourceState::UnorderedAccess );

                    b.Use( SRGType::SRG_PerPass, "CameraInfoPass_CameraData", 0 );
                },
                [this]( CommandContext &ctx, FrameGraphBlackboard & ) -> void {
                    MKT_BEGIN_PROFILER_NAMED();

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

    auto PostEffectsPass::TraverseTextList( CommandContext &ctx ) -> void {
        // Clear text data as we refill it every frame
        m_TextRenderParams.clear();

        auto& registry{ m_Scene->GetRegistry() };
        auto renderables{ registry.view<TransformComponent, TextComponent>() };

        for (auto& entity : renderables) {
            auto& transform{ registry.get<TransformComponent>( entity ) };
            auto& textComponent{ registry.get<TextComponent>( entity ) };

            if (!textComponent.HasFont()) {
                continue;
            }

            SetupTextForRender( ctx, transform, textComponent );
        }

        ctx.UploadBuffer( "TextRenderPass_TextRenderParams", m_TextRenderParams.data(), m_TextRenderParams.size() * sizeof( TextRenderParams ) );
    }

    auto PostEffectsPass::SetupRenderParams( CommandContext &context ) -> void {
        m_TextRenderUBO.OutlineWidth = 0.0f;
        context.UploadBuffer( "TextRenderPass_FontParams", std::addressof( m_TextRenderUBO ), sizeof( m_TextRenderUBO ) );
    }

    auto PostEffectsPass::SetupTextForRender( CommandContext& context, const TransformComponent& transformComponent, const TextComponent& textComponent) -> void {
        using namespace StringUtils;

        FontHandle font{ textComponent.GetFontHandle() };

        const float fontSize{ textComponent.GetSize() };
        const glm::vec4& color{ textComponent.GetColor() };
        const Camera* camera{ textComponent.GetCamera() };
        const glm::vec4& position{ transformComponent.GetTranslation(), 1.0f };

        double xPos{ position.x };
        double yPos{ position.y };
        double scale{ fontSize / font->GetSize() };

        double lineHeight{ font->GetMaxHeight() * scale };

        for ( const auto& character : textComponent.GetContents() ) {
            if ( IsLineFeed(character) ) {
                xPos = position.x;
                yPos += lineHeight;
                continue;
            }

            double advance{ 0 };

            if ( font->HasGlyph(character)) {
                const FontGlyph& glyph{ font->GetGlyph( static_cast<UInt32>( character ) ) };

                if (!IsSpace(character)) {
                    // Quad Coordinates
                    double x0{ xPos + glyph.m_PlaneBounds.x * fontSize };
                    double y0{ yPos - glyph.m_PlaneBounds.y * fontSize };

                    // UV Coordinates
                    TextureHandle atlas{ font->GetAtlas() };
                    double s0{ glyph.m_AtlasBounds.x / atlas->GetWidth() };
                    double t0{ glyph.m_AtlasBounds.w / atlas->GetHeight() };
                    double s1{ glyph.m_AtlasBounds.z / atlas->GetWidth() };
                    double t1{ glyph.m_AtlasBounds.y / atlas->GetHeight() };

                    TextRenderParams fontParams{
                        .Model{ textComponent.IsWorldText() ? transformComponent.GetTransform() : Mat4F{ 1.0f } },
                        .Position{ x0, y0 + std::round( ( font->GetMaxHeight() * scale ) ) - ( glyph.m_Height * scale ), position.z, position.w },
                        .Size{ glyph.m_Width * scale, glyph.m_Height * scale, 0.0f, 0.0f },
                        .Color{ color },
                        .TexCoords{ { s0, t0 }, { s1, t0 }, { s1, t1 }, { s0, t1 } },
                        .TexIndex{ static_cast<UInt32>( context.PushTexture( atlas ) ) }
                    };

                    Mat4F view{};
                    Mat4F projection{};

                    if (!textComponent.IsWorldText()) {
                        if (camera) {
                            view = camera->GetProjection();
                            projection = camera->GetViewMatrix();
                        } else {
                            const auto dimension{ InferDimensions( m_Resolution ) };
                            projection = glm::ortho(0.0f, dimension.first,dimension.second, 0.0f,-1.0f, 1.0f);
                            view = glm::mat4{ 1.0f };
                        }
                    } else {
                        view = m_Camera->GetProjection();
                        projection = m_Camera->GetViewMatrix();
                    }

                    fontParams.Proj = view;
                    fontParams.View = projection;

                    m_TextRenderParams.emplace_back( fontParams );
                }

                advance = glyph.m_AdvanceX * fontSize;
            } else {
                // If the character does not exist just insert space
                // equal to Space character.
                const FontGlyph& glyph{ font->GetGlyph( static_cast<UInt32>( ' ' ) ) };
                advance = glyph.m_AdvanceX * fontSize;
            }

            xPos += advance;
        }
    }
}