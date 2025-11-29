//
// Created by kate on 11/24/25.
//

#ifndef MIKOTO_FRAME_GRAPH_HH
#define MIKOTO_FRAME_GRAPH_HH

#include <ankerl/unordered_dense.h>

#include <Assets//Texture.hh>
#include <Library/Data/ResourcePool.hh>
#include <string>
#include <variant>
#include <vector>

#include "Renderer/Core/Pipeline.hh"

namespace Mikoto {

    class FramePass;
    class GraphicsContext;

    using ResourceHandle = Ref<IResource>;

    enum class FrameResourceType { TEXTURE, BUFFER, PIPELINE, INVALID };

    enum class RenderTargetType {
        COLOR,
        DEPTH,
    };

    struct PipelineDescription {
        PipelineType Type{ };

        // With std::variant
        ComputePipelineDescription ComputeDesc{};
        GraphicsPipelineDescription GraphicsDesc{};

        std::vector<std::string> Shaders{};

        auto AddShader(std::string_view path) -> void;
    };

    struct ResourceDescription {
        FrameResourceType Type{ FrameResourceType::INVALID };

        std::variant<BufferDescription, PipelineDescription, TextureDescription> ResourceDesc{};
    };

    struct FrameResource {
        std::string Name{};
        ResourceDescription Description{};
        ResourceHandle Resource{ ResourceHandle::CreateEmpty() };
    };

    struct FrameNode {
        FramePass* Pass{};

        std::vector<std::string> Inputs{};
        std::vector<std::string> Outputs{};
    };

    class FrameGraphBuilder {
    public:

        auto RegisterInput(FramePass* node, std::string_view name) -> void;
        auto RegisterOutput(FramePass* node, std::string_view name) -> void;

        // Create Resources
        auto CreateNamedBuffer(std::string_view name, BufferDescription description) -> void;
        auto CreateNamedTexture(std::string_view name, TextureDescription description) -> void;
        auto CreateNamedPipeline(std::string_view name, PipelineDescription description, PipelineType type) -> void;
        auto CreateNamedRenderTarget(std::string_view name, TextureDescription description, RenderTargetType) -> void ;

    private:
        struct NodeData {
            std::vector<std::string> Inputs{};
            std::vector<std::string> Outputs{};
        };

        ankerl::unordered_dense::map<FramePass*, NodeData> m_Nodes{};
        ankerl::unordered_dense::map<std::string, FrameResource> m_Resources{};

    };

    class FrameGraph final {
    public:

        explicit FrameGraph( GraphicsContext& Context );

        auto RegisterPass(FramePass* pass) -> FramePass*;

        auto Compile(FrameGraphBuilder& backend) -> void;
        auto Execute(GraphicsContext& backend) -> void;

        MKT_NODISCARD static auto Create(GraphicsContext * context ) -> Unique<FrameGraph>;


    private:
        auto RegisterResource(FrameResource resource) -> void;

        std::vector<FrameNode> m_Nodes{};

        // Maybe it goes to the context after we build the graph this is not needed here
        ankerl::unordered_dense::map<std::string, FrameResource> m_Resources{};

        GraphicsContext* m_GraphicsContex{};

        bool m_Compiled{ false };
    };
}

#endif//MIKOTO_FRAME_GRAPH_HH
