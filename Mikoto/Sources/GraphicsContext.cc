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
    auto GraphicsContext::Create( GraphicsAPI api ) -> Unique<GraphicsContext> {
        Unique<GraphicsContext> result{ nullptr };

        switch ( api ) {
            case GraphicsAPI::VULKAN_API:
                result = CreateScope<VulkanGraphicsContext>();
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

    PassCommandList::PassCommandList( GraphicsContext *context ) {
    }
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

    auto PassCommandList::BindTexture( TextureHandle texture ) -> void {
    }
    auto PassCommandList::BindTexture( std::string_view texture ) -> void {
    }
    auto PassCommandList::BindBuffer( std::string_view buffer, UInt32 set, UInt32 index ) -> void {
    }

    auto PassCommandList::BindSampler( SamplerHandle sampler, std::string_view textureName ) -> void {
    }
    auto PassCommandList::BindSampler( SamplerHandle sampler, TextureHandle texture ) -> void {
    }
    auto PassCommandList::BeginCompute() -> void {
    }
    auto PassCommandList::EndCompute() -> void {
    }
    auto PassCommandList::SetViewport( Int32 x, Int32 y, Int32 width, Int32 height ) -> void {
    }
    auto PassCommandList::SetScissor( Int32 x, Int32 y, Int32 width, Int32 height ) -> void {
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