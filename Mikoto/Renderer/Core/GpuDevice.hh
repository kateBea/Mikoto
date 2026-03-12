//    Copyright 2026 ケイト
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

#ifndef MIKOTO_GPU_DEVICE_HH
#define MIKOTO_GPU_DEVICE_HH

#include <Assets/Texture.hh>
#include <Library/Utility/Types.hh>

#include <Material/Texture2D.hh>
#include <Material/TextureCube.hh>
#include <Material/ShaderModule.hh>

#include <Renderer/Core/Buffer.hh>
#include <Renderer/Core/RenderUtility.hh>

#include <Renderer/Core/Pipeline.hh>
#include <Renderer/Core/Framebuffer.hh>
#include <Renderer/Core/DeviceObject.hh>
#include <Renderer/Core/DeviceObjectHandle.hh>

namespace Mikoto {

    struct RenderInfo {
        LoadOp ColorLoadOp{ LoadOp::CLEAR };
        LoadOp DephtLoadOp{ LoadOp::CLEAR };

        TextureHandle DepthRenderTarget{};
        Vec4F ClearColor{ 0.0f, 0.0f, 0.0f, 1.0f };
        std::vector<TextureHandle> ColorRenderTargets{};

        auto Clear() -> void;
    };

    class ICommandList : public DeviceObject {
    public:
        explicit ICommandList( const bool immediate, QueueType queueType )
            : m_IsImmediate{ immediate }, m_QueueType{ queueType } {}

        virtual auto Begin() -> void = 0;
        virtual auto End() -> void = 0;

        virtual auto BeginRender(RenderInfo& info) -> void = 0;
        virtual auto EndRender(RenderInfo& info) -> void = 0;

        // Consider passing the handle instead, I am not sure if they will stay alive
        virtual auto FillTexture(Buffer* src, Texture* dest) -> void = 0;
        virtual auto FillTexture(const void* src, Size size, Texture* dest) -> void = 0;

        virtual auto CopyBuffer(Buffer* src, Buffer* dest) -> void = 0;
        virtual auto CopyBuffer(const void* src, Size size, Buffer* dest) -> void = 0;

        virtual auto CopyBuffer( Buffer* src, Buffer* dest, Size dstOffset ) -> void = 0;

        virtual auto CopyTexture(Texture* src, Texture* dest) -> void = 0;
        virtual auto CopyTexture(Texture2D* src, TextureCube* dest, UInt32 mipLevel, UInt32 face) -> void = 0;

        virtual auto SetPolygonLineWidth(float value) -> void = 0;

        // Can be device local data
        virtual auto WriteBuffer(Buffer* target, Byte* data, Size size) -> void = 0;
        virtual auto WriteTexture(Texture* target, Byte* data, Size size) -> void = 0;

        virtual auto SetViewport(Int32 x, Int32 y, Int32 width, Int32 height) -> void = 0;
        virtual auto SetViewport( Int32 x, Int32 y, Int32 width, Int32 height, bool flip ) -> void = 0;

        virtual auto SetScissor(Int32 x, Int32 y, Int32 width, Int32 height) -> void = 0;

        virtual auto BindIndexBuffer( BufferHandle indexBuffer)-> void = 0;
        virtual auto BindVertexBuffer( BufferHandle vertexBuffer, UInt32 binding) -> void = 0;

        virtual auto Draw(UInt32 vertexCount, UInt32 instanceCount, UInt32 firstVertex, UInt32 firstInstance) -> void = 0;
        virtual auto DrawIndexed( Size indexCount, UInt32 instanceCount, UInt32 firstIndex, UInt32 vertexOffset, UInt32 firstInstance)-> void = 0;
        virtual auto DrawIndexedIndirect( BufferHandle indexBuffer, UInt32 offset, UInt32 drawCount, UInt32 stride ) -> void = 0;

        virtual auto Dispatch(UInt32 x, UInt32 y, UInt32 z) -> void = 0;

        virtual auto BindPipeline(PipelineHandle pipeline) -> void = 0;

        MKT_NODISCARD auto IsImmediate() -> bool { return m_IsImmediate; }
        MKT_NODISCARD auto GetQueueType() -> QueueType { return m_QueueType; }

        using DeviceObject::Initialize;

    protected:
        bool m_IsImmediate{ false };

        QueueType m_QueueType{ QueueType::INVALID_QUEUE };
    };

    class IQueue {
    public:

        MKT_NODISCARD auto GetType() const -> QueueType { return m_Type; }

    private:
        QueueType m_Type{ QueueType::INVALID_QUEUE };
    };

    struct GpuDeviceCreateInfo {
        GraphicsAPI Api{ GraphicsAPI::VULKAN_API };

        bool EnablePresentation{ true };
    };

    class GpuDevice {
    public:
        virtual auto Init() -> void = 0;
        virtual auto Shutdown() -> void = 0;

        MKT_NODISCARD virtual auto CreateTexture(const TextureDescription& description) -> TextureHandle = 0;
        MKT_NODISCARD virtual auto CreateTexture(const TextureCubeCreateDescription& description) -> TextureHandle = 0;
        MKT_NODISCARD virtual auto CreateBuffer(const BufferDescription& description) -> BufferHandle = 0;
        MKT_NODISCARD virtual auto CreateFrameBuffer(const FramebufferDescription& description) -> FramebufferHandle = 0;
        MKT_NODISCARD virtual auto CreateSampler(const SamplerDescription& description) -> SamplerHandle = 0;

        MKT_NODISCARD virtual auto CreatePipeline(const ComputePipelineDescription& description) -> PipelineHandle = 0;
        MKT_NODISCARD virtual auto CreatePipeline(const GraphicsPipelineDescription& description) -> PipelineHandle = 0;

        MKT_NODISCARD virtual auto LoadShader(const Path& path, ShaderStage stage) -> ShaderModuleHandle = 0;

        MKT_NODISCARD virtual auto GetDeviceName() const -> std::string_view = 0;

        virtual auto SubmitCommands(CommandListHandle cmd) -> void = 0;
        MKT_NODISCARD virtual auto CreateCommandList(QueueType queue, bool immediate) -> CommandListHandle = 0;

        MKT_NODISCARD virtual auto GetNativeHandle( ObjectType ) -> Object { return Object(nullptr); }

        MKT_NODISCARD virtual auto GetMemoryUsage() const -> Size = 0;
        MKT_NODISCARD virtual auto GetMemoryTotal() const -> Size = 0;
        MKT_NODISCARD virtual auto GetMemoryAvailable() const -> Size = 0;

        MKT_NODISCARD virtual auto GetDummySampler() const -> SamplerHandle = 0;

        virtual auto RunGarbageCollection() -> void = 0;

        MKT_NODISCARD auto GetApi() const -> GraphicsAPI { return m_Api; }
        MKT_NODISCARD auto IsInitialized() const -> bool { return m_IsInitialized; }

        virtual ~GpuDevice() = default;

        MKT_NODISCARD static auto Create(const GpuDeviceCreateInfo& createInfo) -> Unique<GpuDevice>;

    protected:
        explicit GpuDevice(GraphicsAPI api);

    protected:
        GraphicsAPI m_Api{};

        bool m_IsInitialized{ false };
    };
}



#endif //MIKOTO_GPU_DEVICE_HH
