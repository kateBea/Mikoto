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

#include <Core/Core.hh>
#include <Core/String.hh>
#include <Core/Types.hh>

#include <Renderer/Rhi/CommandList.hh>

namespace mikoto::renderer::rhi {

    using namespace mikoto::core;
    using namespace mikoto::memory;

    auto CommandListBeginDescription::SetScopeName( eastl::string_view name ) -> CommandListBeginDescription {
        mScopeName = name;
        return *this;
    }

    ICommandList::ICommandList( QueueType queueType, core::u32 selfManagedCommandListCount )
        : mQueueType{ queueType }, mSelfManagedCommandListCount{ selfManagedCommandListCount }
    {}

    auto ICommandList::GetQueueType() const -> QueueType {
        return mQueueType;
    }

    auto RenderPassDescription::SetScopeName( eastl::string_view name ) -> RenderPassDescription & {
        mName = name;
        return *this;
    }

    auto RenderPassDescription::SetRenderArea( const Rect &rec ) -> RenderPassDescription & {
        mRenderArea = rec;
        return *this;
    }

    auto RenderPassDescription::AddDepthTarget( TextureHandle target, LoadOp op ) -> RenderPassDescription & {
        mDepthTarget = RenderTargetState{
            .mClearColor = kColorWhite,
            .mLoadOp = op,
            .mRenderTarget = target,
        };

        return *this;
    }

    auto RenderPassDescription::AddRenderTarget( TextureHandle target, const Color &c, LoadOp op, TextureSubresourceSet set ) -> RenderPassDescription & {
        mCurrentRenderTargets.emplace_back( RenderTargetState{
                .mClearColor = c,
                .mLoadOp = op,
                .mRenderTarget = std::move( target ),
                .mSubresourceSet = set,
        } );

        return *this;
    }

    auto BindResourcesDescription::SetPipelineLayout( IPipelineLayout *layout ) -> BindResourcesDescription & {
        mPipelineLayout = layout;
        return *this;
    }

    auto BindResourcesDescription::SetBindPoint( PipelineType bindPoint ) -> BindResourcesDescription & {
        mBindPoint = bindPoint;
        return *this;
    }

    auto BindResourcesDescription::SetPushConstants( const void *ptr, core::usize sizeBytes, ShaderFlags stage ) -> BindResourcesDescription & {
        MKT_ASSERT( sizeBytes <= sizeof( mPushConstants ), "Exceeded push constants size" );
        mPushConstantVisibility = stage;
        eastl::copy_n( as<core::ubyte *>( ptr ), sizeBytes, mPushConstants.data() );
        return *this;
    }

    auto BindResourcesDescription::AddResourceSet( u32 bindingIndex, IBindingTable *set ) -> BindResourcesDescription & {
        MKT_ASSERT( set, "ResourceSet is null" );
        mResourceSets.insert_or_assign( bindingIndex, set );

        return *this;
    }

    auto BufferBarrierDescription::SetBuffer( IBuffer* buffer ) -> BufferBarrierDescription& {
        MKT_ASSERT( buffer, "Cannot set a null buffer barrier resource" );
        mBuffer = buffer;
        return *this;
    }

    auto BufferBarrierDescription::SetRange( BufferRange range ) -> BufferBarrierDescription& {
        mRange = range;
        return *this;
    }

    auto BufferBarrierDescription::SetBeforeStage( PipelineStageFlags stage ) -> BufferBarrierDescription& {
        mStageBefore = stage;
        return *this;
    }

    auto BufferBarrierDescription::SetBeforeAccess( BarrierAccessFlags access ) -> BufferBarrierDescription& {
        mAccessBefore = access;
        return *this;
    }

    auto BufferBarrierDescription::SetAfterStage( PipelineStageFlags stage ) -> BufferBarrierDescription& {
        mStageAfter = stage;
        return *this;
    }

    auto BufferBarrierDescription::SetAfterAccess( BarrierAccessFlags access ) -> BufferBarrierDescription& {
        mAccessAfter = access;
        return *this;
    }

    auto TextureBarrierDescription::SetTexture( ITexture* texture ) -> TextureBarrierDescription& {
        MKT_ASSERT( texture, "Cannot set a null texture barrier resource" );
        mTexture = texture;
        return *this;
    }

    auto TextureBarrierDescription::SetSubresourceSet( TextureSubresourceSet subresources ) -> TextureBarrierDescription& {
        mSubresourceSet = subresources;
        return *this;
    }

    auto TextureBarrierDescription::SetBeforeLayout( TextureLayoutFlags layout ) -> TextureBarrierDescription& {
        mLayoutBefore = layout;
        return *this;
    }

    auto TextureBarrierDescription::SetBeforeStage( PipelineStageFlags stage ) -> TextureBarrierDescription& {
        mStageBefore = stage;
        return *this;
    }

    auto TextureBarrierDescription::SetBeforeAccess( BarrierAccessFlags access ) -> TextureBarrierDescription& {
        mAccessBefore = access;
        return *this;
    }

    auto TextureBarrierDescription::SetAfterLayout( TextureLayoutFlags layout ) -> TextureBarrierDescription& {
        mLayoutAfter = layout;
        return *this;
    }

    auto TextureBarrierDescription::SetAfterStage( PipelineStageFlags stage ) -> TextureBarrierDescription& {
        mStageAfter = stage;
        return *this;
    }

    auto TextureBarrierDescription::SetAfterAccess( BarrierAccessFlags access ) -> TextureBarrierDescription& {
        mAccessAfter = access;
        return *this;
    }

    auto BarrierDescription::AddBuffer( const BufferBarrierDescription& description ) -> BarrierDescription& {
        mBuffers.push_back( description );
        return *this;
    }

    auto BarrierDescription::AddBuffer( BufferBarrierDescription&& description ) -> BarrierDescription& {
        mBuffers.push_back( eastl::move( description ) );
        return *this;
    }

    auto BarrierDescription::AddTexture( const TextureBarrierDescription& description ) -> BarrierDescription& {
        mTextures.push_back( description );
        return *this;
    }

    auto BarrierDescription::AddTexture( TextureBarrierDescription&& description ) -> BarrierDescription& {
        mTextures.push_back( eastl::move( description ) );
        return *this;
    }

    TransitionDescription::TransitionDescription( IBuffer* buffer, ResourceStates state ) {
        mBuffers.emplace_back( BufferTransition{
            .mBuffer = buffer,
            .mState = state,
        } );
    }

    TransitionDescription::TransitionDescription( ITexture* texture, ResourceStates state ) {
        mTextures.emplace_back( TextureTransition{
            .mTexture = texture,
            .mState = state,
        } );
    }

    auto TransitionDescription::AddBuffer( IBuffer* buffer, ResourceStates state ) -> TransitionDescription& {
        MKT_ASSERT( buffer, "Cannot add a null transition buffer" );
        mBuffers.push_back( { .mBuffer = buffer, .mState = state } );
        return *this;
    }

    auto TransitionDescription::AddTexture( ITexture* texture, ResourceStates state ) -> TransitionDescription& {
        MKT_ASSERT( texture, "Cannot add a null transition texture" );
        mTextures.push_back( { .mTexture = texture, .mState = state } );
        return *this;
    }
}
