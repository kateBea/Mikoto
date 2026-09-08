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

#ifndef MIKOTO_RHI_COMMAND_LIST_HH
#define MIKOTO_RHI_COMMAND_LIST_HH

#include <EASTL/span.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Memory/BufferSpan.hh>
#include <Core/ResourcePool.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Buffer.hh>
#include <Renderer/Rhi/Texture.hh>
#include <Renderer/Rhi/Utility.hh>
#include <Renderer/Rhi/Pipeline.hh>
#include <Renderer/Rhi/DeviceObject.hh>

namespace mikoto::renderer::rhi {

    /**
     * A three-dimensional subregion of a texture mip level and array layer.
     */
    struct TextureSlice {
        core::u32 x{};
        core::u32 y{};
        core::u32 z{};

        core::u32 mWidth{ 4 };
        core::u32 mHeight{ 4 };
        core::u32 mDepth{ 1 };
        core::u32 mMipLevel{};

        core::u32 mArrayLayer{}; // Cube face, for example.
    };

    /**
     * Viewport and scissor state to apply while recording commands.
     */
    struct ViewportState {
        // note: you can only set each of these either in the PSO or per draw call in DrawArguments
        // it is not legal to have the same state set in both the PSO and DrawArguments
        // leaving these vectors empty means no state is set
        eastl::fixed_vector<Viewport, kMaxViewports> mViewports{};
        eastl::fixed_vector<Rect, kMaxViewports> mScissorRects{};

        /**
         * Adds the supplied value through AddViewport.
         *
         * @param v Input value used by this operation.
         * @returns The result of AddViewport.
         */
        auto AddViewport(const Viewport& v) -> ViewportState& { mViewports.push_back(v); return *this; }

        /**
         * Adds the supplied value through AddScissorRect.
         *
         * @param r Input value used by this operation.
         * @returns The result of AddScissorRect.
         */
        auto AddScissorRect(const Rect& r) -> ViewportState& { mScissorRects.push_back(r); return *this; }

        /**
         * Adds the supplied value through AddViewportAndScissorRect.
         *
         * @param v Input value used by this operation.
         * @returns The result of AddViewportAndScissorRect.
         */
        auto AddViewportAndScissorRect(const Viewport& v) -> ViewportState& { return AddViewport(v).AddScissorRect(Rect(v)); }
    };

    /**
     * Parameters for direct indexed or non-indexed draw calls.
     */
    struct DrawArguments {
        core::u32 mVertexCount{ 0 };
        core::u32 mIndexCount{ 0 };

        core::u32 mInstanceCount{ 1 };
        core::u32 mFirstVertex{ 0 };
        core::u32 mFirstInstance{ 0 };

        core::u32 mFirstIndex{ 0 };
        core::i32 mVertexOffset{ 0 };

        /**
         * Sets the value handled by SetVertexCount.
         *
         * @param value Input value used by this operation.
         * @returns The result of SetVertexCount.
         */
        constexpr auto SetVertexCount( core::u32 value ) -> DrawArguments& { mVertexCount = value; return *this; }

        /**
         * Sets the value handled by SetIndexCount.
         *
         * @param value Input value used by this operation.
         * @returns The result of SetIndexCount.
         */
        constexpr auto SetIndexCount(core::u32 value) -> DrawArguments& { mIndexCount = value; return *this; }

        /**
         * Sets the value handled by SetInstanceCount.
         *
         * @param value Input value used by this operation.
         * @returns The result of SetInstanceCount.
         */
        constexpr auto SetInstanceCount(core::u32 value) -> DrawArguments& { mInstanceCount = value; return *this; }

        /**
         * Sets the value handled by SetFirstVertex.
         *
         * @param value Input value used by this operation.
         * @returns The result of SetFirstVertex.
         */
        constexpr auto SetFirstVertex(core::u32 value) -> DrawArguments& { mFirstVertex = value; return *this; }

        /**
         * Sets the value handled by SetFirstInstance.
         *
         * @param value Input value used by this operation.
         * @returns The result of SetFirstInstance.
         */
        constexpr auto SetFirstInstance(core::u32 value) -> DrawArguments& { mFirstInstance = value; return *this; }

