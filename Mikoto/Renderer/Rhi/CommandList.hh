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

#ifndef MIKOTO_RENDERER_RHI_COMMAND_LIST_HH
#define MIKOTO_RENDERER_RHI_COMMAND_LIST_HH

#include <source_location>

#include <EASTL/fixed_vector.h>
#include <EASTL/span.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Profiler.hh>
#include <Core/Resource.hh>
#include <Core/Types.hh>

#include <Memory/BufferSpan.hh>

#include <Renderer/Rhi/Buffer.hh>
#include <Renderer/Rhi/Denoiser.hh>
#include <Renderer/Rhi/DeviceObject.hh>
#include <Renderer/Rhi/Pipeline.hh>
#include <Renderer/Rhi/ShaderTable.hh>
#include <Renderer/Rhi/Texture.hh>
#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Utility.hh>

namespace mikoto::renderer::rhi {
    struct VideoDecodeDescription;

    // Forward declaration
    class IQueue;

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
        auto AddViewport( const Viewport& v ) -> ViewportState&;

        /**
         * Adds the supplied value through AddScissorRect.
         *
         * @param r Input value used by this operation.
         * @returns The result of AddScissorRect.
         */
        auto AddScissorRect( const Rect& r ) -> ViewportState&;

        /**
         * Adds the supplied value through AddViewportAndScissorRect.
         *
         * @param v Input value used by this operation.
         * @returns The result of AddViewportAndScissorRect.
         */
        auto AddViewportAndScissorRect( const Viewport& v ) -> ViewportState&;
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


    /** @brief Invocation dimensions for the currently bound ray-tracing pipeline.
     *
     * Bind the pipeline, resources and constants explicitly before dispatch.
     * The caller owns resource transitions and bindless resource lifetimes.
     */
    struct RayTracingDispatchDescription {
        /** @brief The number of ray generation shader invocations in the X dimension. */
        core::u32 mWidth{};


        /** @brief The number of ray generation shader invocations in the Y dimension. */
        core::u32 mHeight{};


        /** @brief The number of ray generation shader invocations in the Z dimension. Defaults to 1. */
        core::u32 mDepth{ 1 };
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
        auto operator==( const VertexBufferBinding& b ) const -> bool;


        /**
         * Performs the operation represented by operator.
         *
         * @param b Input value used by this operation.
         * @returns The result of operator.
         */
        auto operator!=( const VertexBufferBinding& b ) const -> bool;


        /**
         * Sets the value handled by SetBuffer.
         *
         * @param value Input value used by this operation.
         * @returns The result of SetBuffer.
         */
        auto SetBuffer( IBuffer* value ) -> VertexBufferBinding&;


        /**
         * Sets the value handled by SetBufferBinding.
         *
         * @param value Input value used by this operation.
         * @returns The result of SetBufferBinding.
         */
        auto SetBufferBinding( core::u32 value ) -> VertexBufferBinding&;


        /**
         * Sets the value handled by SetOffset.
         *
         * @param value Input value used by this operation.
         * @returns The result of SetOffset.
         */
        auto SetOffset( core::u64 value ) -> VertexBufferBinding&;


        /**
         * Sets the value handled by SetElementStride.
         *
         * @param value Input value used by this operation.
         * @returns The result of SetElementStride.
         */
        auto SetElementStride( core::u64 value ) -> VertexBufferBinding&;
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
        auto operator==( const IndexBufferBinding& b ) const -> bool;


        /**
         * Performs the operation represented by operator.
         *
         * @param b Input value used by this operation.
         * @returns The result of operator.
         */
        auto operator!=( const IndexBufferBinding& b ) const -> bool;


        /**
         * Sets the value handled by SetBuffer.
         *
         * @param value Input value used by this operation.
         * @returns The result of SetBuffer.
         */
        auto SetBuffer( IBuffer* value ) -> IndexBufferBinding&;


        /**
         * Sets the value handled by SetFormat.
         *
         * @param value Input value used by this operation.
         * @returns The result of SetFormat.
         */
        auto SetFormat( Format value ) -> IndexBufferBinding&;


        /**
         * Sets the value handled by SetOffset.
         *
         * @param value Input value used by this operation.
         * @returns The result of SetOffset.
         */
        auto SetOffset( core::u32 value ) -> IndexBufferBinding&;
    };

