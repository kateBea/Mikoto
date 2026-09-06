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

    ICommandList::ICommandList( QueueType queueType )
        : mQueueType{ queueType }
    {}

    auto ICommandList::GetQueueType() const -> QueueType {
        return mQueueType;
    }

    auto RenderDescription::SetScopeName( eastl::string_view name ) -> RenderDescription & {
        mName = name;
        return *this;
    }

    auto RenderDescription::SetRenderArea( const Rect &rec ) -> RenderDescription & {
        mRenderArea = rec;
        return *this;
    }

    auto RenderDescription::AddDepthTarget( TextureHandle target, LoadOp op ) -> RenderDescription & {
        mDepthTarget = RenderTargetState{
            .mClearColor = kColorWhite,
            .mLoadOp = op,
            .mRenderTarget = target,
        };

        return *this;
    }

    auto RenderDescription::AddRenderTarget( TextureHandle target, const Color &c, LoadOp op, TextureSubresourceSet set ) -> RenderDescription & {
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

    auto BindResourcesDescription::SetPushConstants( const void *ptr, size_t sizeBytes, ShaderFlags stage ) -> BindResourcesDescription & {
        MKT_ASSERT( sizeBytes <= sizeof( mPushConstants ), "Exceeded push constants size" );
        mPushConstantVisibility = stage;
        eastl::copy_n( as<byte_t *>( ptr ), sizeBytes, mPushConstants.data() );
        return *this;
    }

    auto BindResourcesDescription::AddResourceSet( u32 bindingIndex, IBindingTable *set ) -> BindResourcesDescription & {
        MKT_ASSERT( set, "ResourceSet is null" );
        mResourceSets.insert_or_assign( bindingIndex, set );

        return *this;
    }

    auto BufferBarrierDescription::SetBuffer( BufferHandle handle ) -> BufferBarrierDescription & {
        MKT_ASSERT( !handle.IsEmpty(), "Cannot pass en empty handle" );
        mBuffer = handle.GetRaw();
        return *this;
    }

    auto BufferBarrierDescription::SetRange( BufferRange range ) -> BufferBarrierDescription & {
        mRange = range;
        return *this;
    }

    auto BufferBarrierDescription::SetBeforeStage( PipelineStageFlags stage ) -> BufferBarrierDescription & {
        mStageBefore = stage;
        return *this;
    }

    auto BufferBarrierDescription::SetBeforeAccess( AccessType access ) -> BufferBarrierDescription & {
        mAccessBefore = access;
        return *this;
    }

    auto BufferBarrierDescription::SetAfterStage( PipelineStageFlags stage ) -> BufferBarrierDescription & {
        mStageAfter = stage;
        return *this;
    }

    auto BufferBarrierDescription::SetAfterAccess( AccessType access ) -> BufferBarrierDescription & {
        mAccessAfter = access;
        return *this;
    }

    auto TextureBarrierDescription::SetTexture( TextureHandle handle ) -> TextureBarrierDescription & {
        MKT_ASSERT( !handle.IsEmpty(), "Cannot pass en empty handle" );
        mTexture = handle.GetRaw();
        return *this;
    }

    auto TextureBarrierDescription::SetSubresourceSet( TextureSubresourceSet subResources ) -> TextureBarrierDescription & {
        mSubresourceSet = subResources;
        return *this;
    }

    auto TextureBarrierDescription::SetBeforeLayout( TextureLayoutFlags layout ) -> TextureBarrierDescription & {
        mLayoutBefore = layout;
        return *this;
    }

    auto TextureBarrierDescription::SetBeforeStage( PipelineStageFlags stage ) -> TextureBarrierDescription & {
        mStageBefore = stage;
        return *this;
    }

    auto TextureBarrierDescription::SetBeforeAccess( AccessType access ) -> TextureBarrierDescription & {
        mAccessBefore = access;
        return *this;
    }


    auto TextureBarrierDescription::SetAfterLayout( TextureLayoutFlags layout ) -> TextureBarrierDescription & {
        mLayoutAfter = layout;
        return *this;
    }

    auto TextureBarrierDescription::SetAfterStage( PipelineStageFlags stage ) -> TextureBarrierDescription & {
        mStageAfter = stage;
        return *this;
    }

    auto TextureBarrierDescription::SetAfterAccess( AccessType access ) -> TextureBarrierDescription & {
        mAccessAfter = access;
        return *this;
    }
}