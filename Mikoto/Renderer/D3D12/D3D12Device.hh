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

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>

#include <Filesystem/Path.hh>
#include <Renderer/Core/GpuDevice.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

#include <d3d12.h>
#include <dxgi.h>
#include <wrl.h>

namespace mikoto::renderer::d3d12 {

    class BindingLayout final : IBindingLayout {
    public:

        MKT_NODISCARD auto IsBindless() const -> bool override;
        MKT_NODISCARD auto GetRegisterSpace() const -> u32 override;

        ~BindingLayout() override;

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;
    };

    class BindingSet : public IBindingSet {
    public:

    protected:
        auto Initialize() -> void override;
        auto Release() -> void override;
    };

    class DescriptorTable : public IDescriptorTable {
    public:
        MKT_NODISCARD auto GetCapacity() const -> u32 override;
    };

    class InputLayout : public IInputLayout {
    public:
        explicit InputLayout( eastl::span<const VertexAttributeDescription> desc );

        MKT_NODISCARD auto GetNumAttributes() const -> u32 override;
        MKT_NODISCARD auto GetAttributeDescription(u32 index) const -> const VertexAttributeDescription& override;

        ~InputLayout() override;

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

    private:
        eastl::fixed_hash_map<u32, VertexAttributeDescription, kMaxVertexAttributes> mAttributes{};
    };

    class PipelineLayout : public IPipelineLayout {
    public:

        MKT_NODISCARD auto GetBindPoint() const -> PipelineType override;

    protected:
        auto Initialize() -> void override;
        auto Release() -> void override;
    };

    class Device final : public GpuDevice {
    public:
        explicit Device( const GpuDeviceCreateInfo& createInfo );

        auto Init() -> void override;
        auto Shutdown() -> void override;

        MKT_NODISCARD auto CreateTexture( const TextureCreateDescription& description ) -> TextureHandle override;
        MKT_NODISCARD auto CreateTextureNative( ObjectType type, Object object, const TextureCreateDescription& description ) -> TextureHandle override;

        MKT_NODISCARD auto CreateFrameBuffer(const FramebufferDescription& description) -> FramebufferHandle override;

        MKT_NODISCARD auto CreateBuffer( const BufferCreateDescription& description ) -> BufferHandle override;

        MKT_NODISCARD auto CreateSampler( const SamplerCreateDescription& description ) -> SamplerHandle override;

        MKT_NODISCARD auto CreatePipeline( const ComputePipelineDescription& description ) -> PipelineHandle override;
        MKT_NODISCARD auto CreatePipeline( const GraphicsPipelineDescription& description ) -> PipelineHandle override;

        MKT_NODISCARD auto CreateAccelStructure( const AccelStructureCreateDescription& description ) -> AccelStructureHandle override;

        MKT_NODISCARD auto CreateCommandList( QueueType queue ) -> CommandListHandle override;
        MKT_NODISCARD auto CreateCommandList( const CommandListParameters& parameters ) -> CommandListHandle override;

        MKT_NODISCARD auto CreateShader( const ShaderModuleCreateDescription& desc ) -> ShaderModuleHandle override;
        MKT_NODISCARD auto CreateShader( ShaderStage type, const void* code, size_t codeSize  ) -> ShaderModuleHandle override;

        MKT_NODISCARD auto CreateInputLayout(const InputLayoutCreateDescription& desc) -> InputLayoutHandle override;

        MKT_NODISCARD auto CreateBindingLayout( const BindingLayoutDescription& desc ) -> BindingLayoutHandle override;
        MKT_NODISCARD auto CreatePipelineLayout( const PipelineLayoutCreateDescription& desc ) -> PipelineLayoutHandle override;
        MKT_NODISCARD auto CreateBindingSet( const BindingSetDescription& desc, BindingLayoutHandle layout ) -> BindingSetHandle override;

        auto UnMap( IBuffer* buffer ) -> void override;
        MKT_NODISCARD auto Map(IBuffer* buffer) -> const void* override;

        // To support bindless techniques in modern graphics APIs
        MKT_NODISCARD auto CreateBindlessLayout( const BindlessLayoutDescription& desc ) -> BindingLayoutHandle override;

        MKT_NODISCARD auto CreateDescriptorTable( BindingLayoutHandle layout ) -> DescriptorTableHandle override;
        MKT_NODISCARD auto ResizeDescriptorTable( DescriptorTableHandle descriptorTable, u32 newSize, bool keepContents ) -> bool override;
        MKT_NODISCARD auto WriteDescriptorTable( DescriptorTableHandle descriptorTable, const BindingSetItem& item ) -> bool override;

        auto Flush() -> void override;
        auto RunGarbageCollection() -> void override;
        auto SubmitCommands( CommandListHandle cmdList ) -> void override;
        auto ExecuteCommands( CommandListHandle cmdList ) -> void override;

        auto WaitIdle() -> void override;

        ~Device() override = default;

    private:
        Microsoft::WRL::ComPtr<ID3D12Device2> mDevice{};
        Microsoft::WRL::ComPtr<IDXGIAdapter1> mAdapter{};

#if defined(_DEBUG)
        Microsoft::WRL::ComPtr<ID3D12Debug1> mDebugController{};
        Microsoft::WRL::ComPtr<ID3D12DebugDevice> mDebugDevice{};
#endif
    };
}

#endif

#endif// MIKOTO_D3D12DEVICE_HH