        /**
         * Sets the value handled by SetFirstIndex.
         *
         * @param value Input value used by this operation.
         * @returns The result of SetFirstIndex.
         */
        constexpr auto SetFirstIndex(core::u32 value) -> DrawArguments& { mFirstIndex = value; return *this; }

        /**
         * Sets the value handled by SetVertexOffset.
         *
         * @param value Input value used by this operation.
         * @returns The result of SetVertexOffset.
         */
        constexpr auto SetVertexOffset( core::i32 value ) -> DrawArguments& { mVertexOffset = value; return *this; }
    };

    /**
     * GPU-written parameter layout for a non-indexed indirect draw.
     */
    struct DrawIndirectArguments {
        core::u32 mVertexCount{ 0 };
        core::u32 mInstanceCount{ 1 };
        core::u32 mStartVertexLocation{ 0 };
        core::u32 mStartInstanceLocation{ 0 };

        /**
         * Sets the value handled by SetVertexCount.
         *
         * @param value Input value used by this operation.
         * @returns The result of SetVertexCount.
         */
        constexpr auto SetVertexCount(core::u32 value) -> DrawIndirectArguments& { mVertexCount = value; return *this; }

        /**
         * Sets the value handled by SetInstanceCount.
         *
         * @param value Input value used by this operation.
         * @returns The result of SetInstanceCount.
         */
        constexpr auto SetInstanceCount(core::u32 value) -> DrawIndirectArguments& { mInstanceCount = value; return *this; }

        /**
         * Sets the value handled by SetStartVertexLocation.
         *
         * @param value Input value used by this operation.
         * @returns The result of SetStartVertexLocation.
         */
        constexpr auto SetStartVertexLocation(core::u32 value) -> DrawIndirectArguments& { mStartVertexLocation = value; return *this; }

        /**
         * Sets the value handled by SetStartInstanceLocation.
         *
         * @param value Input value used by this operation.
         * @returns The result of SetStartInstanceLocation.
         */
        constexpr auto SetStartInstanceLocation(core::u32 value) -> DrawIndirectArguments& { mStartInstanceLocation = value; return *this; }
    };

    /**
     * GPU-written parameter layout for an indexed indirect draw.
     */
    struct DrawIndexedIndirectArguments {
        core::u32 mIndexCount{ 0 };
        core::u32 mInstanceCount{ 1 };
        core::u32 mStartIndexLocation{ 0 };
        core::i32 mBaseVertexLocation{ 0 };
        core::u32 mStartInstanceLocation{ 0 };

        /**
         * Sets the value handled by SetIndexCount.
         *
         * @param value Input value used by this operation.
         * @returns The result of SetIndexCount.
         */
        constexpr auto SetIndexCount(core::u32 value) -> DrawIndexedIndirectArguments& { mIndexCount = value; return *this; }

        /**
         * Sets the value handled by SetInstanceCount.
         *
         * @param value Input value used by this operation.
         * @returns The result of SetInstanceCount.
         */
        constexpr auto SetInstanceCount(core::u32 value) -> DrawIndexedIndirectArguments& { mInstanceCount = value; return *this; }

        /**
         * Sets the value handled by SetStartIndexLocation.
         *
         * @param value Input value used by this operation.
         * @returns The result of SetStartIndexLocation.
         */
        constexpr auto SetStartIndexLocation(core::u32 value) -> DrawIndexedIndirectArguments& { mStartIndexLocation = value; return *this; }

        /**
         * Sets the value handled by SetBaseVertexLocation.
         *
         * @param value Input value used by this operation.
         * @returns The result of SetBaseVertexLocation.
         */
        constexpr auto SetBaseVertexLocation(core::i32 value) -> DrawIndexedIndirectArguments& { mBaseVertexLocation = value; return *this; }

        /**
         * Sets the value handled by SetStartInstanceLocation.
         *
         * @param value Input value used by this operation.
         * @returns The result of SetStartInstanceLocation.
         */
        constexpr auto SetStartInstanceLocation(core::u32 value) -> DrawIndexedIndirectArguments& { mStartInstanceLocation = value; return *this; }
    };

