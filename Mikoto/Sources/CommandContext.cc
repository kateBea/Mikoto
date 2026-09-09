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

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/Profiler.hh>
#include <Core/Platform.hh>

#include <Memory/Allocator.hh>

#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/CommandContext.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::renderer::rhi;

    MKT_NODISCARD constexpr auto GetPipelineStages( const FGPipelineStage state ) -> PipelineStageFlags {
        switch (state) {
            case FGPipelineStage::eUnknown:                return PipelineStageFlagsBits::None;

            case FGPipelineStage::eConstantBuffer:         return PipelineStageFlagsBits::AllGraphics | PipelineStageFlagsBits::ComputeShader;
            case FGPipelineStage::eVertexBuffer:           return PipelineStageFlagsBits::VertexInput;
            case FGPipelineStage::eIndexBuffer:            return PipelineStageFlagsBits::IndexInput;
            case FGPipelineStage::eIndirectArgument:       return PipelineStageFlagsBits::DrawIndirect;

            case FGPipelineStage::eVertexShader:           return PipelineStageFlagsBits::VertexShader;
            case FGPipelineStage::eHullShader:             return PipelineStageFlagsBits::HullShader;
            case FGPipelineStage::eDomainShader:           return PipelineStageFlagsBits::DomainShader;
            case FGPipelineStage::eGeometryShader:         return PipelineStageFlagsBits::GeometryShader;
            case FGPipelineStage::eComputeShader:          return PipelineStageFlagsBits::ComputeShader;
            case FGPipelineStage::ePixelShader:            return PipelineStageFlagsBits::PixelShader;

            case FGPipelineStage::eUnorderedAccess:        return PipelineStageFlagsBits::AllGraphics | PipelineStageFlagsBits::ComputeShader;

            case FGPipelineStage::eRenderTarget:           return PipelineStageFlagsBits::RenderTarget;
            case FGPipelineStage::eDepthTarget:            return PipelineStageFlagsBits::EarlyFragmentTests | PipelineStageFlagsBits::LateFragmentTests;

            // Small CopyBuffer uploads use vkCmdUpdateBuffer (CLEAR stage);
            // larger uploads use vkCmdCopyBuffer (COPY stage).
            case FGPipelineStage::eCopy:                   return PipelineStageFlagsBits::Copy | PipelineStageFlagsBits::Clear;
            case FGPipelineStage::eResolve:                return PipelineStageFlagsBits::Resolve;
            case FGPipelineStage::ePresent:                return PipelineStageFlagsBits::Bottom;

            case FGPipelineStage::eAccelStructRead:
            case FGPipelineStage::eAccelStructWrite:
            case FGPipelineStage::eAccelStructBuildInput:
            case FGPipelineStage::eAccelStructBuildBlas:   return PipelineStageFlagsBits::AccelerationStructureBuild;
        }

        return PipelineStageFlagsBits::AllCommands;
    }

    MKT_NODISCARD constexpr auto GetTextureLayout( const FGPipelineStage state, const FGResourceAccess access ) -> TextureLayoutFlags {
        switch ( state ) {
            case FGPipelineStage::eUnknown:         return TextureLayoutBits::Unknown;
            case FGPipelineStage::eVertexShader:
            case FGPipelineStage::eHullShader:
            case FGPipelineStage::eDomainShader:
            case FGPipelineStage::eGeometryShader:
            case FGPipelineStage::eComputeShader:
            case FGPipelineStage::ePixelShader:     return access == FGResourceAccess::eWrite ? TextureLayoutBits::UnorderedAccess : TextureLayoutBits::ShaderResource;
            case FGPipelineStage::eUnorderedAccess: return TextureLayoutBits::UnorderedAccess;
            case FGPipelineStage::eRenderTarget:    return TextureLayoutBits::RenderTarget;
            case FGPipelineStage::eDepthTarget:     return TextureLayoutBits::DepthStencil;
            case FGPipelineStage::eCopy:            return access == FGResourceAccess::eWrite ? TextureLayoutBits::CopyDest : TextureLayoutBits::CopySource;
            case FGPipelineStage::eResolve:         return access == FGResourceAccess::eWrite ? TextureLayoutBits::ResolveDest : TextureLayoutBits::ResolveSource;
            case FGPipelineStage::ePresent:         return TextureLayoutBits::Present;
            default:                                return TextureLayoutBits::General;
        }
    }

    MKT_NODISCARD constexpr auto GetBarrierAccess( FGPipelineStage stage, FGResourceAccess access, FGResourceType type ) -> BarrierAccessFlags {
        using BAccessFlags = BarrierAccessFlagsBits;
        if ( access == FGResourceAccess::eNone ) {
            return BAccessFlags::None;
        }

        const bool writes{ access == FGResourceAccess::eWrite };
        switch ( stage ) {
            case FGPipelineStage::eConstantBuffer:   return BAccessFlags::ConstantBuffer;
            case FGPipelineStage::eVertexBuffer:     return BAccessFlags::VertexAttributeRead;
            case FGPipelineStage::eIndexBuffer:      return BAccessFlags::IndexRead;
            case FGPipelineStage::eIndirectArgument: return BAccessFlags::IndirectCommandRead;
            // Load operations, blending, and depth testing can read attachments
            // even when the graph declaration describes a render-target write.
            case FGPipelineStage::eRenderTarget:     return BAccessFlags::ColorAttachmentRead | BAccessFlags::ColorAttachmentWrite;
            case FGPipelineStage::eDepthTarget:      return BAccessFlags::DepthStencilRead | BAccessFlags::DepthStencilWrite;
            case FGPipelineStage::eCopy:             return writes ? BAccessFlags::TransferWrite : BAccessFlags::TransferRead;
            case FGPipelineStage::eResolve:          return writes ? BAccessFlags::ResolveWrite : BAccessFlags::ResolveRead;
            case FGPipelineStage::ePresent:
            case FGPipelineStage::eUnknown:          return BAccessFlags::None;
            case FGPipelineStage::eAccelStructRead:  return BAccessFlags::AccelerationStructureRead;
            case FGPipelineStage::eAccelStructWrite:
            case FGPipelineStage::eAccelStructBuildBlas:
                return BAccessFlags::AccelerationStructureRead | BAccessFlags::AccelerationStructureWrite;
            case FGPipelineStage::eAccelStructBuildInput:
                return BAccessFlags::AccelerationStructureBuildInputRead;
            default:
                if ( writes ) {
                    // Shader writes can be read-modify-write operations.
                    return BAccessFlags::ShaderStorageRead | BAccessFlags::ShaderStorageWrite;
                }
                if ( stage == FGPipelineStage::eUnorderedAccess ) {
                    return BAccessFlags::ShaderStorageRead;
                }
                return type == FGResourceType::eTexture ? BAccessFlags::ShaderSampledRead : BAccessFlags::StructuredBufferRead;
        }
    }

    // External work may have run at a stage that ResourceStates cannot express.
    // Use a conservative source only at that boundary, never for normal graph uses.
    MKT_NODISCARD constexpr auto GetExternalAccess( ResourceStates state, FGResourceType type ) -> BarrierAccessFlags {
        using A = BarrierAccessFlagsBits;
        switch ( state ) {
            case ResourceStates::eUnknown:
            case ResourceStates::ePresent: return A::None;
            case ResourceStates::eConstantBuffer: return A::ConstantBuffer;
            case ResourceStates::eVertexBuffer: return A::VertexAttributeRead;
            case ResourceStates::eIndexBuffer: return A::IndexRead;
            case ResourceStates::eIndirectArgument: return A::IndirectCommandRead;
            case ResourceStates::eShaderResource: return type == FGResourceType::eTexture ? A::ShaderSampledRead : A::StructuredBufferRead;
            case ResourceStates::eRenderTarget: return A::ColorAttachmentRead | A::ColorAttachmentWrite;
            case ResourceStates::eDepthRead: return A::DepthStencilRead;
            case ResourceStates::eDepthWrite: return A::DepthStencilRead | A::DepthStencilWrite;
            case ResourceStates::eCopySource: return A::TransferRead;
            case ResourceStates::eCopyDest: return A::TransferWrite;
            case ResourceStates::eResolveSource: return A::ResolveRead;
            case ResourceStates::eResolveDest: return A::ResolveWrite;
            case ResourceStates::eUnorderedAccess: return A::ShaderStorageRead | A::ShaderStorageWrite;
            case ResourceStates::eAccelStructRead: return A::AccelerationStructureRead;
            case ResourceStates::eAccelStructBuildInput: return A::AccelerationStructureBuildInputRead;
            case ResourceStates::eAccelStructWrite:
            case ResourceStates::eAccelStructBuildBlas: return A::AccelerationStructureRead | A::AccelerationStructureWrite;
            default: return A::MemoryRead | A::MemoryWrite;
        }
    }

    MKT_NODISCARD constexpr auto GetTextureLayout( const ResourceStates state ) -> TextureLayoutFlags {
        switch ( state ) {
            case ResourceStates::eUnknown:         return TextureLayoutBits::Unknown;
            case ResourceStates::eShaderResource:  return TextureLayoutBits::ShaderResource;
            case ResourceStates::eUnorderedAccess: return TextureLayoutBits::UnorderedAccess;
            case ResourceStates::eRenderTarget:    return TextureLayoutBits::RenderTarget;
            case ResourceStates::eDepthRead:       return TextureLayoutBits::DepthStencilReadOnly;
            case ResourceStates::eDepthWrite:      return TextureLayoutBits::DepthStencil;
            case ResourceStates::eCopyDest:        return TextureLayoutBits::CopyDest;
            case ResourceStates::eResolveDest:     return TextureLayoutBits::ResolveDest;
            case ResourceStates::eCopySource:      return TextureLayoutBits::CopySource;
            case ResourceStates::eResolveSource:   return TextureLayoutBits::ResolveSource;
            case ResourceStates::ePresent:         return TextureLayoutBits::Present;
            default:                              return TextureLayoutBits::General;
        }
    }

    MKT_NODISCARD constexpr auto GetResourceState( const FGPipelineStage state, const FGResourceAccess access ) -> ResourceStates {
        switch ( state ) {
            case FGPipelineStage::eUnknown:               return ResourceStates::eUnknown;
            case FGPipelineStage::eRenderTarget:          return ResourceStates::eRenderTarget;
            // BeginRenderPass currently uses writable depth attachment layouts.
            case FGPipelineStage::eDepthTarget:           return ResourceStates::eDepthWrite;
            case FGPipelineStage::ePresent:               return ResourceStates::ePresent;
            case FGPipelineStage::eConstantBuffer:        return ResourceStates::eConstantBuffer;
            case FGPipelineStage::eVertexBuffer:          return ResourceStates::eVertexBuffer;
            case FGPipelineStage::eIndexBuffer:           return ResourceStates::eIndexBuffer;
            case FGPipelineStage::eIndirectArgument:      return ResourceStates::eIndirectArgument;
            case FGPipelineStage::eVertexShader:
            case FGPipelineStage::eHullShader:
            case FGPipelineStage::eDomainShader:
            case FGPipelineStage::eGeometryShader:
            case FGPipelineStage::eComputeShader:
            case FGPipelineStage::ePixelShader:           return access == FGResourceAccess::eWrite ? ResourceStates::eUnorderedAccess : ResourceStates::eShaderResource;
            case FGPipelineStage::eUnorderedAccess:       return ResourceStates::eUnorderedAccess;
            case FGPipelineStage::eCopy:                  return access == FGResourceAccess::eWrite ? ResourceStates::eCopyDest : ResourceStates::eCopySource;
            case FGPipelineStage::eResolve:               return access == FGResourceAccess::eWrite ? ResourceStates::eResolveDest : ResourceStates::eResolveSource;
            case FGPipelineStage::eAccelStructRead:       return ResourceStates::eAccelStructRead;
            case FGPipelineStage::eAccelStructWrite:      return ResourceStates::eAccelStructWrite;
            case FGPipelineStage::eAccelStructBuildInput: return ResourceStates::eAccelStructBuildInput;
            case FGPipelineStage::eAccelStructBuildBlas:  return ResourceStates::eAccelStructBuildBlas;
            default:                                    return ResourceStates::eCommon;
        }
    }

    auto DrawIndirectState::SetBuffer( FGBufferHandle handle ) -> DrawIndirectState& {
        mIndirectBuffer = handle;
        return *this;
    }

    auto DrawIndirectState::SetDrawCount( u32 count ) -> DrawIndirectState& {
        mInstanceCount = count;
        return *this;
    }
    auto ContextRenderState::Clear() -> void {
        mCurrentRenderTargets.clear();
        mDepthTarget = {};
        mRenderArea = {};
    }

    auto ContextRenderState::SetRenderArea( const Rect &rec ) -> ContextRenderState & {
        mRenderArea = rec;
        return *this;
    }

    auto ContextRenderState::AddDepthTarget( FGTextureHandle target, LoadOp op ) -> ContextRenderState & {
        mDepthTarget = RenderTargetState{
            .mClearColor = kColorWhite,
            .mLoadOp = op,
            .mRenderTarget = target,
        };

        return *this;
    }

    auto ContextRenderState::AddRenderTarget( FGTextureHandle target, const Color &c, LoadOp op, u32 faceIndex, u32 mipLevel ) -> ContextRenderState & {
        mCurrentRenderTargets.emplace_back( RenderTargetState{
            .mClearColor = c,
            .mLoadOp = op,
            .mRenderTarget = target,
            .mFaceIndex = faceIndex,
            .mMipLevel = mipLevel,
        });

        return *this;
    }

    CommandContext::CommandContext(  FGNode* pass, FGResourceManager* resourceManager, FGStatisticsManager* statsManager )
        : mNode{ pass }, mResourceManager{ resourceManager } {
        MKT_ASSERT( mNode, "Frame graph node cannot be null" );
        MKT_ASSERT( mResourceManager, "Resource manager cannot be null" );
        MKT_ASSERT( statsManager, "Stats manager cannot be null" );

        mPipelineLayout = resourceManager->GetPipelineLayout();
        mNodeStatistics = statsManager->GetNode( pass->mName );

        mCommands = pass->mCommandList;
    }

    auto CommandContext::BeginRender( const ContextRenderState &gs ) -> void {
        auto graphicsState{ RenderPassDescription{}
            .SetRenderArea( gs.mRenderArea ) };
        if ( gs.mDepthTarget.mRenderTarget.mHandle != FGResourceManager::kInvalidResourceHandle ) {
            graphicsState.AddDepthTarget( mResourceManager->Get( gs.mDepthTarget.mRenderTarget.mHandle ).mResource, gs.mDepthTarget.mLoadOp );
        }

        for (const auto& colorImage : gs.mCurrentRenderTargets) {
            TextureHandle texture{ mResourceManager->Get( colorImage.mRenderTarget.mHandle ).mResource };
            if (!texture.IsEmpty()) {
                graphicsState.AddRenderTarget( texture, colorImage.mClearColor, colorImage.mLoadOp, TextureSubresourceSet{
                    colorImage.mMipLevel,       // mBaseMipLevel
                    1,                          // mNumMipLevels
                    colorImage.mFaceIndex,      // mBaseArraySlice (0 = +X, 1 = -X, etc.)
                    1                           // mNumArraySlices (Face count)
                } );
            }
        }

        mCommands->BeginRenderPass( graphicsState );
    }

    auto CommandContext::EndRender() -> void {
        mCommands->EndRenderPass();
    }

    auto CommandContext::SetViewportState( const ViewportState &vs ) -> void {
        mCommands->SetViewportState( vs );
    }

    auto CommandContext::GetDeviceBufferAddress( FGBufferHandle handle ) -> core::u64 {
        MKT_ASSERT( mResourceManager, "FrameGraph Resource manager cannot be null" );
        return (u64)CacheGpuDeviceAddress( handle );
    }

    auto CommandContext::PushTexture_SRV( FGTextureHandle handle ) -> u32 {
        return CacheResourceDescriptorID_SRV( handle );
    }

    auto CommandContext::PushSampler( FGSamplerHandle handle ) -> u32 {
        return CacheResourceDescriptorID( handle );
    }

    auto CommandContext::PushBuffer_SRV( FGBufferHandle handle ) -> u32 {
        return CacheResourceDescriptorID_SRV( handle );
    }

    auto CommandContext::PushBuffer_UAV( FGBufferHandle handle ) -> u32 {
        return CacheResourceDescriptorID_UAV( handle );
    }

    auto CommandContext::SubmitBarriers( const ankerl::unordered_dense::map<FGResourceHandle, eastl::pair<eastl::string, FGBarrier>>& barriers ) -> void {
        if ( barriers.empty() ) {
            return;
        }

        struct PendingState {
            FGResource* mResource{};
            DeviceObject* mDeviceResource{};
            FGBarrierHistory mHistory{};
        };

        BarrierDescription description{};
        eastl::vector<PendingState> pendingStates{};
        pendingStates.reserve( barriers.size() );

        for ( const auto& [resourceID, namedBarrier] : barriers ) {
            const auto& required{ namedBarrier.second };
            FGResource& resource{ mResourceManager->Get( resourceID ) };
            auto* deviceResource{ checked_cast<DeviceObject*>( resource.mResource.GetPtr() ) };
            const ResourceStates stateBefore{ deviceResource->GetResourceState() };
            const ResourceStates stateAfter{ GetResourceState( required.mNewState, required.mNewAccess ) };
            const auto& history{ resource.mBarrierHistory };

            // A state revision catches external transitions even when they
            // return to the same layout before the graph next sees the resource.
            const bool historyIsCurrent{ history.IsCurrent( stateBefore, deviceResource->GetResourceStateRevision() ) };

            const PipelineStageFlags stageBefore{ historyIsCurrent ? history.mStages :
                stateBefore == ResourceStates::eUnknown ? PipelineStageFlagsBits::None : PipelineStageFlagsBits::AllCommands };
            const BarrierAccessFlags accessBefore{ historyIsCurrent ? history.mAccess : GetExternalAccess( stateBefore, resource.mType ) };
            const PipelineStageFlags stageAfter{ GetPipelineStages( required.mNewState ) };
            const BarrierAccessFlags accessAfter{ GetBarrierAccess( required.mNewState, required.mNewAccess, resource.mType ) };

            switch ( resource.mType ) {
                case FGResourceType::eTexture: {
                    description.AddTexture( TextureBarrierDescription{}
                        .SetTexture( checked_cast<ITexture*>( resource.mResource.GetPtr() ) )
                        .SetBeforeLayout( GetTextureLayout( stateBefore ) )
                        .SetBeforeStage( stageBefore )
                        .SetBeforeAccess( accessBefore )
                        .SetAfterLayout( GetTextureLayout( required.mNewState, required.mNewAccess ) )
                        .SetAfterStage( stageAfter )
                        .SetAfterAccess( accessAfter ) );
                    break;
                }
                case FGResourceType::eBuffer: {
                    description.AddBuffer( BufferBarrierDescription{}
                        .SetBuffer( checked_cast<IBuffer*>( resource.mResource.GetPtr() ) )
                        .SetBeforeStage( stageBefore )
                        .SetBeforeAccess( accessBefore )
                        .SetAfterStage( stageAfter )
                        .SetAfterAccess( accessAfter ) );
                    break;
                }
                default:
                    MKT_ASSERT( false, "Unknown resource type" );
                    continue;
            }

            // Keep all outstanding reader scopes until a write or layout change.
            // A barrier is still emitted between reads: it carries availability
            // from the preceding writer to consumers at a different shader stage.
            const auto nextHistory{ history.Next( stateBefore, deviceResource->GetResourceStateRevision(),
                stateAfter, stageAfter, accessAfter ) };
            pendingStates.push_back( PendingState{ &resource, deviceResource, nextHistory } );
        }

        mCommands->SetBarrier( description );

        // Publish only after recording. Compilation and skipped passes must not
        // advance either the layout or its access history.
        for ( auto& pending : pendingStates ) {
            pending.mDeviceResource->SetResourceState( pending.mHistory.mState );
            pending.mHistory.mResourceRevision = pending.mDeviceResource->GetResourceStateRevision();
            pending.mResource->mBarrierHistory = pending.mHistory;
        }
    }

    auto CommandContext::ImportTexture( TextureHandle handle ) -> FGTextureHandle {
        // Imported textures need to be synchronized externally
        // Before the frame graph runs the client needs to make sure the resource
        // is in the specific state the resource will be used in
        return mResourceManager->ImportTexture( handle );
    }

    auto CommandContext::ImportSampler( SamplerHandle ) -> FGSamplerHandle {
        return {};
    }

    auto CommandContext::ImportBuffer( BufferHandle handle ) -> FGBufferHandle {
        MKT_ASSERT( mResourceManager, "FrameGraph Resource manager cannot be null" );
        return mResourceManager->ImportBuffer(handle);
    }

    auto CommandContext::SetPolygonLineWidth( core::f32 width ) -> void {
        mCommands->SetPolygonLineWidth( width );
    }

    auto CommandContext::BindPipeline( FGPipelineHandle handle ) -> void {
        MKT_ASSERT( mResourceManager, "FrameGraph Resource manager cannot be null" );
        FGResource pipeline{ mResourceManager->Get( handle.mHandle ) };
        mCommands->BindPipeline( checked_cast<IPipeline*>( pipeline.mResource.GetPtr()) );
    }

    auto CommandContext::Draw( u32 vertexCount, u32 instanceCount ) -> void {
        IPipelineLayout* layout{ mPipelineLayout.GetPtr() };
        mCommands->SetPushConstants( layout, mPushConstantsData.data(), kMaxPushConstantSize, ShaderFlagsBits::All );
        mCommands->Draw( DrawArguments{}
            .SetVertexCount( vertexCount )
            .SetInstanceCount( instanceCount ) );
    }

    auto CommandContext::DrawIndirect( const DrawIndirectState& state ) -> void {
        if (state.mInstanceCount == 0) {
            return;
        }

        MKT_ASSERT( mResourceManager, "FrameGraph Resource manager cannot be null" );
        FGResource resource{ mResourceManager->Get( state.mIndirectBuffer.mHandle ) };

        IPipelineLayout* layout{ mPipelineLayout.GetPtr() };
        mCommands->SetPushConstants( layout, mPushConstantsData.data(), kMaxPushConstantSize, ShaderFlagsBits::All );
        mCommands->BindIndirectBuffer( checked_cast<IBuffer*>( resource.mResource.GetPtr() ) );
        mCommands->DrawIndirect( 0, state.mInstanceCount );
    }

    auto CommandContext::Dispatch( u32 groupX, u32 groupY, u32 groupZ ) -> void {
        IPipelineLayout* layout{ mPipelineLayout.GetPtr() };
        mCommands->SetPushConstants( layout, mPushConstantsData.data(), kMaxPushConstantSize, ShaderFlagsBits::All );
        mCommands->Dispatch( groupX, groupY, groupZ );
    }

    auto CommandContext::CopyBuffer( FGBufferHandle dstBuffer, FGBufferHandle srcBuffer ) -> void {
        MKT_ASSERT( mResourceManager, "FrameGraph Resource manager cannot be null" );
        IBuffer* srcResource{ CacheResource( srcBuffer ) };
        IBuffer* dstResource{ CacheResource( dstBuffer ) };
        mCommands->Copy( srcResource, dstResource );
    }

    auto CommandContext::CopyBuffer( FGBufferHandle dstBuffer, IBuffer* src, usize dstOffset ) -> void {
        MKT_ASSERT( mResourceManager, "FrameGraph Resource manager cannot be null" );
        IBuffer* resource{ CacheResource( dstBuffer ) };
        mCommands->Copy( src, resource, dstOffset );
    }

    auto CommandContext::CopyBuffer( FGBufferHandle dstBuffer, usize offset, const void* ptr, usize sizeBytes ) -> void {
        MKT_ASSERT( mResourceManager, "FrameGraph Resource manager cannot be null" );
        IBuffer* buffer{ CacheResource( dstBuffer ) };
        mCommands->Write( buffer, offset, ptr, sizeBytes );
    }

    auto CommandContext::Copy( FGBufferHandle dstBuffer, FGTextureHandle srcImage ) -> void {
        MKT_ASSERT( mResourceManager, "FrameGraph Resource manager cannot be null" );
        IBuffer* buffer{ CacheResource( dstBuffer ) };
        ITexture* image{ CacheResource( srcImage ) };

        mCommands->Copy( buffer, image );
    }

    auto CommandContext::CopyTexture( FGTextureHandle destImage, const void* data, core::usize size ) -> void {
        MKT_ASSERT( mResourceManager, "FrameGraph Resource manager cannot be null" );
        ITexture* image{ CacheResource( destImage ) };
        mCommands->Write( image, data, size );
    }

    auto CommandContext::CacheGpuDeviceAddress( FGBufferHandle handle ) -> rhi::DeviceAddress {
        MKT_ASSERT( mResourceManager, "FrameGraph Resource manager cannot be null" );
        auto itFind{ mCachedBda.find( handle.mHandle ) };
        if (itFind == mCachedBda.end()) {
            FGResource resource{ mResourceManager->Get( handle.mHandle ) };
            IBuffer* buffer{ checked_cast<IBuffer*>( resource.mResource.GetPtr() ) };
            itFind = mCachedBda.try_emplace( itFind, handle.mHandle, buffer->GetGpuDeviceAddress() );
        }

        return itFind->second;
    }

    auto CommandContext::CacheResource( FGBufferHandle handle ) -> IBuffer* {
        MKT_ASSERT( mResourceManager, "FrameGraph Resource manager cannot be null" );
        auto itFind{ mCachedBuffers.find( handle.mHandle ) };
        if (itFind == mCachedBuffers.end()) {
            FGResource resource{ mResourceManager->Get( handle.mHandle ) };
            IBuffer* buffer{ checked_cast<IBuffer*>( resource.mResource.GetPtr() ) };
            itFind = mCachedBuffers.try_emplace( itFind, handle.mHandle, buffer );
        }

        return itFind->second;
    }

    auto CommandContext::CacheResource( FGTextureHandle handle ) -> ITexture* {
        MKT_ASSERT( mResourceManager, "FrameGraph Resource manager cannot be null" );
        auto itFind{ mCachedTextures.find( handle.mHandle ) };
        if (itFind == mCachedTextures.end()) {
            FGResource resource{ mResourceManager->Get( handle.mHandle ) };
            ITexture* texture{ checked_cast<ITexture*>( resource.mResource.GetPtr() ) };
            itFind = mCachedTextures.try_emplace( itFind, handle.mHandle, texture );
        }

        return itFind->second;
    }

    auto CommandContext::CacheResourceDescriptorID_SRV( FGBufferHandle handle ) -> core::u32 {
        MKT_ASSERT( mResourceManager, "FrameGraph Resource manager cannot be null" );
        auto itFind{ mCachedShaderBuffers_SRV.find( handle.mHandle ) };
        if (itFind == mCachedShaderBuffers_SRV.end()) {
            u32 index{ mResourceManager->AllocateBufferIndex_SRV( handle.mHandle ) };
            itFind = mCachedShaderBuffers_SRV.try_emplace( itFind, handle.mHandle, index );
        }

        return itFind->second;
    }

    auto CommandContext::CacheResourceDescriptorID_UAV( FGBufferHandle handle ) -> core::u32 {
        MKT_ASSERT( mResourceManager, "FrameGraph Resource manager cannot be null" );
        auto itFind{ mCachedShaderBuffers_UAV.find( handle.mHandle ) };
        if (itFind == mCachedShaderBuffers_UAV.end()) {
            u32 index{ mResourceManager->AllocateBufferIndex_UAV( handle.mHandle ) };
            itFind = mCachedShaderBuffers_UAV.try_emplace( itFind, handle.mHandle, index );
        }

        return itFind->second;
    }

    auto CommandContext::CacheResourceDescriptorID_SRV( FGTextureHandle handle ) -> core::u32 {
        MKT_ASSERT( mResourceManager, "FrameGraph Resource manager cannot be null" );
        auto itFind{ mCachedShaderTextures_SRV.find( handle.mHandle ) };
        if (itFind == mCachedShaderTextures_SRV.end()) {
            u32 index{ mResourceManager->AllocateTextureIndex_SRV( handle.mHandle ) };
            itFind = mCachedShaderTextures_SRV.try_emplace( itFind, handle.mHandle, index );
        }

        return itFind->second;
    }

    auto CommandContext::CacheResourceDescriptorID_UAV( FGTextureHandle handle ) -> core::u32 {
        MKT_ASSERT( mResourceManager, "FrameGraph Resource manager cannot be null" );
        auto itFind{ mCachedShaderTextures_UAV.find( handle.mHandle ) };
        if (itFind == mCachedShaderTextures_UAV.end()) {
            u32 index{ mResourceManager->AllocateTextureIndex_UAV( handle.mHandle ) };
            itFind = mCachedShaderTextures_UAV.try_emplace( itFind, handle.mHandle, index );
        }

        return itFind->second;

    }

    auto CommandContext::CacheResourceDescriptorID( FGSamplerHandle handle ) -> core::u32 {
        MKT_ASSERT( mResourceManager, "FrameGraph Resource manager cannot be null" );
        auto itFind{ mCachedShaderSamplers.find( handle.mHandle ) };
        if (itFind == mCachedShaderSamplers.end()) {
            u32 index{ mResourceManager->AllocateSamplerIndex( handle.mHandle ) };
            itFind = mCachedShaderSamplers.try_emplace( itFind, handle.mHandle, index );
        }

        return itFind->second;
    }
}// namespace mikoto::renderer
