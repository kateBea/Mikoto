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
#include <Memory/Allocator.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Core/FrameGraph.hh>

namespace Mikoto {
    CommandContext::CommandContext( GraphicsContext *context, FramePassNode &pass, ResourceContainer& container)
        : m_Context{ context }, m_ActivePass{ MKT_ADDRESSOF( pass ) }, m_ResourcesByNames{ MKT_ADDRESSOF( container ) } {
        
    }

    auto CommandContext::BeginPass( CommandListHandle cmd ) -> void {
        MKT_BEGIN_PROFILER_NAMED();
        m_Commands = cmd;

        // We do not validate if the command list is empty
        // because generic passes do not need a command list
        // Validations are ran later though if a generic pass attempts to 
        // run operations it is not allowed to
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

    auto CommandContext::BindBuffer( ResourceGroup group, std::string_view name, ResourceSlot slot ) -> void {
        const auto it{ m_ResourcesByNames->Resources.find( std::string{ name } ) };

        if (it != m_ResourcesByNames->Resources.end()) {
            FramePassResource& resource{ it->second };

            BufferHandle buffer{ m_Context->GetBuffer( name ) };
            MKT_ASSERT( !buffer.IsEmpty(), "Texture cannot be empty" );

            if (resource.IsResource( FrameResourceType::BUFFER )) {
                m_Context->PushBuffer( group, buffer, m_ActivePass->Name, slot );
            }
        }
    }

    auto CommandContext::BindGroup( ResourceGroup group, std::string_view groupName ) -> void {
        std::string name{ groupName };
        if ( !m_ActiveUnoundedResourceGroups[name] ) {
            m_Context->BindImageSamplerUndoundedGroup( groupName, m_Commands );
            m_ActiveUnoundedResourceGroups[name] = true;
        }
    }

    auto CommandContext::BindImageSampler( ResourceGroup group, std::string_view name, ResourceSlot slot ) -> void {
        const auto it{ m_ResourcesByNames->Resources.find( std::string{ name } ) };
        if (it != m_ResourcesByNames->Resources.end()) {
            FramePassResource& resource{ it->second };

            TextureHandle texture{ m_Context->GetTexture( name ) };
            MKT_ASSERT( !texture.IsEmpty(), "Texture cannot be empty" );

            if (resource.IsResource( FrameResourceType::TEXTURE )) {
                m_Context->PushTexture( group, texture, m_ActivePass->Name, slot );
            }
        }
    }

    auto CommandContext::BindImageSampler( ResourceGroup group, std::string_view name, SamplerHandle sampler, ResourceSlot slot ) -> void {
        const auto it{ m_ResourcesByNames->Resources.find( std::string{ name } ) };
        if (it != m_ResourcesByNames->Resources.end()) {
            FramePassResource& resource{ it->second };

            TextureHandle texture{ m_Context->GetTexture( name ) };
            MKT_ASSERT( !texture.IsEmpty(), "Texture cannot be empty" );

            if ( resource.IsResource( FrameResourceType::TEXTURE ) ) {
                m_Context->PushTexture( group, texture, sampler, m_ActivePass->Name, slot );
            }
        }
    }

    auto CommandContext::BindImageSampler( ResourceGroup group, TextureHandle texture, SamplerHandle sampler, ResourceSlot slot ) -> void {
        if (texture.IsEmpty()) {
            return;
        }

        m_Context->PushTexture( group, texture, sampler, m_ActivePass->Name, slot );
    }

    auto CommandContext::BindImageSampler( ResourceGroup group, std::string_view groupName, TextureHandle texture ) -> Int32 {
        Int32 index{ -1 };

        if (texture.IsEmpty()) {
            return index;
        }

        std::string name{ groupName };
        if (!m_ActiveUnoundedResourceGroups[name]) {
            m_Context->BindImageSamplerUndoundedGroup( groupName, m_Commands) ;
            m_ActiveUnoundedResourceGroups[name] = true;
        }

        switch ( group ) {
            case ResourceGroup::UnboundedImageViews:
                index = m_Context->RegisterImageSamplerUndoundedGroup( groupName, texture, SamplerHandle::CreateEmpty() );
                break;
        }
         
        return index;
    }

    auto CommandContext::PushImageSampler( ResourceGroup group, std::string_view groupName, TextureHandle texture ) -> Int32 {
        Int32 index{ -1 };

        if ( texture.IsEmpty() ) {
            return index;
        }

        switch ( group ) {
            case ResourceGroup::UnboundedImageViews:
                index = m_Context->RegisterImageSamplerUndoundedGroup( groupName, texture, SamplerHandle::CreateEmpty() );
                break;
        }

        return index;
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

    auto CommandContext::SetViewport( Int32 x, Int32 y, Int32 width, Int32 height, bool flip ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_ASSERT( !m_Commands.IsEmpty(), "No valid command list handle" );
        m_Commands->SetViewport( x, y, width, height, flip );
    }

    auto CommandContext::SetScissor( Int32 x, Int32 y, Int32 width, Int32 height ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_ASSERT( !m_Commands.IsEmpty(), "No valid command list handle" );
        m_Commands->SetScissor( x, y, width, height );
    }

    auto CommandContext::CopyToCube( std::string_view texture2DName, std::string_view cubeMapName, Size mipLevel, UInt32 face ) -> void {
        auto texture2D{ m_Context->GetTexture( texture2DName ).As<Texture2D>() };
        auto textureCube{ m_Context->GetTexture( cubeMapName ).As<TextureCube>() };

        m_Commands->CopyTexture( texture2D.GetRaw(), textureCube.GetRaw(), mipLevel, face );
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

    auto CommandContext::SetPolygonLineWidth( float value ) -> void {
        m_Commands->SetPolygonLineWidth( value );
    }

    auto CommandContext::Draw( UInt32 vertexCount, UInt32 instanceCount, UInt32 firstVertex, UInt32 firstInstance ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_ASSERT( !m_Commands.IsEmpty(), "No valid command list handle" );

        // Constants are static data at command level that we only need to pass once, only updated if 
        // the block is update from CPU side (i.e previous call to PushConstants(...)
        if (!m_HasSetConstantData) {
            m_Context->PushConstants( m_ActivePass->Name, m_PushConstants, m_Commands );

            m_HasSetConstantData = true;
        }

        // If pass hasn't bound its resources do it once
        if (!m_HasResourcesBound) {
            m_Context->BindShaderResources( m_ActivePass->Name, m_Commands );
            m_HasResourcesBound = true;
        }

        m_Commands->Draw( vertexCount, instanceCount, firstVertex, firstInstance );
    }

    auto CommandContext::DrawIndexed( const DrawIndexedState &info ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (info.InstancesCount == 0) {
            return;
        }

        if ( !m_HasSetConstantData ) {
            m_Context->PushConstants( m_ActivePass->Name, m_PushConstants, m_Commands );

            m_HasSetConstantData = true;
        }

        // If pass hasn't bound its resources do it once
        if (!m_HasResourcesBound) {
            m_Context->BindShaderResources( m_ActivePass->Name, m_Commands );
            m_HasResourcesBound = true;
        }

        MKT_ASSERT( !m_Commands.IsEmpty(), "No valid command list handle" );

        for (auto &[vertexBuffer, binding]: info.VertexBuffers) {
            m_Commands->BindVertexBuffer( vertexBuffer, binding );
        }

        m_Commands->BindIndexBuffer( info.IndexBuffer );
        m_Commands->DrawIndexed( info.IndexBuffer->GetCount(), info.InstancesCount, info.FirstIndex, info.VertexOffset, info.FirstInstance );
    }

    auto CommandContext::DrawIndexedIndirect( const DrawIndirectIndexedState &info ) -> void {
        if ( info.Commands.empty() ) {
            return;
        }

        if ( !m_HasSetConstantData ) {
            m_Context->PushConstants( m_ActivePass->Name, m_PushConstants, m_Commands );

            m_HasSetConstantData = true;
        }

        // If pass hasn't bound its resources do it once
        if (!m_HasResourcesBound) {
            m_Context->BindShaderResources( m_ActivePass->Name, m_Commands );
            m_HasResourcesBound = true;
        }

        MKT_ASSERT( !m_Commands.IsEmpty(), "No valid command list handle" );

        // Bind vertex buffers
        for (auto& [vb, binding] : info.VertexBuffers) {
            m_Commands->BindVertexBuffer( vb, binding );
        }

        // Bind index buffer
        m_Commands->BindIndexBuffer( info.IndexBuffer );

        // Multi-draw in one call
        m_Commands->DrawIndexedIndirect(
                info.IndirectCommandsBuffer,
                0,
                info.DrawCount,
                sizeof( DrawIndexedIndirectCommand ) );
    }

    auto CommandContext::Dispatch( UInt32 invX, UInt32 invY, UInt32 invZ ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( !m_HasSetConstantData ) {
            m_Context->PushConstants( m_ActivePass->Name, m_PushConstants, m_Commands );
            m_HasSetConstantData = true;
        }

        // If pass hasn't bound its resources do it once
        if (!m_HasResourcesBound) {
            m_Context->BindShaderResources( m_ActivePass->Name, m_Commands );
            m_HasResourcesBound = true;
        }

        MKT_ASSERT( !m_Commands.IsEmpty(), "No valid command list handle" );
        m_Commands->Dispatch( invX, invY, invZ );
    }

    auto CommandContext::UploadBufferData( std::string_view bufferName, const void *buffer, Size elementSize, Size elementCount ) const -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (BufferHandle bufferHandle{ m_Context->GetBuffer( bufferName ) }) {
            bufferHandle->CopyToDevice( buffer, elementCount * elementSize );
        }
    }

    auto CommandContext::UploadBuffer( std::string_view bufferName, const void *ptrSrc, Size size, Size offset ) const -> void {
        MKT_BEGIN_PROFILER_NAMED();

        BufferHandle buffer{ m_Context->GetBuffer( bufferName ) };

        MKT_ASSERT( !buffer.IsEmpty(), "Buffer does not exist" );
        MKT_ASSERT( size < buffer->GetSizeBytes(), "Size is bigger than expected" );

        buffer->CopyToDevice( ptrSrc, size, offset ); 
    }

    auto CommandContext::CopyBuffer( std::string_view bufferName, const void *ptrSrc, Size size ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (size == 0) {
            return;
        }

        BufferHandle buffer{ m_Context->GetBuffer( bufferName ) };

        MKT_ASSERT( !buffer.IsEmpty(), "Buffer does not exist" );
        MKT_ASSERT( size < buffer->GetSizeBytes(), "Size is bigger than expected" );

        buffer->Copy( ptrSrc, size, m_Commands ); 
    }

    auto CommandContext::PushConstants( const void *ptr, Size size ) -> void {
        m_PushConstants.SetData( ptr, size );

        // To update constants once in the next call to draw or dispatch
        m_HasSetConstantData = false;
    }

    auto CommandContext::PushTexture( TextureHandle texture ) const -> Int32 {
        MKT_BEGIN_PROFILER_NAMED();

        return -1;
    }

    auto CommandContext::GetNamedBuffer( std::string_view name ) const -> BufferHandle {
        MKT_BEGIN_PROFILER_NAMED();

        return m_Context->GetBuffer( name );
    }

    auto CommandContext::BindImage( TextureHandle handle, SamplerHandle sampler, UInt32 bindingSlot ) -> void {
        MKT_BEGIN_PROFILER_NAMED();
        m_Context->PushTexture( handle, sampler, m_ActivePass->Name, bindingSlot );
    }
    
    auto CommandContext::BindImage( std::string_view name, SamplerHandle sampler, UInt32 bindingSlot ) -> void {
        TextureHandle texture{ m_Context->GetTexture( name ) };
        MKT_ASSERT( !texture.IsEmpty(), "Texture cannot be empty" );

        m_Context->PushTexture( texture, sampler, m_ActivePass->Name, bindingSlot );
    }

    auto CommandContext::CreateSampler( SamplerDescription samplerDescription ) -> SamplerHandle {
        MKT_BEGIN_PROFILER_NAMED();

        return m_Context->CreateSampler( samplerDescription );
    }
}