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
#include <Core/Platform.hh>
#include <Core/Profiler.hh>
#include <Core/Types.hh>

#include <Logging/Assert.hh>

#include <Memory/Allocator.hh>

#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/FrameGraphTypes.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::renderer::rhi;

    auto CommandContext::CachedBottomLevel::CanReuse( const FGAccelerationGeometry &geometry,
        const rhi::BufferHandle &vertices, const rhi::BufferHandle &indices ) const -> bool {
        return geometry.mRevision && mRevision == geometry.mRevision && mStructure &&
               mFirstIndex == geometry.mFirstIndex && mIndexCount == geometry.mIndexCount &&
               mVertexCount == geometry.mVertexCount && mVertexStride == geometry.mVertexStride &&
               mVertices.GetPtr() == vertices.GetPtr() && mIndices.GetPtr() == indices.GetPtr();
    }

    /**
     * @brief Maps a graph stage to synchronization stages.
     * @param state Frame-graph pipeline stage.
     * @returns Participating RHI pipeline-stage flags.
     */
    MKT_NODISCARD static constexpr auto GetPipelineStages( const FGPipelineStage state ) -> renderer::rhi::PipelineStageFlags {
        switch ( state ) {
            case FGPipelineStage::eUnknown:
                return renderer::rhi::PipelineStageFlagsBits::None;

            case FGPipelineStage::eConstantBuffer:
                return renderer::rhi::PipelineStageFlagsBits::AllGraphics | renderer::rhi::PipelineStageFlagsBits::ComputeShader;
            case FGPipelineStage::eVertexBuffer:
                return renderer::rhi::PipelineStageFlagsBits::VertexInput;
            case FGPipelineStage::eIndexBuffer:
                return renderer::rhi::PipelineStageFlagsBits::IndexInput;
            case FGPipelineStage::eIndirectArgument:
                return renderer::rhi::PipelineStageFlagsBits::DrawIndirect;

            case FGPipelineStage::eVertexShader:
                return renderer::rhi::PipelineStageFlagsBits::VertexShader;
            case FGPipelineStage::eHullShader:
                return renderer::rhi::PipelineStageFlagsBits::HullShader;
            case FGPipelineStage::eDomainShader:
                return renderer::rhi::PipelineStageFlagsBits::DomainShader;
            case FGPipelineStage::eGeometryShader:
                return renderer::rhi::PipelineStageFlagsBits::GeometryShader;
            case FGPipelineStage::eComputeShader:
                return renderer::rhi::PipelineStageFlagsBits::ComputeShader;
            case FGPipelineStage::eRayTracingShader:
                return renderer::rhi::PipelineStageFlagsBits::RayTracingShader;
            case FGPipelineStage::ePixelShader:
                return renderer::rhi::PipelineStageFlagsBits::PixelShader;

            case FGPipelineStage::eRenderTarget:
                return renderer::rhi::PipelineStageFlagsBits::RenderTarget;
            case FGPipelineStage::eDepthTarget:
                return renderer::rhi::PipelineStageFlagsBits::EarlyFragmentTests | renderer::rhi::PipelineStageFlagsBits::LateFragmentTests;

            // Small CopyBuffer uploads use vkCmdUpdateBuffer (CLEAR stage);
            // larger uploads use vkCmdCopyBuffer (COPY stage).
            case FGPipelineStage::eCopy:
                return renderer::rhi::PipelineStageFlagsBits::Copy | renderer::rhi::PipelineStageFlagsBits::Clear;
            case FGPipelineStage::eResolve:
                return renderer::rhi::PipelineStageFlagsBits::Resolve;
            case FGPipelineStage::ePresent:
                return renderer::rhi::PipelineStageFlagsBits::Bottom;

            case FGPipelineStage::eAccelStructRead:
            case FGPipelineStage::eAccelStructWrite:
            case FGPipelineStage::eAccelStructBuildInput:
            case FGPipelineStage::eAccelStructBuildBlas:
                return renderer::rhi::PipelineStageFlagsBits::AccelerationStructureBuild;
        }

        return renderer::rhi::PipelineStageFlagsBits::AllCommands;
    }

    /*
     * TODO: Revisit texture layout inference in the FrameGraph.
     *
     * Currently, GetTextureLayout() infers texture layouts from FGPipelineStage
     * and FGResourceAccess. This is ambiguous because pipeline stages describe
     * when a resource is accessed, not how it is used.
     *
     * Problems:
     * - Shader reads can target either sampled textures (ShaderResource) or
     *   storage textures (UnorderedAccess), requiring different layouts.
     * - ReadWrite access is currently treated as ShaderResource instead of
     *   UnorderedAccess.
     * - Depth attachments may require distinct read-only and writable layouts.
     * - FGPipelineStage mixes execution stages with resource usage concepts.
     *
     * Proposed solution:
     * - Introduce FGTextureUsage to explicitly distinguish sampled, storage,
     *   render target, depth/stencil, transfer, resolve, and present usages.
     * - Derive texture layouts from FGTextureUsage rather than FGPipelineStage.
     * - Keep FGResourceAccess for read/write hazard detection.
     * - Keep FGPipelineStage for synchronization stage masks.
     * - Infer these properties automatically from FrameGraph pass declarations.
     *
     * The RHI should translate the resulting resource state into the appropriate
     * Vulkan image layout, access masks, and pipeline stages, or their D3D12
     * Enhanced Barrier equivalents.
     */

    /**
     * @brief Maps a tracked state or declared access to a texture layout.
     * @param state Tracked RHI state, or the declared graph stage for the access overload.
     * @param access Read or write access declared by the pass.
     * @returns Layout required by the resource use.
     */
    MKT_NODISCARD static constexpr auto GetTextureLayout( const FGPipelineStage state, const FGResourceAccess access ) -> renderer::rhi::TextureLayoutFlags {
        switch ( state ) {
            case FGPipelineStage::eUnknown:
                return renderer::rhi::TextureLayoutBits::Unknown;
            case FGPipelineStage::eVertexShader:
            case FGPipelineStage::eHullShader:
            case FGPipelineStage::eDomainShader:
            case FGPipelineStage::eGeometryShader:
            case FGPipelineStage::eComputeShader:
            case FGPipelineStage::eRayTracingShader:
            case FGPipelineStage::ePixelShader:
                return access == FGResourceAccess::eWrite ? renderer::rhi::TextureLayoutBits::UnorderedAccess : renderer::rhi::TextureLayoutBits::ShaderResource;
            case FGPipelineStage::eRenderTarget:
                return renderer::rhi::TextureLayoutBits::RenderTarget;
            case FGPipelineStage::eDepthTarget:
                return renderer::rhi::TextureLayoutBits::DepthStencil;
            case FGPipelineStage::eCopy:
                return access == FGResourceAccess::eWrite ? renderer::rhi::TextureLayoutBits::CopyDest : renderer::rhi::TextureLayoutBits::CopySource;
            case FGPipelineStage::eResolve:
                return access == FGResourceAccess::eWrite ? renderer::rhi::TextureLayoutBits::ResolveDest : renderer::rhi::TextureLayoutBits::ResolveSource;
            case FGPipelineStage::ePresent:
                return renderer::rhi::TextureLayoutBits::Present;
            default:
                return renderer::rhi::TextureLayoutBits::General;
        }
    }

    /**
     * @brief Derives a barrier-access mask from a declared resource use.
     * @param stage Frame-graph pipeline stage declaring the use.
     * @param access Read or write access declared by the pass.
     * @param type Texture or buffer resource kind.
     * @returns Access flags for the stage, mode and resource kind.
     */
    MKT_NODISCARD static constexpr auto GetBarrierAccess( FGPipelineStage stage, FGResourceAccess access, FGResourceType type ) -> renderer::rhi::BarrierAccessFlags {
        using BAccessFlags = renderer::rhi::BarrierAccessFlagsBits;

        if ( access == FGResourceAccess::eNone ) {
            return BAccessFlags::None;
        }

        const bool writes{ access == FGResourceAccess::eWrite };

        switch ( stage ) {
            case FGPipelineStage::eConstantBuffer:
                return BAccessFlags::ConstantBuffer;
            case FGPipelineStage::eVertexBuffer:
                return BAccessFlags::VertexAttributeRead;
            case FGPipelineStage::eIndexBuffer:
                return BAccessFlags::IndexRead;
            case FGPipelineStage::eIndirectArgument:
                return BAccessFlags::IndirectCommandRead;
            // Load operations, blending, and depth testing can read attachments
            // even when the graph declaration describes a render-target write.
            case FGPipelineStage::eRenderTarget:
                return BAccessFlags::ColorAttachmentRead | BAccessFlags::ColorAttachmentWrite;
            case FGPipelineStage::eDepthTarget:
                return BAccessFlags::DepthStencilRead | BAccessFlags::DepthStencilWrite;
            case FGPipelineStage::eCopy:
                return writes ? BAccessFlags::TransferWrite : BAccessFlags::TransferRead;
            case FGPipelineStage::eResolve:
                return writes ? BAccessFlags::ResolveWrite : BAccessFlags::ResolveRead;
            case FGPipelineStage::ePresent:
            case FGPipelineStage::eUnknown:
                return BAccessFlags::None;
            case FGPipelineStage::eAccelStructRead:
                return BAccessFlags::AccelerationStructureRead;
            case FGPipelineStage::eAccelStructWrite:
            case FGPipelineStage::eAccelStructBuildBlas:
                return BAccessFlags::AccelerationStructureRead | BAccessFlags::AccelerationStructureWrite;
            case FGPipelineStage::eAccelStructBuildInput:
                return BAccessFlags::AccelerationStructureBuildInputRead;
            default:
                if ( writes ) {
                    // Shader writes can be read-modify-write operations.
                    return BAccessFlags::ShaderStorageRead | BAccessFlags::ShaderStorageWrite;
                }

                return type == FGResourceType::eTexture ? BAccessFlags::ShaderSampledRead : BAccessFlags::StructuredBufferRead;
        }
    }

    // Imported resources may lack exact graph history. Normal uses retain their scopes.
    /**
     * @brief Derives conservative access flags before exact history is available.
     * @param state Tracked RHI resource state.
     * @param type Texture or buffer resource kind.
     * @returns Access flags describing the tracked state.
     */
    MKT_NODISCARD static constexpr auto GetAccessForState( renderer::rhi::ResourceStates state, FGResourceType type ) -> renderer::rhi::BarrierAccessFlags {
        using A = renderer::rhi::BarrierAccessFlagsBits;

        switch ( state ) {
            case renderer::rhi::ResourceStates::eUnknown:
            case renderer::rhi::ResourceStates::ePresent:
                return A::None;
            case renderer::rhi::ResourceStates::eConstantBuffer:
                return A::ConstantBuffer;
            case renderer::rhi::ResourceStates::eVertexBuffer:
                return A::VertexAttributeRead;
            case renderer::rhi::ResourceStates::eIndexBuffer:
                return A::IndexRead;
            case renderer::rhi::ResourceStates::eIndirectArgument:
                return A::IndirectCommandRead;
            case renderer::rhi::ResourceStates::eShaderResource:
                return type == FGResourceType::eTexture ? A::ShaderSampledRead : A::StructuredBufferRead;
            case renderer::rhi::ResourceStates::eRenderTarget:
                return A::ColorAttachmentRead | A::ColorAttachmentWrite;
            case renderer::rhi::ResourceStates::eDepthRead:
                return A::DepthStencilRead;
            case renderer::rhi::ResourceStates::eDepthWrite:
                return A::DepthStencilRead | A::DepthStencilWrite;
            case renderer::rhi::ResourceStates::eCopySource:
                return A::TransferRead;
            case renderer::rhi::ResourceStates::eCopyDest:
                return A::TransferWrite;
            case renderer::rhi::ResourceStates::eResolveSource:
                return A::ResolveRead;
            case renderer::rhi::ResourceStates::eResolveDest:
                return A::ResolveWrite;
            case renderer::rhi::ResourceStates::eUnorderedAccess:
                return A::ShaderStorageRead | A::ShaderStorageWrite;
            case renderer::rhi::ResourceStates::eAccelStructRead:
                return A::AccelerationStructureRead;
            case renderer::rhi::ResourceStates::eAccelStructBuildInput:
                return A::AccelerationStructureBuildInputRead;
            case renderer::rhi::ResourceStates::eAccelStructWrite:
            case renderer::rhi::ResourceStates::eAccelStructBuildBlas:
                return A::AccelerationStructureRead | A::AccelerationStructureWrite;
            default:
                return A::MemoryRead | A::MemoryWrite;
        }
    }

    auto GetTextureLayout( const renderer::rhi::ResourceStates state ) -> renderer::rhi::TextureLayoutFlags {
        MKT_BEGIN_PROFILER();

        switch ( state ) {
            case rhi::ResourceStates::eVideoDecodeOutput:
                return rhi::TextureLayoutBits::VideoDecodeOutput;
            case rhi::ResourceStates::eVideoDecodeReference:
                return rhi::TextureLayoutBits::VideoDecodeReference;
            case renderer::rhi::ResourceStates::eUnknown:
                return renderer::rhi::TextureLayoutBits::Unknown;
            case renderer::rhi::ResourceStates::eShaderResource:
                return renderer::rhi::TextureLayoutBits::ShaderResource;
            case renderer::rhi::ResourceStates::eUnorderedAccess:
                return renderer::rhi::TextureLayoutBits::UnorderedAccess;
            case renderer::rhi::ResourceStates::eRenderTarget:
                return renderer::rhi::TextureLayoutBits::RenderTarget;
            case renderer::rhi::ResourceStates::eDepthRead:
                return renderer::rhi::TextureLayoutBits::DepthStencilReadOnly;
            case renderer::rhi::ResourceStates::eDepthWrite:
                return renderer::rhi::TextureLayoutBits::DepthStencil;
            case renderer::rhi::ResourceStates::eCopyDest:
                return renderer::rhi::TextureLayoutBits::CopyDest;
            case renderer::rhi::ResourceStates::eResolveDest:
                return renderer::rhi::TextureLayoutBits::ResolveDest;
            case renderer::rhi::ResourceStates::eCopySource:
                return renderer::rhi::TextureLayoutBits::CopySource;
            case renderer::rhi::ResourceStates::eResolveSource:
                return renderer::rhi::TextureLayoutBits::ResolveSource;
            case renderer::rhi::ResourceStates::ePresent:
                return renderer::rhi::TextureLayoutBits::Present;
            default:
                return renderer::rhi::TextureLayoutBits::General;
        }
    }

    auto GetResourceState( const FGPipelineStage state, const FGResourceAccess access ) -> renderer::rhi::ResourceStates {
        MKT_BEGIN_PROFILER();

        switch ( state ) {
            case FGPipelineStage::eUnknown:
                return renderer::rhi::ResourceStates::eUnknown;
            case FGPipelineStage::eRenderTarget:
                return renderer::rhi::ResourceStates::eRenderTarget;
            // BeginRenderPass currently uses writable depth attachment layouts.
            case FGPipelineStage::eDepthTarget:
                return renderer::rhi::ResourceStates::eDepthWrite;
            case FGPipelineStage::ePresent:
                return renderer::rhi::ResourceStates::ePresent;
            case FGPipelineStage::eConstantBuffer:
                return renderer::rhi::ResourceStates::eConstantBuffer;
            case FGPipelineStage::eVertexBuffer:
                return renderer::rhi::ResourceStates::eVertexBuffer;
            case FGPipelineStage::eIndexBuffer:
                return renderer::rhi::ResourceStates::eIndexBuffer;
            case FGPipelineStage::eIndirectArgument:
                return renderer::rhi::ResourceStates::eIndirectArgument;
            case FGPipelineStage::eVertexShader:
            case FGPipelineStage::eHullShader:
            case FGPipelineStage::eDomainShader:
            case FGPipelineStage::eGeometryShader:
            case FGPipelineStage::eComputeShader:
            case FGPipelineStage::eRayTracingShader:
            case FGPipelineStage::ePixelShader:
                return access == FGResourceAccess::eWrite ? renderer::rhi::ResourceStates::eUnorderedAccess : renderer::rhi::ResourceStates::eShaderResource;
            case FGPipelineStage::eCopy:
                return access == FGResourceAccess::eWrite ? renderer::rhi::ResourceStates::eCopyDest : renderer::rhi::ResourceStates::eCopySource;
            case FGPipelineStage::eResolve:
                return access == FGResourceAccess::eWrite ? renderer::rhi::ResourceStates::eResolveDest : renderer::rhi::ResourceStates::eResolveSource;
            case FGPipelineStage::eAccelStructRead:
                return renderer::rhi::ResourceStates::eAccelStructRead;
            case FGPipelineStage::eAccelStructWrite:
                return renderer::rhi::ResourceStates::eAccelStructWrite;
            case FGPipelineStage::eAccelStructBuildInput:
                return renderer::rhi::ResourceStates::eAccelStructBuildInput;
            case FGPipelineStage::eAccelStructBuildBlas:
                return renderer::rhi::ResourceStates::eAccelStructBuildBlas;
            default:
                return renderer::rhi::ResourceStates::eCommon;
        }
    }

    RenderBeginDescription::RenderTargetState::operator FGTextureHandle() const {
        MKT_BEGIN_PROFILER();
        return mRenderTarget;
    }


    auto DrawIndirectState::SetBuffer( FGBufferHandle handle ) -> DrawIndirectState & {
        MKT_BEGIN_PROFILER();
        mIndirectBuffer = handle;
        return *this;
    }

    auto DrawIndirectState::SetDrawCount( core::u32 count ) -> DrawIndirectState & {
        MKT_BEGIN_PROFILER();
        mInstanceCount = count;
        return *this;
    }

    auto RenderBeginDescription::Clear() -> void {
        MKT_BEGIN_PROFILER();
        mCurrentRenderTargets.clear();
        mDepthTarget = {};
        mRenderArea = {};
    }

    auto RenderBeginDescription::SetRenderArea( const renderer::rhi::Rect &rec ) -> RenderBeginDescription & {
        MKT_BEGIN_PROFILER();
        mRenderArea = rec;
        return *this;
    }

    auto RenderBeginDescription::AddDepthTarget( FGTextureHandle target, renderer::rhi::LoadOp op ) -> RenderBeginDescription & {
        MKT_BEGIN_PROFILER();
        mDepthTarget = RenderTargetState{
            .mClearColor = renderer::rhi::kColorWhite,
            .mLoadOp = op,
            .mRenderTarget = target,
        };

        return *this;
    }

    auto RenderBeginDescription::AddRenderTarget( FGTextureHandle target, const renderer::rhi::Color &c, renderer::rhi::LoadOp op, core::u32 faceIndex, core::u32 mipLevel ) -> RenderBeginDescription & {
        MKT_BEGIN_PROFILER();
        mCurrentRenderTargets.emplace_back( RenderTargetState{
            .mClearColor = c,
            .mLoadOp = op,
            .mRenderTarget = target,
            .mFaceIndex = faceIndex,
            .mMipLevel = mipLevel,
        } );

        return *this;
    }

    auto CommandContext::SetVideoRecording( FGScheduler *scheduler ) -> void {
        mScheduler = scheduler;
    }

    auto CommandContext::DecodeVideo( FGVideoHandle handle, const rhi::VideoDecodeDescription &description ) -> void {
        if ( !mCommands || mCommands->GetQueueType() != rhi::QueueType::eVideoDecode || !mScheduler || mResourceManager->Get( handle.mHandle ).mType != FGResourceType::eVideo ) {
            MKT_THROW_RUNTIME_ERROR( "DecodeVideo must be recorded by a video graph pass." );
        }

        const bool coincident{ description.mDecoder->UsesCoincidentOutput() };

        if ( !coincident ) {
            mScheduler->UseVideoResource( mCommands.GetPtr(), description.mOutput->GetImage(), false, true );
        }

        if ( description.mReconstruction.mSurface ) {
            mScheduler->UseVideoResource( mCommands.GetPtr(), description.mReconstruction.mSurface->GetImage(), true, coincident );
        }

        for ( const auto &reference: description.mReferences ) {
            // Coincident DPB images may be displayed later in presentation order.
            // Return reference reads to graphics too, not only this batch's outputs.
            mScheduler->UseVideoResource( mCommands.GetPtr(), reference.mSurface->GetImage(), true, coincident );
        }

        mCommands->DecodeVideo( description );
    }

    CommandContext::CommandContext( FrameGraphPass *pass, FGResourceManager *resourceManager,
        FrameGraphDescriptorAllocator *descriptorAllocator,
        rhi::PipelineLayoutHandle pipelineLayout, FGStatisticsManager *statsManager )
        : mNode{ pass }, mResourceManager{ resourceManager }, mDescriptorAllocator{ descriptorAllocator }, mPipelineLayout{ eastl::move( pipelineLayout ) } {
        MKT_BEGIN_PROFILER();
        MKT_ASSERT( mNode, "Frame graph node cannot be null" );
        MKT_ASSERT( mResourceManager, "Resource manager cannot be null" );
        MKT_ASSERT( mDescriptorAllocator, "Descriptor allocator cannot be null" );
        MKT_ASSERT( mPipelineLayout, "Pipeline layout cannot be empty" );
        MKT_ASSERT( statsManager, "Stats manager cannot be null" );

        mNodeStatistics = statsManager->GetNode( pass->mName );

        mCommands = pass->mCommandList;
    }

    auto CommandContext::BeginRender( const RenderBeginDescription &gs ) -> void {
        MKT_BEGIN_PROFILER();
        auto graphicsState{ renderer::rhi::RenderPassDescription{}
                .SetRenderArea( gs.mRenderArea ) };

        if ( gs.mDepthTarget.mRenderTarget.mHandle != FGResourceManager::kInvalidResourceHandle ) {
            graphicsState.AddDepthTarget( mResourceManager->Get( gs.mDepthTarget.mRenderTarget.mHandle ).mResource, gs.mDepthTarget.mLoadOp );
        }

        for ( const auto &colorImage: gs.mCurrentRenderTargets ) {
            renderer::rhi::TextureHandle texture{ mResourceManager->Get( colorImage.mRenderTarget.mHandle ).mResource };

            if ( !texture.IsEmpty() ) {
                graphicsState.AddRenderTarget( texture,
                    colorImage.mClearColor,
                    colorImage.mLoadOp,
                    renderer::rhi::TextureSubresourceSet{
                        colorImage.mMipLevel, // mBaseMipLevel
                        1,                    // mNumMipLevels
                        colorImage.mFaceIndex,// mBaseArraySlice (0 = +X, 1 = -X, etc.)
                        1                     // mNumArraySlices (Face count)
                    } );
            }
        }

        mCommands->BeginRenderPass( graphicsState );
    }

    auto CommandContext::EndRender() -> void {
        MKT_BEGIN_PROFILER();
        mCommands->EndRenderPass();
    }

    auto CommandContext::SetViewportState( const renderer::rhi::ViewportState &vs ) -> void {
        MKT_BEGIN_PROFILER();
        mCommands->SetViewportState( vs );
    }

    auto CommandContext::GetDeviceBufferAddress( FGBufferHandle handle ) -> core::u64 {
        MKT_BEGIN_PROFILER();
        MKT_ASSERT( mResourceManager, "FrameGraph Resource manager cannot be null" );
        return ( core::u64 )CacheGpuDeviceAddress( handle );
    }

    auto CommandContext::PushTexture_SRV( FGTextureHandle handle ) -> core::u32 {
        MKT_BEGIN_PROFILER();
        auto &resource{ mResourceManager->Get( handle.mHandle ) };
        auto *texture{ core::checked_cast<rhi::ITexture *>( resource.mResource.GetPtr() ) };

        return mDescriptorAllocator->AllocateTextureIndex_SRV( texture );
    }

    auto CommandContext::PushTexture_UAV( FGTextureHandle handle ) -> core::u32 {
        MKT_BEGIN_PROFILER();
        auto &resource{ mResourceManager->Get( handle.mHandle ) };
        auto *texture{ core::checked_cast<rhi::ITexture *>( resource.mResource.GetPtr() ) };

        return mDescriptorAllocator->AllocateTextureIndex_UAV( texture );
    }

    auto CommandContext::PushAccelerationStructure( FGAccelerationStructureHandle handle ) -> core::u32 {
        MKT_BEGIN_PROFILER();
        auto &resource{ mResourceManager->Get( handle.mHandle ) };
        auto *structure{ core::checked_cast<rhi::IAccelerationStructure *>( resource.mResource.GetPtr() ) };

        return mDescriptorAllocator->AllocateAccelerationStructureIndex( structure );
    }

    auto CommandContext::BuildAccelerationStructures(
        FGAccelerationStructureHandle handle,
        FGBufferHandle vertices, FGBufferHandle indices,
        eastl::span<const FGAccelerationGeometry> geometries ) -> void {
        MKT_BEGIN_PROFILER();

        MKT_ASSERT( mResourceManager, "FrameGraph Resource manager cannot be null" );
        MKT_ASSERT( mCommands, "Acceleration structure builds require a command list" );

        auto *device{ mCommands->GetOwningDevice() };
        rhi::BufferHandle vertexBuffer{};
        rhi::BufferHandle indexBuffer{};

        if ( !geometries.empty() ) {
            vertexBuffer = mResourceManager->Get( vertices.mHandle ).mResource;
            indexBuffer = mResourceManager->Get( indices.mHandle ).mResource;
        }

        auto &cached{ mBottomLevelCache[handle.mHandle] };
        cached.resize( geometries.size() );

        rhi::AccelStructureCreateDescription top{
            .mType = rhi::AccelStructureType::eTopLevel
        };
        top.mInstances.reserve( geometries.size() );
        core::usize slot{};

        // Rebuild changed local shapes; instance transforms belong to the TLAS.
        // https://docs.vulkan.org/guide/latest/extensions/ray_tracing.html
        for ( const auto &geometry: geometries ) {
            auto &entry{ cached[slot++] };

            if ( !entry.CanReuse( geometry, vertexBuffer, indexBuffer ) ) {
                const rhi::AccelStructureCreateDescription bottom{
                    .mType = rhi::AccelStructureType::eBottomLevel,
                    .mVertices = vertexBuffer,
                    .mIndices = indexBuffer,
                    .mVertexCount = geometry.mVertexCount,
                    .mVertexStride = geometry.mVertexStride,
                    .mIndexCount = geometry.mIndexCount,
                    .mIndexOffset = core::usize{ geometry.mFirstIndex } * sizeof( core::u32 )
                };

                auto structure{ device->CreateAccelStructure( bottom ) };
                mCommands->Build( structure.GetPtr() );

                entry = CachedBottomLevel{
                    .mRevision = geometry.mRevision,
                    .mFirstIndex = geometry.mFirstIndex,
                    .mIndexCount = geometry.mIndexCount,
                    .mVertexCount = geometry.mVertexCount,
                    .mVertexStride = geometry.mVertexStride,
                    .mVertices = vertexBuffer,
                    .mIndices = indexBuffer,
                    .mStructure = eastl::move( structure )
                };
            }

            top.mInstances.push_back( rhi::RayTracingInstance{
                .mBottomLevel = entry.mStructure,
                .mTransform = geometry.mTransform,
                .mInstanceID = geometry.mInstanceID,
                .mMask = geometry.mMask } );
        }

        auto structure{ device->CreateAccelStructure( top ) };
        mCommands->Build( structure.GetPtr() );

        // Publishing the result keeps descriptor allocation lazy until its first shader use.
        mResourceManager->SetResource( handle.mHandle, structure );
    }

    auto CommandContext::PushSampler( FGSamplerHandle handle ) -> core::u32 {
        MKT_BEGIN_PROFILER();
        auto &resource{ mResourceManager->Get( handle.mHandle ) };
        auto *sampler{ core::checked_cast<rhi::ISampler *>( resource.mResource.GetPtr() ) };

        return mDescriptorAllocator->AllocateSamplerIndex( sampler );
    }

    auto CommandContext::PushBuffer_SRV( FGBufferHandle handle ) -> core::u32 {
        MKT_BEGIN_PROFILER();
        auto &resource{ mResourceManager->Get( handle.mHandle ) };
        auto *buffer{ core::checked_cast<rhi::IBuffer *>( resource.mResource.GetPtr() ) };

        return mDescriptorAllocator->AllocateBufferIndex_SRV( buffer );
    }

    auto CommandContext::PushBuffer_UAV( FGBufferHandle handle ) -> core::u32 {
        MKT_BEGIN_PROFILER();
        auto &resource{ mResourceManager->Get( handle.mHandle ) };
        auto *buffer{ core::checked_cast<rhi::IBuffer *>( resource.mResource.GetPtr() ) };

        return mDescriptorAllocator->AllocateBufferIndex_UAV( buffer );
    }

    auto CommandContext::SubmitBarriers( const ankerl::unordered_dense::map<FGResourceHandle, eastl::pair<eastl::string, FGBarrier>> &barriers ) -> void {
        eastl::vector<eastl::pair<FGResource *, FGBarrier>> resolved{};
        resolved.reserve( barriers.size() );

        for ( const auto &[resourceID, namedBarrier]: barriers ) {
            resolved.emplace_back( &mResourceManager->Get( resourceID ), namedBarrier.second );
        }

        SubmitBarriers( resolved );
    }

    auto CommandContext::SubmitBarriers( const eastl::vector<eastl::pair<FGResource *, FGBarrier>> &barriers ) -> void {
        MKT_BEGIN_PROFILER();
        RefreshBufferCaches();

        if ( barriers.empty() ) {
            return;
        }

        struct PendingState {
            FGResource *mResource{};
            renderer::rhi::DeviceObject *mDeviceResource{};
            FGBarrierHistory mHistory{};
        };

        renderer::rhi::BarrierDescription description{};
        eastl::fixed_vector<PendingState, 20> pendingStates{};
        pendingStates.reserve( barriers.size() );

        for ( const auto &[entry, required]: barriers ) {
            FGResource &resource{ *entry };

            if ( resource.mType == FGResourceType::eAccelerationStructure || resource.mType == FGResourceType::eVideo ) {
                // Graph edges order build and trace passes. Native Build records the
                // AS-write to AS-read barrier, including BLAS-to-TLAS visibility.
                continue;
            }

            auto *deviceResource{ core::checked_cast<renderer::rhi::DeviceObject *>( resource.mResource.GetPtr() ) };
            const renderer::rhi::ResourceStates stateBefore{ deviceResource->GetResourceState() };
            const renderer::rhi::ResourceStates stateAfter{ GetResourceState( required.mNewStages.front(), required.mNewAccess ) };
            const auto &history{ resource.mBarrierHistory };

            const bool historyIsCurrent{ history.IsCurrent( stateBefore ) };

            const renderer::rhi::PipelineStageFlags stageBefore{ historyIsCurrent ? history.mStages : stateBefore == renderer::rhi::ResourceStates::eUnknown ? renderer::rhi::PipelineStageFlagsBits::None
                                                                                                                                                             : renderer::rhi::PipelineStageFlagsBits::AllCommands };

            const renderer::rhi::BarrierAccessFlags accessBefore{ historyIsCurrent ? history.mAccess : GetAccessForState( stateBefore, resource.mType ) };

            renderer::rhi::PipelineStageFlags stageAfter{ renderer::rhi::PipelineStageFlagsBits::None };
            renderer::rhi::BarrierAccessFlags accessAfter{ renderer::rhi::BarrierAccessFlagsBits::None };

            // Stages remain individual graph declarations. Only the barrier's
            // execution and memory scopes are unions, as in Vulkan sync2.
            for ( const auto stage: required.mNewStages ) {
                stageAfter |= GetPipelineStages( stage );
                accessAfter |= GetBarrierAccess( stage, required.mNewAccess, resource.mType );
            }

            switch ( resource.mType ) {
                case FGResourceType::eTexture: {
                    description.AddTexture( renderer::rhi::TextureBarrierDescription{}
                            .SetTexture( core::checked_cast<renderer::rhi::ITexture *>( resource.mResource.GetPtr() ) )
                            .SetBeforeLayout( GetTextureLayout( stateBefore ) )
                            .SetBeforeStage( stageBefore )
                            .SetBeforeAccess( accessBefore )
                            .SetAfterLayout( GetTextureLayout( required.mNewStages.front(), required.mNewAccess ) )
                            .SetAfterStage( stageAfter )
                            .SetAfterAccess( accessAfter ) );
                    break;
                }

                case FGResourceType::eBuffer: {
                    description.AddBuffer( renderer::rhi::BufferBarrierDescription{}
                            .SetBuffer( core::checked_cast<renderer::rhi::IBuffer *>( resource.mResource.GetPtr() ) )
                            .SetBeforeStage( stageBefore )
                            .SetBeforeAccess( accessBefore )
                            .SetAfterStage( stageAfter )
                            .SetAfterAccess( accessAfter ) );
                    break;
                }

                default:
                    MKT_ASSERT( false, "Unknown resource type" );
                    continue;
            }

            // Keep all outstanding reader scopes until a write or layout change.
            // A barrier is still emitted between reads: it carries availability
            // from the preceding writer to consumers at a different shader stage.
            const auto nextHistory{ history.Next( stateBefore, stateAfter, stageAfter, accessAfter ) };
            pendingStates.push_back( PendingState{ &resource, deviceResource, nextHistory } );
        }

        mCommands->SetBarrier( description );

        // Publish only after recording. Compilation and skipped passes must not
        // advance either the layout or its access history.
        for ( auto &pending: pendingStates ) {
            pending.mDeviceResource->SetResourceState( pending.mHistory.mState );
            pending.mResource->mBarrierHistory = pending.mHistory;
        }
    }

    auto CommandContext::ImportSampledTexture( renderer::rhi::TextureHandle texture ) -> FGTextureHandle {
        return mResourceManager->ImportSampledTexture( eastl::move( texture ) );
    }

    auto CommandContext::UseImportedTexture( FGTextureHandle texture ) -> core::u32 {
        return PushTexture_SRV( texture );
    }

    auto CommandContext::ImportTexture( renderer::rhi::TextureHandle handle ) -> FGTextureHandle {
        MKT_BEGIN_PROFILER();
        // Imported textures need to be synchronized externally
        // Before the frame graph runs the client needs to make sure the resource
        // is in the specific state the resource will be used in
        return mResourceManager->ImportTexture( handle );
    }

    auto CommandContext::ImportSampler( renderer::rhi::SamplerHandle ) -> FGSamplerHandle {
        MKT_BEGIN_PROFILER();
        return {};
    }

    auto CommandContext::ImportBuffer( renderer::rhi::BufferHandle handle ) -> FGBufferHandle {
        MKT_BEGIN_PROFILER();
        MKT_ASSERT( mResourceManager, "FrameGraph Resource manager cannot be null" );
        return mResourceManager->ImportBuffer( handle );
    }

    auto CommandContext::SetPolygonLineWidth( core::f32 width ) -> void {
        MKT_BEGIN_PROFILER();
        mCommands->SetPolygonLineWidth( width );
    }

    auto CommandContext::BindPipeline( FGPipelineHandle handle ) -> void {
        MKT_BEGIN_PROFILER();
        MKT_ASSERT( mResourceManager, "FrameGraph Resource manager cannot be null" );
        FGResource pipeline{ mResourceManager->Get( handle.mHandle ) };
        auto *resource{ core::checked_cast<renderer::rhi::IPipeline *>( pipeline.mResource.GetPtr() ) };
        // Graphics, compute and ray tracing use separate binding state. Select
        // the graph's shared layout/table for the pipeline being recorded.
        auto table{ mDescriptorAllocator->GetDescriptorTable() };
        mCommands->BindPipelineResources( renderer::rhi::BindResourcesDescription{}
                .AddResourceSet( 0, table.GetPtr() )
                .SetPipelineLayout( mPipelineLayout.GetPtr() )
                .SetBindPoint( resource->GetPipelineType() ) );
        mCommands->BindPipeline( resource );
    }

    auto CommandContext::BindShaderTable( rhi::ShaderTableHandle table ) -> void {
        MKT_BEGIN_PROFILER();
        mCommands->BindShaderTable( table.GetPtr() );
    }

    auto CommandContext::BindShaderTable( FGShaderTableHandle table ) -> void {
        BindShaderTable( rhi::ShaderTableHandle{ mResourceManager->Get( table.mHandle ).mResource } );
    }

    auto CommandContext::Draw( core::u32 vertexCount, core::u32 instanceCount ) -> void {
        MKT_BEGIN_PROFILER();
        renderer::rhi::IPipelineLayout *layout{ mPipelineLayout.GetPtr() };
        mCommands->SetPushConstants( layout, mPushConstantsData.data(), kMaxPushConstantSize, renderer::rhi::ShaderFlagsBits::All );
        mCommands->Draw( renderer::rhi::DrawArguments{}
                .SetVertexCount( vertexCount )
                .SetInstanceCount( instanceCount ) );
    }

    auto CommandContext::DrawIndirect( const DrawIndirectState &state ) -> void {
        MKT_BEGIN_PROFILER();

        if ( state.mInstanceCount == 0 ) {
            return;
        }


        MKT_ASSERT( mResourceManager, "FrameGraph Resource manager cannot be null" );
        FGResource resource{ mResourceManager->Get( state.mIndirectBuffer.mHandle ) };

        renderer::rhi::IPipelineLayout *layout{ mPipelineLayout.GetPtr() };
        mCommands->SetPushConstants( layout, mPushConstantsData.data(), kMaxPushConstantSize, renderer::rhi::ShaderFlagsBits::All );
        mCommands->BindIndirectBuffer( core::checked_cast<renderer::rhi::IBuffer *>( resource.mResource.GetPtr() ) );
        mCommands->DrawIndirect( state.mFirstDraw * sizeof( DrawIndirectCommand ), state.mInstanceCount );
    }

    auto CommandContext::Dispatch( core::u32 groupX, core::u32 groupY, core::u32 groupZ ) -> void {
        MKT_BEGIN_PROFILER();
        renderer::rhi::IPipelineLayout *layout{ mPipelineLayout.GetPtr() };
        mCommands->SetPushConstants( layout, mPushConstantsData.data(), kMaxPushConstantSize, renderer::rhi::ShaderFlagsBits::All );
        mCommands->Dispatch( groupX, groupY, groupZ );
    }

    auto CommandContext::GetCommandList() -> rhi::ICommandList * {
        return mCommands.GetPtr();
    }

    auto CommandContext::GetTexture( FGTextureHandle texture ) -> rhi::ITexture * {
        return CacheResource( texture );
    }

    auto CommandContext::DispatchExternalCompute( const ExternalComputeDispatch &dispatch ) -> void {
        using namespace rhi;
        MKT_ASSERT( dispatch.mDevice && dispatch.mPipeline, "External compute requires a device and pipeline." );
        BindingTableDescription resources{};
        BindingTableDescription constants{};
        BarrierDescription before{};
        BarrierDescription after{};
        eastl::fixed_hash_set<FGResourceHandle, 64> transitioned{};
        const auto barrier{ []( ITexture *texture, bool restore, bool storage ) {
            TextureBarrierDescription b{};
            b.mTexture = texture;
            b.mStageBefore = b.mStageAfter = PipelineStageFlagsBits::ComputeShader;
            b.mLayoutBefore = restore ? TextureLayoutBits::ShaderResource : TextureLayoutBits::General;
            b.mLayoutAfter = restore || storage ? TextureLayoutBits::General : TextureLayoutBits::ShaderResource;
            b.mAccessBefore = restore ? BarrierAccessFlagsBits::ShaderSampledRead : BarrierAccessFlagsBits::ShaderStorageRead | BarrierAccessFlagsBits::ShaderStorageWrite;
            b.mAccessAfter = restore || storage ? BarrierAccessFlagsBits::ShaderStorageRead | BarrierAccessFlagsBits::ShaderStorageWrite : BarrierAccessFlagsBits::ShaderSampledRead;
            return b;
        } };
        for ( const auto &input: dispatch.mTextures ) {
            auto *texture{ CacheResource( input.mTexture ) };
            resources.AddItem( input.mStorage ? BindingTableItem::TextureUAV( input.mSlot, texture ) : BindingTableItem::TextureSRV( input.mSlot, texture ) );
            if ( input.mInternal && transitioned.insert( input.mTexture.mHandle ).second ) {
                before.mTextures.push_back( barrier( texture, false, input.mStorage ) );
                if ( !input.mStorage ) {
                    after.mTextures.push_back( barrier( texture, true, false ) );
                }
            }
        }
        if ( !before.mTextures.empty() ) {
            mCommands->SetBarrier( before );
        }
        auto resourceTable{ dispatch.mDevice->CreateBindingTable( resources, dispatch.mResourceLayout ) };
        constants.AddItem( BindingTableItem::ConstantBuffer( dispatch.mConstantSlot, CacheResource( dispatch.mConstants ) ) );
        for ( core::u32 i{}; i < dispatch.mSamplers.size(); ++i ) {
            auto sampler{ dispatch.mSamplers[i] };
            constants.AddItem( BindingTableItem::Sampler( dispatch.mSamplerSlot + i, sampler.GetPtr() ) );
        }
        auto constantTable{ dispatch.mDevice->CreateBindingTable( constants, dispatch.mConstantLayout ) };
        auto pipeline{ dispatch.mPipeline };
        mCommands->BindPipeline( pipeline.GetPtr() );
        mCommands->BindPipelineResources( BindResourcesDescription{}
                .SetPipelineLayout( dispatch.mPipeline->GetPipelineLayout().GetPtr() )
                .SetBindPoint( PipelineType::eCompute )
                .AddResourceSet( 0, resourceTable.GetPtr() )
                .AddResourceSet( 1, constantTable.GetPtr() ) );
        mCommands->Dispatch( dispatch.mGroups.x, dispatch.mGroups.y, dispatch.mGroups.z );
        if ( !after.mTextures.empty() ) {
            mCommands->SetBarrier( after );
        }
    }

    auto CommandContext::DispatchRays( core::u32 width, core::u32 height ) -> void {
        MKT_BEGIN_PROFILER();
        mCommands->SetPushConstants( mPipelineLayout.GetPtr(), mPushConstantsData.data(), rhi::kMaxPushConstantSize, rhi::ShaderFlagsBits::All );
        mCommands->DispatchRays( { .mWidth = width, .mHeight = height } );
    }

    auto CommandContext::RefreshBufferCaches() -> void {
        const auto revision{ mResourceManager->GetBufferRevision() };

        if ( revision != mBufferRevision ) {
            // Contexts survive across frames. A stable graph handle must never
            // leave another pass with an address or native pointer for an old buffer.
            mCachedBda.clear();
            mCachedBuffers.clear();
            mBufferRevision = revision;
        }
    }

    auto CommandContext::ReplaceBufferStorage( FGBufferHandle buffer, core::usize bytes ) -> void {
        mResourceManager->ReplaceBufferStorage( buffer, bytes );
        RefreshBufferCaches();
    }

    auto CommandContext::EnsureBufferCapacity( FGBufferHandle buffer, core::usize bytes ) -> bool {
        const bool changed{ mResourceManager->GrowBuffer( buffer, bytes ) };
        RefreshBufferCaches();
        return changed;
    }

    auto CommandContext::CopyBuffer( FGBufferHandle dstBuffer, FGBufferHandle srcBuffer ) -> void {
        MKT_BEGIN_PROFILER();

        MKT_ASSERT( mResourceManager, "FrameGraph Resource manager cannot be null" );

        renderer::rhi::IBuffer *srcResource{ CacheResource( srcBuffer ) };
        renderer::rhi::IBuffer *dstResource{ CacheResource( dstBuffer ) };

        mCommands->Copy( srcResource, dstResource );
    }

    auto CommandContext::CopyBuffer( FGBufferHandle dstBuffer, renderer::rhi::IBuffer *src, core::usize dstOffset ) -> void {
        MKT_BEGIN_PROFILER();

        MKT_ASSERT( mResourceManager, "FrameGraph Resource manager cannot be null" );

        renderer::rhi::IBuffer *resource{ CacheResource( dstBuffer ) };

        mCommands->Copy( src, resource, dstOffset );
    }

    auto CommandContext::CopyBuffer( FGBufferHandle dstBuffer, core::usize offset, const void *ptr, core::usize sizeBytes ) -> void {
        MKT_BEGIN_PROFILER();

        MKT_ASSERT( mResourceManager, "FrameGraph Resource manager cannot be null" );

        renderer::rhi::IBuffer *buffer{ CacheResource( dstBuffer ) };
        mCommands->Write( buffer, offset, ptr, sizeBytes );
    }

    auto CommandContext::Copy( FGBufferHandle dstBuffer, FGTextureHandle srcImage ) -> void {
        MKT_BEGIN_PROFILER();

        MKT_ASSERT( mResourceManager, "FrameGraph Resource manager cannot be null" );

        renderer::rhi::IBuffer *buffer{ CacheResource( dstBuffer ) };
        renderer::rhi::ITexture *image{ CacheResource( srcImage ) };

        mCommands->Copy( buffer, image );
    }

    auto CommandContext::CopyTexture( FGTextureHandle destImage, const void *data, core::usize size ) -> void {
        MKT_BEGIN_PROFILER();
        MKT_ASSERT( mResourceManager, "FrameGraph Resource manager cannot be null" );
        renderer::rhi::ITexture *image{ CacheResource( destImage ) };
        mCommands->Write( image, data, size );
    }

    auto CommandContext::CopyTexture( FGTextureHandle source, const renderer::rhi::TextureSlice &sourceSlice, FGTextureHandle destination, const renderer::rhi::TextureSlice &destinationSlice ) -> void {
        MKT_BEGIN_PROFILER();
        MKT_ASSERT( source.mHandle != destination.mHandle, "Whole-resource copy barriers require distinct textures" );
        MKT_ASSERT( sourceSlice.mWidth == destinationSlice.mWidth && sourceSlice.mHeight == destinationSlice.mHeight && sourceSlice.mDepth == destinationSlice.mDepth,
            "Texture copies require equally sized regions; resampling belongs in a shader pass" );
        mCommands->Copy( CacheResource( source ), sourceSlice, CacheResource( destination ), destinationSlice );
    }

    auto CommandContext::BuildAccelerationStructure( rhi::AccelStructureHandle structure ) -> void {
        mCommands->Build( structure.GetPtr() );
    }

    auto CommandContext::CopyBuffer( rhi::BufferHandle destination, core::usize offset, const void *data, core::usize sizeBytes ) -> void {
        MKT_BEGIN_PROFILER();
        mCommands->Write( destination.GetPtr(), offset, data, sizeBytes );
    }

    auto CommandContext::CopyTexture( FGTextureHandle source, rhi::TextureHandle destination ) -> void {
        MKT_BEGIN_PROFILER();

        auto *texture{ CacheResource( source ) };

        MKT_ASSERT( texture != destination.GetPtr(), "Texture copies require distinct resources." );
        MKT_ASSERT( texture->GetWidth() == destination->GetWidth() && texture->GetHeight() == destination->GetHeight() && texture->GetDepth() == destination->GetDepth(), "Texture copy extents must match." );

        // A private history copy is part of this pass. Restore its declared state
        // afterwards, but discard old shader-only scopes because a copy also ran.
        const auto state{ texture->GetResourceState() };
        mResourceManager->Get( source.mHandle ).mBarrierHistory = {};

        const rhi::TextureSlice slice{ .mWidth = texture->GetWidth(), .mHeight = texture->GetHeight(), .mDepth = texture->GetDepth() };
        mCommands->Copy( texture, slice, destination.GetPtr(), slice );
    }

    auto CommandContext::ResolveTexture( FGTextureHandle source, FGTextureHandle destination ) -> void {
        auto *src{ CacheResource( source ) };
        auto *dst{ CacheResource( destination ) };

        MKT_ASSERT( src->GetWidth() == dst->GetWidth() && src->GetHeight() == dst->GetHeight(), "Resolve extents must match" );

        const rhi::TextureSlice slice{ .mWidth = static_cast<core::u32>( src->GetWidth() ), .mHeight = static_cast<core::u32>( src->GetHeight() ) };
        mCommands->Resolve( src, slice, dst, slice );
    }

    auto CommandContext::CacheGpuDeviceAddress( FGBufferHandle handle ) -> rhi::DeviceAddress {
        MKT_BEGIN_PROFILER();

        MKT_ASSERT( mResourceManager, "FrameGraph Resource manager cannot be null" );

        auto itFind{ mCachedBda.find( handle.mHandle ) };

        if ( itFind == mCachedBda.end() ) {
            FGResource resource{ mResourceManager->Get( handle.mHandle ) };
            renderer::rhi::IBuffer *buffer{ core::checked_cast<renderer::rhi::IBuffer *>( resource.mResource.GetPtr() ) };
            itFind = mCachedBda.try_emplace( itFind, handle.mHandle, buffer->GetShaderDeviceAddress() );
        }

        return itFind->second;
    }

    auto CommandContext::CacheResource( FGBufferHandle handle ) -> renderer::rhi::IBuffer * {
        MKT_BEGIN_PROFILER();

        MKT_ASSERT( mResourceManager, "FrameGraph Resource manager cannot be null" );

        auto itFind{ mCachedBuffers.find( handle.mHandle ) };

        if ( itFind == mCachedBuffers.end() ) {
            FGResource resource{ mResourceManager->Get( handle.mHandle ) };
            renderer::rhi::IBuffer *buffer{ core::checked_cast<renderer::rhi::IBuffer *>( resource.mResource.GetPtr() ) };
            itFind = mCachedBuffers.try_emplace( itFind, handle.mHandle, buffer );
        }

        return itFind->second;
    }

    auto CommandContext::CacheResource( FGTextureHandle handle ) -> renderer::rhi::ITexture * {
        MKT_BEGIN_PROFILER();

        MKT_ASSERT( mResourceManager, "FrameGraph Resource manager cannot be null" );

        auto itFind{ mCachedTextures.find( handle.mHandle ) };

        if ( itFind == mCachedTextures.end() ) {
            FGResource resource{ mResourceManager->Get( handle.mHandle ) };
            renderer::rhi::ITexture *texture{ core::checked_cast<renderer::rhi::ITexture *>( resource.mResource.GetPtr() ) };
            itFind = mCachedTextures.try_emplace( itFind, handle.mHandle, texture );
        }

        return itFind->second;
    }

    auto DrawIndirectState::SetFirstDraw( core::u32 first ) -> DrawIndirectState & {
        MKT_BEGIN_PROFILER();
        mFirstDraw = first;
        return *this;
    }

    auto CommandContext::ResetCallCounts( bool newRecording ) -> void {
        MKT_BEGIN_PROFILER();
        mStatisticsBaseline = newRecording ? rhi::CommandListStatistics{} : mCommands->GetStatistics();
    }

    auto CommandContext::GetStatistics() const -> rhi::CommandListStatistics {
        return mCommands->GetStatistics() - mStatisticsBaseline;
    }

    auto CommandContext::GetDrawCallCount() const -> core::u32 {
        MKT_BEGIN_PROFILER();
        return core::as<core::u32>( GetStatistics().mDrawCallCount );
    }

    auto CommandContext::GetDispatchCallCount() const -> core::u32 {
        MKT_BEGIN_PROFILER();
        return core::as<core::u32>( GetStatistics().mDispatchCallCount );
    }


    auto BufferUploadBatch::Clear() -> void {
        mUploads.clear();
    }

    auto BufferUploadBatch::CopyBuffer( FGBufferHandle buffer, core::usize offset, const void *data, core::usize sizeBytes ) -> void {
        if ( sizeBytes == 0 ) {
            return;
        }

        MKT_ASSERT( data, "A nonempty upload requires CPU data" );
        Upload upload{ .mBuffer = buffer, .mOffset = offset };
        const auto *bytes{ static_cast<const core::ubyte *>( data ) };
        upload.mBytes.assign( bytes, bytes + sizeBytes );
        mUploads.push_back( eastl::move( upload ) );
    }

    auto BufferUploadBatch::Submit( CommandContext &context ) -> void {
        for ( const auto &upload: mUploads ) {
            context.CopyBuffer( upload.mBuffer, upload.mOffset, upload.mBytes.data(), upload.mBytes.size() );
        }

        Clear();
    }
}// namespace mikoto::renderer
