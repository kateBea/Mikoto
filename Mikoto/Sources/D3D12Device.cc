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
#include <Core/Profiler.hh>
#include <Core/Platform.hh>
#include <Renderer/Core/RenderSystem.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

#include <Platform/PlatformWin32.hh>

#include <Renderer/D3D12/D3D12Device.hh>
#include <Renderer/D3D12/D3D12Context.hh>
#include <Renderer/D3D12/D3D12Shader.hh>
#include <Renderer/D3D12/D3D12Buffer.hh>
#include <Renderer/D3D12/D3D12Texture.hh>
#include <Renderer/D3D12/D3D12Pipeline.hh>
#include <Renderer/D3D12/Direct3D12Helpers.hh>


// D3D12 extension library.
#include <directx/d3d12.h>

#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

namespace mikoto::renderer::d3d12 {

    Fence::Fence( u64 initialValue ) {
        mFenceValue = initialValue;
    }

    auto Fence::GetCompletionValue() const -> u64 {
        return mFenceValue; // TODO
    }

    auto Fence::SetDebugName( eastl::string_view name ) -> void {

    }

    auto Fence::GetNativeHandle( ObjectType type ) -> Object {
        return IFence::GetNativeHandle( type );
    }

    auto Fence::GetNativeHandle( ObjectType type ) const -> Object {
        return IFence::GetNativeHandle( type );
    }

    Fence::~Fence() {
        if (mIsAllocated) {
            Release();
        }
    }

    auto Fence::Initialize() -> void {
        Device* device{ checked_cast<Device*>( mDevice ) };
        ThrowIfFailed(device->GetDevice()->CreateFence(
            mFenceValue, D3D12_FENCE_FLAG_NONE,
            IID_PPV_ARGS(&mFence)));

        mIsAllocated = true;
    }

    auto Fence::Release() -> void {
        mIsAllocated = false;
    }

    auto BindingLayout::IsBindless() const -> bool {
        return mIsBindless;
    }

    auto BindingLayout::GetBindingLayoutDesc() const -> const BindingLayoutDescription & {
        return mBindingLayoutDesc;
    }

    BindingLayout::BindingLayout( const BindingLayoutDescription &desc )
        : mRegisterSpace{ desc.mRegisterSpace }, mIsBindless{ false }, mBindingLayoutDesc{ desc } {
    }

    BindingLayout::BindingLayout( const BindlessLayoutDescription &desc )
        : mRegisterSpace{ desc.mRegisterSpace }, mIsBindless{ true }, mBindlessLayoutDesc{ desc } {
    }

    auto BindingLayout::GetBindlessLayoutDesc() const -> const BindlessLayoutDescription & {
        return mBindlessLayoutDesc;
    }

    auto BindingLayout::GetDescriptorRanges() const -> const eastl::vector<D3D12_DESCRIPTOR_RANGE1> & {
        return mDescriptorRanges;
    }

    auto BindingLayout::GetDescriptorSamplerRanges() const -> const eastl::vector<D3D12_DESCRIPTOR_RANGE1> & {
        return mDescriptorRangesSamplers;
    }

    auto BindingLayout::GetBindlessDescriptorRanges() const -> const eastl::vector<D3D12_DESCRIPTOR_RANGE1> & {
        return mBindlessDescriptorRanges;
    }

    auto BindingLayout::GetBindlessDescriptorSamplerRanges() const -> const eastl::vector<D3D12_DESCRIPTOR_RANGE1> & {
        return mBindlessDescriptorRangesSamplers;
    }

    auto BindingLayout::GetNextIndexForDescriptor( D3D12_DESCRIPTOR_RANGE_TYPE descriptor ) const -> u32 {
        return mNextRegisterForType[descriptor]++;
    }

    auto BindingLayout::SetDebugName( eastl::string_view name ) -> void {
        IBindingLayout::SetDebugName( name );
    }

    auto BindingLayout::GetNativeHandle( ObjectType type ) -> Object {
        return IBindingLayout::GetNativeHandle( type );
    }

