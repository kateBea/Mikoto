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

#include <ankerl/unordered_dense.h>

#include <Assets//Texture.hh>
#include <Assets/AssetsService.hh>
#include <Core/TimeService.hh>
#include <Material/TextureCube.hh>
#include <Renderer/Core/FrameGraphBlackboard.hh>
#include <Renderer/Core/FrameGraphStructures.hh>
#include <Renderer/Core/FramePassResource.hh>
#include <Renderer/Core/Pipeline.hh>
#include <Renderer/Core/RenderUtility.hh>
#include <Renderer/Core/ResourceGroupBase.hh>
#include <string>
#include <variant>
#include <vector>

#include "Barrier.hh"

namespace Mikoto {
    class CommandContext;
    class GraphicsContext;

    class FramePassBuilder final {
    public:
        explicit FramePassBuilder( FramePassNode &node );

        auto Use( ResourceGroup type ) -> FramePassBuilder&;
        auto Use( ResourceGroup type, std::string_view name, UInt32 bindSlot ) -> FramePassBuilder&;

        // state indicates the state the resource needs to be in for this pass, this may imply setting
        // a barrier for transition on the resource before the pass starts (only if needed)
        auto Write( std::string_view name, FrameResourceState state) -> FramePassBuilder&;
        auto Read( std::string_view name, FrameResourceState state) -> FramePassBuilder&;

        // Deprecated
        template<typename ResourceType, typename... Args>
        auto Create( Args &&... args ) -> FramePassBuilder& {
            if constexpr (std::is_same_v<ResourceType, Buffer>) {
                CreateBuffer( std::forward<Args>( args )... );
            } else if constexpr (std::is_same_v<ResourceType, Texture>
                    || std::is_same_v<ResourceType, TextureCube>) {
                CreateTexture( std::forward<Args>( args )... );
            } else if constexpr (std::is_same_v<ResourceType, Pipeline>) {
                CreatePipeline( std::forward<Args>( args )... );
            } else {
                MKT_STATIC_ASSERT( false, "Not valid Create type" );
            }

            return *this;
        }

        auto CreateBuffer( const BufferBuilder& description ) -> void;

        // Resources to be used in the shader can be specified at creation or later in
        // the execute callback via calling the corresponding bind methods
        auto UseShader( std::string_view path, ShaderStage stage ) -> FramePassBuilder&;

        MKT_NODISCARD auto GetPass() -> FramePassNode *;

        // TODO: builder helpers for pipeline creation
        // e.g: auto SetDepthTest(bool enable) -> PipelineDesc&;
        // e.g: auto SetDepthWrite(bool enable) -> PipelineDesc&;

        auto CreateBuffer( std::string_view name, BufferDescription description ) -> void;
        auto CreateBuffer( std::string_view name, BufferUsage usage, Size sizeBytes, ResourceUsageType usageType = ResourceUsageType::RESOURCE_USAGE_STREAMING ) -> void;
        auto CreateBuffer( std::string_view name, BufferUsage usage, Size size, Size elementCount, ResourceUsageType usageType = ResourceUsageType::RESOURCE_USAGE_STREAMING ) -> void;

        auto CreatePipeline( std::string_view name, const GraphicsPipelineDescription &description ) -> void;
        auto CreatePipeline( std::string_view name, const ComputePipelineDescription &description ) -> void;

        // For 2D
        auto CreateTexture( std::string_view name, TextureDescription description ) -> void;
        auto CreateTexture( std::string_view name, RenderResolution resolution, TextureFormat format, TextureUsage usage ) -> void;
        auto CreateTexture( std::string_view name, UInt32 width, UInt32 height, TextureFormat format, TextureUsage usage ) -> void;
        auto CreateTexture( std::string_view name, RenderResolution resolution, TextureFormat format, Multisampling msaa, TextureUsage usage ) -> void;
        auto CreateTexture( std::string_view name, UInt32 width, UInt32 height, TextureFormat format, Multisampling msaa, TextureUsage usage ) -> void;
        auto CreateTexture( std::string_view name, UInt32 width, UInt32 height, TextureFormat format, void* ptr, Size sizeBytes) -> void;

        // For cubes
        auto CreateTexture( std::string_view name, TextureCubeCreateDescription description ) -> void;
        auto CreateTexture( std::string_view name, UInt32 dimensions, TextureFormat format, UInt32 mipLevels = 1 ) -> void;

