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

#include <Core/Platform.hh>
#include <Renderer/D3D12/D3D12Device.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

namespace Mikoto {

    D3D12Device::D3D12Device( const GpuDeviceCreateInfo &createInfo )
        : GpuDevice{ createInfo.Api }
    {}

    auto D3D12Device::Init() -> void {}

    auto D3D12Device::Shutdown() -> void {}

    auto D3D12Device::CreateTexture( const TextureDescription &description ) -> TextureHandle {
        return TextureHandle::CreateEmpty();
    }

    auto D3D12Device::CreateTexture( const TextureCubeCreateDescription &description ) -> TextureHandle {
        return TextureHandle::CreateEmpty();
    }

    auto D3D12Device::CreateBuffer( const BufferDescription &description ) -> BufferHandle {
        return TextureHandle::CreateEmpty();
    }

    auto D3D12Device::CreateFrameBuffer( const FramebufferDescription &description ) -> FramebufferHandle {
        return FramebufferHandle::CreateEmpty();
    }

    auto D3D12Device::CreateSampler( const SamplerDescription &description ) -> SamplerHandle {
        return SamplerHandle::CreateEmpty();
    }

    auto D3D12Device::CreatePipeline( const ComputePipelineDescription &description ) -> PipelineHandle {
        return PipelineHandle::CreateEmpty();
    }

    auto D3D12Device::CreatePipeline( const GraphicsPipelineDescription &description ) -> PipelineHandle {
        return PipelineHandle::CreateEmpty();
    }

    auto D3D12Device::CreateCommandList( QueueType queue, bool immediate ) -> CommandListHandle {
        return CommandListHandle::CreateEmpty();
    }

    auto D3D12Device::LoadShader( const Path &path, ShaderStage stage ) -> ShaderModuleHandle {
        return ShaderModuleHandle::CreateEmpty();
    }

    auto D3D12Device::GetDummySampler() const -> SamplerHandle {
        return SamplerHandle::CreateEmpty();
    }

    auto D3D12Device::GetDeviceName() const -> std::string_view {
        return std::string_view{ "D3D12Device" };
    }

    auto D3D12Device::GetNativeHandle( ObjectType object ) -> Object {
        return Object(m_Device.GetAddressOf());
    }

    auto D3D12Device::GetMemoryUsage() const -> Size {
        return Size{ 0 };
    }

    auto D3D12Device::GetMemoryTotal() const -> Size {
        return Size{ 0 };
    }

    auto D3D12Device::GetMemoryAvailable() const -> Size {
        return Size{ 0 };
    }

    auto D3D12Device::RunGarbageCollection() -> void {

    }

    auto D3D12Device::SubmitCommands( CommandListHandle cmd ) -> void {

    }
}

#endif
