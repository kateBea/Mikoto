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

#ifndef MIKOTO_COMMAND_CONTEXT_HH
#define MIKOTO_COMMAND_CONTEXT_HH

#include <EASTL/span.h>
#include <EASTL/utility.h>
#include <EASTL/string_view.h>
#include <EASTL/fixed_vector.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/ReferenceCounted.hh>

#include <Memory/Allocator.hh>
#include <Memory/BufferSpan.hh>

#include <Renderer/Core/Rhi.hh>
#include <Renderer/Core/GpuDevice.hh>
#include <Renderer/Core/FrameGraph.hh>

namespace mikoto::renderer {

    struct DrawIndirectCommand {
        u32 mVertexCount{};
        u32 mInstanceCount{};
        u32 mFirstVertex{}; // Generally 0, start from very first vertex
        u32 mFirstInstance{};
    };

    struct DrawIndirectState {
        u32 mInstanceCount{};
        FGBufferHandle mIndirectBuffer{};

        auto SetBuffer( FGBufferHandle handle ) -> DrawIndirectState&;
        auto SetDrawCount( u32 count ) -> DrawIndirectState&;
    };

    struct ContextRenderState {
        struct RenderTargetState {
            Color mClearColor{ kColorWhite };
            LoadOp mLoadOp{ LoadOp::eLoad };
            FGTextureHandle mRenderTarget{};

            u32 mFaceIndex{};
            u32 mMipLevel{};

            operator FGTextureHandle() const { return mRenderTarget; }
        };

        Rect mRenderArea{};
        RenderTargetState mDepthTarget{};
        eastl::fixed_vector<RenderTargetState, kMaxRenderTargets> mCurrentRenderTargets{};

        auto Clear() -> void;

        auto SetRenderArea( const Rect& rec ) -> ContextRenderState&;
        auto AddDepthTarget(FGTextureHandle target, LoadOp op = LoadOp::eClear ) -> ContextRenderState&;
        auto AddRenderTarget(FGTextureHandle target, const Color& c, LoadOp op = LoadOp::eClear, u32 faceIndex = 0, u32 mipLevel = 0) -> ContextRenderState&;
    };

    class CommandContext final : public ReferenceCounted {
    public:
        CommandContext( FGNode* pass, FGResourceManager* resourceManager );

        auto BeginPass( CommandListHandle cmd ) -> void;
        auto EndPass() -> void;

        auto BeginRender(const ContextRenderState & gs ) -> void;
        auto EndRender() -> void;

        auto SetViewportState(const ViewportState & vs ) -> void;

        auto PushTexture_SRV( FGTextureHandle handle ) -> u32;
        auto PushSampler( FGSamplerHandle handle ) -> u32;

        auto PushBuffer_SRV( FGBufferHandle handle ) -> u32;
        auto PushBuffer_UAV( FGBufferHandle handle ) -> u32;
        auto PushBuffer_Constant( FGBufferHandle handle ) -> u32;

        auto CommitBarriers( const ankerl::unordered_dense::map<FGResourceHandle, FGBarrier>& barriers ) -> void;

        MKT_NODISCARD auto ImportTexture( TextureHandle handle ) -> FGTextureHandle;
        MKT_NODISCARD auto ImportSampler( SamplerHandle handle ) -> FGSamplerHandle;
        MKT_NODISCARD auto ImportBuffer( BufferHandle handle ) -> FGBufferHandle;

        auto BindPipeline( FGPipelineHandle handle ) -> void;

        auto Draw( u32 vertexCount, u32 instanceCount = 1 ) -> void;
        auto DrawIndirect( const DrawIndirectState& state ) -> void;
        auto Dispatch( u32 groupX, u32 groupY, u32 groupZ ) -> void;

        auto CopyBuffer( FGBufferHandle dstBuffer, FGBufferHandle srcBuffer ) -> void;
        auto CopyBuffer( FGBufferHandle dstBuffer, IBuffer* src, size_t dstOffset ) -> void;

        auto Copy( FGBufferHandle dstBuffer, FGTextureHandle srcImage ) -> void;

        auto CopyBuffer( FGBufferHandle dstBuffer, const auto& data, size_t offset ) -> void {
            CopyBuffer( dstBuffer, offset, MKT_ADDRESSOF( data ), MKT_SIZEOF( data ) );
        }

        auto CopyBuffer( FGBufferHandle dstBuffer, size_t offset, const void* ptr, size_t sizeBytes ) -> void;

        auto PushConstants( const auto& data ) -> void {
            const void* ptr{ MKT_ADDRESSOF( data ) };
            const size_t size{ MKT_SIZEOF( data ) };

            eastl::copy_n( as<byte_t*>( ptr ), size, mPushConstantsData.data() );
        }

    private:
        FGNode* mNode{};
        FGResourceManager* mResourceManager{};

        eastl::fixed_vector<byte_t, kMaxPushConstantSize> mPushConstantsData{};

        // Render state
        Rect mScissors{};
        Viewport mViewport{};
        Color mClearColor{};

        IPipeline* mCurrentPipeline{};

        CommandListHandle mCommands{};
    };
}// namespace mikoto::renderer

#endif// MIKOTO_COMMAND_CONTEXT_HH