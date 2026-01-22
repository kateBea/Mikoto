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
    CommandContext::CommandContext( GraphicsContext *context, FrameBlackboard *blackboard, GpuDevice* device )
        : m_Context{ context }, m_Blackboard{ blackboard }, m_Device{ device } {
        MKT_ASSERT( m_Blackboard, "Tried to create PassCommandList with NULL blackboard" );
    }

    auto CommandContext::BeginPass( FramePass *pass ) -> void {
        MKT_ASSERT( pass, "Pass cannot be NULL" );
        MKT_ASSERT( m_Commands.IsEmpty(), "Pass is ongoing, cannot call beginn pass again" );

        m_ActivePass = pass;

        m_Commands = m_Device->CreateCommandList( QueueType::GRAPHICS_QUEUE, false );
        m_Commands->Begin();

        m_Context->InsertResourceBarrier( m_ActivePass, m_Commands );

        m_Context->BeginPass( m_ActivePass );
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
        MKT_ASSERT( m_Blackboard, "Tried to create PassCommandList with NULL blackboard" );
        TextureHandle colorHandle{ m_Blackboard->GetTexture( color ) };

        MKT_ASSERT( !colorHandle.IsEmpty(), "Color render target must not be empty" );

        m_RenderInfo.ColorRenderTargets.emplace_back( colorHandle );
    }

    auto CommandContext::SetDepthRenderTarget( std::string_view depth ) -> void {
        MKT_ASSERT( m_Blackboard, "Tried to create PassCommandList with NULL blackboard" );
        TextureHandle depthTexture{ m_Blackboard->GetTexture( depth ) };

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
        MKT_ASSERT( m_Context, "No valid context for this pass command list" );

        MKT_ASSERT( m_Blackboard, "Blackboard is NULL" );
        const PipelineHandle piepline{ m_Blackboard->GetPipeline( pipelineName ) };

        MKT_ASSERT( !piepline.IsEmpty(), "PipelineHandle must not be empty" );
        MKT_ASSERT( m_ActivePass, "Cannot bind pipeline without a valid pass." );

        // Ensure pass has shader resource slots ready
        m_Context->CreatePipelineResources( m_ActivePass, piepline );

        m_Commands->BindPipeline( piepline );
    }

    auto CommandContext::SetBufferBindSlot( SRGType type, std::string_view buffer, UInt32 index ) const -> void {
        MKT_ASSERT( m_Context, "No valid context for this pass command list" );

        if (IsSRGType( type, SRGType::SRG_PerPass )) {
            SRGPerPass *perPassData{ m_Context->GetPassSRG( m_ActivePass ) };
            MKT_ASSERT( perPassData, "Per pass data must not be NULL" );

            BufferHandle bufferHandle{ m_Blackboard->GetBuffer( buffer ) };
            if (!bufferHandle.IsEmpty()) {
                auto resourceType{ ShaderResourceType::SHADER_RESOURCE_UNDEFINED };

                switch (bufferHandle->GetUsage()) {
                    case BufferUsage::BUFFER_USAGE_UNIFORM:
                        resourceType = ShaderResourceType::SHADER_RESOURCE_UNIFORM_BUFFER;
                        break;
                    case BufferUsage::BUFFER_USAGE_SHADER_STORAGE:
                        resourceType = ShaderResourceType::SHADER_STORAGE_BUFFER;
                        break;
                    default: ;
                }

                perPassData->SetBuffer( buffer, index, resourceType );
            } else {
                // Just warn if the buffer is empty
                MKT_CORE_LOGGER_WARN( "PassCommandList::SetBufferBindSlot - Trying to bind buffer [{}] which does not exist. ", buffer );
            }
        }
    }

    auto CommandContext::SetTextureBindSlot( SRGType type, std::string_view texture, std::string_view sampler, UInt32 index ) -> void {
        MKT_ASSERT( m_Context, "No valid context for this pass command list" );
        MKT_ASSERT( m_Blackboard, "Tried to create PassCommandList with NULL blackboard" );

        if (IsSRGType( type, SRGType::SRG_PerPass )) {
            SRGPerPass *perPassData{ m_Context->GetPassSRG( m_ActivePass ) };
            MKT_ASSERT( perPassData, "Per pass data must not be NULL" );

            perPassData->SetTexture( texture, sampler, index );
        }
    }

    auto CommandContext::BindResourceGroup( const SRGType srgType ) const -> void {
        MKT_ASSERT( m_Context, "No valid context for this pass command list" );

        // First we make sure this pass resources have been commited meaning they ara available for use in the shaders
        m_Context->CommitShaderResources(m_ActivePass);

        switch (srgType) {
            case SRGType::SRG_Textures:
                m_Context->BindTextureList( m_Commands );
                break;
            case SRGType::SRG_PerPass:
                m_Context->BindPassResources( m_ActivePass, m_Commands );
                break;
            default: ;
        }
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
        MKT_ASSERT( m_Blackboard, "Tried to create PassCommandList with NULL blackboard" );

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
        MKT_ASSERT( m_Context, "No valid context for this pass command list" );

        if (BufferHandle bufferHandle{ m_Blackboard->GetBuffer( bufferName ) }; !bufferHandle.IsEmpty()) {
            for (Size count{}; count < elementCount; ++count) {
                const auto *src{ static_cast<const std::byte *>( buffer ) };
                bufferHandle->CopyFromBlock( std::addressof( src[elementSize * count] ), elementSize, count * elementSize );
            }
        }
    }

    auto CommandContext::FillBuffer( std::string_view bufferName, const void *ptrSrc, Size size, Size offset ) const -> void {
        MKT_ASSERT( m_Context, "No valid context for this pass command list" );

        if (BufferHandle buffer{ m_Blackboard->GetBuffer( bufferName ) }; !buffer.IsEmpty()) { if (size > buffer->GetSizeBytes()) { MKT_CORE_LOGGER_WARN( "PassCommandList::FillBuffer - [{}] size is [{}]. Trying to copy [{}] bytes", bufferName, buffer->GetSizeBytes(), size ); } else { buffer->CopyFromBlock( ptrSrc, size, offset ); } }
    }

    auto CommandContext::PushTexture( TextureHandle texture ) const -> Int32 { return m_Context->PushImage( texture ); }

    auto CommandContext::GetNamedBuffer( std::string_view name ) const -> BufferHandle {
        MKT_ASSERT( m_Blackboard, "No valid blackboard for this pass command list" );

        return m_Blackboard->GetBuffer( name );
    }

    auto CommandContext::RegisterNamedTexture( std::string_view name, TextureHandle handle ) const -> void {
        MKT_ASSERT( m_Blackboard, "No valid blackboard for this pass command list" );
        m_Blackboard->RegisterTexture( name, handle );
    }

    auto CommandContext::CreateNamedSampler( std::string_view name, SamplerDescription samplerDescription ) -> void {
        MKT_ASSERT( m_Blackboard, "No valid blackboard for this pass command list" );

        if (!m_Blackboard->GetSampler( name ).IsEmpty()) { return; }

        m_Blackboard->RegisterSample( name, samplerDescription );
    }
}