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

#ifndef MIKOTO_RENDERER_FRAME_GRAPH_TYPES_HH
#define MIKOTO_RENDERER_FRAME_GRAPH_TYPES_HH

#include <ankerl/unordered_dense.h>

#include <EASTL/fixed_hash_map.h>
#include <EASTL/fixed_vector.h>
#include <EASTL/functional.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/unordered_map.h>
#include <EASTL/vector.h>

#include <taskflow/taskflow.hpp>

#include <Assets/AssetRegistryService.hh>

#include <Core/Blackboard.hh>
#include <Core/Core.hh>
#include <Core/Profiler.hh>
#include <Core/Resource.hh>
#include <Core/Types.hh>

#include <Filesystem/Path.hh>

#include <Memory/Allocator.hh>
#include <Memory/FreeListAllocator.hh>
#include <Memory/MemoryArena.hh>

#include <Renderer/Core/GpuQueryProfiler.hh>
#include <Renderer/Rhi/Descriptor.hh>
#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Rhi/Pipeline.hh>
#include <Renderer/Rhi/Types.hh>

namespace mikoto::renderer {

    // Shader define
    typedef core::u64 SPointer;
    typedef core::u64 SArraySize;

    // Represents the handle for any resource
    using FGResourceHandle = core::u32;

    struct FGTextureHandle {
        FGResourceHandle mHandle{};
    };
    struct FGBufferHandle {
        FGResourceHandle mHandle{};
    };
    struct FGDenoiserHandle {
        FGResourceHandle mHandle{};
    };
    struct FGSamplerHandle {
        FGResourceHandle mHandle{};
    };
    struct FGPipelineHandle {
        FGResourceHandle mHandle{};
    };

    struct FGVideoHandle {
        FGResourceHandle mHandle{};
    };

    struct FGShaderTableHandle {
        FGResourceHandle mHandle{};
    };
    struct FGAccelerationStructureHandle {
        FGResourceHandle mHandle{};
    };

    // Runtime video batches have no fixed image until a picture is decoded.
    struct FGVideoDescription {
        eastl::string mName{};
    };

    enum class FGStageType {
        eVertex,
        eHull,
        eDomain,
        eGeometry,
        ePixel,

        eTask,
        eMesh,

        eTransfer,

        eCompute,

        eRayGen,
        eIntersection,
        eAnyHit,
        eClosestHit,
        eMiss,
        eCount,
    };

    // These should represent pipeline stageFGPipelineStage::eUnorderedAccess
    enum class FGPipelineStage {
        eUnknown,
        eConstantBuffer,
        eVertexBuffer,
        eIndexBuffer,
        eIndirectArgument,

        eVertexShader,
        ePixelShader,
        eHullShader,
        eDomainShader,
        eGeometryShader,
        eComputeShader,
        eRayTracingShader,

        eRenderTarget,
        eDepthTarget,
        eCopy,
        eResolve,
        ePresent,

        eAccelStructRead,
        eAccelStructWrite,
        eAccelStructBuildInput,
        eAccelStructBuildBlas,
    };

    // This will represent resource access
    enum class FGResourceAccess {
        eNone,
        eRead,
        // Read-only shader storage access (GENERAL), still a dependency reader.
        eReadStorage,
        eWrite,
    };

    enum class FGExecutionPolicy {
        ePerFrame,
        eOnWake,
        eOnce,
    };

    enum class FGResourceType {
        eInvalid = -1,
        eTexture,
        eBuffer,
        eSampler,
        ePipeline,
        eAccelerationStructure,
        eShaderTable,
        eVideo,
        eDenoiser,
    };

    enum class FGPassType {
        eGraphics,
        eCompute,
        eTransfer,
        eVideo,
        eGeneric,// For passes that not really need any kind of GPU work
    };

    struct ResourceVersion {
        eastl::string mWriterPass{};                 // Each Read() links to the current version's writer -> automatic dependency edge.
        eastl::vector<eastl::string> mReaderPasses{};// Each Write() to a resource creates a new version.

