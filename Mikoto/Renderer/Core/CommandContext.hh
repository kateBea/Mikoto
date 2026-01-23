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

#ifndef MIKOTO_COMMAND_CONTEXT_HH
#define MIKOTO_COMMAND_CONTEXT_HH

#include <string_view>
#include <vector>
#include <initializer_list>

#include <ankerl/unordered_dense.h>

#include <Assets/Model.hh>
#include <Assets//Texture.hh>
#include <Material/Material.hh>

#include <Library/Utility/Types.hh>

#include <Renderer/Core/Buffer.hh>
#include <Renderer/Core/Light.hh>
#include <Renderer/Core/Pipeline.hh>
#include <Renderer/Core/SRGBase.hh>
#include <Renderer/Core/GpuDevice.hh>
#include <Renderer/Core/GraphicsContext.hh>

#include "FrameGraph.hh"

namespace Mikoto {

    struct DrawIndexedState {
        BufferHandle IndexBuffer{};

        // Specifies the buffer and its binding
        std::vector<std::pair<BufferHandle, UInt32>> VertexBuffers{};

        UInt32 IndicesCount{};
        UInt32 InstancesCount{};

        UInt32 FirstIndex{};
        UInt32 VertexOffset{};
        UInt32 FirstInstance{};
    };

    class CommandContext final {
    public:
        explicit CommandContext(GraphicsContext *context, GpuDevice* device);

        auto BeginPass(FramePassNode& pass) -> void;
        auto EndPass() -> void;

        auto BeginRender( const PassRenderInfo& renderInfo = PassRenderInfo{}) -> void;
        auto EndRender() -> void;

        auto SetColorRenderTarget(std::string_view color) -> void;
        auto SetDepthRenderTarget(std::string_view depth) -> void;

        auto SetViewport(Int32 x, Int32 y, Int32 width, Int32 height) -> void ;
        auto SetScissor(Int32 x, Int32 y, Int32 width, Int32 height) -> void;

        // Need to bind pipeline before specifying resources
        auto BindPipeline(std::string_view pipelineName ) -> void;

        auto SetClearColor(const Vec4F& color) -> void;

        auto DrawIndexed(const DrawIndexedState& info) -> void;
        auto Draw(UInt32 vertexCount, UInt32 instanceCount, UInt32 firstVertex, UInt32 firstInstance ) -> void;
        auto Dispatch(UInt32 invX, UInt32 invY, UInt32 invZ) -> void;

        auto FillBufferElement(std::string_view bufferName, const void* buffer, Size elementSize, Size elementCount) const -> void;
        auto FillBuffer(std::string_view bufferName, const void* ptrSrc, Size size, Size offset = 0 ) const -> void;

        MKT_NODISCARD auto PushTexture(TextureHandle texture ) const -> Int32;
        MKT_NODISCARD auto GetNamedBuffer( std::string_view ) const -> BufferHandle;

        auto RegisterNamedTexture( std::string_view name, TextureHandle handle ) const -> void;

        auto CreateNamedSampler( std::string_view name, SamplerDescription samplerDescription ) -> void;

    private:

        CommandListHandle m_Commands{};

        GraphicsContext* m_Context{};
        GpuDevice* m_Device{};

        PipelineHandle m_Pipeline{};

        FramePassNode* m_ActivePass{ nullptr };

        RenderInfo m_RenderInfo{};
    };
}

#endif // MIKOTO_COMMAND_CONTEXT_HH