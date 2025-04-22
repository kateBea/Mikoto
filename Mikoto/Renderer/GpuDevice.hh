//
// Created by zanet on 3/25/2025.
//

#ifndef GPUDEVICE_HH
#define GPUDEVICE_HH

#include <Assets/Texture.hh>
#include <Library/Utility/Types.hh>
#include <Renderer/Buffer.hh>
#include <Renderer/GpuUtility.hh>
#include <Renderer/RenderUtility.hh>

namespace Mikoto {
    class ICommandList;

    class ShaderModule;
    class ComputePipeline;
    class Sampler;
    class Framebuffer;
    class Font;

    class ComputePipeline;
    class GraphicsPipeline;

    struct ComputePipelineDescription;
    struct GraphicsPipelineDescription;
    struct SamplerDescription;
    struct FontLoadDescription;
    struct FramebufferDescription;

    using TextureHandle = Ref<Texture>;
    using BufferHandle = Ref<Buffer>;
    using GraphicsPipelineHandle = Ref<GraphicsPipeline>;
    using ComputePipelineHandle = Ref<ComputePipeline>;
    using ShaderModuleHandle = Ref<ShaderModule>;
    using SamplerHandle = Ref<Sampler>;
    using FramebufferHandle = Ref<Framebuffer>;

    using CommandListHandle = Ref<ICommandList>;

    struct GpuDeviceCreateInfo {
        GraphicsAPI Api{ GraphicsAPI::VULKAN_API };

        auto WithGraphicsAPI( GraphicsAPI api ) -> GpuDeviceCreateInfo&;
    };

    enum class QueueType {
        QUEUE_COMPUTE,
        QUEUE_GRAPHICS,
        QUEUE_TRANSFER,
    };

    struct CommandListCreateDescription {
        bool ImmediateSubmit{ false };
    };

    struct DrawDescription {
        uint32_t vertexCount = 0;
        uint32_t instanceCount = 1;
        uint32_t startIndexLocation = 0;
        uint32_t startVertexLocation = 0;
        uint32_t startInstanceLocation = 0;
    };

    struct ComputeState {
        // Pipeline, etc
        ComputePipelineHandle Pipeline{};
    };

    struct GraphicsState {
        // Pipeline, etc
        GraphicsPipelineHandle Pipeline{};
    };

    class ICommandList : public DeviceObject {
    public:

        virtual auto Begin() -> void = 0;
        virtual auto Close() -> void = 0;

        // Commands
        virtual auto CopyBuffer(Texture* src, Texture* dest) -> void = 0;
        virtual auto CopyTexture(Buffer* src, Buffer* dest) -> void = 0;

        virtual auto WriteBuffer(Buffer* target, Byte_T* data, Size_T size) -> void = 0;
        virtual auto WriteTexture(Texture* target, Byte_T* data, Size_T size) -> void = 0;

        virtual void SetGraphicsState(const GraphicsState& state) = 0;
        virtual auto Draw(const DrawDescription& desc) -> void;
        virtual auto DrawIndexed(const DrawDescription& desc) -> void;

        virtual void SetComputeState(const ComputeState& state) = 0;
        virtual auto Dispatch(UInt32_T groupsX, UInt32_T groupsY = 1, UInt32_T groupsZ = 1) = 0;

    };

    class GpuDevice {
    public:
        virtual auto Init() -> void = 0;
        virtual auto Shutdown() -> void = 0;

        virtual MKT_NODISCARD auto CreateTexture(const TextureDescription& description) -> TextureHandle = 0;
        virtual MKT_NODISCARD auto CreateBuffer(const BufferDescription& description) -> BufferHandle = 0;
        virtual MKT_NODISCARD auto CreateGraphicsPipeline(const GraphicsPipelineDescription& description) -> GraphicsPipelineHandle = 0;
        virtual MKT_NODISCARD auto CreateComputePipeline(const ComputePipelineDescription& description) -> ComputePipelineHandle = 0;
        virtual MKT_NODISCARD auto CreateShaderModule(const ShaderModuleDescription& description) -> ShaderModuleHandle = 0;
        virtual MKT_NODISCARD auto CreateSampler(const SamplerDescription& description) -> SamplerHandle = 0;
        virtual MKT_NODISCARD auto CreateFramebuffer(const FramebufferDescription& description) -> FramebufferHandle = 0;

        virtual MKT_NODISCARD auto CreateImguiTextureHandle(TextureHandle texture) -> RefAny = 0;

        virtual auto SetBarrier(TextureHandle texture) -> void = 0;
        virtual auto SetBarrier(BufferHandle buffer) -> void = 0;

        virtual auto CreateCommandList(const CommandListCreateDescription& params = {}) -> CommandListHandle = 0;
        virtual auto SubmitCommandList(CommandListHandle commandList, QueueType queueType = QueueType::QUEUE_GRAPHICS) -> void = 0;

        template<typename ResourceType>
        auto GetDummyResource() -> decltype(auto) {
            const Size_T id{ typeid(ResourceType).hash_code() };
            return AccessDummyResource(id).As<ResourceType>();
        }

        // Call once per frame
        virtual auto RunGarbageCollection() -> void = 0;

        virtual ~GpuDevice() = default;

        MKT_NODISCARD static auto Create(const GpuDeviceCreateInfo& createInfo) -> Scope_T<GpuDevice>;

    protected:
        explicit GpuDevice(GraphicsAPI api);

        virtual auto AccessDummyResource(Size_T resourceTypeID) -> Ref<IResource> = 0;

    protected:
        GraphicsAPI m_Api{};
    };
}



#endif //GPUDEVICE_HH
