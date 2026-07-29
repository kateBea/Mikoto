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
    Queue::Queue( QueueType type, QueueOpSupportFlags flags )
        : IQueue{ type, flags }
    {

    }

    auto Queue::Wait( IFence *fence, u64 value ) -> void {

    }

    auto Queue::Signal( IFence *fence, u64 value ) -> void {

    }

    auto Queue::ExecuteCommandLists( eastl::span<CommandListHandle> commands ) -> void {

    }

    auto Queue::Flush() -> void {

    }

    auto Queue::RunGarbageCollection() -> void {

    }

    auto Queue::ExecuteCommandList( CommandListHandle cmd ) -> void {

    }

    auto Queue::SubmitCommandList( CommandListHandle cmd ) -> u64 {
        return 0;
    }

    auto Queue::AllocateCmdList() -> CommandListHandle {
        return CommandListHandle::CreateEmpty();
    }

    auto Queue::AllocateCmdList( const CommandListParameters &params ) -> CommandListHandle {
        return CommandListHandle::CreateEmpty();
    }

    auto Queue::WaitIdle() const -> void {

    }

    Queue::operator ID3D12CommandQueue*() const {
        return mQueue.Get();
    }

    Queue::operator ID3D12CommandAllocator *() const {
        return mAllocator.Get();
    }

    Queue::~Queue() {
        if (mIsAllocated) {
            Release();
        }
    }

    auto Queue::Release() -> void {
        mIsAllocated = false;
    }

    auto Queue::Initialize() -> void {
        Device* device{ checked_cast<Device*>( mDevice ) };

        const D3D12_COMMAND_LIST_TYPE cmdQueueType{ d3d12::GetQueueType(mType) };

        D3D12_COMMAND_QUEUE_DESC desc{};
        desc.Type =     cmdQueueType;
        desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        desc.Flags =    D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.NodeMask = 0;

        // Command queue
        ThrowIfFailed(device->GetDevice()->CreateCommandQueue(&desc, IID_PPV_ARGS(&mQueue)));

        // Command allocator
        ThrowIfFailed(device->GetDevice()->CreateCommandAllocator(cmdQueueType, IID_PPV_ARGS(&mAllocator)));

        mIsAllocated = true;
    }

    auto CommandList::BindIndirectBuffer( IBuffer *buffer ) -> void {

    }

    GpuUploadManager::GpuUploadManager( IGpuDevice *device )
        : mDevice{ device }
    {
    }

    auto GpuUploadManager::SubAllocate( size_t byteSize ) -> GpuUploadAllocation * {
        return nullptr;
    }

    auto GpuUploadManager::ReclaimMemory() -> void {

    }

    GpuUploadManager::~GpuUploadManager() {

    }

    auto GpuUploadManager::CreateBuffer() -> StagingAllocation* {
        return nullptr;
    }

    auto GpuUploadManager::CreateSubAllocation( IBuffer *buffer ) -> GpuUploadAllocation * {
        return nullptr;
    }

    Device::Device( const GpuDeviceCreateInfo &createInfo )
        : IGpuDevice{ createInfo.mApi, createInfo.mFeaturesSupport }
    {}

    auto Device::Init() -> void {
        IDXGIFactory4* factory{ checked_cast<Context*>( RenderSystem::Get()->GetContext() )->GetDxiFactory() };
        MKT_ASSERT( factory, "A valid DirectX factory interface is required to create the device." );

        // We use adapter 1 to query available physical devices
        for (UINT adapterIndex{};
            DXGI_ERROR_NOT_FOUND != factory->EnumAdapters1(adapterIndex, &mAdapter1);
            ++adapterIndex)
        {
            DXGI_ADAPTER_DESC1 desc{};
            mAdapter1->GetDesc1(&desc);

            // Don't select the Basic Render Driver adapter.
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
                continue;
            }

            // Check to see if the adapter supports Direct3D 12, but don't create the actual device yet.
            if ( SUCCEEDED( D3D12CreateDevice( mAdapter1.Get(), D3D_FEATURE_LEVEL_12_0, _uuidof( ID3D12Device ), nullptr ) ) ) {
                break;
            }
        }

        // Get our adapter 4 which is the
        // default interface we work with
        ThrowIfFailed( mAdapter1.As( &mAdapter4 ) );
        ThrowIfFailed( mAdapter4->GetDesc3( &mDeviceDescription3 ) );

        ThrowIfFailed( D3D12CreateDevice( mAdapter4.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS( &mDevice ) ) );
        mDevice->SetName( mDeviceDescription3.Description );

        mName = string::FromWChar( mDeviceDescription3.Description );

        InitCommandQueues();
        InitMemoryAllocator();

#if defined(_DEBUG)
        ThrowIfFailed(mDevice->QueryInterface( IID_PPV_ARGS(&mDebugDevice) ));
#endif
    }

    auto Device::Shutdown() -> void {
        mQueues.clear();
    }

    auto Device::InitCommandQueues() -> void {
        // D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_TYPE_COMPUTE and D3D12_COMMAND_LIST_TYPE_COPY
        // are guaranteed to be supported which is all we need for Graphics, Compute, Transfer and Present.
        // For the time being we will use one command queue type for all operations (D3D12_COMMAND_LIST_TYPE_DIRECT in this case)
        QueueHandle queue{ Ref<Queue>::Spawn( QueueType::eGraphics,
            QueueOpSupportFlagsBits::kGraphics | QueueOpSupportFlagsBits::kCompute |
            QueueOpSupportFlagsBits::kTransfer | QueueOpSupportFlagsBits::kPresentation ) };

        queue->Initialize(this);

        // Same queue for all types
        eastl::vector<QueueType> queueTypes{
            QueueType::eGraphics, QueueType::eCompute,
            QueueType::eTransfer, QueueType::ePresent };
        for (const auto& type : queueTypes) {
            mQueues[type] = queue;
        }
    }

    auto Device::InitMemoryAllocator() -> void {
        mGpuAllocator = IGpuAllocator::Create( this );
        if ( !mGpuAllocator ) {
            MKT_THROW_RUNTIME_ERROR( "D3D12Device - Could not create GPU Allocator." );
        }

        mUploadManager = eastl::make_unique<GpuUploadManager>( this );

        mGpuAllocator->Init();
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

    auto Device::GetDevice() -> ID3D12Device * {
        return mDevice.Get();
    }

    auto Device::GetAdapter() -> IDXGIAdapter4 * {
        return mAdapter4.Get();
    }

    auto Device::GetQueue( QueueType type ) -> Queue * {
        MKT_ASSERT( mQueues.contains( type ), "Device does not contain requested type of queue" );
        return mQueues.at(type).GetRaw();
    }

    auto Device::GetQueue( QueueType type ) const -> const Queue* {
        MKT_ASSERT( mQueues.contains( type ), "Device does not contain requested type of queue" );
        return mQueues.at(type).GetRaw();
    }

    auto Device::CreateSwapChain( Window *window, Microsoft::WRL::ComPtr<IDXGIFactory4> dxgiFactory ) -> SwapChainHandle {
        auto handle{ SwapChainHandle::Spawn(window, dxgiFactory) };
        if (!handle.IsEmpty()) {
            handle->Initialize(this);
        }

        return handle;
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
