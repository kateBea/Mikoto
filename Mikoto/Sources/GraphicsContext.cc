//
// Created by kate on 11/25/25.
//

#include <Renderer/Core/GraphicsContext.hh>
#include <Renderer/Vulkan/VulkanGraphicsContext.hh>

namespace Mikoto {
    auto PipelineDescription::AddShader( std::string_view path ) -> void {
        Shaders.emplace_back( path );
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
    auto PassCommandList::Begin() -> void {
    }
    auto PassCommandList::End() -> void {
    }

    PassCommandList::PassCommandList( GraphicsContext *context )
        : m_Context{ context }
    {}

    auto PassCommandList::BeginRender() -> void {
    }
    auto PassCommandList::EndRender() -> void {
    }
    auto PassCommandList::SetColorRenderTarget( TextureHandle color ) -> void {
    }
    auto PassCommandList::SetColorRenderTarget( std::string_view color ) -> void {
    }
    auto PassCommandList::SetDepthRenderTarget( TextureHandle color ) -> void {
    }
    auto PassCommandList::SetDepthRenderTarget( std::string_view color ) -> void {
    }

    auto PassCommandList::BindTexture( TextureHandle texture ) const -> void {
        MKT_ASSERT( m_Context, "No valid context for this pass command list" );
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
        m_Context->RegisterImage(texture, sampler );
    }

    auto PassCommandList::BindBuffer( std::string_view buffer, UInt32 set, UInt32 index ) -> void {
    }

    auto PassCommandList::BeginCompute() -> void {
    }
    auto PassCommandList::EndCompute() -> void {
    }

    auto PassCommandList::SetViewport( Int32 x, Int32 y, Int32 width, Int32 height ) -> void {
        m_Viewport.X = x;
        m_Viewport.Y = y;
        m_Viewport.Width = width;
        m_Viewport.Height = height;
    }

    auto PassCommandList::SetScissor( Int32 x, Int32 y, Int32 width, Int32 height ) -> void {
        m_Scissor.X = x;
        m_Scissor.Y = y;
        m_Scissor.Width = width;
        m_Scissor.Height = height;
    }

    auto PassCommandList::BindPipeline( std::string_view pipelineName ) -> void {
    }
    auto PassCommandList::BindVertexBuffer( BufferHandle vertices ) -> void {
    }
    auto PassCommandList::BindIndexBuffer( BufferHandle indices ) -> void {
    }
    auto PassCommandList::SubmitDraw() -> void {
    }
    auto PassCommandList::Dispatch( UInt32 invX, UInt32 invY, UInt32 invZ ) -> void {
    }
    auto PassCommandList::BindBuffer( BufferHandle texture, UInt32 set, UInt32 index ) -> void {
    }

}// namespace Mikoto