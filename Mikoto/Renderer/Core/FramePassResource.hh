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

#ifndef MIKOTO_FRAME_PASS_RESOURCE_H
#define MIKOTO_FRAME_PASS_RESOURCE_H

#include <string>
#include <variant>

#include <ankerl/unordered_dense.h>

#include <Common/Common.hh>
#include <Assets//Texture.hh>
#include <Renderer/Core/Pipeline.hh>

namespace Mikoto {

    using ResourceHandle = Ref<IResource>;

    enum class FrameResourceAccessType {
        Read,
        Write,
        Undefined,
    };

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

        // Transfer / copies
        TransferSrc,                  // copy src
        TransferDst,                  // copy dst

        // Presentable state (swapchain images)
        Present,


        // Rework states ===========================
        UnorderedAccessView,

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
        ResourceHandle Handle{}; // These should probably not be here because the CommandContext gets the resources from the graphics 
                                 // context which creates the actual resources on the GPU, the Frame graph simply orchestrates dependencies, 
                                 // kept for now just to have some meta data on the type of resource.
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

}
#endif //MIKOTO_FRAME_PASS_RESOURCE_H