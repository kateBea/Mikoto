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

    // Used in command buffers to specify
    // texture pieces to work with
    struct TextureSlice {
        core::u32 x{};
        core::u32 y{};
        core::u32 z{};

        core::u32 mWidth{};
        core::u32 mHeight{};
        core::u32 mDepth{ 1 };
        core::u32 mMipLevel{};

       core:: u32 mArrayLayer{};// Cube face for example
    };

    // Represents viewport and scissors
    struct ViewportState {
        // note: you can only set each of these either in the PSO or per draw call in DrawArguments
        // it is not legal to have the same state set in both the PSO and DrawArguments
        // leaving these vectors empty means no state is set
        eastl::fixed_vector<Viewport, kMaxViewports> mViewports{};
        eastl::fixed_vector<Rect, kMaxViewports> mScissorRects{};

        auto AddViewport(const Viewport& v) -> ViewportState& { mViewports.push_back(v); return *this; }
        auto AddScissorRect(const Rect& r) -> ViewportState& { mScissorRects.push_back(r); return *this; }
        auto AddViewportAndScissorRect(const Viewport& v) -> ViewportState& { return AddViewport(v).AddScissorRect(Rect(v)); }
    };

    struct DrawArguments {
        core::u32 mVertexCount{ 0 };
        core::u32 mIndexCount{ 0 };

        core::u32 mInstanceCount{ 1 };
        core::u32 mFirstVertex{ 0 };
        core::u32 mFirstInstance{ 0 };

        core::u32 mFirstIndex{ 0 };
        core::i32 mVertexOffset{ 0 };

        constexpr auto SetVertexCount(core::u32 value) -> DrawArguments& { mVertexCount = value; return *this; }
        constexpr auto SetIndexCount(core::u32 value) -> DrawArguments& { mIndexCount = value; return *this; }
        constexpr auto SetInstanceCount(core::u32 value) -> DrawArguments& { mInstanceCount = value; return *this; }
        constexpr auto SetFirstVertex(core::u32 value) -> DrawArguments& { mFirstVertex = value; return *this; }
        constexpr auto SetFirstInstance(core::u32 value) -> DrawArguments& { mFirstInstance = value; return *this; }
        constexpr auto SetFirstIndex(core::u32 value) -> DrawArguments& { mFirstIndex = value; return *this; }
        constexpr auto SetVertexOffset(core::i32 value) -> DrawArguments& { mVertexOffset = value; return *this; }
    };

    struct DrawIndirectArguments {
        core::u32 mVertexCount{ 0 };
        core::u32 mInstanceCount{ 1 };
        core::u32 mStartVertexLocation{ 0 };
        core::u32 mStartInstanceLocation{ 0 };

        constexpr auto SetVertexCount(core::u32 value) -> DrawIndirectArguments& { mVertexCount = value; return *this; }
        constexpr auto SetInstanceCount(core::u32 value) -> DrawIndirectArguments& { mInstanceCount = value; return *this; }
        constexpr auto SetStartVertexLocation(core::u32 value) -> DrawIndirectArguments& { mStartVertexLocation = value; return *this; }
        constexpr auto SetStartInstanceLocation(core::u32 value) -> DrawIndirectArguments& { mStartInstanceLocation = value; return *this; }
    };

    struct DrawIndexedIndirectArguments {
        core::u32 mIndexCount{ 0 };
        core::u32 mInstanceCount{ 1 };
        core::u32 mStartIndexLocation{ 0 };
        core::i32 mBaseVertexLocation{ 0 };
        core::u32 mStartInstanceLocation{ 0 };

        constexpr auto SetIndexCount(core::u32 value) -> DrawIndexedIndirectArguments& { mIndexCount = value; return *this; }
        constexpr auto SetInstanceCount(core::u32 value) -> DrawIndexedIndirectArguments& { mInstanceCount = value; return *this; }
        constexpr auto SetStartIndexLocation(core::u32 value) -> DrawIndexedIndirectArguments& { mStartIndexLocation = value; return *this; }
        constexpr auto SetBaseVertexLocation(core::i32 value) -> DrawIndexedIndirectArguments& { mBaseVertexLocation = value; return *this; }
        constexpr auto SetStartInstanceLocation(core::u32 value) -> DrawIndexedIndirectArguments& { mStartInstanceLocation = value; return *this; }
    };

    struct VertexBufferBinding {
        IBuffer* mBuffer{};
        core::u32 mSlot{};
        core::u64 mOffset{};
        core::u64 mElementStride{};

        auto operator==( const VertexBufferBinding& b ) const -> bool {
            return mBuffer == b.mBuffer && mSlot == b.mSlot && mOffset == b.mOffset;
        }
        auto operator!=( const VertexBufferBinding& b ) const -> bool { return !( *this == b ); }

        auto SetBuffer(IBuffer* value)-> VertexBufferBinding& { mBuffer = value; return *this; }
        auto SetBufferBinding(core::u32 value) -> VertexBufferBinding& { mSlot = value; return *this; }
        auto SetOffset(core::u64 value) -> VertexBufferBinding& { mOffset = value; return *this; }
        auto SetElementStride(core::u64 value) -> VertexBufferBinding& { mElementStride = value; return *this; }
    };

    struct IndexBufferBinding {
        IBuffer* mBuffer{};
        Format mFormat{};
        core::u32 mOffset{};

        auto operator==( const IndexBufferBinding& b ) const -> bool {
            return mBuffer == b.mBuffer && mFormat == b.mFormat && mOffset == b.mOffset;
        }
        auto operator!=( const IndexBufferBinding& b ) const -> bool { return !( *this == b ); }

        auto SetBuffer(IBuffer* value)-> IndexBufferBinding& { mBuffer = value; return *this; }
        auto SetFormat(Format value) -> IndexBufferBinding&{ mFormat = value; return *this; }
        auto SetOffset(core::u32 value)-> IndexBufferBinding& { mOffset = value; return *this; }
    };

    struct GraphicsState {
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

        auto SetScopeName( eastl::string_view name ) -> GraphicsState&;
        auto SetRenderArea( const Rect& rec ) -> GraphicsState&;
        auto AddDepthTarget(TextureHandle target, LoadOp op = LoadOp::eClear ) -> GraphicsState&;
        auto AddRenderTarget(TextureHandle target, const Color& c, LoadOp op = LoadOp::eClear, TextureSubresourceSet set = kAllSubResources) -> GraphicsState&;
    };

    struct BufferBarrierDescription {
        IBuffer* mBuffer{ nullptr };
        BufferRange mRange{};

        // Previous State
        PipelineStageFlags mStageBefore{ PipelineStageFlagsBits::kNone };
        AccessFlags mAccessBefore{ AccessFlagsBits::kNone };

        // New State
        PipelineStageFlags mStageAfter{ PipelineStageFlagsBits::kNone };
        AccessFlags mAccessAfter{ AccessFlagsBits::kNone };

        auto SetBuffer( BufferHandle handle ) -> BufferBarrierDescription&;
        auto SetRange( BufferRange range ) -> BufferBarrierDescription&;

        auto SetBeforeStage( PipelineStageFlags stage ) -> BufferBarrierDescription&;
        auto SetBeforeAccess( AccessFlags access ) -> BufferBarrierDescription&;

        auto SetAfterStage( PipelineStageFlags stage ) -> BufferBarrierDescription&;
        auto SetAfterAccess( AccessFlags access ) -> BufferBarrierDescription&;
    };

    struct TextureBarrierDescription {
        ITexture* mTexture{ nullptr };

        TextureSubresourceSet mSubresourceSet{ kAllSubResources };

        // Previous State
        TextureLayoutFlags mLayoutBefore{ TextureLayoutBits::kUnknown };
        PipelineStageFlags mStageBefore{ PipelineStageFlagsBits::kNone };
        AccessFlags mAccessBefore{ AccessFlagsBits::kNone };

        // New State
        TextureLayoutFlags mLayoutAfter{ TextureLayoutBits::kUnknown };
        PipelineStageFlags mStageAfter{ PipelineStageFlagsBits::kNone };
        AccessFlags mAccessAfter{ AccessFlagsBits::kNone };

        auto SetTexture( TextureHandle handle ) -> TextureBarrierDescription&;
        auto SetSubresourceSet( TextureSubresourceSet subResources ) -> TextureBarrierDescription&;

        auto SetBeforeLayout( TextureLayoutFlags layout ) -> TextureBarrierDescription&;
        auto SetBeforeStage( PipelineStageFlags stage ) -> TextureBarrierDescription&;
        auto SetBeforeAccess( AccessFlags access ) -> TextureBarrierDescription&;

        auto SetAfterLayout( TextureLayoutFlags layout ) -> TextureBarrierDescription&;
        auto SetAfterStage( PipelineStageFlags stage ) -> TextureBarrierDescription&;
        auto SetAfterAccess( AccessFlags access ) -> TextureBarrierDescription&;
    };

    struct BindResourcesDescription {
        core::usize mPushConstantSize{ 0 };
        ShaderFlags mPushConstantVisibility{};
        eastl::fixed_vector<core::byte_t, kMaxPushConstantSize> mPushConstants{};

        static constexpr core::u32 kMaxResourceSets{ 32 };

        // key = binding index (set index)
        eastl::fixed_hash_map<core::u32, IBindingSet*, kMaxResourceSets> mResourceSets{};

        IPipelineLayout* mPipelineLayout{};
        PipelineType mBindPoint{};

        auto SetPipelineLayout( IPipelineLayout* layout ) -> BindResourcesDescription&;
        auto SetBindPoint( PipelineType bindPoint ) -> BindResourcesDescription&;

        auto SetPushConstants( const void* ptr, core::usize sizeBytes, ShaderFlags stage ) -> BindResourcesDescription&;

        auto AddResourceSet( core::u32 bindingIndex, IBindingSet* set ) -> BindResourcesDescription&;
    };

    struct CommandListBeginDescription {
        eastl::string mScopeName{};

        auto SetScopeName( eastl::string_view name ) -> CommandListBeginDescription;
    };

    // Command list we can record commands to and submit to a queue.
    // The device offers a helper to facilitate usage.
    // Command buffers are not thread safe, only one thread can record to a command
    // buffer at a time.
    class ICommandList : public DeviceObject {
    public:
        explicit ICommandList( QueueType queueType );

        virtual auto Begin( const CommandListBeginDescription& desc ) -> void = 0;
        virtual auto End() -> void = 0;

        virtual auto RecordBarrier( const BufferBarrierDescription& barrier ) -> void = 0;
        virtual auto RecordBarrier( const TextureBarrierDescription& barrier ) -> void = 0;

        virtual auto RecordTransition(IBuffer* buffer, ResourceStates stateBits) -> void = 0;
        virtual auto RecordTransition(ITexture* buffer, ResourceStates stateBits) -> void = 0;

        virtual auto CommitBarriers() -> void = 0;

        virtual auto SetBarrier( const BufferBarrierDescription& barrier ) -> void = 0;
        virtual auto SetBarrier( const TextureBarrierDescription& barrier ) -> void = 0;

        virtual auto SetTransition(IBuffer* buffer, ResourceStates stateBits) -> void = 0;
        virtual auto SetTransition(ITexture* buffer, ResourceStates stateBits) -> void = 0;

        virtual auto SetEnableAutomaticBarriers(  bool enable  ) -> void = 0;

        virtual auto SetClearColor( TextureHandle renderTargets, Color color ) -> void = 0;

        virtual auto Write( IBuffer* src, ITexture* dest, core::u32 mipLevel ) -> void = 0;
        virtual auto Write( ITexture* target, core::u32 mipLevel, const void* data, core::usize byteSize ) -> void = 0;
        virtual auto Copy( ITexture* src, const TextureSlice& srcSlice, ITexture* dest, const TextureSlice& destSlice ) -> void = 0;

        virtual auto Resolve( ITexture* src, const TextureSlice& srcSlice, ITexture* dest, const TextureSlice& destSlice ) -> void = 0;

        virtual auto Write( IBuffer* target, const void* data, core::usize byteSize ) -> void = 0;
        virtual auto Write( IBuffer* target, core::usize destOffset, const void* data, core::usize byteSize ) -> void = 0;

        virtual auto Copy( IBuffer* src, IBuffer* dest ) -> void = 0;
        virtual auto Copy( IBuffer* src, IBuffer* dest, core::usize destOffset ) -> void = 0;

        virtual auto Copy( IBuffer* dest, ITexture* src ) -> void = 0;

        virtual auto BeginRendering( GraphicsState& state ) -> void = 0;
        virtual auto EndRendering() -> void = 0;

        virtual auto BindPipeline( IPipeline* pipeline ) -> void = 0;

        // I am not sure if I wanna have these because the viewport is set on the images we
        // render to so it makes more sense to tie them to the graphics state when we specify the render targets
        virtual auto SetViewportState( const ViewportState& vs ) -> void = 0;
        virtual auto SetViewport( eastl::span<const Viewport> viewports ) -> void = 0;
        virtual auto SetScissors( eastl::span<const Rect> scissorRects ) -> void = 0;

        virtual auto SetPolygonLineWidth( core::f32 width ) -> void = 0;

        virtual auto BindIndexBuffer( IBuffer* buffer ) -> void = 0;
        virtual auto BindIndirectBuffer( IBuffer* buffer ) -> void = 0;
        virtual auto BindVertexBuffer( const VertexBufferBinding& binding ) -> void = 0;
        virtual auto BindVertexBuffers( eastl::span<const VertexBufferBinding> binding ) -> void = 0;

        virtual auto BindPipelineResources( const BindResourcesDescription& desc ) -> void = 0;

        virtual auto Draw( const DrawArguments& args ) -> void = 0;
        virtual auto DrawIndexed( const DrawArguments& args ) -> void = 0;

        // Use previously bound indirect buffer
        virtual auto DrawIndirect( core::u32 offset, core::u32 drawCount ) -> void = 0;
        virtual auto DrawIndexedIndirect( core::u32 offset, core::u32 drawCount ) -> void = 0;

        virtual auto Dispatch( core::u32 groupsX, core::u32 groupsY, core::u32 groupsZ ) -> void = 0;

        virtual auto SetPushConstants( IPipelineLayout* pipelineLayout, const void* data, core::usize byteSize, ShaderFlags stageVisibility ) -> void = 0;

        MKT_NODISCARD auto GetQueueType() const -> QueueType;

        // DEBUG Utilities
        virtual auto BeginDebugLabel( eastl::string_view name, Color color ) -> void = 0;
        virtual auto EnbDebugLabel() -> void = 0;

        ~ICommandList() override = default;

        using DeviceObject::Initialize;

    protected:
        QueueType mQueueType{ QueueType::eInvalid };
    };

    using CommandListHandle = core::Ref<ICommandList>;
}

#endif//MIKOTO_RHI_COMMAND_LIST_HH
