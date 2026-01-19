//
// Created by kate on 11/23/25.
//

#include <Renderer/D3D11/D3D11Device.hh>

namespace Mikoto {

    D3D11Device::D3D11Device( const GpuDeviceCreateInfo &createInfo )
        : GpuDevice{ createInfo.Api }
    {}

    auto D3D11Device::Init() -> void {}

    auto D3D11Device::Shutdown() -> void {}

    auto D3D11Device::CreateTexture( const TextureDescription &description ) -> TextureHandle {
        return TextureHandle::CreateEmpty();
    }

    auto D3D11Device::CreateTexture( const TextureCubeCreateDescription &description ) -> TextureHandle {
        return TextureHandle::CreateEmpty();
    }

    auto D3D11Device::CreateBuffer( const BufferDescription &description ) -> BufferHandle {
        return TextureHandle::CreateEmpty();
    }

    auto D3D11Device::CreateFrameBuffer( const FramebufferDescription &description ) -> FramebufferHandle {
        return FramebufferHandle::CreateEmpty();
    }

    auto D3D11Device::CreateSampler( const SamplerDescription &description ) -> SamplerHandle {
        return SamplerHandle::CreateEmpty();
    }

    auto D3D11Device::CreatePipeline( const ComputePipelineDescription &description ) -> PipelineHandle {
        return PipelineHandle::CreateEmpty();
    }

    auto D3D11Device::CreatePipeline( const GraphicsPipelineDescription &description ) -> PipelineHandle {
        return PipelineHandle::CreateEmpty();
    }

    auto D3D11Device::CreateCommandList( QueueType queue ) -> CommandListHandle {
        return CommandListHandle::CreateEmpty();
    }

    auto D3D11Device::LoadShader( const Path &path, ShaderStage stage ) -> ShaderModuleHandle {
        return ShaderModuleHandle::CreateEmpty();
    }

    auto D3D11Device::GetDummySampler() const -> SamplerHandle {
        return SamplerHandle::CreateEmpty();
    }

    auto D3D11Device::GetDeviceName() const -> std::string_view {
        return std::string_view{ "D3D11Device" };
    }

    Object D3D11Device::GetNativeHandle( ObjectType object ) { return Object(m_Device.GetAddressOf()); }

    auto D3D11Device::GetMemoryUsage() const -> Size {
        return Size{ 0 };
    }

    auto D3D11Device::GetMemoryTotal() const -> Size {
        return Size{ 0 };
    }

    auto D3D11Device::GetMemoryAvailable() const -> Size {
        return Size{ 0 };
    }

    auto D3D11Device::RunGarbageCollection() -> void {

    }

    auto D3D11Device::SubmitCommands( CommandListHandle cmd ) -> void {

    }
}
