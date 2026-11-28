//    Copyright 2026 ケイト
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

#include <EASTL/utility.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Logging/Assert.hh>

#include <Video/H264Parser.hh>

#include <Renderer/Rhi/CommandList.hh>
#include <Renderer/Rhi/VideoDecoder.hh>

namespace mikoto::renderer::rhi {
    using namespace mikoto::core;
    using namespace mikoto::memory;

    auto ICommandList::InitializeDenoiser( IDenoiser * ) -> bool {
        return false;
    }

    auto ICommandList::DispatchDenoiser( IDenoiser *, const DenoiserDispatch & ) -> DenoiserResult {
        return DenoiserResult::eUnavailable;
    }

    auto operator+=( CommandListStatistics& total,
        const CommandListStatistics& statistics ) -> CommandListStatistics& {
        total.mDrawCallCount += statistics.mDrawCallCount;
        total.mIndirectDrawCallCount += statistics.mIndirectDrawCallCount;
        total.mDispatchCallCount += statistics.mDispatchCallCount;
        total.mRayDispatchCallCount += statistics.mRayDispatchCallCount;
        total.mPipelineBindCount += statistics.mPipelineBindCount;
        total.mBarriers.mImageCount += statistics.mBarriers.mImageCount;
        total.mBarriers.mBufferCount += statistics.mBarriers.mBufferCount;
        total.mBarriers.mMemoryCount += statistics.mBarriers.mMemoryCount;
        total.mBarriers.mCallCount += statistics.mBarriers.mCallCount;

        return total;
    }

    auto operator-( const CommandListStatistics& current,
        const CommandListStatistics& previous ) -> CommandListStatistics {
        CommandListStatistics difference{};

        difference.mDrawCallCount = current.mDrawCallCount - previous.mDrawCallCount;
        difference.mIndirectDrawCallCount = current.mIndirectDrawCallCount - previous.mIndirectDrawCallCount;
        difference.mDispatchCallCount = current.mDispatchCallCount - previous.mDispatchCallCount;
        difference.mRayDispatchCallCount = current.mRayDispatchCallCount - previous.mRayDispatchCallCount;
        difference.mPipelineBindCount = current.mPipelineBindCount - previous.mPipelineBindCount;
        difference.mBarriers.mImageCount = current.mBarriers.mImageCount - previous.mBarriers.mImageCount;
        difference.mBarriers.mBufferCount = current.mBarriers.mBufferCount - previous.mBarriers.mBufferCount;
        difference.mBarriers.mMemoryCount = current.mBarriers.mMemoryCount - previous.mBarriers.mMemoryCount;
        difference.mBarriers.mCallCount = current.mBarriers.mCallCount - previous.mBarriers.mCallCount;

        return difference;
    }

    auto CommandListBeginDescription::SetScopeName( eastl::string_view name ) -> CommandListBeginDescription {
        MKT_BEGIN_PROFILER();
        mScopeName = name;
        return *this;
    }


    auto RenderPassDescription::SetScopeName( eastl::string_view name ) -> RenderPassDescription& {
        MKT_BEGIN_PROFILER();
        mName = name;
        return *this;
    }

    auto RenderPassDescription::SetRenderArea( const Rect& rec ) -> RenderPassDescription& {
        MKT_BEGIN_PROFILER();
        mRenderArea = rec;
        return *this;
    }

    auto RenderPassDescription::AddDepthTarget( TextureHandle target, LoadOp op ) -> RenderPassDescription& {
        MKT_BEGIN_PROFILER();
        mDepthTarget = RenderTargetState{
            .mClearColor = kColorWhite,
            .mLoadOp = op,
            .mRenderTarget = target,
        };

        return *this;
    }

    auto RenderPassDescription::AddRenderTarget( TextureHandle target, const Color& c, LoadOp op, TextureSubresourceSet set ) -> RenderPassDescription& {
        MKT_BEGIN_PROFILER();
        mCurrentRenderTargets.emplace_back( RenderTargetState{
            .mClearColor = c,
            .mLoadOp = op,
            .mRenderTarget = eastl::move( target ),
            .mSubresourceSet = set,
        } );

        return *this;
    }

    auto BindResourcesDescription::SetPipelineLayout( IPipelineLayout* layout ) -> BindResourcesDescription& {
        MKT_BEGIN_PROFILER();
        mPipelineLayout = layout;
        return *this;
    }

    auto BindResourcesDescription::SetBindPoint( PipelineType bindPoint ) -> BindResourcesDescription& {
        MKT_BEGIN_PROFILER();
        mBindPoint = bindPoint;
        return *this;
    }

