//
// Created by kate on 11/25/25.
//

#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/GraphicsContext.hh>

#include <Renderer/Vulkan/VulkanGraphicsContext.hh>

namespace Mikoto {
    auto PipelineDescription::AddShader( std::string_view path, ShaderStage stage ) -> void {
        Shaders[stage] = path;
    }

    // Should probably be created by the render service instead
    auto GraphicsContext::Create( GpuDevice* device ) -> Unique<GraphicsContext> {
        Unique<GraphicsContext> result{ nullptr };

        switch ( device->GetApi() ) {
            case GraphicsAPI::VULKAN_API:
                result = CreateScope<VulkanGraphicsContext>( device );
                break;
            default:
                MKT_CORE_LOGGER_CRITICAL( "RenderService::CreateRendererBackend - Error Unsupported renderer API!" );
                break;
        }

        return result;
    }

    PassCommandList::PassCommandList( GraphicsContext *context, FrameBlackboard* blackboard )
        : m_Context{ context }, m_Blackboard{ blackboard } {
        MKT_ASSERT( m_Blackboard, "Tried to create PassCommandList with NULL blackboard" );
    }

    auto PassCommandList::BeginRender() -> void {
        MKT_ASSERT( m_Context, "No valid context for this pass command list" );

        m_Context->BeginRender( m_RenderInfo );
    }

    auto PassCommandList::EndRender() -> void {
        MKT_ASSERT( m_Context, "No valid context for this pass command list" );

        m_Context->EndRender( m_RenderInfo );
    }

    auto PassCommandList::SetColorRenderTarget( std::string_view color ) -> void {
        TextureHandle colorHandle{ m_Blackboard->GetTexture(color) };
        MKT_ASSERT( !colorHandle.IsEmpty(), "PassCommandList::SetColorRenderTarget - Color render target must not be empty" );

        m_RenderInfo.ColorRenderTargets.emplace_back( colorHandle );
    }

    auto PassCommandList::SetDepthRenderTarget( std::string_view depth ) -> void {
        TextureHandle depthTexture{ m_Blackboard->GetTexture(depth) };
        MKT_ASSERT( !depthTexture.IsEmpty(), "PassCommandList::SetDepthRenderTarget - Depth render target must not be empty" );

        m_RenderInfo.DepthRenderTarget = depthTexture;
    }

    auto PassCommandList::BindTexture( TextureHandle texture ) const -> void {
        MKT_ASSERT( m_Context, "No valid context for this pass command list" );

        if (texture.IsEmpty()) {
            return;
        }

        m_Context->RegisterImage(texture);
    }

    auto PassCommandList::BindTexture( std::string_view texture ) const -> void {
        // Find the texture by its name in the list of named resource
        TextureHandle textureHandle{ /* TODO */ };

        // Call bind texture with default sampler
        BindTexture(textureHandle);
    }

    auto PassCommandList::BindTexture( TextureHandle texture, SamplerHandle sampler ) const -> void {
        MKT_ASSERT( m_Context, "No valid context for this pass command list" );

        if (texture.IsEmpty() || sampler.IsEmpty()) {
            return;
        }

        m_Context->RegisterImage(texture, sampler );
    }

    auto PassCommandList::BindStorageBuffer( SRGType type, std::string_view buffer, UInt32 index ) -> void {
        MKT_ASSERT( m_Context, "No valid context for this pass command list" );
        MKT_ASSERT( m_Blackboard, "No valid blackboard for this pass command list" );

        m_ShaderResources.m_SRGs[type].SetStorageBuffer( buffer, index );
    }

    auto PassCommandList::Draw( UInt32 vertexCount, UInt32 instanceCount, UInt32 firstVertex, UInt32 firstInstance ) -> void {
        MKT_ASSERT( m_Context, "No valid context for this pass command list" );

        m_Context->Draw(vertexCount, instanceCount, firstVertex, firstInstance);
    }

    auto PassCommandList::BeginCompute() -> void {
    }
    auto PassCommandList::EndCompute() -> void {
    }

    auto PassCommandList::SetViewport( Int32 x, Int32 y, Int32 width, Int32 height ) -> void {
        MKT_ASSERT( m_Context, "No valid context for this pass command list" );

        m_Viewport.X = x;
        m_Viewport.Y = y;
        m_Viewport.Width = width;
        m_Viewport.Height = height;

        m_Context->SetViewport( m_Viewport );
    }

    auto PassCommandList::SetScissor( Int32 x, Int32 y, Int32 width, Int32 height ) -> void {
        MKT_ASSERT( m_Context, "No valid context for this pass command list" );

        m_Scissor.X = x;
        m_Scissor.Y = y;
        m_Scissor.Width = width;
        m_Scissor.Height = height;

        m_Context->SetScissor( m_Scissor );
    }

    auto PassCommandList::BindPipeline( std::string_view pipelineName ) -> void {
        PipelineHandle piepline{ m_Blackboard->GetPipeline(pipelineName) };
        MKT_ASSERT( !piepline.IsEmpty(), "PassCommandList::SetDepthRenderTarget - PipelineHandle must not be empty" );

        m_Context->BindPipeline( piepline, m_ShaderResources );
    }

    auto PassCommandList::BindVertexBuffer( BufferHandle vertices ) -> void {
    }
    auto PassCommandList::BindIndexBuffer( BufferHandle indices ) -> void {
    }
    auto PassCommandList::SubmitDraw() -> void {
    }

    auto PassCommandList::Dispatch( UInt32 invX, UInt32 invY, UInt32 invZ ) -> void {
        m_Context->Dispatch( invX, invY, invZ );
    }

    auto PassCommandList::SetClearColor( const Vec4F &color ) -> void {
        m_RenderInfo.ClearColor = color;
    }
}// namespace Mikoto