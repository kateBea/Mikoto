//
// Created by kate on 11/24/25.
//

#ifndef MIKOTO_FRAME_GRAPH_HH
#define MIKOTO_FRAME_GRAPH_HH

#include <string>
#include <vector>

#include <Assets//Texture.hh>
#include <Renderer/Core/GraphicsContext.hh>
#include <Library/Data/ResourcePool.hh>

namespace Mikoto {

    class FramePass;


    using ResourceHandle = Ref<IResource>;

    enum class ResourceType { TEXTURE, BUFFER };

    struct ResourceDescription {

        ResourceType Type{};

        // Texture
        UInt32 Width{};
        UInt32 Height{};
        TextureFormat Format{ TextureFormat::TEXTURE_FORMAT_INVALID };

        // Buffer
        Size SizeBytes{};
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

        auto RegisterPass(FramePass* pass) -> FramePass*;

        auto Compile(GraphicsContext& backend) -> void;
        auto Execute(GraphicsContext& backend) -> void;

    private:

        bool m_Compiled{ false };

        std::vector<FrameNode> m_Nodes{};
        std::vector<FrameResource> m_Resources{};
    };
}

#endif//MIKOTO_FRAME_GRAPH_HH