    /**
     * Associates a vertex buffer with an input-layout binding slot.
     */
    struct VertexBufferBinding {
        IBuffer* mBuffer{};
        core::u32 mSlot{};
        core::u64 mOffset{};
        core::u64 mElementStride{};

        /**
         * Performs the operation represented by operator.
         *
         * @param b Input value used by this operation.
         * @returns The result of operator.
         */
        auto operator==( const VertexBufferBinding& b ) const -> bool {
            return mBuffer == b.mBuffer && mSlot == b.mSlot && mOffset == b.mOffset;
        }

        /**
         * Performs the operation represented by operator.
         *
         * @param b Input value used by this operation.
         * @returns The result of operator.
         */
        auto operator!=( const VertexBufferBinding& b ) const -> bool { return !( *this == b ); }

        /**
         * Sets the value handled by SetBuffer.
         *
         * @param value Input value used by this operation.
         * @returns The result of SetBuffer.
         */
        auto SetBuffer(IBuffer* value)-> VertexBufferBinding& { mBuffer = value; return *this; }

        /**
         * Sets the value handled by SetBufferBinding.
         *
         * @param value Input value used by this operation.
         * @returns The result of SetBufferBinding.
         */
        auto SetBufferBinding(core::u32 value) -> VertexBufferBinding& { mSlot = value; return *this; }

        /**
         * Sets the value handled by SetOffset.
         *
         * @param value Input value used by this operation.
         * @returns The result of SetOffset.
         */
        auto SetOffset(core::u64 value) -> VertexBufferBinding& { mOffset = value; return *this; }

        /**
         * Sets the value handled by SetElementStride.
         *
         * @param value Input value used by this operation.
         * @returns The result of SetElementStride.
         */
        auto SetElementStride(core::u64 value) -> VertexBufferBinding& { mElementStride = value; return *this; }
    };

    /**
     * Associates an index buffer, format and byte offset.
     */
    struct IndexBufferBinding {
        IBuffer* mBuffer{};
        Format mFormat{};
        core::u32 mOffset{};

        /**
         * Performs the operation represented by operator.
         *
         * @param b Input value used by this operation.
         * @returns The result of operator.
         */
        auto operator==( const IndexBufferBinding& b ) const -> bool {
            return mBuffer == b.mBuffer && mFormat == b.mFormat && mOffset == b.mOffset;
        }

        /**
         * Performs the operation represented by operator.
         *
         * @param b Input value used by this operation.
         * @returns The result of operator.
         */
        auto operator!=( const IndexBufferBinding& b ) const -> bool { return !( *this == b ); }

        /**
         * Sets the value handled by SetBuffer.
         *
         * @param value Input value used by this operation.
         * @returns The result of SetBuffer.
         */
        auto SetBuffer(IBuffer* value)-> IndexBufferBinding& { mBuffer = value; return *this; }

        /**
         * Sets the value handled by SetFormat.
         *
         * @param value Input value used by this operation.
         * @returns The result of SetFormat.
         */
        auto SetFormat(Format value) -> IndexBufferBinding&{ mFormat = value; return *this; }

        /**
         * Sets the value handled by SetOffset.
         *
         * @param value Input value used by this operation.
         * @returns The result of SetOffset.
         */
        auto SetOffset(core::u32 value)-> IndexBufferBinding& { mOffset = value; return *this; }
    };

    /**
     * Render-pass-like state, including render targets, clears and render area.
     */
    struct RenderDescription {
        struct RenderTargetState {
            Color mClearColor{ kColorWhite };
            LoadOp mLoadOp{ LoadOp::eLoad };
            TextureHandle mRenderTarget{};
            TextureSubresourceSet mSubresourceSet{};
        };

        eastl::string mName{};

        Rect mRenderArea{};
        RenderTargetState mDepthTarget{};
        eastl::fixed_vector<RenderTargetState, kMaxRenderTargets> mCurrentRenderTargets{};

