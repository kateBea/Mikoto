//
// Created by kate on 11/23/25.
//

#ifndef MIKOTO_D3D11DEVICE_HH
#define MIKOTO_D3D11DEVICE_HH

#include <Common/Common.hh>
#include <Core//Platform.hh>
#include <Library/Utility/Types.hh>
#include <Renderer/Core/GpuDevice.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

#include <d3d11.h>
#include <dxgi1_3.h>
#include <wrl.h>

#include <Renderer/D3D11/Direct3D11Libraries.hh>

namespace Mikoto {

    class D3D11Device final : public GpuDevice {
    public:
        explicit D3D11Device( const GpuDeviceCreateInfo& createInfo );

        auto Init() -> void override;
        auto Shutdown() -> void override;

        MKT_NODISCARD auto CreateTexture( const TextureDescription &description ) -> TextureHandle override;
        MKT_NODISCARD auto CreateTexture( const TextureCubeCreateDescription &description ) -> TextureHandle override;
        MKT_NODISCARD auto CreateBuffer( const BufferDescription &description ) -> BufferHandle override;
        MKT_NODISCARD auto CreateFrameBuffer( const FramebufferDescription &description ) -> FramebufferHandle override;
        MKT_NODISCARD auto CreateSampler( const SamplerDescription &description ) -> SamplerHandle override;
        MKT_NODISCARD auto CreatePipeline( const ComputePipelineDescription &description ) -> PipelineHandle override;
        MKT_NODISCARD auto CreatePipeline( const GraphicsPipelineDescription &description ) -> PipelineHandle override;

        MKT_NODISCARD auto CreateCommandList( QueueType queue, bool immediate ) -> CommandListHandle override;

        MKT_NODISCARD auto LoadShader( const Path &path, ShaderStage stage ) -> ShaderModuleHandle override;

        MKT_NODISCARD auto GetDummySampler() const -> SamplerHandle override;
        MKT_NODISCARD auto GetDeviceName() const -> std::string_view override;
        MKT_NODISCARD auto GetNativeHandle( ObjectType ) -> Object override;

        MKT_NODISCARD auto GetMemoryUsage() const -> Size override;
        MKT_NODISCARD auto GetMemoryTotal() const -> Size override;
        MKT_NODISCARD auto GetMemoryAvailable() const -> Size override;

        auto RunGarbageCollection() -> void override;
        auto SubmitCommands( CommandListHandle cmd ) -> void override;

        ~D3D11Device() override = default;

    private:
        Microsoft::WRL::ComPtr<ID3D11Device> m_Device{};
    };
}

#endif

#endif//MIKOTO_D3D11DEVICE_HH
