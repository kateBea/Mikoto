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

    enum class FrameResourceType { RENDER_TARGET, TEXTURE, BUFFER, PIPELINE, INVALID };

    struct PipelineDescription {
        ankerl::unordered_dense::map<ShaderStage, std::string> Shaders{};
        std::variant<GraphicsPipelineDescription, ComputePipelineDescription> Description{};

        // TODO: temporary, we specify the textures this pipeline will output to
        std::string DepthRenderTargets{};
        std::vector<std::string> ColorRenderTargets{};

        auto AddShader(std::string_view path, ShaderStage stage) -> void;
    };

    struct ResourceDescription {
        FrameResourceType Type{ FrameResourceType::INVALID };

        std::variant<BufferDescription, PipelineDescription, TextureDescription> ResourceDesc{};
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
        auto CreateNamedPipeline(std::string_view name, PipelineDescription description) -> void;
        auto CreateNamedRenderTarget(std::string_view name, TextureDescription description) -> void ;

    private:
        friend class FrameGraph;

        struct NodeData {
            std::vector<std::string> Inputs{};
            std::vector<std::string> Outputs{};
        };

        ankerl::unordered_dense::map<FramePass*, NodeData> m_Nodes{};
        ankerl::unordered_dense::map<std::string, ResourceDescription> m_Resources{};
    };

    class FrameGraph final {
    public:

        explicit FrameGraph( GraphicsContext& Context );

        auto Compile(FrameGraphBuilder& backend) -> void;
        auto Execute() -> void;

        MKT_NODISCARD auto GetNamedTexture(std::string_view name) -> TextureHandle;
        MKT_NODISCARD auto GetNamedPipeline(std::string_view name) -> PipelineHandle;

        MKT_NODISCARD static auto Create(GraphicsContext * context ) -> Unique<FrameGraph>;

    private:
        auto RegisterTexture(std::string_view name, TextureDescription description) -> void;
        auto RegisterPipeline(std::string_view name, PipelineDescription description) -> void;
        auto RegisterRenderTarget(std::string_view name, TextureDescription description) -> void;
        auto RegisterBuffer(std::string_view name, BufferDescription description) -> void;


        auto RegisterResource(std::string_view name, ResourceDescription resource) -> void;

        GraphicsContext* m_GraphicsContex{};

        std::vector<FrameNode> m_Nodes{};

        ankerl::unordered_dense::map<std::string, TextureHandle> m_TexturesByNames{};
        ankerl::unordered_dense::map<std::string, PipelineHandle> m_PipelinesByNames{};
        ankerl::unordered_dense::map<std::string, BufferHandle> m_BuffersByNames{};

        bool m_Compiled{ false };
    };
}

#endif//MIKOTO_FRAME_GRAPH_HH
