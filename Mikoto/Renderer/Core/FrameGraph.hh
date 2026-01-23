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

#include <string>
#include <variant>
#include <vector>

#include <ankerl/unordered_dense.h>

#include <Assets//Texture.hh>

#include <Renderer/Core/Pipeline.hh>
#include <Renderer/Core/FrameBlackboard.hh>
#include <Renderer/Core/FrameGraphBlackboard.hh>
#include <Renderer/Core/FrameResource.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Core/SRGBase.hh>

#include <Assets/AssetsService.hh>
#include <Material/TextureCube.hh>

namespace Mikoto {

    struct ResourceNode {
        std::string Name{};
        FrameResourceState InState{};
        FrameResourceState OutState{};
    };

    struct FramePassNode {
        std::string Name{};

        std::vector<ResourceNode> Reads{};
        std::vector<ResourceNode> Writes{};

        SRGPerPass PerPasSRG{};

        std::function<void( CommandContext& , FrameGraphBlackboard& )> ExecuteCallback{};
    };

    class FramePassBuilder final {
    public:
        explicit FramePassBuilder(FramePassNode& node);

        auto Write( std::string_view name, FrameResourceState inState, FrameResourceState outState ) -> void;
        auto Read( std::string_view name, FrameResourceState inState, FrameResourceState outState ) -> void;

        template<typename ResourceType, typename... Args>
        auto Create(Args&&... args) -> void {
            if constexpr (std::is_same_v<ResourceType, Buffer>) {
                CreateBuffer(std::forward<Args>(args)...);
            }
            else if constexpr (std::is_same_v<ResourceType, Texture>) {
                CreateTexture(std::forward<Args>(args)...);
            }
            else if constexpr (std::is_same_v<ResourceType, TextureCube>) {
                CreateTexture(std::forward<Args>(args)...);
            }
            else if constexpr (std::is_same_v<ResourceType, Pipeline>) {
                CreatePipeline(std::forward<Args>(args)...);
            }
        }

    private:
        // Create Resources
        auto CreateBuffer( std::string_view name, BufferDescription description ) -> void;
        auto CreateTexture( std::string_view name, TextureDescription description ) -> void;
        auto CreatePipeline( std::string_view name, PipelineDescription description ) -> void;

        auto CreateTexture( std::string_view name, TextureCubeCreateDescription description ) -> void;

        auto CreateTexture( std::string_view name, UInt32 width, UInt32 height, TextureFormat format ) -> void;
        auto CreateTexture( std::string_view name, UInt32 dimensions, TextureFormat format, UInt32 mipLevels = 1 ) -> void;

    private:
        friend class FrameGraph;

        FramePassNode* m_Node{};
        ankerl::unordered_dense::map<std::string, FrameResource> m_Creates{};
    };

    class FrameGraph final {
    public:
        explicit FrameGraph( GraphicsContext *context, GpuDevice *device );

        template<typename PassData, typename SetupFn, typename ExecuteFn>
        auto RegisterPass( std::string_view name, SetupFn &&setup, ExecuteFn &&execute ) -> void {
            FramePassNode &node{ CreatePassNode( name ) };

            PassData *data{ m_GraphBlackboard.Add<PassData>() };

            FramePassBuilder builder{ node };
            setup( builder, *data );

            CreateCommitedResources(builder);
            node.ExecuteCallback = [execute](CommandContext &ctx, FrameGraphBlackboard& blackboard) {
                execute( ctx, blackboard );
            };
        }

        template<typename SetupFn, typename ExecuteFn>
        auto RegisterPass( std::string_view name, SetupFn &&setup, ExecuteFn &&execute ) -> void {
            FramePassNode& node{ CreatePassNode( name ) };

            FramePassBuilder builder{ node };
            setup( builder );

            CreateCommitedResources(builder);
            node.ExecuteCallback = [execute](CommandContext &ctx, FrameGraphBlackboard& blackboard) {
                execute( ctx, blackboard );
            };
        }

        auto Compile() -> void;
        auto Execute() -> void;

        MKT_NODISCARD auto GetTexture(std::string_view name) const -> TextureHandle;
        MKT_NODISCARD auto GetBuffer(std::string_view name) const -> BufferHandle;

        MKT_NODISCARD static auto Create( GraphicsContext *context, GpuDevice *device ) -> Unique<FrameGraph>;

    private:

        MKT_NODISCARD auto IsFramePassPresent(std::string_view name) const -> bool;

        auto CreatePassNode( std::string_view name ) -> FramePassNode&;
        auto SetShaderResourceGroups(FramePassNode& node) -> void;
        auto CreateCommitedResources(FramePassBuilder& builder) -> void;

        auto CreateResource( std::string_view name, FrameResource resource ) -> void;

        auto SortPassExecution() -> void;

    private:
        // Backend resource creation and control
        GpuDevice *m_Device{};
        GraphicsContext *m_GraphicsContex{};
        FrameGraphBlackboard m_GraphBlackboard{};

        // Resources by names
        ankerl::unordered_dense::map<std::string, TextureHandle> m_TexturesByNames{};
        ankerl::unordered_dense::map<std::string, PipelineHandle> m_PipelinesByNames{};
        ankerl::unordered_dense::map<std::string, BufferHandle> m_BuffersByNames{};
        ankerl::unordered_dense::map<std::string, SamplerHandle> m_SamplersByNames{};

        // List of registered nodes
        ankerl::unordered_dense::map<std::string, FramePassNode> m_Passes{};

        // Compile flag
        bool m_Compiled{ false };
    };
}

#endif//MIKOTO_FRAME_GRAPH_HH