        MKT_NODISCARD auto HasWriter() const -> bool;
    };

    struct FGNodeResource {
        eastl::string mName{};
        eastl::any mDescription{};
        eastl::vector<ResourceVersion> mVersions{};// version 0, 1, 2...
        bool mIsImported{ false };                 // imported = externally owned (e.g. swapchain)

        // Rebuilds must reload or replace initializer data released after upload.
        bool mInitializersReleased{};
        filesystem::Path mSourcePath{};
    };

    struct FGResourceTrack {
        // Keep each declared stage; a Vulkan barrier combines their stage masks.
        eastl::fixed_vector<FGPipelineStage, 2> mStages{};
        FGResourceAccess mAccess{};
    };

    struct FGDenoiserDescription : rhi::DenoiserDescription {
        eastl::string mName{};

        auto SetName( eastl::string_view name ) -> FGDenoiserDescription &;
        auto SetType( rhi::ImageDenoiserType type ) -> FGDenoiserDescription &;
        auto SetQuality( rhi::ImageDenoiserQuality quality ) -> FGDenoiserDescription &;
        auto SetPreset( rhi::RayReconstructionPreset preset ) -> FGDenoiserDescription &;
        auto SetInputSize( core::uint2 size ) -> FGDenoiserDescription &;
        auto SetOutputSize( core::uint2 size ) -> FGDenoiserDescription &;
    };

    struct FGDenoiserDispatch {
        FGTextureHandle mColor{};
        FGTextureHandle mDiffuseAlbedo{};
        FGTextureHandle mSpecularAlbedo{};
        FGTextureHandle mNormalRoughness{};
        FGTextureHandle mDepth{};
        FGTextureHandle mMotion{};
        FGTextureHandle mOutput{};
        rhi::DenoiserFrameParameters mFrame{};
    };

    struct FGSamplerDescription {
        eastl::string mName{};

        float mMipLevels{ 1.0f };
        float mMaxAnisotropy{ 1.0f };

        renderer::rhi::SamplerFilter mMinFilter{ renderer::rhi::SamplerFilter::eNearest };
        renderer::rhi::SamplerFilter mMagFilter{ renderer::rhi::SamplerFilter::eNearest };
        renderer::rhi::SamplerWrapMode mWrapU{ renderer::rhi::SamplerWrapMode::eRepeat };
        renderer::rhi::SamplerWrapMode mWrapV{ renderer::rhi::SamplerWrapMode::eRepeat };
        renderer::rhi::SamplerWrapMode mWrapW{ renderer::rhi::SamplerWrapMode::eRepeat };

        renderer::rhi::Color mBorderColor{ 0.0f, 0.0f, 0.0f, 1.0f };

        auto SetMipLevels( float mipLevels ) -> FGSamplerDescription &;
        auto SetMaxAnisotropy( float anisotropy ) -> FGSamplerDescription &;

        auto SetBorderColor( const renderer::rhi::Color &color ) -> FGSamplerDescription &;

        auto SetName( eastl::string_view name ) -> FGSamplerDescription &;
        auto SetFilter( renderer::rhi::SamplerFilter filter ) -> FGSamplerDescription &;
        auto SetMinFilter( renderer::rhi::SamplerFilter filter ) -> FGSamplerDescription &;
        auto SetMagFilter( renderer::rhi::SamplerFilter filter ) -> FGSamplerDescription &;

        auto SetWrap( renderer::rhi::SamplerWrapMode wrap ) -> FGSamplerDescription &;
        auto SetWrapU( renderer::rhi::SamplerWrapMode wrap ) -> FGSamplerDescription &;
        auto SetWrapV( renderer::rhi::SamplerWrapMode wrap ) -> FGSamplerDescription &;
        auto SetWrapW( renderer::rhi::SamplerWrapMode wrap ) -> FGSamplerDescription &;
    };

    struct FGPipelineDescription {
        eastl::string mName{};

        rhi::Multisampling mMultisampling{ rhi::Multisampling::eMsaaX1 };
        renderer::rhi::PrimitiveTopology mTopology{ renderer::rhi::PrimitiveTopology::eTriangleList };
        renderer::rhi::PipelineType mPipelineType{ renderer::rhi::PipelineType::eInvalid };