    /**
     * Render-pass-like state, including render targets, clears and render area.
     */
    struct RenderPassDescription {
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
        auto SetScopeName( eastl::string_view name ) -> RenderPassDescription&;


        /**
         * Sets the value handled by SetRenderArea.
         *
         * @param rec Input value used by this operation.
         * @returns The result of SetRenderArea.
         */
        auto SetRenderArea( const Rect& rec ) -> RenderPassDescription&;


        /**
         * Adds the supplied value through AddDepthTarget.
         *
         * @param target Input value used by this operation.
         * @param op Input value used by this operation.
         * @returns The result of AddDepthTarget.
         */
        auto AddDepthTarget( TextureHandle target, LoadOp op = LoadOp::eClear ) -> RenderPassDescription&;


        /**
         * Adds the supplied value through AddRenderTarget.
         *
         * @param target Input value used by this operation.
         * @param c Input value used by this operation.
         * @param op Input value used by this operation.
         * @param set Input value used by this operation.
         * @returns The result of AddRenderTarget.
         */
        auto AddRenderTarget( TextureHandle target, const Color& c, LoadOp op = LoadOp::eClear, TextureSubresourceSet set = kAllSubResources ) -> RenderPassDescription&;
    };

    /**
     * Describes a whole-resource state transition. The command list records the
     * transition immediately for every supplied buffer and texture.
     */
    struct TransitionDescription {
        struct BufferTransition {
            IBuffer* mBuffer{};
            ResourceStates mState{ ResourceStates::eUnknown };
        };

        struct TextureTransition {
            ITexture* mTexture{};
            ResourceStates mState{ ResourceStates::eUnknown };
        };

        eastl::fixed_vector<BufferTransition, 20> mBuffers{};
        eastl::fixed_vector<TextureTransition, 20> mTextures{};

        TransitionDescription() = default;

        TransitionDescription( IBuffer* buffer, ResourceStates state );
        TransitionDescription( ITexture* texture, ResourceStates state );

        /**
         * @brief Adds a buffer and its destination state.
         * @param buffer Buffer to transition.
         * @param state Destination state. */
        auto AddBuffer( IBuffer* buffer, ResourceStates state ) -> TransitionDescription&;


        /**
         * @brief Adds a texture and its destination state.
         * @param texture Texture to transition.
         * @param state Destination state. */
        auto AddTexture( ITexture* texture, ResourceStates state ) -> TransitionDescription&;
    };

    /** Describes synchronization and access requirements for one buffer. */
    struct BufferBarrierDescription {
        IBuffer* mBuffer{};
        BufferRange mRange{};
        PipelineStageFlags mStageBefore{ PipelineStageFlagsBits::None };
        BarrierAccessFlags mAccessBefore{ BarrierAccessFlagsBits::None };
        PipelineStageFlags mStageAfter{ PipelineStageFlagsBits::None };
        BarrierAccessFlags mAccessAfter{ BarrierAccessFlagsBits::None };

        auto SetBuffer( IBuffer* buffer ) -> BufferBarrierDescription&;
        auto SetRange( BufferRange range ) -> BufferBarrierDescription&;
        auto SetBeforeStage( PipelineStageFlags stage ) -> BufferBarrierDescription&;

        /**
         * Sets the source memory-access scope.
         * @param access Combined barrier access flags; None requests no memory availability operation.
         * @returns This description for chained configuration.
         */
        auto SetBeforeAccess( BarrierAccessFlags access ) -> BufferBarrierDescription&;


        /**
         * Sets the execution stage after which the barrier takes effect.
         * @param stage Combined pipeline stage flags specifying when subsequent operations can proceed.
         * @returns This description for chained configuration.
         */
        auto SetAfterStage( PipelineStageFlags stage ) -> BufferBarrierDescription&;


        /**
         * Sets the destination memory-access scope.
         * @param access Combined barrier access flags; None requests no memory visibility operation.
         * @returns This description for chained configuration.
         */
        auto SetAfterAccess( BarrierAccessFlags access ) -> BufferBarrierDescription&;
    };

