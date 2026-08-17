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

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Core/FrameGraph.hh>

namespace mikoto::renderer {

    struct DrawIndirectCommand {
        core::u32 mVertexCount{};
        core::u32 mInstanceCount{};
        core::u32 mFirstVertex{}; // Generally 0, start from very first vertex
        core::u32 mFirstInstance{};
    };

    struct DrawIndirectState {
        core::u32 mInstanceCount{};
        FGBufferHandle mIndirectBuffer{};

        auto SetBuffer( FGBufferHandle handle ) -> DrawIndirectState&;
        auto SetDrawCount( core::u32 count ) -> DrawIndirectState&;
    };

    struct ContextRenderState {
        struct RenderTargetState {
            rhi::Color mClearColor{ rhi::kColorWhite };
            rhi::LoadOp mLoadOp{ rhi::LoadOp::eLoad };
            FGTextureHandle mRenderTarget{};

            core::u32 mFaceIndex{};
            core::u32 mMipLevel{};

            operator FGTextureHandle() const { return mRenderTarget; }
        };

        rhi::Rect mRenderArea{};
        RenderTargetState mDepthTarget{};
        eastl::fixed_vector<RenderTargetState, rhi::kMaxRenderTargets> mCurrentRenderTargets{};

        auto Clear() -> void;

        auto SetRenderArea( const rhi::Rect& rec ) -> ContextRenderState&;
        auto AddDepthTarget(FGTextureHandle target, rhi::LoadOp op = rhi::LoadOp::eClear ) -> ContextRenderState&;
        auto AddRenderTarget(FGTextureHandle target, const rhi::Color& c, rhi::LoadOp op = rhi::LoadOp::eClear, core::u32 faceIndex = 0, core::u32 mipLevel = 0) -> ContextRenderState&;
    };

    class CommandContext final : public core::ReferenceCounted {
    public:
        CommandContext( FGNode* pass, FGResourceManager* resourceManager );

        auto BeginPass( rhi::CommandListHandle cmd ) -> void;
        auto EndPass() -> void;

        auto BeginRender(const ContextRenderState & gs ) -> void;
        auto EndRender() -> void;

        auto SetViewportState(const rhi::ViewportState & vs ) -> void;

        auto PushTexture_SRV( FGTextureHandle handle ) -> core::u32;
        auto PushSampler( FGSamplerHandle handle ) -> core::u32;

        auto PushBuffer_SRV( FGBufferHandle handle ) -> core::u32;
        auto PushBuffer_UAV( FGBufferHandle handle ) -> core::u32;

        auto CommitBarriers( const ankerl::unordered_dense::map<FGResourceHandle, FGBarrier>& barriers ) -> void;

        MKT_NODISCARD auto ImportTexture( rhi::TextureHandle handle ) -> FGTextureHandle;
        MKT_NODISCARD auto ImportSampler( rhi::SamplerHandle handle ) -> FGSamplerHandle;
        MKT_NODISCARD auto ImportBuffer( rhi::BufferHandle handle ) -> FGBufferHandle;

        auto BindPipeline( FGPipelineHandle handle ) -> void;

        auto Draw( core::u32 vertexCount, core::u32 instanceCount = 1 ) -> void;
        auto DrawIndirect( const DrawIndirectState& state ) -> void;
        auto Dispatch( core::u32 groupX, core::u32 groupY, core::u32 groupZ ) -> void;

        auto CopyBuffer( FGBufferHandle dstBuffer, FGBufferHandle srcBuffer ) -> void;
        auto CopyBuffer( FGBufferHandle dstBuffer, rhi::IBuffer* src, core::size_t dstOffset ) -> void;

        auto Copy( FGBufferHandle dstBuffer, FGTextureHandle srcImage ) -> void;

        auto CopyBuffer( FGBufferHandle dstBuffer, const auto& data, core::size_t offset ) -> void {
            CopyBuffer( dstBuffer, offset, MKT_ADDRESSOF( data ), MKT_SIZEOF( data ) );
        }

        auto CopyBuffer( FGBufferHandle dstBuffer, core::size_t offset, const void* ptr, core::size_t sizeBytes ) -> void;

        auto PushConstants( const auto& data ) -> void {
            const void* ptr{ MKT_ADDRESSOF( data ) };
            const core::size_t size{ MKT_SIZEOF( data ) };

            eastl::copy_n( core::as<core::byte_t*>( ptr ), size, mPushConstantsData.data() );
        }

    private:
        FGNode* mNode{};
        FGResourceManager* mResourceManager{};

        rhi::CommandListHandle mCommands{};
        rhi::IPipeline* mCurrentPipeline{};

        eastl::fixed_vector<core::byte_t, rhi::kMaxPushConstantSize> mPushConstantsData{};
    };
}// namespace mikoto::renderer

#endif// MIKOTO_COMMAND_CONTEXT_HH