        /**
         * Sets the value handled by SetScopeName.
         *
         * @param name Input value used by this operation.
         * @returns The result of SetScopeName.
         */
        auto SetScopeName( eastl::string_view name ) -> RenderDescription&;

        /**
         * Sets the value handled by SetRenderArea.
         *
         * @param rec Input value used by this operation.
         * @returns The result of SetRenderArea.
         */
        auto SetRenderArea( const Rect& rec ) -> RenderDescription&;

        /**
         * Adds the supplied value through AddDepthTarget.
         *
         * @param target Input value used by this operation.
         * @param op Input value used by this operation.
         * @returns The result of AddDepthTarget.
         */
        auto AddDepthTarget( TextureHandle target, LoadOp op = LoadOp::eClear ) -> RenderDescription&;

        /**
         * Adds the supplied value through AddRenderTarget.
         *
         * @param target Input value used by this operation.
         * @param c Input value used by this operation.
         * @param op Input value used by this operation.
         * @param set Input value used by this operation.
         * @returns The result of AddRenderTarget.
         */
        auto AddRenderTarget( TextureHandle target, const Color& c, LoadOp op = LoadOp::eClear, TextureSubresourceSet set = kAllSubResources ) -> RenderDescription&;
    };

    /**
     * Describes a synchronization and access transition for a buffer range.
     */
    struct BufferBarrierDescription {
        IBuffer* mBuffer{ nullptr };
        BufferRange mRange{};

        // Previous State
        PipelineStageFlags mStageBefore{ PipelineStageFlagsBits::None };
        AccessType mAccessBefore{ AccessType::eNone };

        // New State
        PipelineStageFlags mStageAfter{ PipelineStageFlagsBits::None };
        AccessType mAccessAfter{ AccessType::eNone };

        /**
         * Sets the value handled by SetBuffer.
         *
         * @param handle Input value used by this operation.
         * @returns The result of SetBuffer.
         */
        auto SetBuffer( BufferHandle handle ) -> BufferBarrierDescription&;

        /**
         * Sets the value handled by SetRange.
         *
         * @param range Input value used by this operation.
         * @returns The result of SetRange.
         */
        auto SetRange( BufferRange range ) -> BufferBarrierDescription&;

        /**
         * Sets the value handled by SetBeforeStage.
         *
         * @param stage Input value used by this operation.
         * @returns The result of SetBeforeStage.
         */
        auto SetBeforeStage( PipelineStageFlags stage ) -> BufferBarrierDescription&;

        /**
         * Sets the value handled by SetBeforeAccess.
         *
         * @param access Input value used by this operation.
         * @returns The result of SetBeforeAccess.
         */
        auto SetBeforeAccess( AccessType access ) -> BufferBarrierDescription&;

        /**
         * Sets the value handled by SetAfterStage.
         *
         * @param stage Input value used by this operation.
         * @returns The result of SetAfterStage.
         */
        auto SetAfterStage( PipelineStageFlags stage ) -> BufferBarrierDescription&;

        /**
         * Sets the value handled by SetAfterAccess.
         *
         * @param access Input value used by this operation.
         * @returns The result of SetAfterAccess.
         */
        auto SetAfterAccess( AccessType access ) -> BufferBarrierDescription&;
    };

    /**
     * Describes a synchronization, access and layout transition for a texture.
     */
    struct TextureBarrierDescription {
        ITexture* mTexture{ nullptr };

        TextureSubresourceSet mSubresourceSet{ kAllSubResources };

        // Previous State
        TextureLayoutFlags mLayoutBefore{ TextureLayoutBits::Unknown };
        PipelineStageFlags mStageBefore{ PipelineStageFlagsBits::None };
        AccessType mAccessBefore{ AccessType::eNone };

        // New State
        TextureLayoutFlags mLayoutAfter{ TextureLayoutBits::Unknown };
        PipelineStageFlags mStageAfter{ PipelineStageFlagsBits::None };
        AccessType mAccessAfter{ AccessType::eNone };

        /**
         * Sets the value handled by SetTexture.
         *
         * @param handle Input value used by this operation.
         * @returns The result of SetTexture.
         */
        auto SetTexture( TextureHandle handle ) -> TextureBarrierDescription&;

