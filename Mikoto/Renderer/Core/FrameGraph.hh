//
// Created by kate on 11/24/25.
//

#ifndef MIKOTO_FRAME_GRAPH_HH
#define MIKOTO_FRAME_GRAPH_HH

#include <string>
#include <variant>
#include <vector>

#include <Assets//Texture.hh>
#include <Library/Data/ResourcePool.hh>

namespace Mikoto {

    class FramePass;
    class GraphicsContext;

    using ResourceHandle = Ref<IResource>;

    enum class ResourceType { TEXTURE, BUFFER, PIPELINE };

    struct ResourcePipelineDes {

    };

    struct ResourceBufferDes {

    };

    struct ResourceTextureDes {

    };

    struct ResourceDescription {

        ResourceType Type{};

        std::variant<ResourceHandle, ResourcePipelineDes, ResourceBufferDes> ResourceDesc{};
    };

    struct FrameResource {
        std::string Name{};
        ResourceDescription Description{};
        ResourceHandle Resource{ ResourceHandle::CreateEmpty() };
    };

    struct FrameNode {
        FramePass* Pass{};

        std::vector<FrameResource> Inputs{};
        std::vector<FrameResource> Outputs{};
    };

    class FrameGraph final {
    public:

        explicit FrameGraph( GraphicsContext& Context );

        auto RegisterPass(FramePass* pass) -> FramePass*;

        auto Compile(GraphicsContext& backend) -> void;
        auto Execute(GraphicsContext& backend) -> void;

        MKT_NODISCARD static auto Create(GraphicsContext * context ) -> Unique<FrameGraph>;


    private:

        GraphicsContext* m_GraphicsContex{};

        bool m_Compiled{ false };

        std::vector<FrameNode> m_Nodes{};
        std::vector<FrameResource> m_Resources{};
    };
}

#endif//MIKOTO_FRAME_GRAPH_HH
