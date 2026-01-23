//
// Created by zanet on 12/20/2025.
//

#ifndef MIKOTO_FRAMERESOURCE_H
#define MIKOTO_FRAMERESOURCE_H

#include <string>
#include <variant>
#include <vector>

#include <ankerl/unordered_dense.h>

#include <Common/Common.hh>
#include <Assets//Texture.hh>
#include <Renderer/Core/Pipeline.hh>

namespace Mikoto {
    // Used for resource state transitioning
    enum class FrameResourceState {
        RenderTarget_Color,
        RenderTarget_Depth,
        ShaderResource_Read,
        ShaderResource_Write,

        Transfer_Src,
        Transfer_Dst,

        Undefined,
    };

    enum class FrameResourceType {
        TEXTURE,
        BUFFER,
        INVALID,
    };

    struct PipelineDescription {
        std::string Name{};

        std::variant<GraphicsPipelineDescription,
            ComputePipelineDescription> Description{};
        ankerl::unordered_dense::map<ShaderStage, std::string> Shaders{};

        auto UseShader( std::string_view path, ShaderStage stage ) -> void;
    };

    struct FrameResource {
        std::variant<
            BufferDescription,
            TextureDescription,
            TextureCubeCreateDescription> Description{};

        FrameResourceType Type{ FrameResourceType::INVALID };
    };

    enum class ShaderResourceType {
        BUFFER,
        COMBINED_IMAGE_SAMPLER,
        SAMPLER,
        UNDEFINED,
    };

    struct ShaderResourceInfo {
        std::string Name{};
        UInt32 GroupBinding{};
        ShaderResourceType ResourceType{ ShaderResourceType::UNDEFINED };
    };
}
#endif //MIKOTO_FRAMERESOURCE_H