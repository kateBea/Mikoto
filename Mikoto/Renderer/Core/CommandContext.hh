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
#include <EASTL/fixed_hash_set.h>
#include <EASTL/fixed_hash_map.h>

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

    class CommandContext final {
    public:
        CommandContext( FGNode* pass, FGResourceManager* resourceManager, FGStatisticsManager* statsManager );

        auto BeginPass( rhi::CommandListHandle cmd ) -> void;
        auto EndPass() -> void;

        auto BeginRender(const ContextRenderState & gs ) -> void;
        auto EndRender() -> void;

        auto SetViewportState(const rhi::ViewportState & vs ) -> void;

        MKT_NODISCARD auto GetDeviceBufferAddress( FGBufferHandle handle ) -> core::u64;

        MKT_NODISCARD auto PushTexture_SRV( FGTextureHandle handle ) -> core::u32;
        MKT_NODISCARD auto PushSampler( FGSamplerHandle handle ) -> core::u32;

        MKT_NODISCARD auto PushBuffer_SRV( FGBufferHandle handle ) -> core::u32;
        MKT_NODISCARD auto PushBuffer_UAV( FGBufferHandle handle ) -> core::u32;

        auto CommitBarriers( const ankerl::unordered_dense::map<FGResourceHandle, FGBarrier>& barriers ) -> void;

        MKT_NODISCARD auto ImportTexture( rhi::TextureHandle handle ) -> FGTextureHandle;
        MKT_NODISCARD auto ImportSampler( rhi::SamplerHandle handle ) -> FGSamplerHandle;
        MKT_NODISCARD auto ImportBuffer( rhi::BufferHandle handle ) -> FGBufferHandle;

        auto BindPipeline( FGPipelineHandle handle ) -> void;

        auto Draw( core::u32 vertexCount, core::u32 instanceCount = 1 ) -> void;
        auto DrawIndirect( const DrawIndirectState& state ) -> void;
        auto Dispatch( core::u32 groupX, core::u32 groupY, core::u32 groupZ ) -> void;

        auto CopyBuffer( FGBufferHandle dstBuffer, FGBufferHandle srcBuffer ) -> void;
        auto CopyBuffer( FGBufferHandle dstBuffer, rhi::IBuffer* src, core::usize dstOffset ) -> void;

        auto Copy( FGBufferHandle dstBuffer, FGTextureHandle srcImage ) -> void;

        auto CopyTexture( FGTextureHandle destImage, const void* data, core::usize sizeBytes ) -> void;

        auto CopyBuffer( FGBufferHandle dstBuffer, const auto& data, core::usize offset ) -> void {
            CopyBuffer( dstBuffer, offset, MKT_ADDRESSOF( data ), MKT_SIZEOF( data ) );
        }

        auto CopyBuffer( FGBufferHandle dstBuffer, core::usize offset, const void* ptr, core::usize sizeBytes ) -> void;

        auto PushConstants( const auto& data ) -> void {
            const void* ptr{ MKT_ADDRESSOF( data ) };
            const core::usize size{ MKT_SIZEOF( data ) };

            eastl::copy_n( core::as<core::ubyte*>( ptr ), size, mPushConstantsData.data() );
        }
    private:
        // [Internal usage]
        auto CacheResource( FGBufferHandle handle ) -> IBuffer*;
        auto CacheResource( FGTextureHandle handle ) -> ITexture*;

        auto CacheResourceDescriptorID( FGBufferHandle handle ) -> core::u32;
        auto CacheResourceDescriptorID( FGTextureHandle handle ) -> core::u32;
        auto CacheResourceDescriptorID( FGSamplerHandle handle ) -> core::u32;

    private:
        FGNode* mNode{};
        FGResourceManager* mResourceManager{};

        FGNodeStatistics* mNodeStatistics{};

        rhi::CommandListHandle mCommands{};
        rhi::PipelineLayoutHandle mPipelineLayout{};

        eastl::fixed_hash_map<FGResourceHandle, rhi::IBuffer*, 20> mCachedBuffers{};
        eastl::fixed_hash_map<FGResourceHandle, rhi::ITexture*, 20> mCachedTextures{};

        // Shader resources get a unique ID into the
        // Frame Graph resource manager descriptor table
        eastl::fixed_hash_map<FGResourceHandle, core::u32, 20> mCachedShaderBuffers_SRV{};
        eastl::fixed_hash_map<FGResourceHandle, core::u32, 20> mCachedShaderBuffers_UAV{};

        eastl::fixed_hash_map<FGResourceHandle, core::u32, 20> mCachedShaderTextures_SRV{};
        eastl::fixed_hash_map<FGResourceHandle, core::u32, 20> mCachedShaderTextures_UAV{};

        eastl::fixed_hash_map<FGResourceHandle, core::u32, 20> mCachedShaderSamplers{};

        eastl::fixed_vector<core::ubyte, rhi::kMaxPushConstantSize> mPushConstantsData{};
    };
}// namespace mikoto::renderer

#endif// MIKOTO_COMMAND_CONTEXT_HH