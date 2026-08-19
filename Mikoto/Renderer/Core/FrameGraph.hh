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

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuDevice.hh>


#include <Assets/AssetsService.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::memory;
    using namespace mikoto::material;
    using namespace mikoto::renderer::rhi;

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
    using FGResourceHandle = core::u32;

    struct FGTextureHandle { FGResourceHandle mHandle{}; };
    struct FGBufferHandle { FGResourceHandle mHandle{};  };
    struct FGSamplerHandle { FGResourceHandle mHandle{}; };
    struct FGPipelineHandle { FGResourceHandle mHandle{}; };

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

    // These should represent pipeline stage
    enum class FGResourceStage {
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

        eUnorderedAccess,
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
        eWrite,
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

        FGResourceStage mCurrentState{ FGResourceStage::eUnknown };
    };

    struct FGResourceTrack {
        FGResourceStage mState{};
        FGResourceAccess mAccess{};
    };

    struct FGNode {
        eastl::string mName{};
        FGPassType mType{ FGPassType::eGraphics };

        bool mIsAlive{ true };

        FGExecutionPolicy mExecutionPolicy{ FGExecutionPolicy::ePerFrame };

        eastl::function<void()> mBuilderCallback{};
        eastl::function<void( CommandContext&, Blackboard& )> mExecuteCallback{};

        static constexpr core::u32 kMaxDependencies{ 32 };
        static constexpr core::u32 kMaxSuccessors{ 32 };
        static constexpr core::u32 kMaxResources{ 32 };

        eastl::fixed_vector<FGResourceHandle, kMaxResources> mReadResources{};
        eastl::fixed_vector<FGResourceHandle, kMaxResources> mWriteResources{};

        ankerl::unordered_dense::set<eastl::string> mDependsOn{};
        eastl::fixed_vector<eastl::string, kMaxSuccessors> mSuccessors{};

        ankerl::unordered_dense::map<FGResourceHandle, FGResourceTrack> mResourceStates{};

        core::u32 mInDegree{};
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

        core::u32 mElementCount{};
        core::u32 mElementSizeBytes{}; // If we do not know the size of individual elements this is equal to the whole range

        rhi::HeapType mHeapType{ rhi::HeapType::eDeviceLocal };
        rhi::BufferUsageFlags mBufferUsageFlags{ BufferUsageFlagsBits::kStorage };

        memory::BufferSpanHandle mInitialContents{};

        auto SetName( eastl::string_view name ) -> FGBufferDescription&;
        auto SetInitialData( memory::BufferSpanHandle data ) -> FGBufferDescription&;
        auto SetUsage( rhi::BufferUsageFlags flags ) -> FGBufferDescription&;
        auto SetSizeBytes( core::size_t byteSize ) -> FGBufferDescription&;
        auto SetElementsSize( core::u32 elementCount, core::size_t elementSizeBytes ) -> FGBufferDescription&;
        auto SetHeapType( rhi::HeapType heap ) -> FGBufferDescription&;
    };

    struct FGTextureDescription {
        eastl::string mName{};

        core::u32 mWidth{};
        core::u32 mHeight{};
        core::u32 mMipCount{ 1 };

        rhi::Multisampling mMultisampling{ rhi::Multisampling::eMsaaX1 };
        rhi::TextureDimension mDimension{ rhi::TextureDimension::eTexture2D };
        rhi::TextureUsageFlags mUsage{ rhi::TextureUsageFlagsBits::kShaderResource };

        rhi::Format mFormat{ rhi::Format::eRGBA8_SNORM };
        rhi::HeapType mHeapType{ rhi::HeapType::eDeviceLocal };

        auto SetName( eastl::string_view name ) -> FGTextureDescription&;
        auto SetWidth( core::u32 width ) -> FGTextureDescription&;
        auto SetHeight( core::u32 height ) -> FGTextureDescription&;
        auto SetMipCount( core::u32 count ) -> FGTextureDescription&;

        auto SetHeapType( rhi::HeapType heapType) -> FGTextureDescription&;
        auto SetMultisampling( rhi::Multisampling sampleCount ) -> FGTextureDescription&;

        auto SetDimensions( rhi::TextureDimension dimensions ) -> FGTextureDescription&;

        auto SetFormat( rhi::Format usage ) -> FGTextureDescription&;
        auto SetUsage( rhi::TextureUsageFlags usage ) -> FGTextureDescription&;
    };

    struct FGResource {
        Ref<IResource> mResource{};
        FGResourceHandle mResourceID{};
        FGResourceType mType{ FGResourceType::eInvalid };
    };

    class FGResourceManager final {
    public:
        static constexpr FGResourceHandle kInvalidResourceHandle{ 0 };

        explicit FGResourceManager( IGpuDevice* device );

        MKT_NODISCARD auto Get( FGResourceHandle handle ) -> FGResource&;
        MKT_NODISCARD auto Get( FGResourceHandle handle ) const -> const FGResource&;

        MKT_NODISCARD auto GetBufferMappedAddress( FGBufferHandle handle ) const -> const void*;

        MKT_NODISCARD auto GetBindlessLayout() const -> BindingLayoutHandle;
        MKT_NODISCARD auto GetPipelineLayout() const -> PipelineLayoutHandle;

        MKT_NODISCARD auto GetDescriptorTable() const -> DescriptorTableHandle;

        MKT_NODISCARD auto Allocate( FGResourceType type, IResource* resource = nullptr ) -> FGResource&;
        MKT_NODISCARD auto Free( FGResourceHandle name ) -> bool;

        auto PushSampler( FGResourceHandle name ) -> u32;
        auto PushTexture_SRV( FGResourceHandle name ) -> u32;
        auto PushTexture_UAV( FGResourceHandle handle ) -> u32;

        MKT_NODISCARD auto PushBuffer_SRV( FGResourceHandle name ) -> u32;
        MKT_NODISCARD auto PushBuffer_UAV( FGResourceHandle name ) -> u32;

        MKT_NODISCARD auto ImportTexture( TextureHandle handle ) -> FGTextureHandle;
        MKT_NODISCARD auto ImportBuffer( BufferHandle handle ) -> FGBufferHandle;

    private:
        IGpuDevice* mDevice{};

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

    struct FGReadbackTask {
        using Callback = eastl::function<void(Blackboard&, const FGResourceManager&)>;
        Callback mCallback{};
        u64 mFenceValue{};


        // It starts as ready so we can update
        // the fence value for the first submission
        bool mIsReady{ true };

        bool mRunsEveryFrame{ false };
    };

    // Manages GPU to CPU readbacks
    class FGReadbackManager final {
    public:
        static constexpr u32 kMaxReadbacks{ 15 };

        explicit FGReadbackManager( Blackboard& blackboard, FGResourceManager& manager );

        auto ExecuteCallbacks(  u64 fenceValue  ) -> void;

        auto UpdateTaskFenceValue( u32 taskID, u64 fenceValue ) -> void;
        auto RegisterCallback(const FGReadbackTask::Callback& callback, bool runEveryFrame = false) -> u32;

    private:
        Blackboard* mBlackboard{};
        FGResourceManager* mResourceManager{};
        ankerl::unordered_dense::map<u32, FGReadbackTask> mReadbackTasks{};
    };

    struct FGNodeControl {
        ankerl::unordered_dense::map<eastl::string, FGNode> mNodes;
        ankerl::unordered_dense::map<eastl::string, CommandContext> mContexts;
        ankerl::unordered_dense::map<FGResourceHandle, FGNodeResource> mResources;

        FGNodeControl();

        auto Clear() -> void;
    };

    class FGNodeBuilder final {
    public:
        explicit FGNodeBuilder( FGNode& node, FGNodeControl& control );

        // I am not sure if I wanna keep these two. The second set of
        // methods offers more relaxed barriers
        auto Read( FGTextureHandle handle, FGResourceStage state ) -> void;
        auto Write( FGTextureHandle handle, FGResourceStage state ) -> void;

        auto Read( FGBufferHandle handle, FGResourceStage state )  -> void;
        auto Write( FGBufferHandle handle, FGResourceStage state )  -> void;

        auto UseResource( FGTextureHandle handle, FGResourceStage state, FGResourceAccess access ) -> void;
        auto UseResource( FGBufferHandle handle, FGResourceStage state, FGResourceAccess access ) -> void;

    private:
        auto Read( FGResourceHandle handle )  -> void;
        auto Write( FGResourceHandle handle )  -> void;

    private:
        FGNode* mGraphNode{};
        FGNodeControl* mNodeControl{};
    };

    struct FGBarrier {
        FGResourceHandle mResourceID{};  // which resource to transition

        FGResourceAccess mAccess{};
        FGResourceStage mOldState{};       // state before this pass
        FGResourceStage mNewState{};       // state this pass requires
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
        explicit FrameGraph( IGpuDevice* device, material::ShaderLibrary* shaderLibrary );

        auto Compile() -> void;
        auto Execute() -> void;

        auto ExecuteReadbacks() -> void;

        auto SetExecutionPolicy( eastl::string_view passName, FGExecutionPolicy policy ) -> void;

        auto DisablePass( eastl::string_view passName ) -> void;
        auto EnablePass( eastl::string_view passName ) -> void;

        MKT_NODISCARD auto GetNodeControl() const -> const FGNodeControl&;

        MKT_NODISCARD auto GetTexture( FGTextureHandle handle ) const -> TextureHandle;
        MKT_NODISCARD auto GetBuffer( FGBufferHandle handle ) const -> BufferHandle;

        MKT_NODISCARD auto ImportTexture( const Path& path ) -> FGTextureHandle;
        MKT_NODISCARD auto ImportTexture( TextureHandle handle ) -> FGTextureHandle;
        MKT_NODISCARD auto ImportBuffer( BufferHandle handle ) -> FGBufferHandle;

        MKT_NODISCARD auto Create( const FGTextureDescription& desc ) -> FGTextureHandle;
        MKT_NODISCARD auto Create( const FGBufferDescription& desc ) -> FGBufferHandle;
        MKT_NODISCARD auto Create( const FGPipelineDescription& desc ) -> FGPipelineHandle;
        MKT_NODISCARD auto Create( const FGSamplerDescription& desc ) -> FGSamplerHandle;

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

        auto RegisterReadback( FGReadbackTask::Callback &&execute, bool runEveryFrame = false ) -> void;

        MKT_NODISCARD static auto Create( IGpuDevice* device, material::ShaderLibrary* shaderLibrary ) -> eastl::unique_ptr<FrameGraph>;

    private:
        auto BindResources( CommandListHandle commandList ) -> void;

        auto BuildNodeEdges() -> void;
        auto CullGraphNodes() -> void;
        auto BuildNodeBarriers() -> void;
        auto BuildExecutionTasks() -> void;

    private:
        struct ReadbackTask {
            u32 mTaskID{};
            FGReadbackTask::Callback mTask{};

            bool mRunsPerFrame{};
            bool mHasBeenRegistered{};
        };

    private:
        IGpuDevice* mDevice{};
        material::ShaderLibrary* mShaderLibrary{};

        Blackboard mBlackboard{};

        FGCompiledPlan mExecutionPlan{};
        eastl::unique_ptr<FGNodeControl> mNodeControl{};
        eastl::unique_ptr<FGResourceManager> mResourceManager{};
        eastl::unique_ptr<FGReadbackManager> mReadbackManager{};

        u64 mFenceValue{};
        FenceHandle mFence{};

        // Callback, runsEveryFrame
        eastl::vector<ReadbackTask> mReadbackTasks{};

        CommandListHandle mGraphicsCommands{};
        CommandListHandle mComputeCommands{};
        CommandListHandle mTransferCommands{};
    };
}

#endif//MIKOTO_FRAME_GRAPH_HH
