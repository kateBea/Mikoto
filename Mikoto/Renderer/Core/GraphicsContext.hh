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

#ifndef MIKOTO_GRAPHICS_CONTEXT_HH
#define MIKOTO_GRAPHICS_CONTEXT_HH

#include <string_view>
#include <vector>
#include <initializer_list>

#include <ankerl/unordered_dense.h>

#include <Assets//Texture.hh>
#include <Assets/Model.hh>
#include <Library/Utility/Types.hh>
#include <Material/Material.hh>
#include <Renderer/Core/Buffer.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/GpuDevice.hh>
#include <Renderer/Core/Light.hh>
#include <Renderer/Core/Pipeline.hh>

#include <Renderer/Core/SRGBase.hh>

namespace Mikoto {
    class CommandContext;
    class CommandContext;

    struct PassViewport {
        float X{}, Y{}, Width{}, Height{};
    };

    struct PassScissor {
        float X{}, Y{}, Width{}, Height{};
    };

    struct PassRenderInfo {
        LoadOp ColorLoadOp{ LoadOp::CLEAR };
        LoadOp DephtLoadOp{ LoadOp::CLEAR };
    };

    class GraphicsContext {
    public:
        virtual ~GraphicsContext() = default;

        virtual auto Init() -> void = 0;
        virtual auto Shutdown() -> void = 0;

        virtual auto BeginFrame()-> void = 0;
        virtual auto EndFrame()-> void = 0;

        virtual auto GetSampler(std::string_view name) -> SamplerHandle = 0;
        virtual auto GetTexture(std::string_view name) -> TextureHandle = 0;
        virtual auto GetPipeline(std::string_view name) -> PipelineHandle = 0;
        virtual auto GetBuffer(std::string_view name) -> BufferHandle = 0;

        virtual auto CreateTexture(std::string_view name, const TextureDescription &description) -> TextureHandle = 0;
        virtual auto CreateTexture(std::string_view name, const TextureCubeCreateDescription& description) -> TextureHandle = 0;

        virtual auto CreateBuffer(std::string_view name, BufferDescription description) -> BufferHandle = 0;

        virtual auto CreateSampler( SamplerDescription& description ) -> SamplerHandle = 0;
        virtual auto CreateSampler( std::string_view name, const SamplerDescription& description ) -> void = 0;

        virtual auto UpdateResourceBindings(std::string_view passName, SRGPerPass& passData ) -> void = 0;
        virtual auto PrepareResourceBindings(std::string_view passName, PipelineDescription& desc) -> void = 0;
        virtual auto BindShaderResources(std::string_view passName, CommandListHandle cmdList  ) -> void = 0;

        // Managing global sampler 2D images
        virtual auto BindGlobalTextures(CommandListHandle cmdList) -> void = 0;
        virtual auto PushGlobalTexture( TextureHandle texture ) -> Int32 = 0;

        // Make visible a buffer as shader resource to a specific pass
        virtual auto PushBuffer(BufferHandle handle, std::string_view passName, UInt32 bindingSlot) -> void = 0;
        virtual auto PushTexture(TextureHandle handle, SamplerHandle sampler, std::string_view passName, UInt32 bindingSlot) -> void = 0;

        MKT_NODISCARD static auto Create(GpuDevice* device) -> Unique<GraphicsContext>;

        virtual auto InsertResourceBarrier(BufferHandle buffer, FrameResourceState previousState, FrameResourceState newState, CommandListHandle cmd) -> bool = 0;
        virtual auto InsertResourceBarrier(TextureHandle texture, FrameResourceState previousState, FrameResourceState newState, CommandListHandle cmd) -> bool = 0;

        // virtual auto PushImage(TextureHandle texture) -> Int32 = 0;

    protected:
        explicit GraphicsContext(GpuDevice* device)
            : m_Device{ device } {}

    protected:
        GpuDevice* m_Device{ nullptr };

        // Global list of sampled textures
        SRGTextures m_SrgTextures{};
    };
}

#endif//MIKOTO_GRAPHICS_CONTEXT_HH