        eastl::fixed_hash_map<FGStageType, filesystem::Path,
            core::as<core::u32>( FGStageType::eCount )>
            mShaders{};

        renderer::rhi::PolygonMode mPolygonMode{ renderer::rhi::PolygonMode::eFill };
        renderer::rhi::CullMode mCullMode{ renderer::rhi::CullMode::eNone };

        renderer::rhi::Format mDepthFormat{};
        eastl::fixed_vector<renderer::rhi::Format, rhi::kMaxColorFormats> mColorFormats{};

        core::u32 mRayPayloadBytes{ 32 };
        core::u32 mRayRecursionDepth{ 1 };
        eastl::fixed_hash_map<FGStageType, eastl::string, core::as<core::u32>( FGStageType::eCount )> mEntryPoints{};

        bool mEnableDepthTest{ true };
        bool mEnableDepthWrite{ true };
        rhi::BlendState mAlphaBlendState{};

        auto SetCullMode( renderer::rhi::CullMode mode ) -> FGPipelineDescription &;
        auto SetPolygonMode( renderer::rhi::PolygonMode mode ) -> FGPipelineDescription &;

        auto SetMultisampling( renderer::rhi::Multisampling msaa ) -> FGPipelineDescription &;

        auto SetDepthTest( bool value ) -> FGPipelineDescription &;
        auto SetDepthWrite( bool value ) -> FGPipelineDescription &;
        auto SetDepthFormat( renderer::rhi::Format format ) -> FGPipelineDescription &;

        auto SetAlphaBlendState( const rhi::BlendState &state ) -> FGPipelineDescription &;
        auto SetBlendEnable( bool value ) -> FGPipelineDescription &;

        auto AddColorFormat( renderer::rhi::Format format ) -> FGPipelineDescription &;
        auto SetName( eastl::string_view name ) -> FGPipelineDescription &;
        auto SetPipelineType( renderer::rhi::PipelineType type ) -> FGPipelineDescription &;
        auto PushShader( const filesystem::Path &path, FGStageType stage, eastl::string_view entryPoint = "main" ) -> FGPipelineDescription &;
        auto SetTopology( renderer::rhi::PrimitiveTopology ) -> FGPipelineDescription &;
    };

    struct FGBufferDescription {
        eastl::string mName{};

        core::u32 mElementCount{};
        core::u32 mElementSizeBytes{};// If we do not know the size of individual elements this is equal to the whole range

        rhi::HeapType mHeapType{ rhi::HeapType::eDeviceLocal };
        rhi::BufferUsageFlags mBufferUsageFlags{ renderer::rhi::BufferUsageFlagsBits::Structured };

        memory::BufferSpanHandle mInitialContents{};

        auto SetName( eastl::string_view name ) -> FGBufferDescription &;
        auto SetInitialData( memory::BufferSpanHandle data ) -> FGBufferDescription &;
        auto SetUsage( rhi::BufferUsageFlags flags ) -> FGBufferDescription &;
        auto SetSizeBytes( core::usize byteSize ) -> FGBufferDescription &;
        auto SetElementsSize( core::u32 elementCount, core::usize elementSizeBytes ) -> FGBufferDescription &;
        auto SetHeapType( rhi::HeapType heap ) -> FGBufferDescription &;
    };

    struct FGAccelerationStructureDescription {
        eastl::string mName{};
    };

    struct FGShaderTableDescription {
        eastl::string mName{};
        FGPipelineHandle mPipeline{};
    };

    // One mesh range in the shared build buffers and its TLAS instance transform.
    struct FGAccelerationGeometry {
        core::u32 mFirstIndex{};
        core::u32 mIndexCount{};
        core::u32 mVertexCount{};
        core::u32 mVertexStride{ 12 };
        eastl::array<float, 12> mTransform{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 };
        core::u32 mInstanceID{};
        core::u8 mMask{ 0xFF };
        core::u64 mRevision{};// Nonzero local-shape identity permits reuse of an unchanged BLAS.
    };