        MKT_NODISCARD auto HasPipeline() const -> bool;

    private:
        friend class FrameGraph;

        FramePassNode *m_Node{};

        bool m_HasActivePipeline{ false };
        PipelineDescription m_PipelineDescription{};

        ankerl::unordered_dense::map<std::string, FramePassResourceDescription> m_Creates{};
    };

    using PassList = ankerl::unordered_dense::map<std::string, FramePassNode>;

    struct ResourceContainer {
        // Resources by names to be used for resource transition and barriers
        ankerl::unordered_dense::map<std::string, FramePassResource> Resources{};
    };

    // This will be passed to the command context.
    // Passes that need to create resources on the fly will go through 
    // this manager, for example right now we are using boundless descriptor
    // with lots of sampler 2D, we need control over that like knowing whether it exists and for how long
    class FrameGraphResourceAllocator {

    };

    class FrameGraph final {
    public:
        explicit FrameGraph( GraphicsContext *context, GpuDevice *device );

        template<typename PassData, typename SetupFn, typename ExecuteFn>
        auto RegisterPass( const std::string_view name, SetupFn &&setup, ExecuteFn &&execute, FramePassNodeType nodeType = FramePassNodeType::GRAPHICS ) -> void {
            FramePassNode &node{ CreatePassNode( name, nodeType ) };

            PassData &data{ m_GraphBlackboard.Add<PassData>() };

            FramePassBuilder builder{ node };
            setup( builder, data );

            CreateCommitedResources( builder );
            node.ExecuteCallback = [execute]( CommandContext &ctx, FrameGraphBlackboard &blackboard ) { execute( ctx, blackboard ); };
        }

        template<typename SetupFn, typename ExecuteFn>
        auto RegisterPass( const std::string_view name, SetupFn &&setup, ExecuteFn &&execute, FramePassNodeType nodeType = FramePassNodeType::GRAPHICS ) -> void {
            FramePassNode &node{ CreatePassNode( name, nodeType ) };

            FramePassBuilder builder{ node };
            setup( builder );

            CreateCommitedResources( builder );
            node.ExecuteCallback = [execute]( CommandContext &ctx, FrameGraphBlackboard &blackboard ) { execute( ctx, blackboard ); };
        }

        auto Compile() -> void;
        auto Execute() -> void;

        auto SetNodeExecutionPolicy(std::string_view name, FramePassExecutionPolicy policy) -> void;

        MKT_NODISCARD auto IsCompiled() const -> bool;

        MKT_NODISCARD auto GetPassList() const -> const PassList&;

        MKT_NODISCARD auto GetTexture( std::string_view name ) const -> TextureHandle;
        MKT_NODISCARD auto GetBuffer( std::string_view name ) const -> BufferHandle;

        MKT_NODISCARD static auto Create( GraphicsContext *context, GpuDevice *device ) -> Unique<FrameGraph>;

    private:
        auto InsertResourceBarriers(FramePassNode& node, CommandListHandle cmd) -> void;

        MKT_NODISCARD auto IsFramePassPresent( std::string_view name ) const -> bool;

        auto SortPassExecution() -> void;

        auto CreatePassNode( std::string_view name, FramePassNodeType type ) -> FramePassNode &;
        auto CreateCommitedResources( FramePassBuilder &builder ) -> void;

        auto CreateResource( std::string_view name, FramePassResourceDescription resource ) -> void;

        auto DebugDumpResources() -> void;

        auto SubmitCommandLists() -> void;
        auto InitializeCommandList( FramePassNodeType type ) -> CommandListHandle;

    private:
        // Backend resource creation and control
        GpuDevice *m_Device{};
        GraphicsContext *m_GraphicsContex{};
        FrameGraphBlackboard m_GraphBlackboard{};

        // List of registered nodes
        PassList m_Passes{};

        CommandListHandle m_ComputeCommandList{};
        CommandListHandle m_GraphicsCommandList{};
        CommandListHandle m_TransferCommandList{};

        ankerl::unordered_dense::map<ICommandList *, std::vector<ResourceBarrierInfo>> m_CommandBarriers{}; 

        ResourceContainer m_ResourcesByNames{};

        // Compile flag
        bool m_Compiled{ false };

        // To control pass execution time
        // In milliseconds
        double m_ElapsedTime{ 0.0 };
        double m_ElapsedTimeUpdatedInterval{ 500 };
    };
}

#endif//MIKOTO_FRAME_GRAPH_HH
