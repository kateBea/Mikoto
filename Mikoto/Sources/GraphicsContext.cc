//
// Created by kate on 11/25/25.
//

#include <Renderer/Core/GraphicsContext.hh>

namespace Mikoto {
    auto PipelineDescription::AddShader( std::string_view path ) -> void {
        Shaders.emplace_back( path );
    }


    auto GraphicsContext::Create( GraphicsAPI api ) -> Unique<GraphicsContext> {
        return nullptr;
    }

    PassCommandList::PassCommandList( GraphicsContext *context ) {
    }
    auto PassCommandList::BeginRender() -> void {
    }
    auto PassCommandList::EndRender() -> void {
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
    auto PassCommandList::DrawIndexed() -> void {
    }
    auto PassCommandList::Dispatch( UInt32 invX, UInt32 invY, UInt32 invZ ) -> void {
    }
}// namespace Mikoto