    struct FGTextureDescription {
        eastl::string mName{};

        core::u32 mWidth{};
        core::u32 mHeight{};
        core::u32 mDepth{ 1 };
        core::u32 mMipCount{ 1 };

        rhi::Multisampling mMultisampling{ rhi::Multisampling::eMsaaX1 };
        rhi::TextureDimension mDimension{ rhi::TextureDimension::eTexture2D };
        rhi::TextureUsageFlags mUsage{ rhi::TextureUsageFlagsBits::ShaderResource };

        rhi::Format mFormat{ rhi::Format::eRGBA8_SNORM };
        rhi::HeapType mHeapType{ rhi::HeapType::eDeviceLocal };

        auto SetName( eastl::string_view name ) -> FGTextureDescription &;
        auto SetWidth( core::u32 width ) -> FGTextureDescription &;
        auto SetHeight( core::u32 height ) -> FGTextureDescription &;
        auto SetDepth( core::u32 depth ) -> FGTextureDescription &;
        auto SetMipCount( core::u32 count ) -> FGTextureDescription &;

        auto SetHeapType( rhi::HeapType heapType ) -> FGTextureDescription &;
        auto SetMultisampling( rhi::Multisampling sampleCount ) -> FGTextureDescription &;

        auto SetDimensions( rhi::TextureDimension dimensions ) -> FGTextureDescription &;

        auto SetFormat( rhi::Format usage ) -> FGTextureDescription &;
        auto SetUsage( rhi::TextureUsageFlags usage ) -> FGTextureDescription &;
    };

    /** Access scopes recorded by graph passes and command-context helpers. */
    struct FGBarrierHistory {
        rhi::PipelineStageFlags mStages{ rhi::PipelineStageFlagsBits::None };
        rhi::BarrierAccessFlags mAccess{ rhi::BarrierAccessFlagsBits::None };
        rhi::ResourceStates mState{ rhi::ResourceStates::eUnknown };
        bool mIsValid{};

        /**
         * Checks whether this history still describes the recorded resource.
         * @param state Current resource state.
         * @returns True when the recorded history matches the tracked state.
         */
        MKT_NODISCARD auto IsCurrent( rhi::ResourceStates state ) const -> bool;

        /**
         * Prepares access history for the next recorded use, retaining compatible readers.
         * @param stateBefore Current resource state.
         * @param stateAfter Required state for the next use.
         * @param stages Pipeline stages of the next use.
         * @param accesses Memory accesses of the next use.
         * @returns Pending history; publish after recording the barrier.
         */
        MKT_NODISCARD auto Next( rhi::ResourceStates stateBefore,
            rhi::ResourceStates stateAfter, rhi::PipelineStageFlags stages, rhi::BarrierAccessFlags accesses ) const -> FGBarrierHistory;
    };

    struct FGResource {
        core::Ref<core::Resource> mResource{};
        FGResourceHandle mResourceID{};
        FGResourceType mType{ FGResourceType::eInvalid };
        FGBarrierHistory mBarrierHistory{};
        // Registration allocates only this slot; compilation creates its GPU object.
        bool mNeedsRebuild{ true };
    };


    /**
     * @brief Maps a graph stage and access mode to a resource state.
     * @param stage Frame-graph pipeline stage declaring the use.
     * @param access Read or write access declared by the pass.
     * @returns RHI state required by the declared use.
     */
    MKT_NODISCARD auto GetResourceState( FGPipelineStage stage, FGResourceAccess access ) -> rhi::ResourceStates;

    /**
     * @brief  Converts the tracked texture state into the layout used by graph barriers.
     * @param state Tracked RHI state, or the declared graph stage for the access overload.
     * @returns Layout required by the resource use.
     */
    MKT_NODISCARD auto GetTextureLayout( rhi::ResourceStates state ) -> rhi::TextureLayoutFlags;

}// namespace mikoto::renderer

#endif//MIKOTO_RENDERER_FRAME_GRAPH_TYPES_HH
