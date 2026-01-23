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

#include <Renderer/Core/CommandContext.hh>

namespace Mikoto {
    CommandContext::CommandContext( GraphicsContext *context, GpuDevice* device )
        : m_Context{ context }, m_Device{ device } {
        MKT_ASSERT( m_Device, "Command context must have a valid device" );
    }

    auto CommandContext::BeginPass( FramePassNode& pass ) -> void {
        MKT_ASSERT( m_Commands.IsEmpty(), "Pass is ongoing, cannot call begin pass again" );

        m_ActivePass = std::addressof( pass );

        m_Commands = m_Device->CreateCommandList( QueueType::GRAPHICS_QUEUE, false );
        m_Commands->Begin();

        m_Context->BindShaderResources( pass.Name, m_Commands );
    }

    auto CommandContext::EndPass() -> void {
        m_Commands->End();
        m_Device->SubmitCommands( m_Commands );
    }

    auto CommandContext::BeginRender( const PassRenderInfo& renderInfo ) -> void {
        MKT_ASSERT( !m_Commands.IsEmpty(), "No valid command list handle" );

        // There must be at least one valid color target
        MKT_ASSERT( !m_RenderInfo.ColorRenderTargets.empty() && !m_RenderInfo.ColorRenderTargets.front().IsEmpty(),
            "No valid color target" );

        m_RenderInfo.ColorLoadOp = renderInfo.ColorLoadOp;
        m_RenderInfo.DephtLoadOp = renderInfo.DephtLoadOp;

        m_Commands->BeginRender( m_RenderInfo );
    }

    auto CommandContext::EndRender() -> void {
        // Clear render info so we can render again
        m_Commands->EndRender( m_RenderInfo );

        m_RenderInfo.Clear();
    }

    auto CommandContext::SetColorRenderTarget( std::string_view color ) -> void {
        TextureHandle colorHandle{ m_Context->GetTexture( color ) };
        MKT_ASSERT( !colorHandle.IsEmpty(), "Color render target must not be empty" );

        m_RenderInfo.ColorRenderTargets.emplace_back( colorHandle );
    }

    auto CommandContext::SetDepthRenderTarget( std::string_view depth ) -> void {
        TextureHandle depthTexture{ m_Context->GetTexture( depth ) };
        MKT_ASSERT( !depthTexture.IsEmpty(), "Depth render target must not be empty" );

        m_RenderInfo.DepthRenderTarget = depthTexture;
    }

    auto CommandContext::SetViewport( Int32 x, Int32 y, Int32 width, Int32 height ) -> void {
        MKT_ASSERT( !m_Commands.IsEmpty(), "No valid command list handle" );
        m_Commands->SetViewport( x, y, width, height );
    }

    auto CommandContext::SetScissor( Int32 x, Int32 y, Int32 width, Int32 height ) -> void {
        MKT_ASSERT( !m_Commands.IsEmpty(), "No valid command list handle" );
        m_Commands->SetScissor( x, y, width, height );
    }

    auto CommandContext::BindPipeline( std::string_view pipelineName ) -> void {
        const PipelineHandle piepline{ m_Context->GetPipeline( pipelineName ) };

        MKT_ASSERT( !piepline.IsEmpty(), "PipelineHandle must not be empty" );
        MKT_ASSERT( m_ActivePass, "Cannot bind pipeline without a valid pass." );

        m_Commands->BindPipeline( piepline );
    }

    auto CommandContext::SetClearColor( const Vec4F &color ) -> void {
        m_RenderInfo.ClearColor = color;
    }

    auto CommandContext::Draw( UInt32 vertexCount, UInt32 instanceCount, UInt32 firstVertex, UInt32 firstInstance ) -> void {
        MKT_ASSERT( !m_Commands.IsEmpty(), "No valid command list handle" );
        m_Commands->Draw( vertexCount, instanceCount, firstVertex, firstInstance );
    }

    auto CommandContext::DrawIndexed( const DrawIndexedState &info ) -> void {
        MKT_ASSERT( !m_Commands.IsEmpty(), "No valid command list handle" );

        for (auto &[vertexBuffer, binding]: info.VertexBuffers) {
            m_Commands->BindVertexBuffer( vertexBuffer, binding );
        }

        m_Commands->BindIndexBuffer( info.IndexBuffer );
        m_Commands->DrawIndexed( info.IndexBuffer->GetCount(), info.InstancesCount, info.FirstIndex, info.VertexOffset, info.FirstInstance );
    }

    auto CommandContext::Dispatch( UInt32 invX, UInt32 invY, UInt32 invZ ) -> void {
        MKT_ASSERT( !m_Commands.IsEmpty(), "No valid command list handle" );
        m_Commands->Dispatch( invX, invY, invZ );
    }

    auto CommandContext::FillBufferElement( std::string_view bufferName, const void *buffer, Size elementSize, Size elementCount ) const -> void {
        if (BufferHandle bufferHandle{ m_Context->GetBuffer( bufferName ) }; !bufferHandle.IsEmpty()) {
            for (Size count{}; count < elementCount; ++count) {
                const auto *src{ static_cast<const std::byte *>( buffer ) };
                bufferHandle->CopyFromBlock( std::addressof( src[elementSize * count] ), elementSize, count * elementSize );
            }
        }
    }

    auto CommandContext::FillBuffer( std::string_view bufferName, const void *ptrSrc, Size size, Size offset ) const -> void {
        if (BufferHandle buffer{ m_Context->GetBuffer( bufferName ) }; !buffer.IsEmpty()) {
            if (size > buffer->GetSizeBytes()) {
                MKT_CORE_LOGGER_WARN( "PassCommandList::FillBuffer - [{}] size is [{}]. Trying to copy [{}] bytes", bufferName, buffer->GetSizeBytes(), size );
            } else { buffer->CopyFromBlock( ptrSrc, size, offset ); }
        }
    }

    auto CommandContext::PushTexture( TextureHandle texture ) const -> Int32 { return m_Context->PushImage( texture ); }

    auto CommandContext::GetNamedBuffer( std::string_view name ) const -> BufferHandle {
        return m_Context->GetBuffer( name );
    }

    auto CommandContext::RegisterNamedTexture( std::string_view name, TextureHandle handle ) const -> void {
        //m_Blackboard->RegisterTexture( name, handle );
    }

    auto CommandContext::CreateNamedSampler( std::string_view name, SamplerDescription samplerDescription ) -> void {
        if (!m_Context->GetSampler( name ).IsEmpty()) { return; }

        //m_Context->RegisterSample( name, samplerDescription );
    }
}