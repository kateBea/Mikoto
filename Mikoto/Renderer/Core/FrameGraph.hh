//
// Created by kate on 11/24/25.
//

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

        auto RegisterShaderResource(FramePass* pass, std::string_view name, UInt32 groupIndex, UInt32 groupBinding, ShaderResourceType type) -> void;

    private:
        friend class FrameGraph;

        struct NodeData {
            std::vector<std::string> ReadBuffers{};
            std::vector<std::string> WriteBuffers{};

            std::vector<std::string> ReadTextures{};
            std::vector<std::string> WriteTextures{};

            // TODO: Group index -> (ShaderResourceInfo)
            ankerl::unordered_dense::map<UInt32, ShaderResourceInfo> ShaderResources{};
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

        auto Compile(FrameGraphBuilder& backend) -> void;
        auto Execute() -> void;

        MKT_NODISCARD auto GetBlackboard() const -> FrameBlackboard*;

        MKT_NODISCARD static auto Create(GraphicsContext* context, GpuDevice* device ) -> Unique<FrameGraph>;

    private:

        auto RegisterResource(std::string_view name, FrameResource resource) const -> void;

    private:
        std::vector<FrameNode> m_Nodes{};
        GraphicsContext* m_GraphicsContex{};

        Unique<FrameBlackboard> m_Blackboard{};

        bool m_Compiled{ false };
    };
}

#endif//MIKOTO_FRAME_GRAPH_HH
