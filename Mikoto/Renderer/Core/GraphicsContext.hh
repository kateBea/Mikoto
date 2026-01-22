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
#include <Renderer/Core/FrameBlackboard.hh>
#include <Renderer/Core/GpuDevice.hh>
#include <Renderer/Core/Light.hh>
#include <Renderer/Core/Pipeline.hh>

#include <Renderer/Core/SRGBase.hh>

namespace Mikoto {
    class FramePass;
    class CommandContext;

    struct PassViewport {
        float X{}, Y{}, Width{}, Height{};
    };

    struct PassScissor {
        float X{}, Y{}, Width{}, Height{};
    };

    struct PassResources {
        FramePass* Pass{ nullptr };
        FrameBlackboard* Blackboard{ nullptr };
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

        virtual auto BeginPass(FramePass* pass) -> void = 0;
        virtual auto EndPass(FramePass* pass) -> void = 0;

        virtual auto BeginFrame(FrameBlackboard* blackboard)-> void = 0;
        virtual auto EndFrame()-> void = 0;

        virtual auto PushImage(TextureHandle texture) -> Int32 = 0;

        virtual auto BindTextureList(CommandListHandle cmdList) -> void = 0;
        virtual auto BindPassResources(FramePass* pass, CommandListHandle cmdList  ) -> void = 0;
        virtual auto CommitShaderResources(FramePass* pass ) -> void = 0;

        virtual auto CreatePipelineResources(FramePass* pass, PipelineHandle pipeline) -> void = 0;

        virtual auto GetPassSRG( FramePass* pass ) -> SRGPerPass* = 0;

        virtual auto InsertResourceBarrier(FramePass* pass, CommandListHandle cmdList ) -> void = 0;

        MKT_NODISCARD static auto Create(GpuDevice* device) -> Unique<GraphicsContext>;

    protected:
        explicit GraphicsContext(GpuDevice* device)
            : m_Device{ device } {}

    protected:
        GpuDevice* m_Device{ nullptr };

        ankerl::unordered_dense::map<SRGType, Unique<SRGBase>> m_SRG{};
    };
}

#endif//MIKOTO_GRAPHICS_CONTEXT_HH
