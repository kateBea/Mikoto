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

#ifndef MIKOTO_D3D12DEVICE_HH
#define MIKOTO_D3D12DEVICE_HH

#include <Common/Common.hh>
#include <Core//Platform.hh>
#include <Library/Utility/Types.hh>
#include <Renderer/Core/GpuDevice.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

#include <d3d12.h>
#include <wrl.h>

namespace Mikoto {

    class D3D12Device final : public GpuDevice {
    public:
        explicit D3D12Device( const GpuDeviceCreateInfo& createInfo );

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

        ~D3D12Device() override = default;

    private:
        Microsoft::WRL::ComPtr<ID3D12Device> m_Device{};
    };
}

#endif

#endif// MIKOTO_D3D12DEVICE_HH
