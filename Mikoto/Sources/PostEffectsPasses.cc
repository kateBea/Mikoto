#include <Scene/Scene.hh>
#include <Scene/Component.hh>
#include <Library/String/String.hh>
#include <Renderer/Core/FramePass.hh>
#include <Renderer/Core/FrameResource.hh>
#include <Renderer/Passes/ShaderRenderParams.hh>
#include <Renderer/Passes/PostEffectsPasses.hh>

namespace Mikoto {
    auto TextRenderPass::Execute( PassCommandList &commandList ) -> void {
        MKT_ASSERT( m_Scene != nullptr, "Scene cannot be NULL" );

        commandList.SetColorRenderTarget( "FinalCompositionPass_ColorTarget" );
        commandList.SetDepthRenderTarget( "FinalCompositionPass_DepthTarget" );

        commandList.BeginRender(this, LoadOp::LOAD, LoadOp::LOAD);
        commandList.BindPipeline( "TextRenderPass_Pipeline" );

        // Set render targets
        commandList.SetViewport( 0, 0, 1920, 1080 );
        commandList.SetScissor( 0, 0, 1920, 1080 );

        commandList.SetBufferBindSlot( SRGType::SRG_PerPass, "TextRenderPass_FontParams", 0 );
        commandList.SetBufferBindSlot( SRGType::SRG_PerPass, "TextRenderPass_TextRenderParams", 1 );

        commandList.BindResourceGroup(SRGType::SRG_Textures);
        commandList.BindResourceGroup(SRGType::SRG_PerPass);

        TraverseTextList(commandList);
        SetupRenderParams(commandList);

        DrawIndexedState drawIndexedState{};

        BufferHandle vertexBuffer{ commandList.GetNamedBuffer("TextRenderPass_FontVertexBuffer") };
        BufferHandle indexBuffer{ commandList.GetNamedBuffer("TextRenderPass_FontIndexBuffer") };

        drawIndexedState.IndexBuffer = indexBuffer;
        drawIndexedState.VertexBuffers.emplace_back( vertexBuffer, 0);

        drawIndexedState.IndicesCount = indexBuffer->GetCount();
        drawIndexedState.InstancesCount = m_TextRenderParams.size();

        commandList.DrawIndexed(drawIndexedState);

        commandList.EndRender();
    }

    auto TextRenderPass::SetScene( Scene *scene ) -> void {
        m_Scene = scene;
    }

    auto TextRenderPass::TraverseTextList( PassCommandList &commandList ) -> void {
        FontHandle testFont{ AssetsService::Get()->LoadAsset<Font>( Path{ "Resources/Fonts/Google_Sans_Code/GoogleSansCode-VariableFont_wght.ttf" } ) };

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
            const glm::vec4 color{ textComponent.GetColor() };
            const glm::vec4 position{ transform.GetTranslation(), 1.0f };
            const Camera* textCamera{ textComponent.GetCamera() };

            SetupTextForRender(textComponent.GetFontHandle(), textCamera, position, textComponent.GetContents(), textSize, color, commandList );
        }

        commandList.FillBuffer( "TextRenderPass_TextRenderParams", m_TextRenderParams.data(), m_TextRenderParams.size() * sizeof( TextRenderParams ) );
    }

    auto TextRenderPass::SetupRenderParams(PassCommandList &commandList) -> void {
        m_TextRenderUBO.OutlineWidth = 0.0f;

        commandList.FillBuffer( "TextRenderPass_FontParams", std::addressof( m_TextRenderUBO ), sizeof( m_TextRenderUBO ) );
    }

    auto TextRenderPass::SetupTextForRender( FontHandle font, const Camera* camera, Vec4F position, std::string_view text, double fontSize, Vec4F color, PassCommandList& commandList ) -> void {
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
                FontGlyph& glyph{ font->GetGlyph( static_cast<UInt32>( character ) ) };

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
                FontGlyph& glyph{ font->GetGlyph( static_cast<UInt32>( ' ' ) ) };
                advance = glyph.m_AdvanceX * fontSize;
            }

            xPos += advance;
        }
    }

    auto TextRenderPass::Setup( FrameGraphBuilder &builder ) -> void {
        // Create resources it needs
        PipelineDescription pipelineDesc{};

        pipelineDesc.AddShader( "./Resources/Shaders/vulkan-spirv/Text_Vert.sprv", ShaderStage::VERTEX_STAGE );
        pipelineDesc.AddShader( "./Resources/Shaders/vulkan-spirv/Text_Frag.sprv", ShaderStage::FRAGMENT_STAGE );

        // Configure pipeline stage
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
            .DefaultVertexLayout { layout },
            .InputRateSpec{ .BindingIndex = { 0 }, .AttributeRate { InputRate::PER_VERTEX } }
        } };
        graphicsDesc.PrimitiveTopology = Topology::TRIANGLE_LIST;

        pipelineDesc.Description = graphicsDesc;

        pipelineDesc.ColorRenderTargets.emplace_back( "FinalCompositionPass_ColorTarget" );
        pipelineDesc.DepthRenderTargets = "FinalCompositionPass_DepthTarget";

        builder.CreateNamedPipeline( "TextRenderPass_Pipeline", pipelineDesc );

        BufferDescription fontRenderParams{};
        fontRenderParams.WithData( nullptr )
                .WithUsage( BufferUsage::BUFFER_USAGE_SHADER_STORAGE )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_DYNAMIC )
                .WithSizeBytes( sizeof(TextRenderParams) * MAX_STRING );
        builder.CreateNamedBuffer( "TextRenderPass_TextRenderParams", fontRenderParams );

        BufferDescription fontParamsUBO{};
        fontParamsUBO.WithData( nullptr )
                .WithUsage( BufferUsage::BUFFER_USAGE_UNIFORM )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_DYNAMIC )
                .ForElement( sizeof(TextParamsUBO), 1 );
        builder.CreateNamedBuffer( "TextRenderPass_FontParams", fontParamsUBO );

        // Vertex buffer and index buffer for the quads
        BufferDescription vertexDesc{};
        vertexDesc.WithData( reinterpret_cast<Byte*>( VERTICES.data() ) )
                .WithUsage( BufferUsage::BUFFER_USAGE_VERTEX )
                .WithSizeBytes( InferSize<FontVertex>( VERTICES.size() ) )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_STATIC );
        builder.CreateNamedBuffer( "TextRenderPass_FontVertexBuffer", vertexDesc );

        BufferDescription indexDesc{};
        indexDesc.WithData( reinterpret_cast<Byte*>( INDICES.data() ) )
                .WithUsage( BufferUsage::BUFFER_USAGE_INDEX )
                .WithSizeBytes( InferSize<UInt32>( INDICES.size() ) )
                .WithBufferDataType( BufferDataType::BUFFER_DATA_UINT32 )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_STATIC );
        builder.CreateNamedBuffer( "TextRenderPass_FontIndexBuffer", indexDesc );

        builder.ReadTexture( this, "FinalCompositionPass_ColorTarget" );
        builder.ReadTexture( this, "FinalCompositionPass_DepthTarget" );

        builder.WriteBuffer( this, "TextRenderPass_FontVertexBuffer" );
        builder.WriteBuffer( this, "TextRenderPass_FontIndexBuffer" );
        builder.WriteBuffer( this, "TextRenderPass_FontParams" );
        builder.WriteBuffer( this, "TextRenderPass_TextRenderParams" );
    }
}