        /**
         * Sets the value handled by SetSubresourceSet.
         *
         * @param subResources Input value used by this operation.
         * @returns The result of SetSubresourceSet.
         */
        auto SetSubresourceSet( TextureSubresourceSet subResources ) -> TextureBarrierDescription&;

        /**
         * Sets the value handled by SetBeforeLayout.
         *
         * @param layout Input value used by this operation.
         * @returns The result of SetBeforeLayout.
         */
        auto SetBeforeLayout( TextureLayoutFlags layout ) -> TextureBarrierDescription&;

        /**
         * Sets the value handled by SetBeforeStage.
         *
         * @param stage Input value used by this operation.
         * @returns The result of SetBeforeStage.
         */
        auto SetBeforeStage( PipelineStageFlags stage ) -> TextureBarrierDescription&;

        /**
         * Sets the value handled by SetBeforeAccess.
         *
         * @param access Input value used by this operation.
         * @returns The result of SetBeforeAccess.
         */
        auto SetBeforeAccess( AccessType access ) -> TextureBarrierDescription&;

        /**
         * Sets the value handled by SetAfterLayout.
         *
         * @param layout Input value used by this operation.
         * @returns The result of SetAfterLayout.
         */
        auto SetAfterLayout( TextureLayoutFlags layout ) -> TextureBarrierDescription&;

        /**
         * Sets the value handled by SetAfterStage.
         *
         * @param stage Input value used by this operation.
         * @returns The result of SetAfterStage.
         */
        auto SetAfterStage( PipelineStageFlags stage ) -> TextureBarrierDescription&;

        /**
         * Sets the value handled by SetAfterAccess.
         *
         * @param access Input value used by this operation.
         * @returns The result of SetAfterAccess.
         */
        auto SetAfterAccess( AccessType access ) -> TextureBarrierDescription&;
    };

    /**
     * Resources and push constants bound to a pipeline before drawing or dispatching.
     */
    struct BindResourcesDescription {
        core::usize mPushConstantSize{ 0 };
        ShaderFlags mPushConstantVisibility{};
        eastl::fixed_vector<core::ubyte, kMaxPushConstantSize> mPushConstants{};

        static constexpr core::u32 kMaxResourceSets{ 32 };

        // key = binding index (set index)
        eastl::fixed_hash_map<core::u32, IBindingTable*, kMaxResourceSets> mResourceSets{};

        IPipelineLayout* mPipelineLayout{};
        PipelineType mBindPoint{};

        /**
         * Sets the value handled by SetPipelineLayout.
         *
         * @param layout Input value used by this operation.
         * @returns The result of SetPipelineLayout.
         */
        auto SetPipelineLayout( IPipelineLayout* layout ) -> BindResourcesDescription&;

        /**
         * Sets the value handled by SetBindPoint.
         *
         * @param bindPoint Input value used by this operation.
         * @returns The result of SetBindPoint.
         */
        auto SetBindPoint( PipelineType bindPoint ) -> BindResourcesDescription&;

        /**
         * Sets the value handled by SetPushConstants.
         *
         * @param ptr Input value used by this operation.
         * @param sizeBytes Input value used by this operation.
         * @param stage Input value used by this operation.
         * @returns The result of SetPushConstants.
         */
        auto SetPushConstants( const void* ptr, core::usize sizeBytes, ShaderFlags stage ) -> BindResourcesDescription&;

        /**
         * Adds the supplied value through AddResourceSet.
         *
         * @param bindingIndex Input value used by this operation.
         * @param set Input value used by this operation.
         * @returns The result of AddResourceSet.
         */
        auto AddResourceSet( core::u32 bindingIndex, IBindingTable* set ) -> BindResourcesDescription&;
    };

    /**
     * Per-recording metadata supplied when a command list begins.
     */
    struct CommandListBeginDescription {
        eastl::string mScopeName{};

        /**
         * Sets the value handled by SetScopeName.
         *
         * @param name Input value used by this operation.
         * @returns The result of SetScopeName.
         */
        auto SetScopeName( eastl::string_view name ) -> CommandListBeginDescription;
    };

