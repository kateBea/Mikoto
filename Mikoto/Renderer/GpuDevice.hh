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
