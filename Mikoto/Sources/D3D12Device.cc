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

#include <EASTL/sort.h>
#include <EASTL/algorithm.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/Platform.hh>

#include <Math/Math.hh>

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

    auto Fence::IsCompleted( u64 fenceValue ) const -> bool {
        return mFence->GetCompletedValue() >= fenceValue;
    }

    auto Fence::WaitForFenceValue( u64 fenceValue ) -> void {
        if (!IsCompleted( fenceValue )) {
            ThrowIfFailed(mFence->SetEventOnCompletion(mFenceValue, mFenceEvent));
            ::WaitForSingleObject(mFenceEvent, INFINITE);
        }
    }

    auto Fence::GetNativeHandle( ObjectType type ) -> Object {
        switch (type) {
            case ObjectType::D3D12_Fence:
                return Object(mFence.Get());
            default:;
        }

        return Object(nullptr);
    }

    auto Fence::GetNativeHandle( ObjectType type ) const -> Object {
        switch (type) {
            case ObjectType::D3D12_Fence:
                return Object(mFence.Get());
            default:;
        }

        return Object(nullptr);
    }

    Fence::operator HANDLE() const {
        return mFenceEvent;
    }

    Fence::operator ID3D12Fence*() const {
        return mFence.Get();
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

        mFenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
        MKT_ASSERT( mFenceEvent, "Failed to create fence event." );

        mIsAllocated = true;
    }

    auto Fence::Release() -> void {
        mIsAllocated = false;
    }

    StaticDescriptorHeap::StaticDescriptorHeap( IGpuDevice* device ) {
        Device* pDev{ checked_cast<Device*>( device ) };
        mDevice = pDev->GetDevice();
    }

    auto StaticDescriptorHeap::CopyToShaderVisibleHeap( DescriptorIndex index, u32 count ) -> void {
        // https://stackoverflow.com/questions/45303008/dx12-descriptor-heaps-management
        mDevice->CopyDescriptorsSimple(count,
            GetCpuHandleShaderVisible(index),
            GetCpuHandle(index), mHeapType);
    }

    auto StaticDescriptorHeap::AllocateDescriptor() -> DescriptorIndex {
        return AllocateDescriptors( 1 );
    }

    auto StaticDescriptorHeap::AllocateDescriptors( u32 count ) -> DescriptorIndex {
        std::lock_guard lockGuard{ mMutex };

        u32 freeCount{ 0 };
        bool found{ false };
        DescriptorIndex foundIndex{ 0 };

        // Find a contiguous range of 'count' indices for which m_AllocatedDescriptors[index] is false
        for ( DescriptorIndex index{ mSearchStart }; index < mNumDescriptors; index++ ) {
            if ( mAllocatedDescriptors[index] ) {
                freeCount = 0;
            } else {
                freeCount += 1;
            }

            if ( freeCount >= count ) {
                foundIndex = index - count + 1;
                found = true;
                break;
            }
        }

        if ( !found ) {
            foundIndex = mNumDescriptors;
            if ( FAILED( Grow( mNumDescriptors + count ) ) ) {
                MKT_CORE_LOGGER_ERROR( "Failed to grow a descriptor heap!" );
                return kInvalidDescriptorIndex;
            }
        }

        for ( DescriptorIndex index{ foundIndex }; index < foundIndex + count; index++ ) {
            mAllocatedDescriptors[index] = true;
        }

        mNumAllocatedDescriptors += count;
        mSearchStart = foundIndex + count;

        return foundIndex;
    }

    auto StaticDescriptorHeap::ReleaseDescriptor( DescriptorIndex index ) -> void {
        ReleaseDescriptors( index, 1 );
    }

    auto StaticDescriptorHeap::ReleaseDescriptors( DescriptorIndex baseIndex, u32 count ) -> void {
        if ( count == 0 ) {
            return;
        }

        std::lock_guard lockGuard{ mMutex };
        for ( DescriptorIndex index{ baseIndex }; index < baseIndex + count; index++ ) {
#if !defined( NDEBUG )
            if ( !mAllocatedDescriptors[index] ) {
                MKT_CORE_LOGGER_ERROR( "Attempted to release an un-allocated descriptor" );
            }
#endif

            mAllocatedDescriptors[index] = false;
        }

        mNumAllocatedDescriptors -= count;

        if ( mSearchStart > baseIndex ) {
            mSearchStart = baseIndex;
        }
    }

    auto StaticDescriptorHeap::AllocateResources( D3D12_DESCRIPTOR_HEAP_TYPE heapType, u32 numDescriptors, bool shaderVisible ) -> HRESULT {
        mHeap = nullptr;
        mShaderVisibleHeap = nullptr;

        D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
        heapDesc.Type = heapType;
        heapDesc.NumDescriptors = numDescriptors;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        HRESULT hr{ mDevice->CreateDescriptorHeap( &heapDesc, IID_PPV_ARGS( &mHeap ) ) };
        if ( FAILED( hr ) ) {
            return hr;
        }

        if ( shaderVisible ) {
            heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            hr = mDevice->CreateDescriptorHeap( &heapDesc, IID_PPV_ARGS( &mShaderVisibleHeap ) );
            if ( FAILED( hr ) ) {
                return hr;
            }

            mStartCpuHandleShaderVisible = mShaderVisibleHeap->GetCPUDescriptorHandleForHeapStart();
            mStartGpuHandleShaderVisible = mShaderVisibleHeap->GetGPUDescriptorHandleForHeapStart();
        }

        mHeapType = heapDesc.Type;
        mNumDescriptors = heapDesc.NumDescriptors;

        mStartCpuHandle = mHeap->GetCPUDescriptorHandleForHeapStart();
        mStride = mDevice->GetDescriptorHandleIncrementSize( heapDesc.Type );
        mAllocatedDescriptors.resize( mNumDescriptors );

        return S_OK;
    }

    auto StaticDescriptorHeap::GetHeap() const -> ID3D12DescriptorHeap* {
        return mHeap.Get();
    }

    auto StaticDescriptorHeap::GetHeapType() const -> D3D12_DESCRIPTOR_HEAP_TYPE {
        return mHeapType;
    }

    auto StaticDescriptorHeap::GetCpuHandle( DescriptorIndex index ) const -> D3D12_CPU_DESCRIPTOR_HANDLE {
        D3D12_CPU_DESCRIPTOR_HANDLE result{ mStartCpuHandle };
        result.ptr += index * mStride;
        return result;
    }

    auto StaticDescriptorHeap::GetCpuHandleShaderVisible( DescriptorIndex index ) const -> D3D12_CPU_DESCRIPTOR_HANDLE {
        D3D12_CPU_DESCRIPTOR_HANDLE result{ mStartCpuHandleShaderVisible };
        result.ptr += index * mStride;
        return result;
    }

    auto StaticDescriptorHeap::GetGpuHandle( DescriptorIndex index ) const -> D3D12_GPU_DESCRIPTOR_HANDLE {
        D3D12_GPU_DESCRIPTOR_HANDLE result{ mStartGpuHandleShaderVisible };
        result.ptr += index * mStride;
        return result;
    }

    auto StaticDescriptorHeap::GetShaderVisibleHeap() const -> ID3D12DescriptorHeap* {
        return mShaderVisibleHeap.Get();
    }

    auto StaticDescriptorHeap::Grow( u32 minRequiredSize ) -> HRESULT {
        u32 oldSize{ mNumDescriptors };
        u32 newSize{ math::NextPowerOf2(minRequiredSize) };

        bool const isShaderVisible{ mShaderVisibleHeap != nullptr };

        if (isShaderVisible) {
            const u32 maxSize( (mHeapType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
                ? D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1 // Not a power of 2!
                : D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE );

            newSize = eastl::min(newSize, maxSize);

            if (newSize < minRequiredSize) {
                return E_OUTOFMEMORY;
            }
        }

        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> oldHeap{ mHeap };

        HRESULT hr{ AllocateResources(mHeapType, newSize, isShaderVisible) };
        if (FAILED(hr)) {
            return hr;
        }

        mDevice->CopyDescriptorsSimple(oldSize, mStartCpuHandle, oldHeap->GetCPUDescriptorHandleForHeapStart(), mHeapType);
        if (mShaderVisibleHeap != nullptr) {
            mDevice->CopyDescriptorsSimple(oldSize, mStartCpuHandleShaderVisible, oldHeap->GetCPUDescriptorHandleForHeapStart(), mHeapType);
        }

        return S_OK;
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
                    mDescriptorRangesSamplers.emplace_back(range);
                } else {
                    mDescriptorRanges.emplace_back(range);
                }
            }
        }

        mIsAllocated = true;
    }

    auto BindingLayout::Release() -> void {
        mIsAllocated = false;
    }

    BindingSet::BindingSet( const BindingSetDescription &desc, BindingLayoutHandle layout, DeviceResources& resources )
       : mBindingLayout{ layout }, mBindingDescription{ desc }, mDeviceResources{ MKT_ADDRESSOF( resources ) }
    {}

    auto BindingSet::SetDebugName( eastl::string_view name ) -> void {

    }

    auto BindingSet::GetNativeHandle( ObjectType type ) -> Object {
        return IBindingSet::GetNativeHandle( type );
    }

    auto BindingSet::GetNativeHandle( ObjectType type ) const -> Object {
        return IBindingSet::GetNativeHandle( type );
    }

    auto BindingSet::GetSrvRange() const -> const DescriptorRange& {
        return mSrvRange;
    }

    auto BindingSet::GetSamplerRange() const -> const DescriptorRange& {
        return mSamplerRange;
    }

    BindingSet::~BindingSet() {
        if (mIsAllocated) {
            Release();
        }
    }

    auto BindingSet::Initialize() -> void {
        Device* device{ checked_cast<Device*>( mDevice ) };

        // Allocate one contiguous range for all SRV/etc descriptors
        u32 descriptorCount{ as<u32>(mBindingDescription.mBindings.size()) };
        mSrvRange.mBaseIndex = mDeviceResources->mShaderResourceViewHeap->AllocateDescriptors(descriptorCount);
        mSrvRange.mCount = descriptorCount;

        for (u32 i{}; i < descriptorCount; ++i) {
            const BindingSetItem& binding{ mBindingDescription.mBindings[i] };

            DescriptorIndex index{ mSrvRange.mBaseIndex + i };
            D3D12_CPU_DESCRIPTOR_HANDLE cpu{ mDeviceResources->mShaderResourceViewHeap->GetCpuHandle(index) };

            if (rhi::IsBuffer( binding.mType )) {
                Buffer* buffer{ checked_cast<Buffer*>(binding.mResource) };
                if (binding.mResource) {
                    buffer->CreateSRV(cpu.ptr, binding.mRange, binding.mType, binding.mFormat);
                }
                else {
                    Buffer::CreateNullSRV(cpu.ptr, binding.mFormat, device->GetDevice());
                }
            } else if (rhi::IsTexture( binding.mType )) {
                Texture* texture{ checked_cast<Texture*>(binding.mResource) };
                if (binding.mResource) {
                    texture->CreateSRV(cpu.ptr, binding.mSubResourceSet, binding.mFormat, binding.mDimension);
                }
            } else if ( rhi::IsSampler(binding.mType) ) {

            } else {
                MKT_ASSERT( false, "Invalid type" );
            }

            mDeviceResources->mShaderResourceViewHeap->CopyToShaderVisibleHeap(index);
        }

        mSrvRange.mGpuHandle = mDeviceResources->mShaderResourceViewHeap->GetGpuHandle(mSrvRange.mBaseIndex);

        mIsAllocated = true;
    }

    auto BindingSet::Release() -> void {
        // Release descriptor indices
        u32 descriptorCount{ as<u32>(mBindingDescription.mBindings.size()) };
        mDeviceResources->mShaderResourceViewHeap->ReleaseDescriptors(mSrvRange.mBaseIndex, descriptorCount);

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

    auto PipelineLayout::GetRootConstantIndex() const -> u32 {
        return mRootConstantIndex;
    }

    auto PipelineLayout::GetMax32BitValuesCount() const -> u32 {
        return m32BitValueCount;
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


        // Root parameter index is determined by which position they occupy on the array?
        eastl::sort(mDescription.mBindingLayouts.begin(), mDescription.mBindingLayouts.end(),
            [](BindingLayoutHandle lhs, BindingLayoutHandle rhs) {
                BindingLayout* pLhs{ checked_cast<BindingLayout*>( lhs.GetRaw() ) };
                BindingLayout* pRhs{ checked_cast<BindingLayout*>( rhs.GetRaw() ) };

                return pLhs->GetRegisterSpace() < pRhs->GetRegisterSpace();
            });

        eastl::vector<D3D12_ROOT_PARAMETER1> rootParameters{};
        for (auto& item : mDescription.mBindingLayouts) {
            BindingLayout* bindingLayout{ checked_cast<BindingLayout*>( item.GetRaw() ) };

            // Root constants always at register space 0, just find
            // the next available cBuffer index
            if (!bindingLayout->IsBindless()) {
                const BindingLayoutDescription& desc{ bindingLayout->GetBindingLayoutDesc() };

                // Root constants go to space 0 and take the next available index for CBV
                if (desc.mRegisterSpace == 0) {
                    mRootConstantIndex = bindingLayout->GetNextIndexForDescriptor(D3D12_DESCRIPTOR_RANGE_TYPE_CBV);
                }

                const auto& descriptorRanges{ bindingLayout->GetDescriptorRanges() };
                if (!descriptorRanges.empty()) {
                    auto& rootParameter{ rootParameters.emplace_back() };
                    rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                    rootParameter.ShaderVisibility = d3d12::GetShaderVisibility(desc.mStageVisibility);

                    rootParameter.DescriptorTable.NumDescriptorRanges = as<UINT>(descriptorRanges.size());
                    rootParameter.DescriptorTable.pDescriptorRanges = descriptorRanges.data();
                }

                const auto& samplerDescriptorRanges{ bindingLayout->GetDescriptorSamplerRanges() };
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
                    mRootConstantIndex = bindingLayout->GetNextIndexForDescriptor(D3D12_DESCRIPTOR_RANGE_TYPE_CBV);
                }

                const auto& descriptorRanges{ bindingLayout->GetDescriptorRanges() };
                if (!descriptorRanges.empty()) {
                    auto& rootParameter{ rootParameters.emplace_back() };
                    rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                    rootParameter.ShaderVisibility = d3d12::GetShaderVisibility(desc.mStageVisibility);

                    rootParameter.DescriptorTable.NumDescriptorRanges = as<UINT>(descriptorRanges.size());
                    rootParameter.DescriptorTable.pDescriptorRanges = descriptorRanges.data();
                }

                const auto& samplerDescriptorRanges{ bindingLayout->GetDescriptorSamplerRanges() };
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
        rootParamConstants.Constants.ShaderRegister = mRootConstantIndex;
        rootParamConstants.Constants.RegisterSpace  = 0;
        rootParamConstants.Constants.Num32BitValues = m32BitValueCount;

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
        Fence* pFence{ checked_cast<Fence*>( fence ) };
        pFence->WaitForFenceValue( value );
    }

    auto Queue::Signal( IFence *fence, u64 value ) -> void {
        Fence* pFence{ checked_cast<Fence*>( fence ) };

        ID3D12Fence* d3d12Fence{ *pFence };
        ThrowIfFailed(mQueue->Signal(d3d12Fence, value));
    }

    auto Queue::ExecuteCommandLists( eastl::span<CommandListHandle> commands ) -> void {

    }

    auto Queue::Flush() -> void {

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
        //ClearState();
    }

    auto CommandList::End() -> void {
        //ThrowIfFailed(mCommandList->Close());
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

    auto CommandList::PushBarrier( const BufferBarrierDescription &barrierDescription ) -> void {
        Buffer* buffer{ checked_cast<Buffer*>( barrierDescription.mBuffer ) };
        ID3D12Resource* resource{ *buffer };

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = resource;
        barrier.Transition.StateBefore = d3d12::GetResourceState( barrierDescription.mStateBefore );
        barrier.Transition.StateAfter = d3d12::GetResourceState( barrierDescription.mStateAfter );
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        mResourceBarriers.emplace_back( barrier );
    }

    auto CommandList::PushBarrier( const TextureBarrierDescription &barrierDescription ) -> void {
        Texture* texture{ checked_cast<Texture*>( barrierDescription.mTexture ) };
        ID3D12Resource* resource{ *texture };

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = resource;
        barrier.Transition.StateBefore = d3d12::GetResourceState( barrierDescription.mStateBefore );
        barrier.Transition.StateAfter = d3d12::GetResourceState( barrierDescription.mStateAfter );
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        mResourceBarriers.emplace_back( barrier );
    }

    auto CommandList::BeginTrackingState( IBuffer *buffer, ResourceStates stateBits ) -> void {
        Buffer* d3d12Buffer{ checked_cast<Buffer*>( buffer ) };
        ID3D12Resource* resource{ *d3d12Buffer };

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = resource;
        barrier.Transition.StateBefore = d3d12::GetResourceState( buffer->GetResourceState() );
        barrier.Transition.StateAfter = d3d12::GetResourceState( stateBits );
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        mResourceBarriers.emplace_back( barrier );
    }

    auto CommandList::BeginTrackingState( ITexture *texture, ResourceStates stateBits ) -> void {
        Texture* d3d12Texture{ checked_cast<Texture*>( texture ) };
        ID3D12Resource* resource{ *d3d12Texture };

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = resource;
        barrier.Transition.StateBefore = d3d12::GetResourceState( texture->GetResourceState() );
        barrier.Transition.StateAfter = d3d12::GetResourceState( stateBits );
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        mResourceBarriers.emplace_back( barrier );
    }

    auto CommandList::SetResourceState( IBuffer *buffer, ResourceStates stateBits ) -> void {
        Buffer* d3d12Buffer{ checked_cast<Buffer*>( buffer ) };
        ID3D12Resource* resource{ *d3d12Buffer };

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = resource;
        barrier.Transition.StateBefore = d3d12::GetResourceState( buffer->GetResourceState() );
        barrier.Transition.StateAfter = d3d12::GetResourceState( stateBits );
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        mCommandList->ResourceBarrier(1, &barrier);
    }

    auto CommandList::SetResourceState( ITexture *texture, ResourceStates stateBits ) -> void {
        Texture* d3d12Texture{ checked_cast<Texture*>( texture ) };
        ID3D12Resource* resource{ *d3d12Texture };

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = resource;
        barrier.Transition.StateBefore = d3d12::GetResourceState( texture->GetResourceState() );
        barrier.Transition.StateAfter = d3d12::GetResourceState( stateBits );
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        mCommandList->ResourceBarrier(1, &barrier);
    }

    auto CommandList::SetBarrier( const BufferBarrierDescription &barrierDescription ) -> void {
        Buffer* buffer{ checked_cast<Buffer*>( barrierDescription.mBuffer ) };
        ID3D12Resource* resource{ *buffer };

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = resource;
        barrier.Transition.StateBefore = d3d12::GetResourceState( barrierDescription.mStateBefore );
        barrier.Transition.StateAfter = d3d12::GetResourceState( barrierDescription.mStateAfter );
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        mCommandList->ResourceBarrier(1, &barrier);
    }

    auto CommandList::SetBarrier( const TextureBarrierDescription &barrierDescription ) -> void {
        Texture* texture{ checked_cast<Texture*>( barrierDescription.mTexture ) };
        ID3D12Resource* resource{ *texture };

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = resource;
        barrier.Transition.StateBefore = d3d12::GetResourceState( barrierDescription.mStateBefore );
        barrier.Transition.StateAfter = d3d12::GetResourceState( barrierDescription.mStateAfter );
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        mCommandList->ResourceBarrier(1, &barrier);
    }

    auto CommandList::CommitBarriers() -> void {
        mCommandList->ResourceBarrier(as<UINT>(mResourceBarriers.size()), mResourceBarriers.data());
    }

    auto CommandList::SetEnableAutomaticBarriers( bool enable ) -> void {
        mEnableAutomaticBarriers = enable;
    }

    auto CommandList::SetClearColor( FramebufferHandle frameBuffer, Color color ) -> void {

    }

    auto CommandList::SetClearColor( TextureHandle renderTarget, Color color ) -> void {
        Device* device{ checked_cast<Device*>( mDevice ) };
        Texture* texture{ checked_cast<Texture*>( renderTarget.GetRaw() ) };

        const DeviceResources* deviceResources{ device->GetHeapResources() };
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandler{
            deviceResources->mRenderTargetViewHeap->GetCpuHandle( texture->GetRtvDescriptorIndex() ) };

        const float clearColor[]{ color.mR, color.mG, color.mB, color.mA };
        mCommandList->ClearRenderTargetView(rtvHandler, clearColor, 0, nullptr);
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
        Buffer* bSrc{ checked_cast<Buffer*>( buffer ) };

        switch (bSrc->GetHeapType()) {
            case HeapType::eDeviceLocal: {
                if (mEnableAutomaticBarriers) {
                    SetResourceState(buffer, ResourceStates::eCopyDest);
                }

                GpuUploadAllocation* allocation{ mUploadManager->SubAllocate( byteSize ) };
                SetResourceState( allocation->mBuffer, ResourceStates::eCopySource );

                std::memcpy(allocation->mMappedMemory, data, byteSize);

                Buffer* bDest{ checked_cast<Buffer*>( allocation->mBuffer ) };

                ID3D12Resource* d3d12BufferSrc{ *bSrc };
                ID3D12Resource* d3d12BufferDest{ *bDest };

                mCommandList->CopyBufferRegion( d3d12BufferDest, allocation->mOffset, d3d12BufferSrc, 0, byteSize );

                std::lock_guard lock{ mUploadAllocationsMutex };
                mUploadAllocations.emplace_back( allocation );
                break;
            }
            case HeapType::eUpload: {
                void* mappedAddress{ bSrc->PersistentMap() };

                MKT_ASSERT( mappedAddress, "Failed to map buffer" );
                std::memcpy(mappedAddress, data, byteSize);

                bSrc->PersistentUnmap();
                break;
            }
            case HeapType::eReadback:
                MKT_ASSERT( false, "Cannot upload on a readback heap." );
                break;
        }
    }

    auto CommandList::Copy( IBuffer *src, IBuffer *dest ) -> void {
        // https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-copybufferregion
        // Consider using the CopyResource method when copying an entire resource, and use this method for copying regions of a resource.
        Buffer* bSrc{ checked_cast<Buffer*>( src ) };
        Buffer* bDest{ checked_cast<Buffer*>( dest ) };

        ID3D12Resource* d3d12BufferSrc{ *bSrc };
        ID3D12Resource* d3d12BufferDest{ *bDest };

        mCommandList->CopyResource( d3d12BufferDest, d3d12BufferSrc );
    }

    auto CommandList::Copy( IBuffer *src, IBuffer *dest, size_t destOffset ) -> void {
        Buffer* bSrc{ checked_cast<Buffer*>( src ) };
        Buffer* bDest{ checked_cast<Buffer*>( dest ) };

        ID3D12Resource* d3d12BufferSrc{ *bSrc };
        ID3D12Resource* d3d12BufferDest{ *bDest };

        mCommandList->CopyBufferRegion( d3d12BufferDest, destOffset, d3d12BufferSrc, 0, src->GetSizeBytes() );
    }

    auto CommandList::Copy( IBuffer *dest, ITexture *src ) -> void {

    }

    auto CommandList::BeginRendering( GraphicsState &state ) -> void {
        Device* device{ checked_cast<Device*>( mDevice ) };
        const DeviceResources* deviceResources{ device->GetHeapResources() };

        eastl::fixed_vector<D3D12_CPU_DESCRIPTOR_HANDLE, kMaxRenderTargets> rtvHandles{};

        for (auto& renderTarget : state.mCurrentRenderTargets ) {
            if (mEnableAutomaticBarriers) {
                SetResourceState( renderTarget.mRenderTarget.GetRaw(), ResourceStates::eRenderTarget );
            }

            Texture* texture{ checked_cast<Texture*>( renderTarget.mRenderTarget.GetRaw() ) };
            D3D12_CPU_DESCRIPTOR_HANDLE rtvHandler{
                deviceResources->mRenderTargetViewHeap->GetCpuHandle( texture->GetRtvDescriptorIndex() ) };

            const float clearColor[]{
                renderTarget.mClearColor.mR,
                renderTarget.mClearColor.mG,
                renderTarget.mClearColor.mB,
                renderTarget.mClearColor.mA };
            mCommandList->ClearRenderTargetView(rtvHandler, clearColor, 0, nullptr);

            rtvHandles.emplace_back(  rtvHandler );
        }

        mCommandList->OMSetRenderTargets(as<UINT>(rtvHandles.size()), rtvHandles.data(), FALSE, nullptr);
    }

    auto CommandList::EndRendering() -> void {

    }

    auto CommandList::BindPipeline( IPipeline* pipeline ) -> void {
        const PipelineType pipelineType{ pipeline->GetPipelineType() };
        if ( pipelineType == PipelineType::eGraphics ) {
            auto* gfxPipeline{ checked_cast<GraphicsPipeline*>( pipeline ) };
            const auto& desc{ gfxPipeline->GetDescription() };
            ID3D12PipelineState* gfxPipelineState{ *gfxPipeline };
            D3D_PRIMITIVE_TOPOLOGY gfxPipelineTopology{ d3d12::GetCmdTopologyType( desc.mPrimitiveTopology ) };

            mCommandList->SetPipelineState( gfxPipelineState );
            mCommandList->IASetPrimitiveTopology( gfxPipelineTopology );
        } else if ( pipelineType == PipelineType::eCompute ) {
            auto* computePipeline{ checked_cast<ComputePipeline*>( pipeline ) };
            ID3D12PipelineState* computePipelineState{ *computePipeline };

            mCommandList->SetPipelineState( computePipelineState );
        }
    }

    auto CommandList::SetViewport( eastl::span<const Viewport> viewports ) -> void {
        eastl::fixed_vector<D3D12_VIEWPORT, kMaxViewports> d3dViewports{};
        for ( const auto &viewport: viewports ) {
            D3D12_VIEWPORT value{
                .TopLeftX = viewport.mMinX,
                .TopLeftY = viewport.mMinY,
                .Width = viewport.GetWidth(),
                .Height = viewport.GetHeight(),
                .MinDepth = viewport.mMinZ,
                .MaxDepth = viewport.mMaxZ };
            d3dViewports.emplace_back( value );
        }

        mCommandList->RSSetViewports( as<UINT>( d3dViewports.size() ), d3dViewports.data() );
    }


    auto CommandList::SetScissors( eastl::span<const Rect> scissorRects ) -> void {
        eastl::fixed_vector<D3D12_RECT, kMaxScissors> scissors{};
        for (const auto& scissor : scissorRects) {
            scissors.emplace_back( D3D12_RECT{
                .left   = scissor.mMinX,
                .top    = scissor.mMinY,
                .right  = scissor.mMinX + as<LONG>(scissor.ComputeWidth()),
                .bottom = scissor.mMinY + as<LONG>(scissor.ComputeHeight())
            } );
        }

        mCommandList->RSSetScissorRects(as<UINT>(scissors.size()), scissors.data());
    }

    auto CommandList::SetViewportState( const ViewportState &vs ) -> void {
        SetViewport( vs.mViewports );
        SetScissors( vs.mScissorRects );
    }

    auto CommandList::BindIndexBuffer( IBuffer *buffer ) -> void {
        Buffer* indexBuffer{ checked_cast<Buffer*>( buffer ) };
        ID3D12Resource* resource{ *indexBuffer };

        D3D12_INDEX_BUFFER_VIEW indexBufferView{};

        indexBufferView.BufferLocation = resource->GetGPUVirtualAddress();
        indexBufferView.Format = d3d12::GetFormat( indexBuffer->GetFormat() );
        indexBufferView.SizeInBytes = indexBuffer->GetSizeBytes();

        mCommandList->IASetIndexBuffer(&indexBufferView);
    }

    auto CommandList::BindVertexBuffer( const VertexBufferBinding &binding ) -> void {
        BindVertexBuffer(eastl::array{ binding } );
    }

    auto CommandList::BindVertexBuffer( eastl::span<const VertexBufferBinding> binding ) -> void {
        eastl::fixed_vector<D3D12_VERTEX_BUFFER_VIEW, kMaxVertexBuffers> vertexBufferViews{};

        for (const auto& vertexBuffer : binding) {
            Buffer* buffer{ checked_cast<Buffer*>( vertexBuffer.mBuffer ) };
            ID3D12Resource* resource{ *buffer };

            D3D12_VERTEX_BUFFER_VIEW bufferView{};
            bufferView.SizeInBytes = buffer->GetSizeBytes();
            bufferView.BufferLocation = resource->GetGPUVirtualAddress();
            bufferView.StrideInBytes = vertexBuffer.mElementStride;

            vertexBufferViews.emplace_back( bufferView );
        }

        mCommandList->IASetVertexBuffers(0, as<UINT>(vertexBufferViews.size()), vertexBufferViews.data());
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

        descriptorHeaps.emplace_back( deviceResources->mSamplerHeap->GetHeap() );
        descriptorHeaps.emplace_back( deviceResources->mShaderResourceViewHeap->GetHeap() );

        // At most one CBV/SRV/UAV combined heap and one Sampler heap can be bound at any one time.
        // These heaps are shared between both the graphics and compute pipelines (described in their PSOs).
        mCommandList->SetDescriptorHeaps(as<UINT>(descriptorHeaps.size()), descriptorHeaps.data());

        const auto bindResource{
            [&](const DescriptorRange& range, u32 rootParamIndex, PipelineType pipelineType) {
                if (range.mCount == 0) {
                    return;
                }

                switch (pipelineType) {
                    case PipelineType::eGraphics:
                        mCommandList->SetGraphicsRootDescriptorTable(rootParamIndex, range.mGpuHandle);
                        break;
                    case PipelineType::eCompute:
                        mCommandList->SetComputeRootDescriptorTable(rootParamIndex, range.mGpuHandle);
                        break;
                    default:;
                }
            }
        };

        for (const auto& [rootParameterIndex, bindingSet] : desc.mResourceSets) {
            const BindingSet* table{ checked_cast<BindingSet*>(bindingSet) };
            const DescriptorRange& srvRange{ table->GetSrvRange() };
            const DescriptorRange& samplerRange{ table->GetSamplerRange() };

            bindResource(srvRange, rootParameterIndex, desc.mBindPoint);

            if (samplerRange.mCount > 0) {
                // Samplers come right after shader resource views in the root signature
                // that's why we add 1, see PipelineLayout initialization
                bindResource(samplerRange, rootParameterIndex + 1, desc.mBindPoint);
            }
        }
    }

    auto CommandList::Draw( const DrawArguments &args ) -> void {
        mCommandList->DrawInstanced(
            args.mVertexCount,
            args.mInstanceCount,
            args.mFirstVertex,
            args.mFirstInstance
        );
    }

    auto CommandList::DrawIndexed( const DrawArguments &args ) -> void {
        mCommandList->DrawIndexedInstanced(
            args.mIndexCount,
            args.mInstanceCount,
            args.mFirstIndex,
            args.mVertexOffset,
            args.mFirstInstance
        );
    }

    auto CommandList::DrawIndirect( u32 offset, u32 drawCount ) -> void {

    }

    auto CommandList::DrawIndexedIndirect( u32 offset, u32 drawCount ) -> void {

    }

    auto CommandList::Dispatch( u32 groupsX, u32 groupsY, u32 groupsZ ) -> void {
        mCommandList->Dispatch( groupsX, groupsY, groupsZ );
    }

    auto CommandList::SetPushConstants( IPipelineLayout *pipelineLayout, const void *data, size_t byteSize, ShaderStage visibility ) -> void {
        PipelineLayout* d3d12PipelineLayout{ checked_cast<PipelineLayout*>( pipelineLayout ) };
        u32 rootParameterIndex{ d3d12PipelineLayout->GetRootConstantIndex() };
        u32 numValues32Bit{ as<u32>( ( byteSize + 3 ) / 4 ) };

        if ( visibility == rhi::ShaderFlagsBits::kCompute ) {
            mCommandList->SetComputeRoot32BitConstants(
                rootParameterIndex,
                numValues32Bit,
                data,
                0
            );
        } else {
            mCommandList->SetGraphicsRoot32BitConstants(
                rootParameterIndex,
                numValues32Bit,
                data,
                0
            );
        }
    }

    auto CommandList::GetNativeHandle( ObjectType type ) -> Object{
        return ICommandList::GetNativeHandle( type );
    }

    auto CommandList::GetNativeHandle( ObjectType type ) const -> Object{
        return ICommandList::GetNativeHandle( type );
    }

    CommandList::operator ID3D12GraphicsCommandList*() const {
        return mCommandList.Get();
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

        mUploadManager = checked_cast<Device*>(mDevice)->GetUploadManager();

        mIsAllocated = true;
    }

    auto CommandList::Release() -> void {
        mIsAllocated = false;
    }

    auto CommandList::ClearState() -> void {
        ThrowIfFailed( mCommandAllocator->Reset() );
        ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), nullptr));

        for (auto& subAllocations : mUploadAllocations) {
            // Set it to false we to tell the allocator
            // this allocation can already be destroyed
            subAllocations->mInUse.clear();
        }
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
        InitDescriptorHeapManager();

#if defined(_DEBUG)
        ThrowIfFailed(mDevice->QueryInterface( IID_PPV_ARGS(&mDebugDevice) ));
#endif
    }

    auto Device::Shutdown() -> void {
        mQueues.clear();

#if defined(_DEBUG)
        mDebugDevice->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL);
        if ( mInfoQueue1 ) {
            mInfoQueue1->UnregisterMessageCallback( mInfoQueueCallbackCookie );
        }