    /**
     * Allocation and queue configuration for a command list.
     */
    struct CommandListCreateDescription {
        eastl::string mName{};
        QueueType mQueueType{ QueueType::eInvalid };
        QueueOpSupportFlags mQueueOpSupportFlags{ };

        // If different to 0 the command buffer pre-allocates
        // this amount of backend specific command buffers
        // and recycles them on every usage
        core::u32 mSelfManagedCommandListCount{ 3 };
    };

    /**
     * Records GPU work for later execution by an @ref IQueue.
     *
     * Command lists are not thread-safe: one thread may record a given list at a time.
     */
    class ICommandList : public DeviceObject {
    public:

        /**
         * Begins a new recording scope using @p desc.
         *
         * @param desc Input value used by this operation.
         */
        virtual auto Begin( const CommandListBeginDescription& desc ) -> void = 0;

        /**
         * Ends the current recording scope.
         */
        virtual auto End() -> void = 0;

        /**
         * Records a buffer barrier.
         *
         * @param barrier Buffer synchronization and access transition.
         */
        virtual auto RecordBarrier( const BufferBarrierDescription& barrier ) -> void = 0;

        /**
         * Records a texture barrier.
         *
         * @param barrier Texture synchronization, access, and layout transition.
         */
        virtual auto RecordBarrier( const TextureBarrierDescription& barrier ) -> void = 0;

        /**
         * Records a transition for an entire buffer.
         *
         * @param buffer Buffer to transition.
         * @param stateBits Destination resource state.
         */
        virtual auto RecordTransition( IBuffer* buffer, ResourceStates stateBits ) -> void = 0;

        /**
         * Records a transition for an entire texture.
         *
         * @param texture Texture to transition.
         * @param stateBits Destination resource state.
         */
        virtual auto RecordTransition( ITexture* texture, ResourceStates stateBits ) -> void = 0;

        /**
         * Commits all barriers recorded since the previous commit.
         */
        virtual auto CommitBarriers() -> void = 0;

        /**
         * Queues a buffer barrier for automatic batching.
         *
         * @param barrier Buffer barrier to queue.
         */
        virtual auto SetBarrier( const BufferBarrierDescription& barrier ) -> void = 0;

        /**
         * Queues a texture barrier for automatic batching.
         *
         * @param barrier Texture barrier to queue.
         */
        virtual auto SetBarrier( const TextureBarrierDescription& barrier ) -> void = 0;

        /**
         * Immediately records a transition for an entire buffer.
         *
         * Use @ref RecordTransition followed by @ref CommitBarriers when
         * batching several transitions into one barrier command.
         * This operation must occur before @ref BeginRendering.
         * It also emits a memory dependency when the current and destination
         * states are identical.
         *
         * @param buffer Buffer to transition.
         * @param stateBits Destination resource state.
         */
        virtual auto SetTransition( IBuffer* buffer, ResourceStates stateBits ) -> void = 0;

        /**
         * Immediately records a transition for an entire texture.
         *
         * Use @ref RecordTransition followed by @ref CommitBarriers when
         * batching several transitions into one barrier command.
         * This operation must occur before @ref BeginRendering.
         * It also emits a memory dependency when the current and destination
         * states are identical.
         *
         * @param texture Texture to transition.
         * @param stateBits Destination resource state.
         */
        virtual auto SetTransition( ITexture* texture, ResourceStates stateBits ) -> void = 0;

        /**
         * Enables or disables automatic barrier insertion.
         *
         * @param enable True to enable automatic barriers.
         */
        virtual auto SetEnableAutomaticBarriers( bool enable ) -> void = 0;

        /**
         * Sets the clear color used for a render target.
         *
         * @param renderTarget Render target to configure.
         * @param color Clear color.
         */
        virtual auto SetClearColor( ITexture* renderTarget, Color color ) -> void = 0;

        /**
         * Uploads a buffer's contents to a texture.
         *
         * @param src Source buffer.
         * @param dest Destination texture.
         */
        virtual auto Write( IBuffer* src, ITexture* dest ) -> void = 0;

