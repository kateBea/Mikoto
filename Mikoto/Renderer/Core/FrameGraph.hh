//    Copyright 2025 ケイト
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

// =================================================
// Taken as reference
// https://stoleckipawel.dev/posts/frame-graph-theory/
// https://stoleckipawel.dev/posts/frame-graph-build-it/
// https://stoleckipawel.dev/posts/frame-graph-advanced/
// https://stoleckipawel.dev/posts/frame-graph-production/
//
// =================================================

#ifndef MIKOTO_FRAME_GRAPH_HH
#define MIKOTO_FRAME_GRAPH_HH

#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/string_view.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/fixed_hash_map.h>
#include <EASTL/functional.h>

#include <taskflow/taskflow.hpp>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/String.hh>
#include <Core/Types.hh>
#include <Core/Blackboard.hh>
#include <Core/ResourcePool.hh>

#include <Material/ShaderLibrary.hh>

#include <Memory/Allocator.hh>
#include <Memory/MemoryArena.hh>
#include <Memory/FreeListAllocator.hh>

#include <Renderer/Core/GpuDevice.hh>
#include <Renderer/Core/Rhi.hh>


#include <Assets/AssetsService.hh>

namespace mikoto::renderer {

    // See shaders: slang/Base.slang
#define MKT_DEFAULT_REGISTER_SPACE 0

#define MKT_STRUCTURED_SRV_BINDING 0
#define MKT_STRUCTURED_UAV_BINDING 1

#define MKT_SAMPLER_BINDING 2

#define MKT_TEXTURE_SRV_BINDING 3
#define MKT_TEXTURE_UAV_BINDING 4

#define MKT_ACCELERATION_STRUCTURE_BINDING 5

#define MKT_SHADER_TRUE 1U
#define MKT_SHADER_FALSE 0U

    class CommandContext;

    // Represents the handle for any resource
    using FGResourceHandle = u32;

    struct FGTextureHandle { FGResourceHandle mHandle{}; };
    struct FGBufferHandle { FGResourceHandle mHandle{};  };
    struct FGSamplerHandle { FGResourceHandle mHandle{}; };
    struct FGPipelineHandle { FGResourceHandle mHandle{}; };

    enum class FGStageType {
        eVertex,
        eTessellationControl,
        eTessellationEvaluation,
        eGeometry,
        eFragment,

        eTask,
        eMesh,

        eTransfer,

        eCompute,

        eRayGen,
        eIntersection,
        eAnyHit,
        eClosestHit,
        eMiss,
        eCount
    };

    enum class FGResourceAccess {
        eRead,
        eWrite,
        eReadWrite,
    };

    /*
        FrameGraph Resource State Rules

        - Each pass declares HOW it uses a resource (not future usage).
        - States represent GPU access patterns (stage + access + layout).
        - FrameGraph resolves transitions BETWEEN passes automatically.
        - A pass must not lie about its usage — correctness depends on it.
    */
    enum class FGResourceState {
        // Unknown / initial state
        eUnknown,

        // =========================
        // BUFFER STATES
        // =========================

        // Constant/uniform buffer (read-only in shader)
        // Example: camera data, per-frame uniforms
        // Access: SHADER_READ
        // Stage: ALL_SHADER_STAGES
        eConstantBuffer,

        // Vertex buffer input
        // Example: bound via vertex input stage
        // Access: VERTEX_ATTRIBUTE_READ
        // Stage: VERTEX_INPUT
        eVertexBuffer,

        // Index buffer input
        // Access: INDEX_READ
        // Stage: VERTEX_INPUT
        eIndexBuffer,

        // Indirect draw arguments
        // Access: INDIRECT_COMMAND_READ
        // Stage: DRAW_INDIRECT
        eIndirectArgument,

        // Read-only structured / byte-address buffer
        // Example: StructuredBuffer<T>, ByteAddressBuffer
        // Access: SHADER_READ
        // Stage: ALL_SHADER_STAGES
        eShaderResource,

