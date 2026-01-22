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
#include <Library/Data/ResourcePool.hh>

#include <Renderer/Core/Pipeline.hh>
#include <Renderer/Core/FrameBlackboard.hh>
#include <Renderer/Core/FrameResource.hh>

namespace Mikoto {

    class FramePass;
    class GraphicsContext;

    struct FrameNode {
        FramePass* Pass{};

        std::vector<std::string> Inputs{};
        std::vector<std::string> Outputs{};
    };

    // Used for resource transitioning
    enum class FrameResourceState {
        RenderTarget_Read,
        RenderTarget_Write,
        ShaderResource_Read,
        ShaderResource_Write,
    };

    class FrameGraphBuilder {
    public:

        // Declare Writes
        auto WriteTexture(FramePass* node, std::string_view name) -> void;
        auto WriteBuffer(FramePass* node, std::string_view name) -> void;

        // Declare Reads
        auto ReadTexture(FramePass* node, std::string_view name) -> void;
        auto ReadBuffer(FramePass* node, std::string_view name) -> void;

        // Create Resources
        auto CreateNamedBuffer(std::string_view name, BufferDescription description) -> void;
        auto CreateNamedTexture(std::string_view name, TextureDescription description) -> void;
        auto CreateNamedPipeline(std::string_view name, PipelineDescription description) -> void;

        auto CreateNamedRenderTarget(std::string_view name, TextureDescription description) -> void ;
        auto CreateNamedRenderTarget(std::string_view name, TextureCubeCreateDescription description) -> void ;

        auto CreateColorRenderTarget(std::string_view name, UInt32 width, UInt32 height, TextureFormat format) -> void;
        auto CreateDepthRenderTarget(std::string_view name, UInt32 width, UInt32 height, TextureFormat format) -> void;
        auto CreateCubeRenderTarget(std::string_view name, UInt32 dimensions, TextureFormat format, UInt32 mipLevels = 1) -> void;

    private:
        friend class FrameGraph;

        struct NodeData {
            std::vector<std::string> ReadBuffers{};
            std::vector<std::string> WriteBuffers{};

            std::vector<std::string> ReadTextures{};
            std::vector<std::string> WriteTextures{};
        };

        // Hold nodes and their dependencies, what textures
        // they read from and write to, same for buffers
        ankerl::unordered_dense::map<FramePass*, NodeData> m_Nodes{};

        // Holds frame resources that need to be created
        ankerl::unordered_dense::map<std::string, FrameResource> m_Pipelines{};
        ankerl::unordered_dense::map<std::string, FrameResource> m_Resources{};
    };

    class FrameGraph final {
    public:

        explicit FrameGraph( GraphicsContext* context, GpuDevice* device );

        auto RegisterPass(FramePass* pass) -> void;

        auto Compile(FrameGraphBuilder& builder) -> void;
        auto Execute() -> void;

        MKT_NODISCARD auto GetBlackboard() const -> FrameBlackboard*;

        MKT_NODISCARD static auto Create(GraphicsContext* context, GpuDevice* device ) -> Unique<FrameGraph>;

    private:
        auto SortPassExecution(FrameGraphBuilder& builder) -> void;
        auto RegisterResource(std::string_view name, FrameResource resource) const -> void;

    private:
        std::vector<FrameNode> m_Nodes{};
        GpuDevice* m_Device{};
        GraphicsContext* m_GraphicsContex{};

        Unique<FrameBlackboard> m_Blackboard{};

        bool m_Compiled{ false };
    };
}

#endif//MIKOTO_FRAME_GRAPH_HH