    /** Describes synchronization, access, and layout requirements for one texture. */
    struct TextureBarrierDescription {
        ITexture* mTexture{};
        TextureSubresourceSet mSubresourceSet{ kAllSubResources };
        TextureLayoutFlags mLayoutBefore{ TextureLayoutBits::Unknown };
        PipelineStageFlags mStageBefore{ PipelineStageFlagsBits::None };
        BarrierAccessFlags mAccessBefore{ BarrierAccessFlagsBits::None };
        TextureLayoutFlags mLayoutAfter{ TextureLayoutBits::Unknown };
        PipelineStageFlags mStageAfter{ PipelineStageFlagsBits::None };
        BarrierAccessFlags mAccessAfter{ BarrierAccessFlagsBits::None };

        auto SetTexture( ITexture* texture ) -> TextureBarrierDescription&;
        auto SetSubresourceSet( TextureSubresourceSet subresources ) -> TextureBarrierDescription&;
        auto SetBeforeLayout( TextureLayoutFlags layout ) -> TextureBarrierDescription&;
        auto SetBeforeStage( PipelineStageFlags stage ) -> TextureBarrierDescription&;

        /**
         * Sets the source memory-access scope independently of the source layout.
         * @param access Combined barrier access flags.
         * @returns This description for chained configuration.
         */
        auto SetBeforeAccess( BarrierAccessFlags access ) -> TextureBarrierDescription&;


        auto SetAfterLayout( TextureLayoutFlags layout ) -> TextureBarrierDescription&;


        auto SetAfterStage( PipelineStageFlags stage ) -> TextureBarrierDescription&;


        /**
         * Sets the destination memory-access scope independently of the destination layout.
         * @param access Combined barrier access flags.
         * @returns This description for chained configuration.
         */
        auto SetAfterAccess( BarrierAccessFlags access ) -> TextureBarrierDescription&;
    };

    // Global visibility for opaque resource allocations, such as SDK-owned
    // weights and history. A buffer barrier cannot cover these hidden objects.
    struct MemoryBarrierDescription {
        PipelineStageFlags mStageBefore{ PipelineStageFlagsBits::None };
        BarrierAccessFlags mAccessBefore{ BarrierAccessFlagsBits::None };
        PipelineStageFlags mStageAfter{ PipelineStageFlagsBits::None };
        BarrierAccessFlags mAccessAfter{ BarrierAccessFlagsBits::None };
    };

    /** Groups all enhanced barriers recorded by one SetBarrier call. */
    struct BarrierDescription {
        eastl::fixed_vector<MemoryBarrierDescription, 4> mMemory{};
        eastl::fixed_vector<BufferBarrierDescription, 10> mBuffers{};
        eastl::fixed_vector<TextureBarrierDescription, 10> mTextures{};

        auto AddBuffer( const BufferBarrierDescription& description ) -> BarrierDescription&;
        auto AddBuffer( BufferBarrierDescription&& description ) -> BarrierDescription&;
        auto AddTexture( const TextureBarrierDescription& description ) -> BarrierDescription&;
        auto AddTexture( TextureBarrierDescription&& description ) -> BarrierDescription&;
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

    struct CommandListStatistics {
        // Count submitted native barriers,
        // implicit transitions do not create image/buffer entries.
        struct BarrierStatistics {
            core::u64 mImageCount{};
            core::u64 mBufferCount{};
            core::u64 mMemoryCount{};
            core::u64 mCallCount{};
        };

        // Count recorded API calls; an indirect batch is one draw submission.
        core::u64 mDrawCallCount{};
        core::u64 mIndirectDrawCallCount{};
        core::u64 mDispatchCallCount{};
        core::u64 mRayDispatchCallCount{};
        core::u64 mPipelineBindCount{};

        BarrierStatistics mBarriers{};
    };

    class IQueryPool;
    class IAccelerationStructure;

    /**
     * Records GPU work for later execution by an @ref IQueue.
     *
     * Command lists are not thread-safe: one thread may record to a given list at a time.
     */
    class ICommandList : public DeviceObject {
    public:
        /**
         * @brief Retrieves the counters for the current command-list recording.
         * @returns Borrowed counters owned by this command list, initially zero.
         */
        MKT_NODISCARD virtual auto GetStatistics() const -> const CommandListStatistics& = 0;