#endif
    }

    auto Device::InitInfoQueue() -> void {
#if defined(_DEBUG)
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
#endif
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

    auto Device::InitDescriptorHeapManager() -> void {
        mResourceHeaps.mRenderTargetViewHeap = eastl::make_unique<StaticDescriptorHeap>( this );
        mResourceHeaps.mDepthStencilViewHeap = eastl::make_unique<StaticDescriptorHeap>( this );

        mResourceHeaps.mSamplerHeap = eastl::make_unique<StaticDescriptorHeap>( this );
        mResourceHeaps.mShaderResourceViewHeap = eastl::make_unique<StaticDescriptorHeap>( this );


        // Pre-allocate
        ThrowIfFailed( mResourceHeaps.mRenderTargetViewHeap->AllocateResources( D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 100, false ) );
        ThrowIfFailed( mResourceHeaps.mDepthStencilViewHeap->AllocateResources( D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 100, false ) );

        ThrowIfFailed( mResourceHeaps.mSamplerHeap->AllocateResources( D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 100, true ) );
        ThrowIfFailed( mResourceHeaps.mShaderResourceViewHeap->AllocateResources( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 100, true ) );
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
        BindingSetHandle set{ Ref<BindingSet>::Spawn( desc, layout, mResourceHeaps ) };

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
        Fence* fence{ checked_cast<Fence*>( handle.GetRaw() ) };
        mQueues[type]->Signal( fence, fenceValue );
    }

    auto Device::ExecutePendingCommands() -> void {

    }

    auto Device::ExecuteCommands( CommandListHandle cmdList ) -> void {
        eastl::array commandLists{ cmdList };

        ExecuteCommands( commandLists );
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

    auto Device::GetUploadManager() -> GpuUploadManager* {
        return mUploadManager.get();
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
        mUploadManager->ReclaimMemory();
    }

    auto Device::SubmitCommands( CommandListHandle cmd ) -> u64 {
        MKT_ASSERT( mQueues.contains( cmd->GetQueueType() ), "No queue" );
        return mQueues[cmd->GetQueueType()]->SubmitCommandList( cmd );
    }
}

#endif