        /**
         * Uploads raw data to a texture.
         *
         * @param target Destination texture.
         * @param data Source data.
         * @param byteSize Number of source bytes.
         */
        virtual auto Write( ITexture* target, const void* data, core::usize byteSize ) -> void = 0;

        /**
         * Copies a texture region.
         *
         * @param src Source texture.
         * @param srcSlice Source texture region.
         * @param dest Destination texture.
         * @param destSlice Destination texture region.
         */
        virtual auto Copy( ITexture* src, const TextureSlice& srcSlice, ITexture* dest, const TextureSlice& destSlice ) -> void = 0;

        /**
         * Resolves multisampled texture data into a destination texture.
         *
         * @param src Source multisampled texture.
         * @param srcSlice Source texture region.
         * @param dest Destination texture.
         * @param destSlice Destination texture region.
         */
        virtual auto Resolve( ITexture* src, const TextureSlice& srcSlice, ITexture* dest, const TextureSlice& destSlice ) -> void = 0;

        /**
         * Uploads raw data to the beginning of a buffer.
         *
         * @param target Destination buffer.
         * @param data Source data.
         * @param byteSize Number of source bytes.
         */
        virtual auto Write( IBuffer* target, const void* data, core::usize byteSize ) -> void = 0;

        /**
         * Uploads raw data to a buffer at an offset.
         *
         * @param target Destination buffer.
         * @param destOffset Destination byte offset.
         * @param data Source data.
         * @param byteSize Number of source bytes.
         */
        virtual auto Write( IBuffer* target, core::usize destOffset, const void* data, core::usize byteSize ) -> void = 0;

        /**
         * Copies an entire buffer.
         *
         * @param src Source buffer.
         * @param dest Destination buffer.
         */
        virtual auto Copy( IBuffer* src, IBuffer* dest ) -> void = 0;

        /**
         * Copies a buffer to a destination offset.
         *
         * @param src Source buffer.
         * @param dest Destination buffer.
         * @param destOffset Destination byte offset.
         */
        virtual auto Copy( IBuffer* src, IBuffer* dest, core::usize destOffset ) -> void = 0;

        /**
         * Copies a texture into a buffer.
         *
         * @param dest Destination buffer.
         * @param src Source texture.
         */
        virtual auto Copy( IBuffer* dest, ITexture* src ) -> void = 0;

        /**
         * Copies a texture region into a buffer.
         *
         * @param dest Destination buffer.
         * @param src Source texture.
         * @param srcSlice Source texture region.
         */
        virtual auto Copy( IBuffer* dest, ITexture* src, const TextureSlice& srcSlice ) -> void = 0;

        /**
         * Begins a rendering scope.
         *
         * @param state Render targets, clears, and render area.
         */
        virtual auto BeginRendering( RenderDescription& state ) -> void = 0;

        /**
         * Ends the current rendering scope.
         */
        virtual auto EndRendering() -> void = 0;

        /**
         * Binds a graphics, compute, or ray-tracing pipeline.
         *
         * @param pipeline Pipeline to bind.
         */
        virtual auto BindPipeline( IPipeline* pipeline ) -> void = 0;

        /**
         * Applies viewport and scissor state.
         *
         * @param vs Viewport and scissor state.
         */
        virtual auto SetViewportState( const ViewportState& vs ) -> void = 0;

        /**
         * Sets active viewports.
         *
         * @param viewports Viewports to set.
         */
        virtual auto SetViewport( eastl::span<const Viewport> viewports ) -> void = 0;

        /**
         * Sets active scissor rectangles.
         *
         * @param scissorRects Scissor rectangles to set.
         */
        virtual auto SetScissors( eastl::span<const Rect> scissorRects ) -> void = 0;

        /**
         * Sets rasterized line width.
         *
         * @param width Line width in pixels.
         */
        virtual auto SetPolygonLineWidth( core::f32 width ) -> void = 0;