        /**
         * @brief Records the decoding of one H.264 picture.
         * 
         * @param description Decoder, parsed picture, output and reconstruction surfaces,
         * reference pictures with their DPB slots, and optional session-reset request.
         * 
         * @throws core::RuntimeException If decoding is unsupported, the recording scope
         * is invalid, or the picture and resources fail backend validation.
         */
        virtual auto DecodeVideo( const VideoDecodeDescription& description ) -> void = 0;


        /**
         * Opens a new recording scope using @p desc.
         *
         * @param desc Input value used by this operation.
         */
        virtual auto Open( const CommandListBeginDescription& desc ) -> void = 0;


        /**
         * Closes the current recording scope.
         */
        virtual auto Close() -> void = 0;



        /**
         * @brief Records explicit synchronization barriers immediately.
         * @param description Resource ranges, access masks and synchronization scopes.
         * @pre This command list is recording.
         */
        virtual auto SetBarrier( const BarrierDescription& description ) -> void = 0;

        // Record prepared uploads/feature creation once, without submitting work.
        // Calls retain the feature and its images until the submission completes.
        MKT_NODISCARD virtual auto InitializeDenoiser( IDenoiser *denoiser ) -> bool;
        MKT_NODISCARD virtual auto DispatchDenoiser( IDenoiser *denoiser, const DenoiserDispatch &dispatch ) -> DenoiserResult;



        /**
         * @brief Records whole-resource state transitions immediately.
         * @param description Resources and their required destination states.
         * @pre This command list is recording.
         */
        virtual auto SetTransition( const TransitionDescription& description ) -> void = 0;


        /**
         * Enables or disables automatic barrier insertion.
         * May be set before Open() and persists across recordings. Changes
         * affect subsequently recorded commands, not commands already recorded.
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
         * Vulkan/D3D12 upload heaps are written immediately and do not require
         * Open/Close or submission. The caller must avoid in-flight GPU reads
         * of that range. Device-local uploads require an open command list.
         *
         * @param target Destination buffer.
         * @param data Source data.
         * @param byteSize Number of source bytes.
         */
        virtual auto Write( IBuffer* target, const void* data, core::usize byteSize ) -> void = 0;


        /**
         * Uploads raw data to a buffer at an offset.
         * Has the same upload-heap and recording requirements as the zero-offset overload.
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
        virtual auto BeginRenderPass( RenderPassDescription& state ) -> void = 0;


        /**
         * Ends the current rendering scope.
         */
        virtual auto EndRenderPass() -> void = 0;


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
         * @brief Dispatches rays using the currently bound pipeline and resources.
         *
         * Requires a recording graphics command list outside a render pass.
         * Bind pipeline, tables and constants separately and transition resources
         * before dispatch. Bindless resources must be retained until GPU completion.
         *
         * @param desc Ray-generation invocation dimensions.
         * @throws core::RuntimeException If no ray pipeline is bound or dimensions are invalid.
         */
        virtual auto DispatchRays( const RayTracingDispatchDescription& desc ) -> void = 0;


        /** @brief Binds a built shader table for subsequent ray dispatches.
         *
         * @param table Table from this device, associated with the ray pipeline.
         * @throws core::RuntimeException If the table is unbuilt or incompatible.
         */
        virtual auto BindShaderTable( IShaderTable* table ) -> void = 0;


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
        MKT_NODISCARD virtual auto GetQueueType() const -> QueueType = 0;


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

        /** Collects earlier GPU timings before recording a profiled graphics batch. */
        virtual auto CollectGpuProfile() -> void {}

        /** Starts a named GPU timing zone with the caller's source location. */
        virtual auto BeginGpuProfileZone( eastl::string_view, std::source_location ) -> void {}

        /** Ends the current GPU timing zone outside a render pass. */
        virtual auto EndGpuProfileZone() -> void {}


        /**
         * @brief Prepares all slots in a query pool for a new submission batch.
         *
         * Resets the pool's recording state and records a native reset where required.
         * This does not reset the command list or wait for earlier GPU work.
         *
         * @param pool Initialized pool owned by the same device and compatible queue.
         * @pre This command list is recording. No query in the pool is active.
         * @pre The completion fence for the pool's previous batch has completed.
         *      Use separate pools for submissions that may overlap in flight.
         * @note Record outside a render pass for portability across backends.
         */
        virtual auto Reset( IQueryPool* pool ) -> void = 0;


