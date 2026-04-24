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

#ifndef MIKOTO_FRAME_GRAPH_HH
#define MIKOTO_FRAME_GRAPH_HH

#include <EASTL/string.h>
#include <EASTL/functional.h>
#include <EASTL/string_view.h>
#include <EASTL/unique_ptr.h>

#include <taskflow/taskflow.hpp>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/String.hh>
#include <Core/Types.hh>
#include <Core/Blackboard.hh>
#include <Core/ResourcePool.hh>

#include <Memory/Allocator.hh>

#include <Material/ShaderLibrary.hh>

#include <Renderer/Core/Rhi.hh>
#include <Renderer/Core/GpuDevice.hh>
#include <Renderer/Core/FrameGraphNode.hh>

namespace mikoto::renderer {

    class CommandContext;

    using ResourceID = u32;

    enum class FrameGraphNodeType {
        eGraphics,
        eCompute,
        eTransfer,
        eGeneric, // For passes that not really need any kind of GPU work
    };

    enum class FrameGraphResourceAccessType {
        eRead,
        eWrite,
    };

    enum class FrameGraphStageType {
        eVertex,
        eTessellationControl,
        eTessellationEvaluation,
        eGeometry,
        eFragment,

        eTask,
        eMesh,

        eCompute,

        eRayGen,
        eIntersection,
        eAnyHit,
        eClosestHit,
        eMiss,
        eCount
    };

    enum class FrameGraphResourceType {
        eInvalid = -1,
        eTexture,
        eBuffer,
    };

    struct FrameGraphPipelineDescription {
        eastl::string mName{};

        PipelineType mPipelineType{ PipelineType::eInvalid };

        eastl::fixed_hash_map<FrameGraphStageType, Path,
            as<u32>( FrameGraphStageType::eCount )> mShaders{};

        auto SetName( eastl::string_view name ) -> FrameGraphPipelineDescription&;
        auto SetPipelineType( PipelineType type ) -> FrameGraphPipelineDescription&;
        auto PushShader( const Path& path, FrameGraphStageType stage) -> FrameGraphPipelineDescription&;
    };

    struct FrameGraphBufferDescription {
        eastl::string mName{};

        u32 mElementCount{};
        u32 mElementSizeBytes{}; // If we do not know the size of individual elements this is equal to the whole range

        BufferUsageFlags mBufferUsageFlags{};
        HeapType mHeapType{ HeapType::eDeviceLocal };
        ResourceType mResourceType{ ResourceType::eInvalid };

        auto SetName( eastl::string_view name ) -> FrameGraphBufferDescription&;

        auto SetUsage( BufferUsageFlags flags ) -> FrameGraphBufferDescription&;
        auto SetSizeBytes( size_t byteSize ) -> FrameGraphBufferDescription&;
        auto SetElementsSize( u32 elementCount, size_t elementSizeBytes ) -> FrameGraphBufferDescription&;
        auto SetCpuAccess( HeapType heap ) -> FrameGraphBufferDescription&;
        auto SetResourceType( ResourceType resource ) -> FrameGraphBufferDescription&;
    };

    struct FrameGraphTextureDescription {
        eastl::string mName{};
        u32 mWidth{};
        u32 mHeight{};
        u32 mMipCount{ 1 };

        Multisampling mMSAA{ Multisampling::eMsaaX1 };
        TextureDimension mDimension{ TextureDimension::eTexture2D };
        TextureUsageFlags mUsage{ TextureUsageFlagsBits::kShaderResource };

        Format mFormat{ Format::eRGBA8_SNORM };
        HeapType mHeapType{ HeapType::eDeviceLocal };
        ResourceType mResourceType{ ResourceType::eInvalid };

        auto SetName( eastl::string_view name ) -> FrameGraphTextureDescription&;
        auto SetWidth( u32 width ) -> FrameGraphTextureDescription&;
        auto SetHeight( u32 height ) -> FrameGraphTextureDescription&;
        auto SetMipCount( u32 count ) -> FrameGraphTextureDescription&;

        auto SetHeapType( HeapType heapType) -> FrameGraphTextureDescription&;
        auto SetMultisampling( Multisampling sampleCount ) -> FrameGraphTextureDescription&;

        auto SetDimensions( TextureDimension dimensions ) -> FrameGraphTextureDescription&;