    auto BindingLayout::GetNativeHandle( ObjectType type ) const -> Object {
        return IBindingLayout::GetNativeHandle( type );
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
        if (!mIsBindless) {
            for (const auto& descriptor : mBindingLayoutDesc.mBindings) {
                D3D12_DESCRIPTOR_RANGE1 range{};

                // Convert the RHI type to DX12 range type (SRV, UAV, CBV, or SAMPLER)
                D3D12_DESCRIPTOR_RANGE_TYPE dx12Type{ d3d12::GetDescriptorRangeType(descriptor.mType) };

                range.RangeType = dx12Type;
                range.NumDescriptors = 1;
                range.RegisterSpace = mRegisterSpace;

                // Let DX12 calculate the offset automatically
                range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

                range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;

                // Automatically assign the next free register for this specific resource type
                range.BaseShaderRegister = mNextRegisterForType[dx12Type]++;

                // Separate Samplers because DX12 requires them in a dedicated Sampler Descriptor Heap
                if (descriptor.mType == ResourceType::eSampler) {
                    mDescriptorRangesSamplers.emplace_back(range);
                } else {
                    mDescriptorRanges.emplace_back(range);
                }
            }
        } else {
            // Each bindless array gets its own space starting
            // from the one we specified as base
            u32 registerSpaceOffsetIndex{ mRegisterSpace };

            for (const auto& descriptor : mBindlessLayoutDesc.mSlots) {
                D3D12_DESCRIPTOR_RANGE1 range{};

                // Convert the RHI type to DX12 range type (SRV, UAV, CBV, or SAMPLER)
                D3D12_DESCRIPTOR_RANGE_TYPE dx12Type{ d3d12::GetDescriptorRangeType(descriptor.mType) };

                range.RangeType = dx12Type;
                range.BaseShaderRegister = 0;
                range.NumDescriptors = UINT_MAX;
                range.RegisterSpace = registerSpaceOffsetIndex++;

                // Let DX12 calculate the offset automatically
                range.OffsetInDescriptorsFromTableStart = 0;

                range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;

                // Separate Samplers because DX12 requires them in a dedicated Sampler Descriptor Heap
                if (descriptor.mType == ResourceType::eSampler) {
                    mBindlessDescriptorRangesSamplers.emplace_back(range);
                } else {
                    mBindlessDescriptorRanges.emplace_back(range);
                }
            }
        }

        mIsAllocated = true;
    }

    auto BindingLayout::Release() -> void {
        mIsAllocated = false;
    }

    BindingSet::BindingSet( const BindingSetDescription &desc, BindingLayoutHandle layout )
       : mBindingLayout{ layout }, mBindingDescription{ desc }
    {}

    auto BindingSet::SetDebugName( eastl::string_view name ) -> void {

    }

    auto BindingSet::GetNativeHandle( ObjectType type ) -> Object {
        return IBindingSet::GetNativeHandle( type );
    }

    auto BindingSet::GetNativeHandle( ObjectType type ) const -> Object {
        return IBindingSet::GetNativeHandle( type );
    }

    BindingSet::~BindingSet() {
        if (mIsAllocated) {
            Release();
        }
    }

    auto BindingSet::Initialize() -> void {
        mIsAllocated = true;
    }

    auto BindingSet::Release() -> void {
        mIsAllocated = false;
    }

    InputLayout::InputLayout( const InputLayoutCreateDescription &desc )
        : mDescription{ desc }
    {
        for (const auto& bindingDesc : desc.mVertexAttributeDescriptions) {
            const auto bindingIt{ std::ranges::find_if(desc.mVertexBindingDescriptions,
                [index = bindingDesc.mBinding](const VertexBindingDescription& item) {
                return item.mBinding == index;
            })};

            MKT_ASSERT( bindingIt != desc.mVertexBindingDescriptions.end(), "No binding found for the given index");
            const auto& binding{ *bindingIt };

            D3D12_INPUT_ELEMENT_DESC description{
                .SemanticName = bindingDesc.mName.c_str(),
                .SemanticIndex = mSemanticIndexPerName[bindingDesc.mName]++,
                .Format = d3d12::GetFormat( bindingDesc.mFormat ),
                .InputSlot = bindingDesc.mBinding,
                .AlignedByteOffset = bindingDesc.mOffset,
                .InputSlotClass = (binding.mRate == InputRate::ePerVertex)
                    ? D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA
                    : D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,
                .InstanceDataStepRate = as<UINT>( binding.mRate == InputRate::ePerVertex ? 0 : 1 ) };
            mInputElems.emplace_back( description );
        }
    }

    auto InputLayout::GetNumAttributes() const -> u32 {
        return mAttributes.size();
    }

    auto InputLayout::GetAttributeDescription( u32 index ) const -> const VertexAttributeDescription &{
        return  mAttributes.at(index);
    }

