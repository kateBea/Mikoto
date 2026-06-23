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

    MKT_NODISCARD constexpr auto GetResourceState(FGResourceState state) -> ResourceStates {
    switch (state) {
        case FGResourceState::eUnknown:                return ResourceStates::eUnknown;

        // Buffers
        case FGResourceState::eConstantBuffer:         return ResourceStates::eConstantBuffer;
        case FGResourceState::eVertexBuffer:           return ResourceStates::eVertexBuffer;
        case FGResourceState::eIndexBuffer:            return ResourceStates::eIndexBuffer;
        case FGResourceState::eIndirectArgument:       return ResourceStates::eIndirectArgument;
        case FGResourceState::eShaderResource:         return ResourceStates::eShaderResource;
        case FGResourceState::eUnorderedAccess:        return ResourceStates::eUnorderedAccess;

        // Images
        case FGResourceState::eRenderTarget:           return ResourceStates::eRenderTarget;
        case FGResourceState::eDepthWrite:             return ResourceStates::eDepthWrite;
        case FGResourceState::eDepthRead:              return ResourceStates::eDepthRead;

        // Transfer
        case FGResourceState::eCopyDest:               return ResourceStates::eCopyDest;
        case FGResourceState::eCopySource:             return ResourceStates::eCopySource;
        case FGResourceState::eResolveDest:            return ResourceStates::eResolveDest;
        case FGResourceState::eResolveSource:          return ResourceStates::eResolveSource;

        // Present
        case FGResourceState::ePresent:                return ResourceStates::ePresent;

        // Raytracing
        case FGResourceState::eAccelStructRead:        return ResourceStates::eAccelStructRead;
        case FGResourceState::eAccelStructWrite:       return ResourceStates::eAccelStructWrite;
        case FGResourceState::eAccelStructBuildInput:  return ResourceStates::eAccelStructBuildInput;
        case FGResourceState::eAccelStructBuildBlas:   return ResourceStates::eAccelStructBuildBlas;
    }

    // Fallback (should never happen if enum is exhaustive)
    return ResourceStates::eUnknown;
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

    CommandContext::CommandContext(  FGNode* pass, FGResourceManager* resourceManager )
        : mNode{ pass }, mResourceManager{ resourceManager } {
        MKT_ASSERT( mNode, "Frame graph node cannot be null" );
        MKT_ASSERT( mResourceManager, "Resource manager cannot be null" );
    }

    auto CommandContext::BeginPass( CommandListHandle cmd ) -> void {
        mCommands = cmd;
    }

    auto CommandContext::EndPass() -> void {
        if (mNode->mExecutionPolicy == FGExecutionPolicy::eOnce ||
            mNode->mExecutionPolicy == FGExecutionPolicy::eOnChange) {
            mNode->mIsAlive = false;
        }
    }

    auto CommandContext::BeginRender( const ContextRenderState &gs ) -> void {
        auto graphicsState{ GraphicsState{}
            .SetRenderArea( gs.mRenderArea )
            .SetScopeName( string::Format( "Render: {}", mNode->mName ))
        };

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

        mCommands->BeginRendering( graphicsState );
    }

    auto CommandContext::EndRender() -> void {
        mCommands->EndRendering();
    }

    auto CommandContext::SetViewportState( const ViewportState &vs ) -> void {
        mCommands->SetViewportState( vs );
    }

    auto CommandContext::PushTexture_SRV( FGTextureHandle handle ) -> u32 {
        MKT_ASSERT( mResourceManager, "FrameGraph Resource manager cannot be null" );
        FGResource resource{ mResourceManager->Get( handle.mHandle ) };
        TextureHandle texture{ resource.mResource };
        return mResourceManager->PushTexture_SRV( handle.mHandle );
    }

    auto CommandContext::PushSampler( FGSamplerHandle handle ) -> u32 {
        MKT_ASSERT( mResourceManager, "FrameGraph Resource manager cannot be null" );
        return mResourceManager->PushSampler( handle.mHandle );
    }

    auto CommandContext::PushBuffer_SRV( FGBufferHandle handle ) -> u32 {
        // TODO: add checks if u registered the resource to use as SRV in the setup lambda

        MKT_ASSERT( mResourceManager, "FrameGraph Resource manager cannot be null" );
        return mResourceManager->PushBuffer_SRV( handle.mHandle );
    }

    auto CommandContext::PushBuffer_UAV( FGBufferHandle handle ) -> u32 {
        MKT_ASSERT( mResourceManager, "FrameGraph Resource manager cannot be null" );
        return mResourceManager->PushBuffer_UAV( handle.mHandle );
    }

    auto CommandContext::PushBuffer_Constant( FGBufferHandle handle ) -> u32 {
        MKT_ASSERT( mResourceManager, "FrameGraph Resource manager cannot be null" );
        return mResourceManager->PushBuffer_Constant( handle.mHandle );
    }

    auto CommandContext::CommitBarriers( const ankerl::unordered_dense::map<FGResourceHandle, FGBarrier>& barriers ) -> void {
        if (barriers.empty()) {
            return;
        }

        for (const auto& [resourceID, barrier] : barriers) {
            FGResource resource{ mResourceManager->Get( barrier.mResourceID ) };
            auto desired{ GetResourceState(barrier.mNewState) };

            switch (resource.mType) {
                case FGResourceType::eTexture:
                    mCommands->BeginTrackingState( checked_cast<ITexture*>( resource.mResource.GetRaw() ), desired );
                    break;
                case FGResourceType::eBuffer:
                    mCommands->BeginTrackingState( checked_cast<IBuffer*>( resource.mResource.GetRaw() ), desired );
                    break;
                default:
                    MKT_ASSERT( false, "Unknown resource type" );
                    break;
            }
        }

        mCommands->CommitBarriers();
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

    auto CommandContext::BindPipeline( FGPipelineHandle handle ) -> void {
        MKT_ASSERT( mResourceManager, "FrameGraph Resource manager cannot be null" );
        FGResource pipeline{ mResourceManager->Get( handle.mHandle ) };
        mCommands->BindPipeline( checked_cast<IPipeline*>( pipeline.mResource.GetRaw()) );

        mCurrentPipeline = checked_cast<IPipeline*>( pipeline.mResource.GetRaw());
    }

    auto CommandContext::Draw( u32 vertexCount, u32 instanceCount ) -> void {
        IPipelineLayout* layout{ mCurrentPipeline->GetPipelineLayout().GetRaw() };
        mCommands->SetPushConstants( layout, mPushConstantsData.data(), kMaxPushConstantSize, ShaderFlagsBits::kAll );
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

        IPipelineLayout* layout{ mCurrentPipeline->GetPipelineLayout().GetRaw() };
        mCommands->SetPushConstants( layout, mPushConstantsData.data(), kMaxPushConstantSize, ShaderFlagsBits::kAll );
        mCommands->BindIndirectBuffer( checked_cast<IBuffer*>( resource.mResource.GetRaw() ) );
        mCommands->DrawIndirect( 0, state.mInstanceCount );
    }

    auto CommandContext::Dispatch( u32 groupX, u32 groupY, u32 groupZ ) -> void {
        IPipelineLayout* layout{ mCurrentPipeline->GetPipelineLayout().GetRaw() };
        mCommands->SetPushConstants( layout, mPushConstantsData.data(), kMaxPushConstantSize, ShaderFlagsBits::kAll );
        mCommands->Dispatch( groupX, groupY, groupZ );
    }

    auto CommandContext::CopyBuffer( FGBufferHandle dstBuffer, FGBufferHandle srcBuffer ) -> void {
        MKT_ASSERT( mResourceManager, "FrameGraph Resource manager cannot be null" );
        FGResource srcResource{ mResourceManager->Get( srcBuffer.mHandle ) };
        FGResource dstResource{ mResourceManager->Get( dstBuffer.mHandle ) };
        mCommands->Copy( checked_cast<IBuffer*>( srcResource.mResource.GetRaw() ), checked_cast<IBuffer*>( dstResource.mResource.GetRaw() ) );
    }

    auto CommandContext::CopyBuffer( FGBufferHandle dstBuffer, IBuffer* src, size_t dstOffset ) -> void {
        MKT_ASSERT( mResourceManager, "FrameGraph Resource manager cannot be null" );
        FGResource resource{ mResourceManager->Get( dstBuffer.mHandle ) };
        mCommands->Copy( src, checked_cast<IBuffer*>( resource.mResource.GetRaw() ), dstOffset );
    }

    auto CommandContext::Copy( FGBufferHandle dstBuffer, FGTextureHandle srcImage ) -> void {
        MKT_ASSERT( mResourceManager, "FrameGraph Resource manager cannot be null" );
        FGResource buffer{ mResourceManager->Get( dstBuffer.mHandle ) };
        FGResource image{ mResourceManager->Get( srcImage.mHandle ) };

        mCommands->Copy( checked_cast<IBuffer*>( buffer.mResource.GetRaw() ), checked_cast<ITexture*>( image.mResource.GetRaw() ) );
    }

    auto CommandContext::CopyBuffer( FGBufferHandle dstBuffer, size_t offset, const void* ptr, size_t sizeBytes ) -> void {
        MKT_ASSERT( mResourceManager, "FrameGraph Resource manager cannot be null" );
        FGResource resource{ mResourceManager->Get( dstBuffer.mHandle ) };
        mCommands->Write( checked_cast<IBuffer*>( resource.mResource.GetRaw() ), offset, ptr, sizeBytes );
    }
}// namespace mikoto::renderer