        // Read-write buffer in shader
        // Example: RWStructuredBuffer, RWByteAddressBuffer
        // Access: SHADER_READ | SHADER_WRITE
        // Stage: ALL_SHADER_STAGES (usually compute or fragment)
        eUnorderedAccess,

        // =========================
        // IMAGE STATES
        // =========================

        // Render target (color attachment)
        // Access: COLOR_ATTACHMENT_WRITE
        // Stage: COLOR_ATTACHMENT_OUTPUT
        eRenderTarget,

        // Depth write (depth attachment)
        // Access: DEPTH_STENCIL_ATTACHMENT_WRITE
        // Stage: EARLY/LATE_FRAGMENT_TESTS
        eDepthWrite,

        // Depth read (sampling depth or read-only depth)
        // Access: DEPTH_STENCIL_ATTACHMENT_READ
        // Stage: EARLY/LATE_FRAGMENT_TESTS
        eDepthRead,

        // =========================
        // TRANSFER STATES
        // =========================

        // Copy destination (GPU writes via transfer)
        // Example: uploading data into buffer/texture
        // Access: TRANSFER_WRITE
        // Stage: TRANSFER
        eCopyDest,

        // Copy source (GPU reads for transfer)
        // Access: TRANSFER_READ
        // Stage: TRANSFER
        eCopySource,

        // Resolve destination (MSAA resolve target)
        // Access: TRANSFER_WRITE
        // Stage: TRANSFER
        eResolveDest,

        // Resolve source (MSAA source)
        // Access: TRANSFER_READ
        // Stage: TRANSFER
        eResolveSource,

        // =========================
        // PRESENT
        // =========================

        // Swapchain present
        // Access: NONE
        // Stage: BOTTOM_OF_PIPE / PRESENT
        ePresent,

        // =========================
        // RAYTRACING (future-proof)
        // =========================

        eAccelStructRead,
        eAccelStructWrite,
        eAccelStructBuildInput,
        eAccelStructBuildBlas,
    };

    enum class FGExecutionPolicy {
        ePerFrame,
        eOnChange,
        eOnce,
    };

    enum class FGResourceType {
        eInvalid = -1,
        eTexture,
        eBuffer,
        eSampler,
        ePipeline,
    };


    enum class FGPassType {
        eGraphics,
        eCompute,
        eTransfer,
        eGeneric, // For passes that not really need any kind of GPU work
    };

    struct ResourceVersion {
        eastl::string mWriterPass{};    // Each Read() links to the current version's writer → automatic dependency edge.
        eastl::vector<eastl::string> mReaderPasses{};  // Each Write() to a resource creates a new version.

        MKT_NODISCARD auto HasWriter() const { return !mWriterPass.empty(); }
    };

    struct FGNodeResource {
        eastl::string mName{};
        eastl::any mDescription{};
        eastl::vector<ResourceVersion> mVersions{};  // version 0, 1, 2...
        bool mIsImported{ false };  // imported = externally owned (e.g. swapchain)

        FGResourceState mCurrentState{ FGResourceState::eUnknown };
    };

    struct FGResourceTrack {
        FGResourceState mState{}; // Compute, Fragment, vertex, color attachment output or whatever see the TexturePipelinedescription in RHI
        FGResourceAccess mAccess{};
    };

    struct FGNode {
        eastl::string mName{};
        FGPassType mType{ FGPassType::eGraphics };

        bool mIsAlive{ true };

        FGExecutionPolicy mExecutionPolicy{ FGExecutionPolicy::ePerFrame };

        eastl::function<void()> mBuilderCallback{};
        eastl::function<void( CommandContext&, Blackboard& )> mExecuteCallback{};

