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
#include <Renderer/Core/CommandContext.hh>

namespace Mikoto {
    CommandContext::CommandContext( GraphicsContext *context, CommandListHandle cmd )
        : m_Context{ context }, m_Commands{ cmd } {
        MKT_ASSERT( !m_Commands.IsEmpty(), "Command context must have a valid command list handle" );
    }

    auto CommandContext::BeginPass( FramePassNode& pass ) -> void {
        MKT_BEGIN_PROFILER_NAMED();
        m_ActivePass = std::addressof( pass );

        if (m_ActivePass->HasResources()) {
            m_Context->BindShaderResources( pass.Name, m_Commands );
        }
    }

    auto CommandContext::EndPass() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_ActivePass->HasExecuted = true;
        m_ActivePass->IsDirty = false;

        // Put the node to sleep if it is not supposed to run every frame
        if (!m_ActivePass->IsExecutionPolicy(FramePassExecutionPolicy::PER_FRAME)) {
            m_ActivePass->Status = FramePassNodeStatus::SLEEPING;
        }
    }

    auto CommandContext::BeginRender( const PassRenderInfo& renderInfo ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_ASSERT( !m_Commands.IsEmpty(), "No valid command list handle" );

        // There must be at least one valid color target
        MKT_ASSERT( !m_RenderInfo.ColorRenderTargets.empty() && !m_RenderInfo.ColorRenderTargets.front().IsEmpty() ||
                !m_RenderInfo.DepthRenderTarget.IsEmpty(),
            "No valid color or depth target" );

        m_RenderInfo.ColorLoadOp = renderInfo.ColorLoadOp;
        m_RenderInfo.DephtLoadOp = renderInfo.DephtLoadOp;

        m_Commands->BeginRender( m_RenderInfo );
    }

    auto CommandContext::EndRender() -> void {
        MKT_BEGIN_PROFILER_NAMED();
        // Clear render info so we can render again
        m_Commands->EndRender( m_RenderInfo );

        m_RenderInfo.Clear();
    }

    auto CommandContext::SetColorRenderTarget( std::string_view color ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        TextureHandle colorHandle{ m_Context->GetTexture( color ) };
        MKT_ASSERT( !colorHandle.IsEmpty(), "Color render target must not be empty" );

        m_RenderInfo.ColorRenderTargets.emplace_back( colorHandle );
    }

    auto CommandContext::SetDepthRenderTarget( std::string_view depth ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        TextureHandle depthTexture{ m_Context->GetTexture( depth ) };
        MKT_ASSERT( !depthTexture.IsEmpty(), "Depth render target must not be empty" );

        m_RenderInfo.DepthRenderTarget = depthTexture;
    }

    auto CommandContext::SetViewport( Int32 x, Int32 y, Int32 width, Int32 height ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_ASSERT( !m_Commands.IsEmpty(), "No valid command list handle" );
        m_Commands->SetViewport( x, y, width, height );
    }

    auto CommandContext::SetScissor( Int32 x, Int32 y, Int32 width, Int32 height ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_ASSERT( !m_Commands.IsEmpty(), "No valid command list handle" );
        m_Commands->SetScissor( x, y, width, height );
    }

    auto CommandContext::BindGlobalTextures() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_ASSERT( !m_Commands.IsEmpty(), "No valid command list handle" );
        m_Context->BindGlobalTextures( m_Commands );
    }

    auto CommandContext::BindPipeline( std::string_view pipelineName ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        const PipelineHandle piepline{ m_Context->GetPipeline( pipelineName ) };

        MKT_ASSERT( !piepline.IsEmpty(), "PipelineHandle must not be empty" );
        MKT_ASSERT( m_ActivePass, "Cannot bind pipeline without a valid pass." );

        m_Commands->BindPipeline( piepline );
    }

    auto CommandContext::SetClearColor( const Vec4F &color ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_RenderInfo.ClearColor = color;
    }

    auto CommandContext::Draw( UInt32 vertexCount, UInt32 instanceCount, UInt32 firstVertex, UInt32 firstInstance ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_ASSERT( !m_Commands.IsEmpty(), "No valid command list handle" );
        m_Commands->Draw( vertexCount, instanceCount, firstVertex, firstInstance );
    }

    auto CommandContext::DrawIndexed( const DrawIndexedState &info ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (!m_ActivePass->ConstantsShaderResources.IsEmpty()) {
            m_Context->PushConstants(m_ActivePass->Name, m_ActivePass->ConstantsShaderResources, m_Commands);
        }

        MKT_ASSERT( !m_Commands.IsEmpty(), "No valid command list handle" );

        for (auto &[vertexBuffer, binding]: info.VertexBuffers) {
            m_Commands->BindVertexBuffer( vertexBuffer, binding );
        }

        m_Commands->BindIndexBuffer( info.IndexBuffer );
        m_Commands->DrawIndexed( info.IndexBuffer->GetCount(), info.InstancesCount, info.FirstIndex, info.VertexOffset, info.FirstInstance );
    }

    auto CommandContext::Dispatch( UInt32 invX, UInt32 invY, UInt32 invZ ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_ASSERT( !m_Commands.IsEmpty(), "No valid command list handle" );
        m_Commands->Dispatch( invX, invY, invZ );
    }

    auto CommandContext::UploadBufferData( std::string_view bufferName, const void *buffer, Size elementSize, Size elementCount ) const -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (BufferHandle bufferHandle{ m_Context->GetBuffer( bufferName ) }; !bufferHandle.IsEmpty()) {
            bufferHandle->CopyFromBlock( buffer, elementCount * elementSize );

            // for (Size count{}; count < elementCount; ++count) {
            //     const auto *src{ static_cast<const std::byte *>( buffer ) };
            //     //bufferHandle->CopyFromBlock( std::addressof( src[elementSize * count] ), elementSize, count * elementSize );
            // }
        }
    }

    auto CommandContext::UploadBuffer( std::string_view bufferName, const void *ptrSrc, Size size, Size offset ) const -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (BufferHandle buffer{ m_Context->GetBuffer( bufferName ) }; !buffer.IsEmpty()) {
            if (size > buffer->GetSizeBytes()) {
                MKT_CORE_LOGGER_WARN( "PassCommandList::FillBuffer - [{}] size is [{}]. Trying to copy [{}] bytes", bufferName, buffer->GetSizeBytes(), size );
            } else { buffer->CopyFromBlock( ptrSrc, size, offset ); }
        }
    }

    auto CommandContext::PushContants( const void *ptr, Size size ) -> void {
        m_ActivePass->ConstantsShaderResources.SetData( ptr, size );
    }

    auto CommandContext::PushTexture( TextureHandle texture ) const -> Int32 {
        MKT_BEGIN_PROFILER_NAMED();

        return m_Context->PushGlobalTexture( texture );
    }

    auto CommandContext::GetNamedBuffer( std::string_view name ) const -> BufferHandle {
        MKT_BEGIN_PROFILER_NAMED();

        return m_Context->GetBuffer( name );
    }

    auto CommandContext::BindImage( TextureHandle handle, SamplerHandle sampler, UInt32 bindingSlot ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_Context->PushTexture( handle, sampler, m_ActivePass->Name, bindingSlot );
    }

    auto CommandContext::RegisterNamedTexture( std::string_view name, TextureHandle handle ) const -> void {
        MKT_BEGIN_PROFILER_NAMED();

        //m_Blackboard->RegisterTexture( name, handle );
    }

    auto CommandContext::CreateSampler( SamplerDescription samplerDescription ) -> SamplerHandle {
        MKT_BEGIN_PROFILER_NAMED();

        return m_Context->CreateSampler( samplerDescription );
    }
}