        /**
         * Binds an index buffer.
         *
         * @param buffer Index buffer to bind.
         */
        virtual auto BindIndexBuffer( IBuffer* buffer ) -> void = 0;

        /**
         * Binds an indirect-argument buffer.
         *
         * @param buffer Indirect-argument buffer to bind.
         */
        virtual auto BindIndirectBuffer( IBuffer* buffer ) -> void = 0;

        /**
         * Binds one vertex buffer.
         *
         * @param binding Vertex-buffer binding.
         */
        virtual auto BindVertexBuffer( const VertexBufferBinding& binding ) -> void = 0;

        /**
         * Binds multiple vertex buffers.
         *
         * @param binding Vertex-buffer bindings.
         */
        virtual auto BindVertexBuffers( eastl::span<const VertexBufferBinding> binding ) -> void = 0;

        /**
         * Binds descriptor tables and push constants for the active pipeline.
         *
         * @param desc Pipeline-resource binding description.
         */
        virtual auto BindPipelineResources( const BindResourcesDescription& desc ) -> void = 0;

        /**
         * Records a non-indexed draw.
         *
         * @param args Draw arguments.
         */
        virtual auto Draw( const DrawArguments& args ) -> void = 0;

        /**
         * Records an indexed draw.
         *
         * @param args Draw arguments.
         */
        virtual auto DrawIndexed( const DrawArguments& args ) -> void = 0;

        /**
         * Records draws from the bound indirect buffer.
         *
         * @param offset Byte offset of the first indirect argument.
         * @param drawCount Number of draw arguments to execute.
         */
        virtual auto DrawIndirect( core::u32 offset, core::u32 drawCount ) -> void = 0;

        /**
         * Records indexed draws from the bound indirect buffer.
         *
         * @param offset Byte offset of the first indirect argument.
         * @param drawCount Number of draw arguments to execute.
         */
        virtual auto DrawIndexedIndirect( core::u32 offset, core::u32 drawCount ) -> void = 0;

        /**
         * Records a compute dispatch.
         *
         * @param groupsX Work-group count along X.
         * @param groupsY Work-group count along Y.
         * @param groupsZ Work-group count along Z.
         */
        virtual auto Dispatch( core::u32 groupsX, core::u32 groupsY, core::u32 groupsZ ) -> void = 0;

        /**
         * Sets pipeline push constants.
         *
         * @param pipelineLayout Pipeline layout declaring the constants.
         * @param data Constant data.
         * @param byteSize Number of constant bytes.
         * @param stageVisibility Shader stages that consume the constants.
         */
        virtual auto SetPushConstants( IPipelineLayout* pipelineLayout, const void* data, core::usize byteSize, ShaderFlags stageVisibility ) -> void = 0;

        /**
         * Returns the queue type that executes this command list.
         *
         * @returns The queue type.
         */
        MKT_NODISCARD auto GetQueueType() const -> QueueType;

        /**
         * Starts a labeled region in GPU debugging tools.
         *
         * @param name Label text.
         * @param color Label color.
         */
        virtual auto BeginDebugLabel( eastl::string_view name, Color color ) -> void = 0;

        /**
         * Ends the current GPU debug-label region.
         */
        virtual auto EndDebugLabel() -> void = 0;

        /**
         * Destroys the command list.
         */
        ~ICommandList() override = default;

        using DeviceObject::Initialize;

    protected:
        // When set to true the command list will allocate a set amount of backend specific
        // command lists and cycle through them like ring buffer.
        // If selfManagedCommandListCount different to 0 the command buffer pre-allocates this
        // amount of backend specific command buffers and recycles them on every usage

        /**
         * Constructs a command list.
         *
         * @param queueType Queue type that executes the list.
         * @param selfManagedCommandListCount Number of backend lists to recycle.
         */
        explicit ICommandList( QueueType queueType, core::u32 selfManagedCommandListCount = 4 );

    protected:
        QueueType mQueueType{ QueueType::eInvalid };
        core::u32 mSelfManagedCommandListCount{ 4 };
    };

    using CommandListHandle = core::Ref<ICommandList>;
}

#endif//MIKOTO_RHI_COMMAND_LIST_HH