        static constexpr u32 kMaxDependencies{ 32 };
        static constexpr u32 kMaxSuccessors{ 32 };
        static constexpr u32 kMaxResources{ 32 };
        eastl::fixed_vector<FGResourceHandle, kMaxResources> mReadResources{};
        eastl::fixed_vector<FGResourceHandle, kMaxResources> mWriteResources{};
        eastl::fixed_vector<FGResourceHandle, kMaxResources> mReadWriteResources{}; // UAV

        eastl::fixed_vector<eastl::string, kMaxSuccessors> mSuccessors{};

        ankerl::unordered_dense::set<eastl::string> mDependsOn{};

        ankerl::unordered_dense::map<FGResourceHandle, FGResourceTrack> mResourceStates{};

        u32 mInDegree{};
    };

    // A more specialized type of blackboard for the framegraph
    class FGBlackboard {
    public:


    private:
    };

    struct FGSamplerDescription {
        eastl::string mName{};

        float mMipLevels{ 1.0f };

        SamplerFilter mMinFilter{ SamplerFilter::eNearest };
        SamplerFilter mMagFilter{ SamplerFilter::eNearest };
        SamplerWrapMode mWrapU{ SamplerWrapMode::eRepeat };
        SamplerWrapMode mWrapV{ SamplerWrapMode::eRepeat };
        SamplerWrapMode mWrapW{ SamplerWrapMode::eRepeat };

        Color mBorderColor{ 0.0f, 0.0f, 0.0f, 1.0f };

        auto SetMipLevels( float mipLevels) -> FGSamplerDescription&;

        auto SetBorderColor( const Color& color ) -> FGSamplerDescription&;

        auto SetName( eastl::string_view name ) -> FGSamplerDescription&;
        auto SetFilter( SamplerFilter filter ) -> FGSamplerDescription&;
        auto SetMinFilter( SamplerFilter filter ) -> FGSamplerDescription&;
        auto SetMagFilter( SamplerFilter filter ) -> FGSamplerDescription&;

        auto SetWrap( SamplerWrapMode wrap ) -> FGSamplerDescription&;
        auto SetWrapU( SamplerWrapMode wrap ) -> FGSamplerDescription&;
        auto SetWrapV( SamplerWrapMode wrap ) -> FGSamplerDescription&;
        auto SetWrapW( SamplerWrapMode wrap ) -> FGSamplerDescription&;
    };

    struct FGPipelineDescription {
        eastl::string mName{};

        PrimitiveTopology mTopology{ PrimitiveTopology::eTriangleList };
        PipelineType mPipelineType{ PipelineType::eInvalid };

        eastl::fixed_hash_map<FGStageType, Path,
            as<u32>( FGStageType::eCount )> mShaders{};

        PolygonMode mPolygonMode{ PolygonMode::eFill };
        CullMode mCullMode{ CullMode::eCullBack };

        Format mDepthFormat{};
        eastl::fixed_vector<Format, kMaxColorFormats> mColorFormats{};

        bool mEnableDepthTest{ true };
        bool mEnableDepthWrite{ true };
        bool mEnableBlend{ true };

        auto SetCullMode( CullMode mode ) -> FGPipelineDescription&;
        auto SetPolygonMode( PolygonMode mode ) -> FGPipelineDescription&;

        auto SetDepthTest( bool value ) -> FGPipelineDescription&;
        auto SetDepthWrite( bool value ) -> FGPipelineDescription&;
        auto SetDepthFormat( Format format ) -> FGPipelineDescription&;

        auto SetBlendEnable( bool value ) -> FGPipelineDescription&;

        auto AddColorFormat( Format format ) -> FGPipelineDescription&;
        auto SetName( eastl::string_view name ) -> FGPipelineDescription&;
        auto SetPipelineType( PipelineType type ) -> FGPipelineDescription&;
        auto PushShader( const Path& path, FGStageType stage) -> FGPipelineDescription&;
        auto SetTopology(PrimitiveTopology) -> FGPipelineDescription&;
    };