        auto SetFormat( Format usage ) -> FrameGraphTextureDescription&;
        auto SetUsage( TextureUsageFlags usage ) -> FrameGraphTextureDescription&;
        auto SetResourceType( ResourceType usage ) -> FrameGraphTextureDescription&;
    };

    struct FrameGraphResource {
        using ResourceHandle = Ref<IResource>;

        ResourceID mResourceID{};
        ResourceHandle mHandle{};

        FrameGraphResourceType mFrameGraphResourceType{ FrameGraphResourceType::eInvalid };

        MKT_NODISCARD auto HasResource() const -> bool;
    };

    class FrameGraphResourceManager final {
    public:
        explicit FrameGraphResourceManager( size_t initialIdsCount );

        MKT_NODISCARD auto Get( ResourceID id ) -> FrameGraphResource&;
        MKT_NODISCARD auto Get( ResourceID id ) const -> const FrameGraphResource&;

        MKT_NODISCARD auto Allocate() -> FrameGraphResource&;
        MKT_NODISCARD auto Free( ResourceID id ) -> bool;

    private:
        ankerl::unordered_dense::set<ResourceID> mFreeIds{};
        ankerl::unordered_dense::map<ResourceID, eastl::unique_ptr<FrameGraphResource>> mResources{};
    };

    class FrameGraphNodeBuilder final {
    public:
        explicit FrameGraphNodeBuilder( material::ShaderLibrary* shaderLibrary,
            GpuDevice* device, FrameGraphNode* node, FrameGraphResourceManager* resourceManager);

        auto Create( const FrameGraphPipelineDescription& desc) -> ResourceID;
        auto Create( const FrameGraphBufferDescription& desc) -> ResourceID;
        auto Create( const FrameGraphTextureDescription& desc) -> ResourceID;

        auto Write(ResourceID resource, FrameGraphResourceAccessType accessType, FrameGraphStageType shaderStage ) -> void;

    private:
        GpuDevice* mDevice{};
        FrameGraphNode* mGraphNode{};
        FrameGraphResourceManager* mResourceManager{};

        material::ShaderLibrary* mShaderLibrary{};
    };

    class FrameGraph final {
    public:
        explicit FrameGraph( GpuDevice* device );

        auto Compile() -> void;
        auto Execute() -> void;

        template<typename PassData, typename SetupFn, typename ExecuteFn>
        auto RegisterPass( eastl::string_view name, FrameGraphNodeType nodeType, SetupFn &&setup, ExecuteFn &&execute ) -> void {
            FrameGraphNode &node{ PushNode( name, nodeType ) };
            PassData &data{ mBlackboard.Add<PassData>() };

            FrameGraphNodeBuilder builder{ mShaderLibrary.get(), mDevice, MKT_ADDRESSOF( node ), mResourceManager.get() };
            setup( builder, data );

            CreateGpuResources( builder );
            node.mCallback = [execute]( CommandContext &ctx, Blackboard &blackboard ) {
                execute( ctx, blackboard );
            };
        }

        template<typename SetupFn, typename ExecuteFn>
        auto RegisterPass( eastl::string_view name, FrameGraphNodeType nodeType, SetupFn &&setup, ExecuteFn &&execute ) -> void {
            FrameGraphNode &node{ PushNode( name, nodeType ) };

            FrameGraphNodeBuilder builder{ mShaderLibrary.get(), mDevice, MKT_ADDRESSOF( node ), mResourceManager.get() };
            setup( builder );

            CreateGpuResources( builder );
            node.mCallback = [execute]( CommandContext &ctx, Blackboard &blackboard ) {
                execute( ctx, blackboard );
            };
        }

        MKT_NODISCARD static auto Create( GpuDevice *device ) -> eastl::unique_ptr<FrameGraph>;

        ~FrameGraph();

    private:
        auto CreateGpuResources( FrameGraphNodeBuilder& builder ) -> void;
        auto PushNode( eastl::string_view passName, FrameGraphNodeType type ) -> FrameGraphNode&;

    private:
        GpuDevice* mDevice{};
        Blackboard mBlackboard{};

        eastl::unique_ptr<material::ShaderLibrary> mShaderLibrary{};

        // When we compile we create the context for each pass
        eastl::unique_ptr<FrameGraphResourceManager> mResourceManager{};
        ankerl::unordered_dense::map<eastl::string, eastl::unique_ptr<FrameGraphNode>> mNodes{};

        tf::Taskflow mExecutionGraph{};
    };
}

#endif//MIKOTO_FRAME_GRAPH_HH