    auto BindResourcesDescription::SetPushConstants( const void* ptr, core::usize sizeBytes, ShaderFlags stage ) -> BindResourcesDescription& {
        MKT_BEGIN_PROFILER();
        MKT_ASSERT( sizeBytes <= sizeof( mPushConstants ), "Exceeded push constants size" );
        mPushConstantVisibility = stage;
        eastl::copy_n( core::as<core::ubyte*>( ptr ), sizeBytes, mPushConstants.data() );
        return *this;
    }

    auto BindResourcesDescription::AddResourceSet( core::u32 bindingIndex, IBindingTable* set ) -> BindResourcesDescription& {
        MKT_BEGIN_PROFILER();
        MKT_ASSERT( set, "ResourceSet is null" );
        mResourceSets.insert_or_assign( bindingIndex, set );

        return *this;
    }

    auto BufferBarrierDescription::SetBuffer( IBuffer* buffer ) -> BufferBarrierDescription& {
        MKT_BEGIN_PROFILER();
        MKT_ASSERT( buffer, "Cannot set a null buffer barrier resource" );
        mBuffer = buffer;
        return *this;
    }

    auto BufferBarrierDescription::SetRange( BufferRange range ) -> BufferBarrierDescription& {
        MKT_BEGIN_PROFILER();
        mRange = range;
        return *this;
    }

    auto BufferBarrierDescription::SetBeforeStage( PipelineStageFlags stage ) -> BufferBarrierDescription& {
        MKT_BEGIN_PROFILER();
        mStageBefore = stage;
        return *this;
    }

    auto BufferBarrierDescription::SetBeforeAccess( BarrierAccessFlags access ) -> BufferBarrierDescription& {
        MKT_BEGIN_PROFILER();
        mAccessBefore = access;
        return *this;
    }

    auto BufferBarrierDescription::SetAfterStage( PipelineStageFlags stage ) -> BufferBarrierDescription& {
        MKT_BEGIN_PROFILER();
        mStageAfter = stage;
        return *this;
    }

    auto BufferBarrierDescription::SetAfterAccess( BarrierAccessFlags access ) -> BufferBarrierDescription& {
        MKT_BEGIN_PROFILER();
        mAccessAfter = access;
        return *this;
    }

    auto TextureBarrierDescription::SetTexture( ITexture* texture ) -> TextureBarrierDescription& {
        MKT_BEGIN_PROFILER();
        MKT_ASSERT( texture, "Cannot set a null texture barrier resource" );
        mTexture = texture;
        return *this;
    }

    auto TextureBarrierDescription::SetSubresourceSet( TextureSubresourceSet subresources ) -> TextureBarrierDescription& {
        MKT_BEGIN_PROFILER();
        mSubresourceSet = subresources;
        return *this;
    }

    auto TextureBarrierDescription::SetBeforeLayout( TextureLayoutFlags layout ) -> TextureBarrierDescription& {
        MKT_BEGIN_PROFILER();
        mLayoutBefore = layout;
        return *this;
    }

    auto TextureBarrierDescription::SetBeforeStage( PipelineStageFlags stage ) -> TextureBarrierDescription& {
        MKT_BEGIN_PROFILER();
        mStageBefore = stage;
        return *this;
    }

    auto TextureBarrierDescription::SetBeforeAccess( BarrierAccessFlags access ) -> TextureBarrierDescription& {
        MKT_BEGIN_PROFILER();
        mAccessBefore = access;
        return *this;
    }

    auto TextureBarrierDescription::SetAfterLayout( TextureLayoutFlags layout ) -> TextureBarrierDescription& {
        MKT_BEGIN_PROFILER();
        mLayoutAfter = layout;
        return *this;
    }

    auto TextureBarrierDescription::SetAfterStage( PipelineStageFlags stage ) -> TextureBarrierDescription& {
        MKT_BEGIN_PROFILER();
        mStageAfter = stage;
        return *this;
    }

    auto TextureBarrierDescription::SetAfterAccess( BarrierAccessFlags access ) -> TextureBarrierDescription& {
        MKT_BEGIN_PROFILER();
        mAccessAfter = access;
        return *this;
    }

    auto BarrierDescription::AddBuffer( const BufferBarrierDescription& description ) -> BarrierDescription& {
        MKT_BEGIN_PROFILER();
        mBuffers.push_back( description );
        return *this;
    }

    auto BarrierDescription::AddBuffer( BufferBarrierDescription&& description ) -> BarrierDescription& {
        MKT_BEGIN_PROFILER();
        mBuffers.push_back( eastl::move( description ) );
        return *this;
    }