    struct FGBufferDescription {
        eastl::string mName{};

        BufferSpanHandle mSpanHandle{};

        u32 mElementCount{};
        u32 mElementSizeBytes{}; // If we do not know the size of individual elements this is equal to the whole range

        BufferUsageFlags mBufferUsageFlags{};
        HeapType mHeapType{ HeapType::eDeviceLocal };

        auto SetName( eastl::string_view name ) -> FGBufferDescription&;
        auto SetInitialData( BufferSpanHandle data ) -> FGBufferDescription&;
        auto SetUsage( BufferUsageFlags flags ) -> FGBufferDescription&;
        auto SetSizeBytes( size_t byteSize ) -> FGBufferDescription&;
        auto SetElementsSize( u32 elementCount, size_t elementSizeBytes ) -> FGBufferDescription&;
        auto SetHeapType( HeapType heap ) -> FGBufferDescription&;
    };

    struct FGTextureDescription {
        eastl::string mName{};
        u32 mWidth{};
        u32 mHeight{};
        u32 mMipCount{ 1 };

        Multisampling mMSAA{ Multisampling::eMsaaX1 };
        TextureDimension mDimension{ TextureDimension::eTexture2D };
        TextureUsageFlags mUsage{ TextureUsageFlagsBits::kShaderResource };

        Format mFormat{ Format::eRGBA8_SNORM };
        HeapType mHeapType{ HeapType::eDeviceLocal };

        auto SetName( eastl::string_view name ) -> FGTextureDescription&;
        auto SetWidth( u32 width ) -> FGTextureDescription&;
        auto SetHeight( u32 height ) -> FGTextureDescription&;
        auto SetMipCount( u32 count ) -> FGTextureDescription&;

        auto SetHeapType( HeapType heapType) -> FGTextureDescription&;
        auto SetMultisampling( Multisampling sampleCount ) -> FGTextureDescription&;

        auto SetDimensions( TextureDimension dimensions ) -> FGTextureDescription&;

        auto SetFormat( Format usage ) -> FGTextureDescription&;
        auto SetUsage( TextureUsageFlags usage ) -> FGTextureDescription&;
    };

    struct FGResource {
        Ref<IResource> mResource{};
        FGResourceHandle mHandle{};
        FGResourceType mType{ FGResourceType::eInvalid };
    };

    class FGResourceManager final {
    public:
        static constexpr FGResourceHandle kInvalidResourceHandle{ 0 };

        explicit FGResourceManager( GpuDevice* device );

        MKT_NODISCARD auto Get( FGResourceHandle handle ) -> FGResource&;
        MKT_NODISCARD auto Get( FGResourceHandle name ) const -> const FGResource&;

        MKT_NODISCARD auto GetBindlessLayout() const -> BindingLayoutHandle;
        MKT_NODISCARD auto GetPipelineLayout() const -> PipelineLayoutHandle;

        MKT_NODISCARD auto GetDescriptorTable() const -> DescriptorTableHandle;

        MKT_NODISCARD auto Allocate( FGResourceType type, IResource* resource = nullptr ) -> FGResource&;
        MKT_NODISCARD auto Free( FGResourceHandle name ) -> bool;

        // Make resource available to shaders
        auto PushContantBuffer( FGResourceHandle name ) -> u32;

        auto PushSampler( FGResourceHandle name ) -> u32;
        auto PushTexture_SRV( FGResourceHandle name ) -> u32;
        auto PushTexture_UAV( FGResourceHandle handle ) -> u32;

        MKT_NODISCARD auto PushBuffer_SRV( FGResourceHandle name ) -> u32;
        MKT_NODISCARD auto PushBuffer_UAV( FGResourceHandle name ) -> u32;
        MKT_NODISCARD auto PushBuffer_Constant( FGResourceHandle name ) -> u32;

        MKT_NODISCARD auto ImportTexture( TextureHandle handle ) -> FGTextureHandle;

