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

    auto PassCommandList::BeginRender(FramePass* pass, LoadOp loadOp) -> void {
        MKT_ASSERT( m_Context, "No valid context for this pass command list" );

        m_RenderInfo.LoadOp = loadOp;

        m_Context->BeginRender( m_RenderInfo );

        m_ActivePass = pass;
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

    auto PassCommandList::SetBufferBindSlot( SRGType type, std::string_view buffer, UInt32 index ) const -> void {
        MKT_ASSERT( m_Context, "No valid context for this pass command list" );

        if (IsSRGType( type, SRGType::SRG_PerPass)) {
            SRGPerPass* perPassData{ m_Context->GetPassSRG( m_ActivePass ) };
            MKT_ASSERT( perPassData, "Per pass data must not be NULL" );

            BufferHandle bufferHandle{ m_Blackboard->GetBuffer( buffer ) };
            if (bufferHandle.IsEmpty()) {
                MKT_CORE_LOGGER_WARN( "PassCommandList::SetBufferBindSlot - Trying to bind buffer [{}] which does not exist. ", buffer );
            } else {
                ShaderResourceType resourceType{ ShaderResourceType::SHADER_RESOURCE_UNDEFINED };
                switch (bufferHandle->GetUsage()) {

                    case BufferUsage::BUFFER_USAGE_UNIFORM:
                        resourceType = ShaderResourceType::SHADER_RESOURCE_UNIFORM_BUFFER;
                        break;
                    case BufferUsage::BUFFER_USAGE_SHADER_STORAGE:
                        resourceType = ShaderResourceType::SHADER_STORAGE_BUFFER;
                        break;
                    default:;
                }

                perPassData->SetBuffer( buffer, index, resourceType );
            }
        }
    }

    auto PassCommandList::SetTextureBindSlot( SRGType type, std::string_view texture, std::string_view sampler, UInt32 index ) -> void {
        MKT_ASSERT( m_Context, "No valid context for this pass command list" );
        MKT_ASSERT( m_Blackboard, "Tried to create PassCommandList with NULL blackboard" );

        if (IsSRGType( type, SRGType::SRG_PerPass)) {
            SRGPerPass* perPassData{ m_Context->GetPassSRG( m_ActivePass ) };
            MKT_ASSERT( perPassData, "Per pass data must not be NULL" );

            perPassData->SetTexture(  texture, sampler, index );
        }
    }

    auto PassCommandList::Draw( UInt32 vertexCount, UInt32 instanceCount, UInt32 firstVertex, UInt32 firstInstance ) const -> void {
        MKT_ASSERT( m_Context, "No valid context for this pass command list" );
        m_Context->Draw(vertexCount, instanceCount, firstVertex, firstInstance);
    }

    auto PassCommandList::BeginCompute(FramePass* pass) -> void {
        MKT_ASSERT( m_Context, "No valid context for this pass command list" );

        m_Context->BeginCompute();

        m_ActivePass = pass;
    }

    auto PassCommandList::EndCompute() const -> void {
        MKT_ASSERT( m_Context, "No valid context for this pass command list" );

        m_Context->EndCompute();
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

    auto PassCommandList::BindPipeline( std::string_view pipelineName ) const -> void {
        const PipelineHandle piepline{ m_Blackboard->GetPipeline(pipelineName) };

        MKT_ASSERT( !piepline.IsEmpty(), "PassCommandList::SetDepthRenderTarget - PipelineHandle must not be empty" );
        MKT_ASSERT( m_ActivePass, "Cannot bind pipeline without a valid pass." );

        m_Context->BindPipeline( piepline, m_ActivePass );
    }

    auto PassCommandList::DrawIndexed(const DrawIndexedState& info) const -> void {
        MKT_ASSERT( m_Context, "No valid context for this pass command list" );
        MKT_ASSERT( m_Blackboard, "Tried to create PassCommandList with NULL blackboard" );

        for (auto& [vertexBuffer, binding] : info.VertexBuffers) {
            m_Context->BindVertexBuffer(vertexBuffer, binding);
        }

        m_Context->BindIndexBuffer(info.IndexBuffer);
        m_Context->DrawInstanced(info.IndexBuffer->GetCount(), info.InstancesCount, info.FirstIndex, info.VertexOffset, info.FirstInstance);
    }

    auto PassCommandList::Dispatch( UInt32 invX, UInt32 invY, UInt32 invZ ) const -> void {
        m_Context->Dispatch( invX, invY, invZ );
    }

    auto PassCommandList::SetClearColor( const Vec4F &color ) -> void {
        m_RenderInfo.ClearColor = color;
    }

    auto PassCommandList::FillBuffer( std::string_view bufferName, const void *ptrSrc, Size size ) const -> void {
        MKT_ASSERT( m_Context, "No valid context for this pass command list" );

        if ( BufferHandle buffer{ m_Blackboard->GetBuffer( bufferName ) }; !buffer.IsEmpty()) {
            if (size > buffer->GetSizeBytes()) {
                MKT_CORE_LOGGER_WARN( "PassCommandList::FillBuffer - [{}] size is [{}]. Trying to copy [{}] bytes", bufferName, buffer->GetSizeBytes(), size );
            } else {
                buffer->CopyFromBlock( ptrSrc, size );
            }
        }
    }

    auto PassCommandList::PushTexture( TextureHandle texture ) const -> Int32 {
        return m_Context->PushImage(texture);
    }

    auto PassCommandList::BindResourceGroup( const SRGType srgType ) const -> void {
        MKT_ASSERT( m_Context, "No valid context for this pass command list" );

        switch (srgType) {

            case SRGType::SRG_Textures:
                m_Context->BindTextureList();
                break;
            case SRGType::SRG_PerPass:
                m_Context->BindPassResources(m_ActivePass);
                break;
            case SRGType::SRG_PerFrame:
                m_Context->BindFrameResources();
                break;
            default:;
        }
    }

    auto PassCommandList::GetNamedBuffer( std::string_view name ) const -> BufferHandle {
        MKT_ASSERT( m_Blackboard, "No valid blackboard for this pass command list" );

        return m_Blackboard->GetBuffer( name );
    }

    auto PassCommandList::RegisterNamedTexture( std::string_view name, TextureHandle handle ) const -> void {
        MKT_ASSERT( m_Blackboard, "No valid blackboard for this pass command list" );
        m_Blackboard->RegisterTexture( name, handle );
    }

    auto PassCommandList::CreateNamedSampler( std::string_view name, SamplerDescription samplerDescription ) -> void {
        MKT_ASSERT( m_Blackboard, "No valid blackboard for this pass command list" );

        if (!m_Blackboard->GetSampler( name ).IsEmpty()) {
            return;
        }

        m_Blackboard->RegisterSample( name, samplerDescription );
    }
}// namespace Mikoto