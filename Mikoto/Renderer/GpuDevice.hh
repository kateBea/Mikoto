//
// Created by zanet on 3/25/2025.
//

#ifndef GPUDEVICE_HH
#define GPUDEVICE_HH

#include <Library/Utility/Types.hh>
#include <Renderer/RenderUtility.hh>
#include <Renderer/GpuUtility.hh>
#include <Assets/Texture.hh>
#include <Renderer/Buffer.hh>

namespace Mikoto {

    class ICommandList : public DeviceObject {
    public:
        explicit ICommandList( const QueueType queue = QueueType::INVALID_QUEUE )
            : m_Type{ queue }
        {  }

        virtual auto Begin() -> void = 0;
        virtual auto Close() -> void = 0;

        virtual auto FillTexture(Buffer* src, Texture* dest) -> void = 0;
        virtual auto CopyTexture(Buffer* src, Buffer* dest) -> void = 0;
        virtual auto CopyBuffer(Texture* src, Texture* dest) -> void = 0;

        virtual auto WriteBuffer(Buffer* target, Byte* data, Size size) -> void = 0;
        virtual auto WriteTexture(Texture* target, Byte* data, Size size) -> void = 0;

    private:
        QueueType m_Type{ QueueType::INVALID_QUEUE };
    };

    class IQueue : public DeviceObject {
    public:

        MKT_NODISCARD auto GetType() const -> QueueType { return m_Type; }

    private:
        QueueType m_Type{ QueueType::INVALID_QUEUE };
    };

    using CommandListHandle = Ref<ICommandList>;

    struct GpuDeviceCreateInfo {
        GraphicsAPI Api{ GraphicsAPI::VULKAN_API };

        bool EnablePresentation{ true };
    };

    class GpuDevice {
    public:
        virtual auto Init() -> void = 0;
        virtual auto Shutdown() -> void = 0;

        MKT_NODISCARD virtual auto CreateTexture(const TextureDescription& description) -> TextureHandle = 0;
        MKT_NODISCARD virtual auto CreateBuffer(const BufferDescription& description) -> BufferHandle = 0;

        virtual auto SubmitCommands(CommandListHandle cmd) -> void = 0;
        MKT_NODISCARD virtual auto CreateCommandList(QueueType queue = QueueType::GRAPHICS_QUEUE) -> CommandListHandle = 0;

        virtual auto RunGarbageCollection() -> void = 0;

        virtual ~GpuDevice() = default;

        MKT_NODISCARD static auto Create(const GpuDeviceCreateInfo& createInfo) -> Unique<GpuDevice>;

    protected:
        explicit GpuDevice(GraphicsAPI api);

    protected:
        GraphicsAPI m_Api{};

        bool m_IsInitialized{ false };
    };
}



#endif //GPUDEVICE_HH