        /**
         * @brief Begins a range query at the specified pool slot.
         *
         * @param pool Initialized non-timestamp pool for this device and queue.
         * @param index Zero-based slot index, less than the pool's capacity.
         * @pre This command list is recording and the slot has been reset.
         *      The pool must not already contain an active range query.
         * @note Match with EndQuery() on this same command list and slot.
         *       Record occlusion queries around draws in the relevant render pass.
         * @see WriteTimestamp()
         */
        virtual auto BeginQuery( IQueryPool* pool, core::u32 index ) -> void = 0;


        /**
         * @brief Ends the active range query at the specified pool slot.
         *
         * @param pool Pool passed to the matching BeginQuery().
         * @param index Slot passed to the matching BeginQuery().
         * @pre This command list is recording and owns the matching active query.
         * @note Ending a query does not make its result immediately readable.
         *       Resolve the slot and wait for the submission's completion fence.
         */
        virtual auto EndQuery( IQueryPool* pool, core::u32 index ) -> void = 0;


        /**
         * @brief Records a GPU timestamp into a reset slot.
         *
         * @param pool Initialized timestamp pool for this device and queue.
         * @param index Zero-based slot index, less than the pool's capacity.
         * @pre This command list is recording and the slot has been reset.
         * @note Timestamp queries do not use BeginQuery()/EndQuery(). Values use
         *       backend-specific units; use IQueryPool::GetElapsedMilliseconds()
         *       to convert a pair of results into a duration.
         */
        virtual auto WriteTimestamp( IQueryPool* pool, core::u32 index ) -> void = 0;


        /**
         * @brief Prepares a completed query range for host readback.
         *
         * Records a result copy where the backend requires one, or marks the
         * range for direct host retrieval. This method neither waits nor returns
         * result values. Unlike the texture overload, it does not resolve MSAA.
         *
         * @param pool Initialized pool for this device and queue.
         * @param first Zero-based index of the first slot to resolve.
         * @param count Number of consecutive slots; the range must fit the pool.
         * @pre This command list is recording. Every slot in the range has been
         *      ended with EndQuery() or written with WriteTimestamp().
         * @note Record outside a render pass for portability. After submission,
         *       pass a fence/value covering this resolve to IQueryPool::TryGetResult().
         */
        virtual auto Resolve( IQueryPool* pool, core::u32 first, core::u32 count ) -> void = 0;


        /**
         * @brief Records an initial immutable acceleration-structure build.
         *
         * A top-level build also builds any previously unbuilt bottom-level
         * dependencies. Inputs and the structure are retained by the recording.
         * Build and DispatchRays() may share a submission; do not discard the
         * recording before submitting it.
         *
         * @param structure Initialized acceleration structure owned by this device.
         * @pre Ray tracing is supported and this graphics command list is recording
         *      outside a render pass. Input geometry/instance data is valid and ready.
         * @note This is not an update/refit operation. Create a new acceleration
         *       structure when its geometry changes.
         * @see DispatchRays()
         */
        virtual auto Build( IAccelerationStructure* structure ) -> void = 0;


        /**
         * @brief Acquires a buffer range from another queue on the receiving command list.
         *
         * Pair with ReleaseResource() on sourceQueue using the same description.
         * Record before the receiving queue's first access to the range. The caller
         * must order the submissions with a queue signal/wait; this call does not
         * submit work or wait for the releasing queue.
         *
         * @param description Buffer range and producer/consumer stage and access scopes.
         *       D3D12 legacy barriers transition the whole buffer, regardless of range.
         *       Upload/readback heaps and acceleration-structure storage keep their
         *       fixed states; acceleration structures cannot transfer to copy queues.
         * @param sourceQueue Queue releasing the buffer, belonging to the same device.
         * @pre This command list is recording outside a render pass.
         * @note Vulkan emits a barrier only between different queue families. D3D12
         *       transitions through COMMON only between different queue types.
         *       D3D11/OpenGL do nothing. Same-family/type calls do not replace ordinary
         *       barriers or submission dependencies. Disable automatic barriers and
         *       manage resource states explicitly when using queue handoffs.
         *       Vulkan resources must use exclusive sharing; submission signal/wait
         *       scopes must cover the release/acquire barriers (ALL_COMMANDS is safe).
         */
        virtual auto AcquireResource( const BufferBarrierDescription& description, IQueue* sourceQueue ) -> void = 0;


