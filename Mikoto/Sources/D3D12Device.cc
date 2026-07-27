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

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>

#include <Renderer/D3D12/D3D12Context.hh>
#include <Renderer/D3D12/D3D12Device.hh>
#include <Renderer/D3D12/Direct3D12Helpers.hh>

#include <Renderer/Core/RenderSystem.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

#include <Platform/PlatformWin32.hh>

// D3D12 extension library.
#include <directx/d3d12.h>

#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

namespace mikoto::renderer::d3d12 {

    auto BindingLayout::IsBindless() const -> bool {
        return false;
    }

    auto BindingLayout::GetRegisterSpace() const -> u32 {
        return 0; // TODO
    }

    BindingLayout::~BindingLayout() {
        if (mIsAllocated) {
            Release();
        }
    }

    auto BindingLayout::Initialize() -> void {
        mIsAllocated = true;
    }

    auto BindingLayout::Release() -> void {
        mIsAllocated = false;
    }

    auto CommandList::BindIndirectBuffer( IBuffer *buffer ) -> void {

    }

    Device::Device( const GpuDeviceCreateInfo &createInfo )
        : GpuDevice{ createInfo.mApi, createInfo.mFeaturesSupport }
    {}

    auto Device::Init() -> void {
        IDXGIFactory4* factory{ checked_cast<Context*>( RenderSystem::Get()->GetContext() )->GetDxiFactory() };
        MKT_ASSERT( factory, "A valid DirectX factory interface is required to create the device." );

        for (UINT adapterIndex{}; DXGI_ERROR_NOT_FOUND != factory->EnumAdapters1(adapterIndex, &mAdapter); ++adapterIndex) {
            DXGI_ADAPTER_DESC1 desc{};
            mAdapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
                // Don't select the Basic Render Driver adapter.
                continue;
            }

            // Check to see if the adapter supports Direct3D 12, but don't create the actual device yet.
            if ( SUCCEEDED( D3D12CreateDevice( mAdapter.Get(), D3D_FEATURE_LEVEL_12_0, _uuidof( ID3D12Device ), nullptr ) ) ) {
                break;
            }
        }

        ThrowIfFailed(D3D12CreateDevice( mAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&mDevice)));
        mDevice->SetName(L"Hello Triangle Device");

#if defined(_DEBUG)
        ThrowIfFailed(mDevice->QueryInterface( IID_PPV_ARGS(&mDebugDevice) ));
#endif
    }

    auto Device::Shutdown() -> void {

    }

    auto Device::CreateTexture( const TextureCreateDescription &description ) -> TextureHandle {
        return TextureHandle::CreateEmpty();
    }

    auto Device::CreateTextureNative( ObjectType type, Object object, const TextureCreateDescription &description ) -> TextureHandle {
        return TextureHandle::CreateEmpty();
    }

    auto Device::CreateFrameBuffer( const FramebufferDescription &description ) -> FramebufferHandle {
        return FramebufferHandle::CreateEmpty();
    }

    auto Device::CreateSampler( const SamplerCreateDescription &description ) -> SamplerHandle {
        return SamplerHandle::CreateEmpty();
    }

    auto Device::CreateAccelStructure( const AccelStructureCreateDescription &description ) -> AccelStructureHandle {
        return AccelStructureHandle::CreateEmpty();
    }

    auto Device::CreateCommandList( QueueType queue ) -> CommandListHandle {
        return CommandListHandle::CreateEmpty();
    }

    auto Device::CreateCommandList( const CommandListParameters &parameters ) -> CommandListHandle {
        return CommandListHandle::CreateEmpty();
    }

    auto Device::CreateShader( const ShaderModuleCreateDescription &desc ) -> ShaderModuleHandle {
        return ShaderModuleHandle::CreateEmpty();
    }

    auto Device::CreateShader( ShaderStage type, const void *code, size_t codeSize ) -> ShaderModuleHandle {
        return ShaderModuleHandle::CreateEmpty();
    }

    auto Device::CreateInputLayout( const InputLayoutCreateDescription& desc ) -> InputLayoutHandle {
        return InputLayoutHandle::CreateEmpty();
    }

    auto Device::CreateBindingLayout( const BindingLayoutDescription &desc ) -> BindingLayoutHandle {
        return BindingLayoutHandle::CreateEmpty();
    }

    auto Device::CreatePipelineLayout( const PipelineLayoutCreateDescription& desc ) -> PipelineLayoutHandle {
        return PipelineLayoutHandle::CreateEmpty();
    }

    auto Device::CreateBindingSet( const BindingSetDescription &desc, BindingLayoutHandle layout ) -> BindingSetHandle {
        return BindingSetHandle::CreateEmpty();
    }

    auto Device::CreateFence( u64 fenceInitialValue ) -> FenceHandle {
        return FenceHandle::CreateEmpty();
    }

    auto Device::UnMap( IBuffer *buffer ) -> void {
    }

    auto Device::Map( IBuffer *buffer ) -> const void * {
        return nullptr;
    }

    auto Device::CreateBindlessLayout( const BindlessLayoutDescription &desc ) -> BindingLayoutHandle {
        return BindingLayoutHandle::CreateEmpty();
    }

    auto Device::CreateDescriptorTable( BindingLayoutHandle layout ) -> DescriptorTableHandle {
        return DescriptorTableHandle::CreateEmpty();
    }

    auto Device::ResizeDescriptorTable( DescriptorTableHandle descriptorTable, u32 newSize, bool keepContents ) -> bool {
        return false;
    }

    auto Device::WriteDescriptorTable( DescriptorTableHandle descriptorTable, const BindingSetItem &item ) -> bool {
        return false;
    }

    auto Device::Wait( QueueType type, FenceHandle handle, u64 fenceValue ) -> void {

    }

    auto Device::Signal( QueueType type, FenceHandle handle, u64 fenceValue ) -> void {

    }

    auto Device::ExecutePendingCommands() -> void {

    }

    auto Device::ExecuteCommands( CommandListHandle cmdList ) -> void {

    }

    auto Device::ExecuteCommands( eastl::span<CommandListHandle> cmdList ) -> void {

    }

    auto Device::WaitIdle() -> void {

    }

    auto Device::CreateBuffer( const BufferCreateDescription &description ) -> BufferHandle {
        return BufferHandle::CreateEmpty();
    }

    auto Device::CreatePipeline( const ComputePipelineDescription &description ) -> PipelineHandle {
        return PipelineHandle::CreateEmpty();
    }

    auto Device::CreatePipeline( const GraphicsPipelineDescription &description ) -> PipelineHandle {
        return PipelineHandle::CreateEmpty();
    }

    auto Device::RunGarbageCollection() -> void {

    }

    auto Device::SubmitCommands( CommandListHandle cmd ) -> u64 {
        return 0;
    }
}

#endif