    auto InputLayout::GetInputElements() -> const D3D12_INPUT_ELEMENT_DESC * {
        return mInputElems.data();
    }

    auto InputLayout::GetInputElementsCount() -> UINT {
        return as<UINT>(mInputElems.size());
    }

    InputLayout::~InputLayout() {
        if (mIsAllocated) {
            Release();
        }
    }

    auto InputLayout::Initialize() -> void {
        mIsAllocated = true;
    }

    auto InputLayout::Release() -> void {
        mIsAllocated = false;
    }

    PipelineLayout::PipelineLayout( const PipelineLayoutCreateDescription &description )
        : mDescription{ description }
    {

    }

    auto PipelineLayout::SetDebugName( const eastl::string_view name ) -> void {
        mDebugName = name;
        mRootSignature->SetName( string::ToWide( mDebugName ).c_str() );
    }

    auto PipelineLayout::GetNativeHandle( ObjectType type ) -> Object {
        return IPipelineLayout::GetNativeHandle( type );
    }

    auto PipelineLayout::GetNativeHandle( ObjectType type ) const -> Object {
        return IPipelineLayout::GetNativeHandle( type );
    }

    auto PipelineLayout::GetDescription() const -> const PipelineLayoutCreateDescription & {
        return mDescription;
    }

    PipelineLayout::operator ID3D12RootSignature *() const {
        return mRootSignature.Get();
    }

    PipelineLayout::~PipelineLayout() {
        if (mIsAllocated) {
            Release();
        }
    }

    auto PipelineLayout::Initialize() -> void {
        Device* device{ checked_cast<Device*>( mDevice ) };
        ID3D12Device2* d3d12Device{ device->GetDevice() };

        // Check root signature availability
        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData{};
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
        if (FAILED(d3d12Device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE,
            MKT_ADDRESSOF( featureData ), MKT_SIZEOF(featureData))))
        {
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }

        u32 rootConstantsBufferIndex{};

        // Non-bindless path
        eastl::vector<D3D12_ROOT_PARAMETER1> rootParameters{};
        for (auto& item : mDescription.mBindingLayouts) {
            BindingLayout* bindingLayout{ checked_cast<BindingLayout*>( item.GetRaw() ) };

            // Root constants always at register space 0, just find
            // the next available cBuffer index
            if (!bindingLayout->IsBindless()) {
                const BindingLayoutDescription& desc{ bindingLayout->GetBindingLayoutDesc() };

                // Root constants go to space 0 and take the next available index for CBV
                if (desc.mRegisterSpace == 0) {
                    rootConstantsBufferIndex = bindingLayout->GetNextIndexForDescriptor(D3D12_DESCRIPTOR_RANGE_TYPE_CBV);
                }

                const auto& descriptorRanges{ bindingLayout->GetDescriptorRanges() };
                const auto& samplerDescriptorRanges{ bindingLayout->GetDescriptorSamplerRanges() };

                auto& rootParameter{ rootParameters.emplace_back() };
                rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                rootParameter.ShaderVisibility = d3d12::GetShaderVisibility(desc.mStageVisibility);

                rootParameter.DescriptorTable.NumDescriptorRanges = as<UINT>(descriptorRanges.size());
                rootParameter.DescriptorTable.pDescriptorRanges = descriptorRanges.data();

                if (!samplerDescriptorRanges.empty()) {
                    auto& rootParameterSamplers{ rootParameters.emplace_back() };
                    rootParameterSamplers.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                    rootParameterSamplers.ShaderVisibility = d3d12::GetShaderVisibility(desc.mStageVisibility);

                    rootParameterSamplers.DescriptorTable.NumDescriptorRanges = as<UINT>(samplerDescriptorRanges.size());
                    rootParameterSamplers.DescriptorTable.pDescriptorRanges = samplerDescriptorRanges.data();
                }
            } else {
                const BindlessLayoutDescription& desc{ bindingLayout->GetBindlessLayoutDesc() };
                if (desc.mRegisterSpace == 0) {
                    rootConstantsBufferIndex = bindingLayout->GetNextIndexForDescriptor(D3D12_DESCRIPTOR_RANGE_TYPE_CBV);
                }

                const auto& descriptorRanges{ bindingLayout->GetBindlessDescriptorRanges() };
                const auto& samplerDescriptorRanges{ bindingLayout->GetBindlessDescriptorSamplerRanges() };

                auto& rootParameter{ rootParameters.emplace_back() };
                rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                rootParameter.ShaderVisibility = d3d12::GetShaderVisibility(desc.mStageVisibility);

                rootParameter.DescriptorTable.NumDescriptorRanges = as<UINT>(descriptorRanges.size());
                rootParameter.DescriptorTable.pDescriptorRanges = descriptorRanges.data();

                if (!samplerDescriptorRanges.empty()) {
                    auto& rootParameterSamplers{ rootParameters.emplace_back() };
                    rootParameterSamplers.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                    rootParameterSamplers.ShaderVisibility = d3d12::GetShaderVisibility(desc.mStageVisibility);

                    rootParameterSamplers.DescriptorTable.NumDescriptorRanges = as<UINT>(samplerDescriptorRanges.size());
                    rootParameterSamplers.DescriptorTable.pDescriptorRanges = samplerDescriptorRanges.data();
                }
            }
        }

