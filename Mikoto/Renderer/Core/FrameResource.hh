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
    enum class FrameResourceType {
        RENDER_TARGET,
        TEXTURE,
        BUFFER,
        PIPELINE,
        INVALID
    };

    struct FrameResourceBuffer {};

    struct FrameResourceTexture {};

    struct FrameResourcePipeline {};

    struct PipelineDescription {
        ankerl::unordered_dense::map<ShaderStage, std::string> Shaders{};
        std::variant<GraphicsPipelineDescription, ComputePipelineDescription> Description{};

        // TODO: temporary, we specify the textures this pipeline will output to
        std::string DepthRenderTargets{};
        std::vector<std::string> ColorRenderTargets{};

        auto AddShader( std::string_view path, ShaderStage stage ) -> void;
    };

    struct FrameResource {
        FrameResourceType Type{ FrameResourceType::INVALID };
        std::variant<BufferDescription, PipelineDescription, TextureDescription, TextureCubeCreateDescription> Description{};
    };

    enum class ShaderResourceType {
        SHADER_STORAGE_BUFFER,
        SHADER_RESOURCE_UNIFORM_BUFFER,
        SHADER_RESOURCE_COMBINED_IMAGE_SAMPLER,
        SHADER_RESOURCE_UNDEFINED,
    };

    struct ShaderResourceInfo {
        std::string Name{};
        UInt32 GroupBinding{};
        ShaderResourceType ResourceType{ ShaderResourceType::SHADER_RESOURCE_UNDEFINED };
    };
}
#endif //MIKOTO_FRAMERESOURCE_H