    private:
        GpuDevice* mDevice{};

        std::mutex mResourceMutex{};
        eastl::atomic<u32> mResourceCount{};

        ankerl::unordered_dense::map<IResource*, FGResourceHandle> mImportedResources{};
        ankerl::unordered_dense::map<FGResourceHandle, eastl::unique_ptr<FGResource>> mResources{};

        // Gpu side resources
        rhi::BindingLayoutHandle mBindlessLayout{};
        rhi::PipelineLayoutHandle mPipelineLayout{};

        static constexpr u32 kMaxTables{ 5 };
        static constexpr u32 kMaxResourcePerTable{ 1096 };

        std::mutex mTableWriteMutex{};
        DescriptorTableHandle mDescriptorTable{};

        // Example: [binding(0, 0)] Textures[] textures;
        // data.mTexture01 is at textures[0]
        using ResourceList = eastl::fixed_hash_map<FGResourceHandle, u32, kMaxResourcePerTable>;
        using ResourceTable = eastl::fixed_hash_map<u32, ResourceList, kMaxTables>;

        // Descriptor type binding index ->
        // List of table resources and their ID
        ResourceTable mResourceTable{};
    };

    struct FGNodeControl {
        ankerl::unordered_dense::map<eastl::string, FGNode> mNodes{};
        ankerl::unordered_dense::map<eastl::string, Ref<CommandContext>> mContexts{};
        ankerl::unordered_dense::map<FGResourceHandle, FGNodeResource> mResources{};

        auto Clear() -> void;
    };

    class FGNodeBuilder final {
    public:
        explicit FGNodeBuilder( FGNode& node, FGNodeControl& control );

        auto Read( FGTextureHandle handle, FGResourceState state ) -> void;
        auto Write( FGTextureHandle handle, FGResourceState state ) -> void;
        auto ReadWrite( FGTextureHandle handle, FGResourceState state ) -> void;

        auto Read( FGBufferHandle handle, FGResourceState state )  -> void;
        auto Write( FGBufferHandle handle, FGResourceState state )  -> void;
        auto ReadWrite( FGBufferHandle handle, FGResourceState state )  -> void;

    private:
        auto Read( FGResourceHandle handle )  -> void;
        auto Write( FGResourceHandle handle )  -> void;
        auto ReadWrite( FGResourceHandle handle )  -> void;

    private:
        FGNode* mGraphNode{};
        FGNodeControl* mNodeControl{};
    };

    struct FGBarrier {
        FGResourceHandle mResourceID{};  // which resource to transition

        FGResourceState mOldState{};       // state before this pass
        FGResourceState mNewState{};       // state this pass requires
    };

    struct FGCompiledPlan {
        // topological execution order
        eastl::vector<eastl::string> mSorted{};

        // barriers[passName] -> pre-pass transitions
        ankerl::unordered_dense::map<eastl::string, ankerl::unordered_dense::map<FGResourceHandle, FGBarrier>> mBarriers{};

        // Execution task graph
        tf::Taskflow mExecutionGraph{};
        ankerl::unordered_dense::map<FGNode*, tf::Task> mTaskMap{};
    };

    class FrameGraph final {
    public:
        explicit FrameGraph( GpuDevice* device, material::ShaderLibrary* shaderLibrary );

        auto Compile() -> void;
        auto Execute() -> void;

        auto SetExecutionPolicy( eastl::string_view passName, FGExecutionPolicy policy ) -> void;

        auto DisablePass( eastl::string_view passName ) -> void;
        auto EnablePass( eastl::string_view passName ) -> void;

        MKT_NODISCARD auto GetTexture( FGTextureHandle handle ) const -> TextureHandle;
        MKT_NODISCARD auto GetBuffer( FGBufferHandle handle ) const -> BufferHandle;