        /**
         * @brief Releases a buffer range to another queue on the producing command list.
         *
         * Record after the producing queue's last access to the range. Pair with
         * AcquireResource() on destQueue using the same description. The caller
         * must order the submissions with a queue signal/wait; this call does not
         * submit work or signal the receiving queue.
         *
         * @param description Buffer range and producer/consumer stage and access scopes.
         *       D3D12 legacy barriers transition the whole buffer, regardless of range.
         *       Upload/readback heaps and acceleration-structure storage keep their
         *       fixed states; acceleration structures cannot transfer to copy queues.
         * @param destQueue Queue acquiring the buffer, belonging to the same device.
         * @pre This command list is recording outside a render pass.
         * @note Vulkan emits a barrier only between different queue families. D3D12
         *       transitions through COMMON only between different queue types.
         *       D3D11/OpenGL do nothing. Same-family/type calls do not replace ordinary
         *       barriers or submission dependencies. Disable automatic barriers and
         *       manage resource states explicitly when using queue handoffs.
         *       Vulkan resources must use exclusive sharing; submission signal/wait
         *       scopes must cover the release/acquire barriers (ALL_COMMANDS is safe).
         */
        virtual auto ReleaseResource( const BufferBarrierDescription& description, IQueue* destQueue ) -> void = 0;


        /**
         * @brief Acquires texture subresources from another queue on the receiving command list.
         *
         * Pair with ReleaseResource() on sourceQueue using the same description,
         * including explicit (non-Unknown) before/after layouts. Record before the
         * receiving queue's first access. The caller must order the submissions
         * with a queue signal/wait; this call does not submit work or wait.
         *
         * @param description Texture subresources, layouts, and producer/consumer access scopes.
         * @param sourceQueue Queue releasing the texture, belonging to the same device.
         * @pre This command list is recording outside a render pass.
         * @note Vulkan emits a barrier only between different queue families. D3D12
         *       transitions through COMMON only between different queue types.
         *       D3D11/OpenGL do nothing. Same-family/type calls do not replace ordinary
         *       barriers or submission dependencies. Disable automatic barriers and
         *       manage resource states explicitly when using queue handoffs.
         *       Vulkan resources must use exclusive sharing; submission signal/wait
         *       scopes must cover the release/acquire barriers (ALL_COMMANDS is safe).
         */
        virtual auto AcquireResource( const TextureBarrierDescription& description, IQueue* sourceQueue ) -> void = 0;


        /**
         * @brief Releases texture subresources to another queue on the producing command list.
         *
         * Record after the producing queue's last access to these subresources.
         * Pair with AcquireResource() on destQueue using the same description,
         * including explicit (non-Unknown) before/after layouts. The caller must
         * order submissions with a queue signal/wait; this call does not submit
         * work or signal a queue.
         *
         * @param description Texture subresources, layouts, and producer/consumer access scopes.
         * @param destQueue Queue acquiring the texture, belonging to the same device.
         * @pre This command list is recording outside a render pass.
         * @note Vulkan emits a barrier only between different queue families. D3D12
         *       transitions through COMMON only between different queue types.
         *       D3D11/OpenGL do nothing. Same-family/type calls do not replace ordinary
         *       barriers or submission dependencies. Disable automatic barriers and
         *       manage resource states explicitly when using queue handoffs.
         *       Vulkan resources must use exclusive sharing; submission signal/wait
         *       scopes must cover the release/acquire barriers (ALL_COMMANDS is safe).
         */
        virtual auto ReleaseResource( const TextureBarrierDescription& description, IQueue* destQueue ) -> void = 0;


        /**
         * Destroys the command list.
         */
        ~ICommandList() override = default;

        using DeviceObject::Initialize;
    };

    auto operator+=( CommandListStatistics& total,
        const CommandListStatistics& statistics ) -> CommandListStatistics&;

    MKT_NODISCARD auto operator-( const CommandListStatistics& current,
        const CommandListStatistics& previous ) -> CommandListStatistics;
    using CommandListHandle = core::Ref<ICommandList>;
}

#endif// MIKOTO_RENDERER_RHI_COMMAND_LIST_HH
