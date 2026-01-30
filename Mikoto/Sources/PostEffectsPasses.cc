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

    auto PostEffectsPass::RegisterPasses( FrameGraph &graph, GpuDevice* device) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        RegisterTextRender( graph, device );
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
                [this]( FramePassBuilder &b ) {
                    MKT_BEGIN_PROFILER_NAMED();

                    b.Create<Buffer>( "TextRenderPass_TextRenderParams", BufferUsage::SSBO, sizeof(TextRenderParams) * MAX_STRING )
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

                    b.Read( "FinalShadingPass_DepthTarget", FrameResourceState::DepthRead )
                        .Read( "FinalShadingPass_ColorTarget", FrameResourceState::RenderTarget )
                        .Read( "FinalCompositionPass_CameraInfo", FrameResourceState::UniformBuffer );

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

    auto PostEffectsPass::TraverseTextList( CommandContext &commandList ) -> void {
        // Clear text data as we refill it every frame
        m_TextRenderParams.clear();

        auto& registry{ m_Scene->GetRegistry() };
        auto renderables{ registry.view<TagComponent, TransformComponent, TextComponent>() };

        for (auto& entity : renderables) {
            auto& tag{ registry.get<TagComponent>( entity ) };
            auto& transform{ registry.get<TransformComponent>( entity ) };
            auto& textComponent{ registry.get<TextComponent>( entity ) };

            if (!textComponent.HasFont()) {
                continue;
            }

            const float textSize{ textComponent.GetSize() };
            const glm::vec4& color{ textComponent.GetColor() };
            const glm::vec4& position{ transform.GetTranslation(), 1.0f };
            const Camera* textCamera{ textComponent.GetCamera() };

            SetupTextForRender(textComponent.GetFontHandle(), textCamera, position, textComponent.GetContents(), textSize, color, commandList );
        }

        commandList.UploadBuffer( "TextRenderPass_TextRenderParams", m_TextRenderParams.data(), m_TextRenderParams.size() * sizeof( TextRenderParams ) );
    }

    auto PostEffectsPass::SetupRenderParams( CommandContext &context ) -> void {
        m_TextRenderUBO.OutlineWidth = 0.0f;
        context.UploadBuffer( "TextRenderPass_FontParams", std::addressof( m_TextRenderUBO ), sizeof( m_TextRenderUBO ) );
    }

    auto PostEffectsPass::SetupTextForRender( FontHandle font, const Camera *camera, Vec4F position, std::string_view text, double fontSize, Vec4F color, CommandContext &commandList ) -> void {
        using namespace StringUtils;

        double xPos{ position.x };
        double yPos{ position.y };
        double scale{ fontSize / font->GetSize() };

        double lineHeight{ font->GetMaxHeight() * scale };

        for ( const auto& character : text ) {
            if ( IsLineFeed(character) ) {
                xPos = position.x;
                yPos -= lineHeight;
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
                        .Position{ x0, y0 + std::round( ( font->GetMaxHeight() * scale ) ) - ( glyph.m_Height * scale ), position.z, position.w },
                        .Size{ glyph.m_Width * scale, glyph.m_Height * scale, 0.0f, 0.0f },
                        .Color{ color },
                        .TexCoords{ { s0, t0 }, { s1, t0 }, { s1, t1 }, { s0, t1 } },
                        .TexIndex{ static_cast<UInt32>( commandList.PushTexture( atlas ) ) }
                    };

                    if (camera != nullptr) {
                        fontParams.Proj = camera->GetProjection();
                        fontParams.View = camera->GetViewMatrix();
                    } else {
                        fontParams.Proj = glm::ortho(0.0f, 1920.0f,1080.0f, 0.0f,-1.0f, 1.0f);
                        fontParams.View = glm::mat4{ 1.0f };
                    }

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