    auto BarrierDescription::AddTexture( const TextureBarrierDescription& description ) -> BarrierDescription& {
        MKT_BEGIN_PROFILER();
        mTextures.push_back( description );
        return *this;
    }

    auto BarrierDescription::AddTexture( TextureBarrierDescription&& description ) -> BarrierDescription& {
        MKT_BEGIN_PROFILER();
        mTextures.push_back( eastl::move( description ) );
        return *this;
    }

    TransitionDescription::TransitionDescription( IBuffer* buffer, ResourceStates state ) {
        MKT_BEGIN_PROFILER();
        mBuffers.emplace_back( BufferTransition{
            .mBuffer = buffer,
            .mState = state,
        } );
    }

    TransitionDescription::TransitionDescription( ITexture* texture, ResourceStates state ) {
        MKT_BEGIN_PROFILER();
        mTextures.emplace_back( TextureTransition{
            .mTexture = texture,
            .mState = state,
        } );
    }

    auto TransitionDescription::AddBuffer( IBuffer* buffer, ResourceStates state ) -> TransitionDescription& {
        MKT_BEGIN_PROFILER();
        MKT_ASSERT( buffer, "Cannot add a null transition buffer" );
        mBuffers.push_back( { .mBuffer = buffer, .mState = state } );
        return *this;
    }

    auto TransitionDescription::AddTexture( ITexture* texture, ResourceStates state ) -> TransitionDescription& {
        MKT_BEGIN_PROFILER();
        MKT_ASSERT( texture, "Cannot add a null transition texture" );
        mTextures.push_back( { .mTexture = texture, .mState = state } );
        return *this;
    }


    auto ViewportState::AddViewport( const Viewport& v ) -> ViewportState& {
        MKT_BEGIN_PROFILER();
        mViewports.push_back( v );
        return *this;
    }


    auto ViewportState::AddScissorRect( const Rect& r ) -> ViewportState& {
        MKT_BEGIN_PROFILER();
        mScissorRects.push_back( r );
        return *this;
    }


    auto ViewportState::AddViewportAndScissorRect( const Viewport& v ) -> ViewportState& {
        MKT_BEGIN_PROFILER();
        return AddViewport( v ).AddScissorRect( Rect( v ) );
    }


    auto VertexBufferBinding::SetBuffer( IBuffer* value ) -> VertexBufferBinding& {
        MKT_BEGIN_PROFILER();
        mBuffer = value;
        return *this;
    }


    auto VertexBufferBinding::SetBufferBinding( core::u32 value ) -> VertexBufferBinding& {
        MKT_BEGIN_PROFILER();
        mSlot = value;
        return *this;
    }


    auto VertexBufferBinding::SetOffset( core::u64 value ) -> VertexBufferBinding& {
        MKT_BEGIN_PROFILER();
        mOffset = value;
        return *this;
    }


    auto VertexBufferBinding::SetElementStride( core::u64 value ) -> VertexBufferBinding& {
        MKT_BEGIN_PROFILER();
        mElementStride = value;
        return *this;
    }


    auto IndexBufferBinding::SetBuffer( IBuffer* value ) -> IndexBufferBinding& {
        MKT_BEGIN_PROFILER();
        mBuffer = value;
        return *this;
    }


    auto IndexBufferBinding::SetFormat( Format value ) -> IndexBufferBinding& {
        MKT_BEGIN_PROFILER();
        mFormat = value;
        return *this;
    }


    auto IndexBufferBinding::SetOffset( core::u32 value ) -> IndexBufferBinding& {
        MKT_BEGIN_PROFILER();
        mOffset = value;
        return *this;
    }


    auto VertexBufferBinding::operator==( const VertexBufferBinding& b ) const -> bool {
        MKT_BEGIN_PROFILER();
        return mBuffer == b.mBuffer && mSlot == b.mSlot && mOffset == b.mOffset;
    }

    auto VertexBufferBinding::operator!=( const VertexBufferBinding& b ) const -> bool {
        MKT_BEGIN_PROFILER();
        return !( *this == b );
    }


    auto IndexBufferBinding::operator==( const IndexBufferBinding& b ) const -> bool {
        MKT_BEGIN_PROFILER();
        return mBuffer == b.mBuffer && mFormat == b.mFormat && mOffset == b.mOffset;
    }

    auto IndexBufferBinding::operator!=( const IndexBufferBinding& b ) const -> bool {
        MKT_BEGIN_PROFILER();
        return !( *this == b );
    }
}