        // For now we are always assuming root constant are available
        D3D12_ROOT_PARAMETER1 rootParamConstants{};
        rootParamConstants.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        rootParamConstants.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        // Configure the layout metadata
        rootParamConstants.Constants.ShaderRegister = rootConstantsBufferIndex;
        rootParamConstants.Constants.RegisterSpace  = 0;
        rootParamConstants.Constants.Num32BitValues = 32;

        rootParameters.emplace_back( rootParamConstants );

        D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc{};
        rootSignatureDesc.Version = featureData.HighestVersion;
        rootSignatureDesc.Desc_1_1.Flags =
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED |
            D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED;
        rootSignatureDesc.Desc_1_1.NumParameters = as<UINT>(rootParameters.size());
        rootSignatureDesc.Desc_1_1.pParameters = rootParameters.data();
        rootSignatureDesc.Desc_1_1.NumStaticSamplers = 0;
        rootSignatureDesc.Desc_1_1.pStaticSamplers = nullptr;

        HRESULT hr{ D3D12SerializeVersionedRootSignature(&rootSignatureDesc,
            &mSignatureBlob, &mErrorMessages) };

        if (mErrorMessages && mErrorMessages->GetBufferSize() != 0) {
            eastl::string msg{ (const char *)mErrorMessages->GetBufferPointer() };
            MKT_CORE_LOGGER_ERROR( "D3D12SerializeVersionedRootSignature Errors: {}", msg.c_str() );
        }

        ThrowIfFailed( hr );

        ThrowIfFailed(d3d12Device->CreateRootSignature(0,
            mSignatureBlob->GetBufferPointer(),
            mSignatureBlob->GetBufferSize(),
            IID_PPV_ARGS(&mRootSignature)));

