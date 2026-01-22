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

#include <Renderer/D3D11/D3D11Device.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

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

    auto D3D11Device::CreateCommandList( QueueType queue, bool immediate ) -> CommandListHandle {
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

#endif
