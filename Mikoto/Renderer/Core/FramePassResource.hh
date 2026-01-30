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

    using ResourceHandle = Ref<IResource>;

    enum class FrameResourceState {
        // Shader read-only (Textures, Buffers)
        ShaderRead_GraphicsPipeline,  // sampled images, uniform texel buffers
        ShaderRead_ComputePipeline,   // compute shader sampled/readonly

        // Geometry input
        VertexIndexBuffer,            // vertex/index input buffers
        UniformBuffer,                // constant/uniform buffer
        IndirectArgument,             // indirect draw/dispatch

        // Render targets
        RenderTarget,                 // color attachment write
        DepthWrite,                   // depth/stencil write
        DepthRead,                    // depth read (sampling or read-only)
        UnorderedAccess,              // RWTexture / storage image/buffer

        // Transfer / copies
        TransferSrc,                  // copy src
        TransferDst,                  // copy dst

        // Presentable state (swapchain images)
        Present,

        // Unknown / initial
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

    struct FramePassResourceDescription {
        std::variant<
            BufferDescription,
            TextureDescription,
            TextureCubeCreateDescription> Description{};

        FrameResourceType Type{ FrameResourceType::INVALID };
    };

    struct FramePassResource {
        ResourceHandle Handle{};
        FrameResourceType Type{ FrameResourceType::INVALID };
        FrameResourceState CurrentState{ FrameResourceState::Undefined };

        MKT_NODISCARD auto IsResource(FrameResourceType type) const -> bool { return Type == type; }
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