        // Loads the texture using asset service
        MKT_NODISCARD auto ImportTexture( const Path& path ) -> FGTextureHandle;
        MKT_NODISCARD auto ImportTexture( TextureHandle handle ) -> FGTextureHandle;
        MKT_NODISCARD auto ImportBuffer( BufferHandle handle ) -> FGBufferHandle;

        MKT_NODISCARD auto Create( const FGTextureDescription& desc ) -> FGTextureHandle;
        MKT_NODISCARD auto Create( const FGBufferDescription& desc ) -> FGBufferHandle;
        MKT_NODISCARD auto Create( const FGPipelineDescription& desc ) -> FGPipelineHandle;
        MKT_NODISCARD auto Create( const FGSamplerDescription& desc ) -> FGSamplerHandle;

        // Graph is marked as dirty and recompiled on the next call to Execute
        auto SetEnablePass( bool enable, eastl::string_view name ) -> void;

        template<typename PassData, typename SetupFunc, typename ExecuteFunc>
        auto RegisterPass( eastl::string_view name, FGPassType nodeType, SetupFunc &&setup, ExecuteFunc &&execute ) -> void {
            FGNode& node{ mNodeControl->mNodes[name.data()] = FGNode{
                .mName{ name },
                .mType = nodeType,
                .mExecuteCallback = [execute]( CommandContext &ctx, Blackboard &blackboard ) -> void {
                    execute( ctx, blackboard );
                },
            } };
            PassData& data{ GetOrCreate<PassData>() };
            node.mBuilderCallback = [&]() -> void {
                FGNodeBuilder builder{ node, *mNodeControl };
                setup( builder, data );
            };

            node.mBuilderCallback();
        }

        template<typename SetupFunc, typename ExecuteFunc>
        auto RegisterPass( eastl::string_view name, FGPassType nodeType, SetupFunc &&setup, ExecuteFunc &&execute ) -> void {
            FGNode& node{ mNodeControl->mNodes[name.data()] = FGNode{
                .mName{ name },
                .mType = nodeType,
                .mExecuteCallback = [execute]( CommandContext &ctx, Blackboard &blackboard ) -> void {
                    execute( ctx, blackboard );
                },
            } };
            node.mBuilderCallback = [&]() -> void {
                FGNodeBuilder builder{ node, *mNodeControl };
                setup( builder, mBlackboard );
            };

            node.mBuilderCallback();
        }

        template<typename T, typename... Args>
        auto GetOrCreate(Args&&... args) -> T& {
            if (mBlackboard.Contains<T>()) {
                return mBlackboard.Get<T>();
            }

            return mBlackboard.Add<T>( eastl::forward<Args>(args)... );
        }

        template<typename T>
        auto GetData() const -> const T& {
            return mBlackboard.Get<T>();
        }

        template<typename T>
        MKT_NODISCARD auto Exists() const -> bool {
            return mBlackboard.Contains<T>();
        }

        MKT_NODISCARD static auto Create( GpuDevice* device, material::ShaderLibrary* shaderLibrary ) -> eastl::unique_ptr<FrameGraph>;

    private:
        auto BuildNodeEdges() -> void;
        auto CullGraphNodes() -> void;
        auto BuildNodeBarriers() -> void;
        auto BuildExecutionTasks() -> void;

        auto RecordCommands( CommandListHandle cmd ) -> void;

    private:
        GpuDevice* mDevice{};
        material::ShaderLibrary* mShaderLibrary{};

        // TODO: use the FGBlackboard
        Blackboard mBlackboard{};
        FGBlackboard mFGBlackboard{};

        FGCompiledPlan mExecutionPlan{};
        eastl::unique_ptr<FGNodeControl> mNodeControl{};
        eastl::unique_ptr<FGResourceManager> mResourceManager{};

        CommandListHandle mGraphicsCommands{};
        CommandListHandle mComputeCommands{};
        CommandListHandle mTransferCommands{};
    };
}

#endif//MIKOTO_FRAME_GRAPH_HH