        mIsAllocated = true;
    }

    auto PipelineLayout::Release() -> void {
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
        MKT_BEGIN_PROFILER_NAMED();

        auto description{ CommandListParameters{}
            .SetQueueType( mType )
            .SetMaxThreadConcurrency( 0 ) };

        CommandListHandle handle{ Ref<CommandList>::Spawn( description ) };
        handle->Initialize( mDevice );

        return handle;
    }

    auto Queue::AllocateCmdList( const CommandListParameters &params ) -> CommandListHandle {
        MKT_BEGIN_PROFILER_NAMED();

        CommandListHandle handle{ Ref<CommandList>::Spawn( params ) };
        handle->Initialize( mDevice );

        return handle;
    }

    auto Queue::WaitIdle() const -> void {

    }

    Queue::operator ID3D12CommandQueue*() const {
        return mQueue.Get();
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

        mIsAllocated = true;
    }

    CommandList::CommandList( const CommandListParameters &desc )
        : ICommandList{ desc.mQueueType }
    {
    }

    auto CommandList::Begin( const CommandListBeginDescription &desc ) -> void {

    }

    auto CommandList::End() -> void {

    }

    auto CommandList::BeginParallel() -> void {

    }

    auto CommandList::EndParallel() -> void {

    }

    auto CommandList::SetDebugName( eastl::string_view name ) -> void {
        if (name.empty()) {
            return;
        }

        mDebugName = name;
        mCommandList->SetName( string::ToWide( mDebugName ).c_str() );
    }

    auto CommandList::PushBarrier( const BufferBarrierDescription &barrier ) -> void {

    }

    auto CommandList::PushBarrier( const TextureBarrierDescription &barrier ) -> void {

    }

    auto CommandList::BeginTrackingState( IBuffer *buffer, ResourceStates stateBits ) -> void {

    }

    auto CommandList::BeginTrackingState( ITexture *buffer, ResourceStates stateBits ) -> void {

    }

    auto CommandList::SetResourceState( IBuffer *buffer, ResourceStates stateBits ) -> void {

    }

    auto CommandList::SetResourceState( ITexture *buffer, ResourceStates stateBits ) -> void {

    }

    auto CommandList::SetBarrier( const BufferBarrierDescription &barrier ) -> void {

    }

    auto CommandList::SetBarrier( const TextureBarrierDescription &barrier ) -> void {

    }

    auto CommandList::CommitBarriers() -> void {

    }

    auto CommandList::SetEnableAutomaticBarriers( bool enable ) -> void {

    }

    auto CommandList::SetClearColor( FramebufferHandle frameBuffer, Color color ) -> void {

    }

    auto CommandList::SetClearColor( TextureHandle renderTargets, Color color ) -> void {

    }

    auto CommandList::Write( IBuffer *src, ITexture *dest, u32 mipLevel ) -> void {

    }

    auto CommandList::Write( ITexture *texture, u32 mipLevel, const void *data, size_t byteSize ) -> void {

    }

    auto CommandList::Copy( ITexture *src, const TextureSlice &srcSlice, ITexture *dest, const TextureSlice &destSlice ) -> void {

    }

    auto CommandList::WriteVolatile( IBuffer *target, size_t dstOffset, const void *data, size_t byteSize ) -> void {

    }

    auto CommandList::Write( IBuffer *buffer, size_t destOffset, const void *data, size_t byteSize ) -> void {

    }

    auto CommandList::Write( IBuffer *buffer, const void *data, size_t byteSize ) -> void {

    }

    auto CommandList::Copy( IBuffer *src, IBuffer *dest ) -> void {

    }

    auto CommandList::Copy( IBuffer *src, IBuffer *dest, size_t destOffset ) -> void {

    }

    auto CommandList::Copy( IBuffer *dest, ITexture *src ) -> void {

    }

    auto CommandList::BeginRendering( GraphicsState &state ) -> void {

    }

    auto CommandList::EndRendering() -> void {

    }

    auto CommandList::BindPipeline( IPipeline *pipeline ) -> void {

    }

    auto CommandList::SetViewport( eastl::span<const Viewport> viewports ) -> void {

    }

    auto CommandList::SetScissors( eastl::span<const Rect> scissorRects ) -> void {

    }

    auto CommandList::SetViewportState( const ViewportState &vs ) -> void {

    }

    auto CommandList::BindIndexBuffer( IBuffer *buffer ) -> void {

    }

    auto CommandList::BindVertexBuffer( const VertexBufferBinding &binding ) -> void {

    }

    auto CommandList::BindPipelineResources( const BindResourcesDescription &desc ) -> void {
        // https://learn.microsoft.com/en-us/windows/win32/direct3d12/descriptor-heaps-overview

        Device* device{ checked_cast<Device*>( mDevice ) };
        PipelineLayout* pipelineLayout{ checked_cast<PipelineLayout*>( desc.mPipelineLayout ) };

        // Root signature
        ID3D12RootSignature* rootSignature{ *pipelineLayout };
        mCommandList->SetGraphicsRootSignature(rootSignature);

        const DeviceResources* deviceResources{ device->GetHeapResources() };
        eastl::vector<ID3D12DescriptorHeap*> descriptorHeaps{};

        descriptorHeaps.emplace_back( deviceResources->mSamplerHeap.GetHeap() );
        descriptorHeaps.emplace_back( deviceResources->mShaderResourceViewHeap.GetHeap() );

        // At most one CBV/SRV/UAV combined heap and one Sampler heap can be bound at any one time.
        // These heaps are shared between both the graphics and compute pipelines (described in their PSOs).
        mCommandList->SetDescriptorHeaps(as<UINT>(descriptorHeaps.size()), descriptorHeaps.data());

        //D3D12_GPU_DESCRIPTOR_HANDLE cbvHandle{ constantBuffersHeap->GetGPUDescriptorHandleForHeapStart() };
        //mCommandList->SetGraphicsRootDescriptorTable(0, cbvHandle);
    }

    auto CommandList::Draw( const DrawArguments &args ) -> void {

    }

    auto CommandList::DrawIndexed( const DrawArguments &args ) -> void {

    }

    auto CommandList::DrawIndirect( u32 offset, u32 drawCount ) -> void {

    }

    auto CommandList::DrawIndexedIndirect( u32 offset, u32 drawCount ) -> void {

    }

    auto CommandList::Dispatch( u32 groupsX, u32 groupsY, u32 groupsZ ) -> void {

    }

    auto CommandList::SetPushConstants( IPipelineLayout *pipelineLayout, const void *data, size_t byteSize, ShaderStage visibility ) -> void {
    }

    auto CommandList::GetNativeHandle( ObjectType type ) -> Object{
        return ICommandList::GetNativeHandle( type );
    }

    auto CommandList::GetNativeHandle( ObjectType type ) const -> Object{
        return ICommandList::GetNativeHandle( type );
    }

    CommandList::~CommandList() {
        if (mIsAllocated) {
            Release();
        }
    }

    auto CommandList::Initialize() -> void {
        Device* device{ checked_cast<Device*>( mDevice ) };

        const D3D12_COMMAND_LIST_TYPE cmdQueueType{ d3d12::GetQueueType(mQueueType) };

        ThrowIfFailed(device->GetDevice()->CreateCommandAllocator(cmdQueueType, IID_PPV_ARGS(&mCommandAllocator)));

        ThrowIfFailed(device->GetDevice()->CreateCommandList(
            0, d3d12::GetQueueType( mQueueType ),
            mCommandAllocator.Get(), nullptr,
            IID_PPV_ARGS(&mCommandList)));

        mIsAllocated = true;
    }

    auto CommandList::Release() -> void {
        mIsAllocated = false;
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
        IDXGIFactory4* factory{ checked_cast<Context*>( RenderSystem::Get()->GetContext() )->GetDxGIFactory() };
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

        InitInfoQueue();
        InitCommandQueues();
        InitMemoryAllocator();

#if defined(_DEBUG)
        ThrowIfFailed(mDevice->QueryInterface( IID_PPV_ARGS(&mDebugDevice) ));
#endif
    }

    auto Device::Shutdown() -> void {
        mDebugDevice->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL);

        mQueues.clear();

        if ( mInfoQueue1 ) {
            mInfoQueue1->UnregisterMessageCallback( mInfoQueueCallbackCookie );
        }
    }

    auto Device::InitInfoQueue() -> void {
        if ( FAILED( mDevice.As( &mInfoQueue ) ) ) {
            MKT_CORE_LOGGER_DEBUG( "Failed to acquire ID3D12InfoQueue." );
            return;
        }

        mInfoQueue->SetBreakOnSeverity( D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE );
        mInfoQueue->SetBreakOnSeverity( D3D12_MESSAGE_SEVERITY_ERROR, TRUE );
        mInfoQueue->SetBreakOnSeverity( D3D12_MESSAGE_SEVERITY_WARNING, TRUE );
        mInfoQueue->SetBreakOnSeverity( D3D12_MESSAGE_SEVERITY_INFO, TRUE );
        mInfoQueue->SetBreakOnSeverity( D3D12_MESSAGE_SEVERITY_MESSAGE, TRUE );

        // https://github.com/microsoft/DirectX-Specs/blob/master/d3d/MessageCallback.md
        if ( SUCCEEDED(mDevice.As(&mInfoQueue1) )) {
            ThrowIfFailed(
                mInfoQueue1->RegisterMessageCallback(
                    &DebugMessageCallback,
                    D3D12_MESSAGE_CALLBACK_FLAG_NONE,
                    this,
                    &mInfoQueueCallbackCookie ) );

            MKT_CORE_LOGGER_DEBUG( "Using ID3D12InfoQueue1 message callback." );
        } else {
            MKT_CORE_LOGGER_DEBUG( "ID3D12InfoQueue1 unavailable, falling back to polling." );
        }
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
        TextureHandle texture{ Ref<Texture>::Spawn(description, mResourceHeaps) };

        if ( texture.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "D3D12Device - Failed to allocate texture" );
            return TextureHandle::CreateEmpty();
        }

        texture->Initialize( this );

        return texture;
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

    auto Device::CreateCommandList( QueueType queueType ) -> CommandListHandle {
        Queue* queue{ GetQueue( queueType ) };

        auto handle{ CommandListHandle::CreateEmpty() };
        if (queue) {
            handle = queue->AllocateCmdList();
        }

        return handle;
    }

    auto Device::CreateCommandList( const CommandListParameters &parameters ) -> CommandListHandle {
        return CommandListHandle::CreateEmpty();
    }

    auto Device::CreateShader( const ShaderModuleCreateDescription &desc ) -> ShaderModuleHandle {
        ShaderModuleHandle result{ Ref<Shader>::Spawn(desc) };

        if ( result.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to create shader." );
            return ShaderModuleHandle::CreateEmpty();
        }

        result->Initialize( this );

        return result;
    }

    auto Device::CreateShader( ShaderStage type, const void *code, size_t codeSize ) -> ShaderModuleHandle {
        return ShaderModuleHandle::CreateEmpty();
    }

    auto Device::CreateInputLayout( const InputLayoutCreateDescription& desc ) -> InputLayoutHandle {
        InputLayoutHandle layout{ Ref<InputLayout>::Spawn( desc ) };

        if ( layout.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate binding layout resource." );
            return InputLayoutHandle::CreateEmpty();
        }

        layout->Initialize( this );

        return layout;
    }

    auto Device::CreateBindingLayout( const BindingLayoutDescription &desc ) -> BindingLayoutHandle {
        BindingLayoutHandle layout{ Ref<BindingLayout>::Spawn( desc ) };

        if ( layout.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate binding layout resource." );
            return BindingLayoutHandle::CreateEmpty();
        }

        layout->Initialize( this );

        return layout;
    }

    auto Device::CreatePipelineLayout( const PipelineLayoutCreateDescription& desc ) -> PipelineLayoutHandle {
        PipelineLayoutHandle layout{ Ref<PipelineLayout>::Spawn( desc ) };

        if ( layout.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate pipeline layout resource." );
            return PipelineLayoutHandle::CreateEmpty();
        }

        layout->Initialize( this );

        return layout;
    }

    auto Device::CreateBindingSet( const BindingSetDescription &desc, BindingLayoutHandle layout ) -> BindingSetHandle {
        BindingSetHandle set{ Ref<BindingSet>::Spawn( desc, layout ) };

        if ( set.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate binding set resource." );
            return BindingSetHandle::CreateEmpty();
        }

        set->Initialize( this );

        return set;
    }

    auto Device::CreateFence( u64 fenceInitialValue ) -> FenceHandle {
        FenceHandle fence{ Ref<Fence>::Spawn( fenceInitialValue ) };

        if ( fence.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate fence resource." );
            return FenceHandle::CreateEmpty();
        }

        fence->Initialize( this );

        return fence;
    }

    auto Device::UnMap( IBuffer *buffer ) -> void {
        Buffer* b{ checked_cast<Buffer*>( buffer ) };
        if (b->IsMapped()) {
            b->PersistentUnmap();
        }
    }

    auto Device::Map( IBuffer *buffer ) -> const void * {
        Buffer* b{ checked_cast<Buffer*>( buffer ) };
        if (!b->IsMapped()) {
            b->PersistentMap();
        }

        return b->GetMappedAddress();
    }

    auto Device::CreateBindlessLayout( const BindlessLayoutDescription &desc ) -> BindingLayoutHandle {
        BindingLayoutHandle layout{ Ref<BindingLayout>::Spawn( desc ) };

        if ( layout.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate binding layout resource." );
            return BindingLayoutHandle::CreateEmpty();
        }

        layout->Initialize( this );

        return layout;
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

    auto Device::GetDevice() -> ID3D12Device2 * {
        return mDevice.Get();
    }

    auto Device::GetAdapter() -> IDXGIAdapter4 * {
        return mAdapter4.Get();
    }

    auto Device::GetHeapResources() const -> const DeviceResources * {
        return MKT_ADDRESSOF( mResourceHeaps );
    }

    auto Device::GetQueue( QueueType type ) -> Queue * {
        MKT_ASSERT( mQueues.contains( type ), "Device does not contain requested type of queue" );
        return mQueues.at(type).GetRaw();
    }

    auto Device::GetQueue( QueueType type ) const -> const Queue* {
        MKT_ASSERT( mQueues.contains( type ), "Device does not contain requested type of queue" );
        return mQueues.at(type).GetRaw();
    }

    auto Device::GetAllocator() -> GpuMemoryAllocator * {
        return checked_cast<GpuMemoryAllocator*>( mGpuAllocator.get() );
    }

    auto Device::DumpMessages() -> void {
#if !defined(NDEBUG)

        if (!mInfoQueue) {
            return;
        }

        const UINT64 messageCount{ mInfoQueue->GetNumStoredMessages() };

        for (UINT64 i{}; i < messageCount; ++i) {
            SIZE_T messageSize{};
            ThrowIfFailed( mInfoQueue->GetMessage( i, nullptr, &messageSize ) );

            eastl::vector<std::byte> storage( messageSize );
            auto* message{ reinterpret_cast<D3D12_MESSAGE*>( storage.data() ) };

            ThrowIfFailed( mInfoQueue->GetMessage( i, message, &messageSize ) );

            switch (message->Severity) {
                case D3D12_MESSAGE_SEVERITY_CORRUPTION:
                    MKT_CORE_LOGGER_CRITICAL( "[D3D12] {}", message->pDescription );
                    break;

                case D3D12_MESSAGE_SEVERITY_ERROR:
                    MKT_CORE_LOGGER_ERROR( "[D3D12] {}", message->pDescription );
                    break;

                case D3D12_MESSAGE_SEVERITY_WARNING:
                    MKT_CORE_LOGGER_WARN( "[D3D12] {}", message->pDescription );
                    break;

                case D3D12_MESSAGE_SEVERITY_INFO:
                    MKT_CORE_LOGGER_INFO( "[D3D12] {}", message->pDescription );
                    break;

                case D3D12_MESSAGE_SEVERITY_MESSAGE:
                    MKT_CORE_LOGGER_DEBUG( "[D3D12] {}", message->pDescription );
                    break;

                default:
                    MKT_CORE_LOGGER_DEBUG( "[D3D12][UNKNOWN] {}", message->pDescription );
                    break;
            }
        }

        mInfoQueue->ClearStoredMessages();
#endif
    }

    auto CALLBACK Device::DebugMessageCallback(
            D3D12_MESSAGE_CATEGORY,
            D3D12_MESSAGE_SEVERITY severity,
            D3D12_MESSAGE_ID,
            LPCSTR description,
            void * ) -> void {
        switch ( severity ) {
            case D3D12_MESSAGE_SEVERITY_CORRUPTION:
                MKT_CORE_LOGGER_CRITICAL( "[D3D12] [CORRUPTION] {}", description );
                break;
            case D3D12_MESSAGE_SEVERITY_ERROR:
                MKT_CORE_LOGGER_ERROR( "[D3D12] [ERROR] {}", description );
                break;
            case D3D12_MESSAGE_SEVERITY_WARNING:
                MKT_CORE_LOGGER_WARN( "[D3D12] [WARNING] {}", description );
                break;
            case D3D12_MESSAGE_SEVERITY_INFO:
                MKT_CORE_LOGGER_INFO( "[D3D12] [INFO] {}", description );
                break;
            case D3D12_MESSAGE_SEVERITY_MESSAGE:
                MKT_CORE_LOGGER_DEBUG( "[D3D12] [MESSAGE] {}", description );
                break;
            default:
                MKT_CORE_LOGGER_DEBUG( "[D3D12][UNKNOWN] {}", description );
                break;
        }
    }

    auto Device::CreateSwapChain( Window *window, Microsoft::WRL::ComPtr<IDXGIFactory4> dxgiFactory ) -> SwapChainHandle {
        auto handle{ SwapChainHandle::Spawn(window, dxgiFactory) };
        if (!handle.IsEmpty()) {
            handle->Initialize(this);
        }

        return handle;
    }

    auto Device::CreateBuffer( const BufferCreateDescription &description ) -> BufferHandle {
        BufferHandle buffer{ Ref<Buffer>::Spawn(description, mResourceHeaps ) };

        if ( buffer.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate buffer resource." );
            return BufferHandle::CreateEmpty();
        }

        buffer->Initialize( this );

        return buffer;
    }

    auto Device::CreatePipeline( const ComputePipelineDescription &description ) -> PipelineHandle {
        PipelineHandle computePipeline{ Ref<ComputePipeline>::Spawn( description ) };

        if ( computePipeline.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate compute pipeline resource." );
            return PipelineHandle::CreateEmpty();
        }

        computePipeline->Initialize( this );

        return computePipeline;
    }

    auto Device::CreatePipeline( const GraphicsPipelineDescription &description ) -> PipelineHandle {
        PipelineHandle graphicsPipeline{ Ref<GraphicsPipeline>::Spawn( description ) };

        if ( graphicsPipeline.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate graphics pipeline resource." );
            return PipelineHandle::CreateEmpty();
        }

        graphicsPipeline->Initialize( this );

        return graphicsPipeline;
    }

    auto Device::RunGarbageCollection() -> void {

    }

    auto Device::SubmitCommands( CommandListHandle cmd ) -> u64 {
        return 0;
    }
}

#endif
