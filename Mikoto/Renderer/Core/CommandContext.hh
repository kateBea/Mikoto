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

#ifndef MIKOTO_RENDERER_CORE_COMMAND_CONTEXT_HH
#define MIKOTO_RENDERER_CORE_COMMAND_CONTEXT_HH

#include <EASTL/utility.h>
#include <EASTL/span.h>
#include <EASTL/vector.h>
#include <EASTL/hash_map.h>
#include <EASTL/string_view.h>
#include <EASTL/fixed_vector.h>
#include <EASTL/fixed_hash_map.h>
#include <EASTL/fixed_hash_set.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
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
        core::u32 mFirstDraw{};
        FGBufferHandle mIndirectBuffer{};

        auto SetBuffer( FGBufferHandle handle ) -> DrawIndirectState&;
        auto SetDrawCount( core::u32 count ) -> DrawIndirectState&;
        auto SetFirstDraw( core::u32 first ) -> DrawIndirectState&;
    };

    struct RenderBeginDescription {
        struct RenderTargetState {
            rhi::Color mClearColor{ rhi::kColorWhite };
            rhi::LoadOp mLoadOp{ rhi::LoadOp::eLoad };
            FGTextureHandle mRenderTarget{};

            core::u32 mFaceIndex{};
            core::u32 mMipLevel{};

            operator FGTextureHandle() const;
        };

        rhi::Rect mRenderArea{};
        RenderTargetState mDepthTarget{};
        eastl::fixed_vector<RenderTargetState, rhi::kMaxRenderTargets> mCurrentRenderTargets{};

        auto Clear() -> void;

        auto SetRenderArea( const rhi::Rect& rec ) -> RenderBeginDescription&;
        auto AddDepthTarget(FGTextureHandle target, rhi::LoadOp op = rhi::LoadOp::eClear ) -> RenderBeginDescription&;
        auto AddRenderTarget(FGTextureHandle target, const rhi::Color& c, rhi::LoadOp op = rhi::LoadOp::eClear, core::u32 faceIndex = 0, core::u32 mipLevel = 0) -> RenderBeginDescription&;
    };

    class CommandContext final {
    public:
        CommandContext( FrameGraphPass* pass, FGResourceManager* resourceManager,
            FrameGraphDescriptorAllocator* descriptorAllocator,
            rhi::PipelineLayoutHandle pipelineLayout, FGStatisticsManager* statsManager );

        auto SetVideoRecording( FGScheduler* scheduler ) -> void;
        auto DecodeVideo( FGVideoHandle handle, const rhi::VideoDecodeDescription& description ) -> void;
        auto PrepareVideoSampling( rhi::VideoSurfaceHandle surface, rhi::TextureHandle output ) -> void;
        auto FinishVideoPresentation( rhi::TextureHandle output ) -> void;

        auto BeginRender(const RenderBeginDescription & gs ) -> void;
        auto EndRender() -> void;

        auto SetViewportState(const rhi::ViewportState & vs ) -> void;

        MKT_NODISCARD auto GetDeviceBufferAddress( FGBufferHandle handle ) -> core::u64;

        MKT_NODISCARD auto PushTexture_SRV( FGTextureHandle handle ) -> core::u32;
        MKT_NODISCARD auto PushTexture_UAV( FGTextureHandle handle ) -> core::u32;
        MKT_NODISCARD auto PushAccelerationStructure( FGAccelerationStructureHandle handle ) -> core::u32;

        /** Reuses this pass context's unchanged BLASes, builds the TLAS and publishes it to the manager.
         * Cached shapes are grouped by the target handle and survive across graph executions.
         * Recompiling the graph recreates its contexts and discards these caches.
         */
        auto BuildAccelerationStructures(
            FGAccelerationStructureHandle handle,
            FGBufferHandle vertices, FGBufferHandle indices,
            eastl::span<const FGAccelerationGeometry> geometries ) -> void;

        MKT_NODISCARD auto PushSampler( FGSamplerHandle handle ) -> core::u32;

        MKT_NODISCARD auto PushBuffer_SRV( FGBufferHandle handle ) -> core::u32;
        MKT_NODISCARD auto PushBuffer_UAV( FGBufferHandle handle ) -> core::u32;

        MKT_NODISCARD auto ImportTexture( rhi::TextureHandle handle ) -> FGTextureHandle;
        MKT_NODISCARD auto ImportSampler( rhi::SamplerHandle handle ) -> FGSamplerHandle;
        MKT_NODISCARD auto ImportBuffer( rhi::BufferHandle handle ) -> FGBufferHandle;

        // Not all backends support this, and those who do which is basically vulkan
        // support is limited to and dependent on the hardware
        auto SetPolygonLineWidth( core::f32 width ) -> void;

        auto BindPipeline( FGPipelineHandle handle ) -> void;
        auto BindShaderTable( rhi::ShaderTableHandle table ) -> void;
        auto BindShaderTable( FGShaderTableHandle table ) -> void;

        auto Draw( core::u32 vertexCount, core::u32 instanceCount = 1 ) -> void;
        auto DrawIndirect( const DrawIndirectState& state ) -> void;
        auto Dispatch( core::u32 groupX, core::u32 groupY, core::u32 groupZ ) -> void;

        auto DispatchRays( core::u32 width, core::u32 height ) -> void;

        auto BuildAccelerationStructure( rhi::AccelStructureHandle structure ) -> void;

        MKT_NODISCARD auto ImportSampledTexture( rhi::TextureHandle texture ) -> FGTextureHandle;
        MKT_NODISCARD auto UseImportedTexture( FGTextureHandle texture ) -> core::u32;

        auto ResetCallCounts() -> void;
        auto GetDrawCallCount() const -> core::u32;
        auto GetDispatchCallCount() const -> core::u32;

        // CPU allocation only: call in a preparation pass, then initialize the
        // complete replacement in a following upload pass. Existing bytes are not copied.
        auto EnsureBufferCapacity( FGBufferHandle buffer, core::usize bytes ) -> bool;
        auto ReplaceBufferStorage( FGBufferHandle buffer, core::usize bytes ) -> void;

        auto CopyBuffer( FGBufferHandle dstBuffer, FGBufferHandle srcBuffer ) -> void;
        auto CopyBuffer( FGBufferHandle dstBuffer, rhi::IBuffer* src, core::usize dstOffset ) -> void;

        auto Copy( FGBufferHandle dstBuffer, FGTextureHandle srcImage ) -> void;

        auto CopyTexture( FGTextureHandle destImage, const void* data, core::usize sizeBytes ) -> void;

        // The pass declares source as copy/read and destination as copy/write.
        // This records an exact region copy; all barriers belong to the graph.
        auto CopyTexture( FGTextureHandle source, const rhi::TextureSlice& sourceSlice, FGTextureHandle destination, const rhi::TextureSlice& destinationSlice ) -> void;
        auto ResolveTexture( FGTextureHandle source, FGTextureHandle destination ) -> void;

        auto CopyBuffer( FGBufferHandle dstBuffer, const auto& data, core::usize offset ) -> void {
            MKT_BEGIN_PROFILER();
            CopyBuffer( dstBuffer, offset, MKT_ADDRESSOF( data ), MKT_SIZEOF( data ) );
        }

        auto CopyBuffer( FGBufferHandle dstBuffer, core::usize offset, const void* ptr, core::usize sizeBytes ) -> void;
        auto CopyBuffer( rhi::BufferHandle destination, core::usize offset, const void* data, core::usize sizeBytes ) -> void;
        auto CopyTexture( FGTextureHandle source, rhi::TextureHandle destination ) -> void;

        auto PushConstants( const auto& data ) -> void {
            MKT_BEGIN_PROFILER();
            const void* ptr{ MKT_ADDRESSOF( data ) };
            const core::usize size{ MKT_SIZEOF( data ) };

            static_assert( sizeof( data ) <= rhi::kMaxPushConstantSize, "Graph push constants exceed the shared layout." );
            mPushConstantsData.fill( 0 );
            std::memcpy( mPushConstantsData.data(), ptr, size );
        }
    private:
        struct CachedBottomLevel {
            core::u64 mRevision{};
            core::u32 mFirstIndex{};
            core::u32 mIndexCount{};
            core::u32 mVertexCount{};
            core::u32 mVertexStride{};
            rhi::BufferHandle mVertices{};
            rhi::BufferHandle mIndices{};
            rhi::AccelStructureHandle mStructure{};

            MKT_NODISCARD auto CanReuse( const FGAccelerationGeometry& geometry,
                const rhi::BufferHandle& vertices, const rhi::BufferHandle& indices ) const -> bool;
        };

    private:
        // [Internal usage]
        friend class FrameGraph;
        // Pass callbacks declare uses; only the graph records their barriers.
        auto SubmitBarriers(
            const ankerl::unordered_dense::map<FGResourceHandle,
            eastl::pair<eastl::string, FGBarrier>>& barriers ) -> void;

        auto CacheGpuDeviceAddress( FGBufferHandle handle ) -> rhi::DeviceAddress;

        auto RefreshBufferCaches() -> void;
        core::u64 mBufferRevision{};
        auto CacheResource( FGBufferHandle handle ) -> renderer::rhi::IBuffer*;
        auto CacheResource( FGTextureHandle handle ) -> renderer::rhi::ITexture*;

    private:
        FrameGraphPass* mNode{};
        FGResourceManager* mResourceManager{};
        FrameGraphDescriptorAllocator* mDescriptorAllocator{};

        FGNodeStatistics* mNodeStatistics{};
        core::u32 mDrawCallCount{};
        core::u32 mDispatchCallCount{};

        rhi::CommandListHandle mCommands{};
        FGScheduler* mScheduler{};
        rhi::PipelineLayoutHandle mPipelineLayout{};

        // Contexts are pass-local; separate targets within the pass keep independent caches.
        eastl::hash_map<FGResourceHandle, eastl::vector<CachedBottomLevel>> mBottomLevelCache{};

        eastl::fixed_hash_map<FGResourceHandle, rhi::DeviceAddress, 20> mCachedBda{};

        eastl::fixed_hash_map<FGResourceHandle, rhi::IBuffer*, 20> mCachedBuffers{};
        eastl::fixed_hash_map<FGResourceHandle, rhi::ITexture*, 20> mCachedTextures{};

        eastl::array<core::ubyte, rhi::kMaxPushConstantSize> mPushConstantsData{};
    };

    /** Owns CPU upload bytes between a preparation pass and its transfer pass.
     * Submit() records only copies; resource declarations supply every barrier.
     */
    class BufferUploadBatch final {
    public:
        auto Clear() -> void;
        auto CopyBuffer( FGBufferHandle buffer, core::usize offset, const void* data, core::usize sizeBytes ) -> void;
        auto CopyBuffer( FGBufferHandle buffer, const auto& value, core::usize offset ) -> void {
            CopyBuffer( buffer, offset, MKT_ADDRESSOF( value ), sizeof( value ) );
        }
        auto Submit( CommandContext& context ) -> void;

    private:
        struct Upload {
            FGBufferHandle mBuffer{};
            core::usize mOffset{};
            eastl::vector<core::ubyte> mBytes{};
        };
        eastl::vector<Upload> mUploads{};
    };
}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_CORE_COMMAND_CONTEXT_HH
