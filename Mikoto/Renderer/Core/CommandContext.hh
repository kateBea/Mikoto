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

#include <ankerl/unordered_dense.h>

#include <Assets//Texture.hh>
#include <Assets/Model.hh>
#include <Library/Utility/Types.hh>
#include <Material/Material.hh>
#include <Renderer/Core/Buffer.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/GpuDevice.hh>
#include <Renderer/Core/GraphicsContext.hh>
#include <Renderer/Core/Light.hh>
#include <Renderer/Core/Pipeline.hh>
#include <Renderer/Core/ResourceGroupBase.hh>
#include <initializer_list>
#include <string_view>
#include <vector>

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
        explicit CommandContext(GraphicsContext *context, CommandListHandle cmd);

        auto BeginPass(FramePassNode& pass) -> void;
        auto EndPass() -> void;

        auto BeginRender( const PassRenderInfo& renderInfo = PassRenderInfo{}) -> void;
        auto EndRender() -> void;

        auto SetColorRenderTarget(std::string_view color) -> void;
        auto SetDepthRenderTarget(std::string_view depth) -> void;

        auto SetViewport( Int32 x, Int32 y, Int32 width, Int32 height) -> void;
        auto SetViewport( Int32 x, Int32 y, Int32 width, Int32 height, bool flip ) -> void;
        auto SetScissor(Int32 x, Int32 y, Int32 width, Int32 height) -> void;

        auto CopyToCube(std::string_view texture2DName, std::string_view cubeMapName, Size mipLevel, UInt32 face ) -> void;

        auto BindGlobalTextures() -> void;

        // Need to bind pipeline before specifying resources
        auto BindPipeline(std::string_view pipelineName ) -> void;

        auto SetClearColor(const Vec4F& color) -> void;

        auto SetPolygonLineWidth(float value) -> void;

        auto DrawIndexed(const DrawIndexedState& info) -> void;
        auto Draw(UInt32 vertexCount, UInt32 instanceCount = 1, UInt32 firstVertex = 0, UInt32 firstInstance = 0 ) -> void;
        auto Dispatch(UInt32 invX, UInt32 invY, UInt32 invZ) -> void;

        auto PushConstants(const void* ptr, Size size) -> void;

        auto UploadBufferData(std::string_view bufferName, const void* buffer, Size elementSize, Size elementCount) const -> void;

        template<typename T>
        auto UploadBuffer(std::string_view bufferName, const void* ptrSrc ) const -> void {
            UploadBuffer( bufferName, ptrSrc, sizeof(T));
        }

        template<typename T>
        auto UploadBuffer(std::string_view bufferName, T& ref ) const -> void {
            UploadBuffer( bufferName, std::addressof( ref ), sizeof(T));
        }

        auto UploadBuffer(std::string_view bufferName, const void* ptrSrc, Size size, Size offset = 0 ) const -> void;

        MKT_NODISCARD auto PushTexture(TextureHandle texture ) const -> Int32;
        MKT_NODISCARD auto GetNamedBuffer( std::string_view ) const -> BufferHandle;

        auto BindImage(TextureHandle handle, SamplerHandle sampler, UInt32 bindingSlot) -> void;
        auto BindImage(std::string_view name, SamplerHandle sampler, UInt32 bindingSlot) -> void;

        auto CreateSampler( SamplerDescription samplerDescription ) -> SamplerHandle;

     private:
        CommandListHandle m_Commands{};

        GraphicsContext* m_Context{};
        PipelineHandle m_Pipeline{};

        FramePassNode* m_ActivePass{ nullptr };

        RenderInfo m_RenderInfo{};

        bool m_HasSetConstantData{ false };
    };
}

#endif // MIKOTO_COMMAND_CONTEXT_HH