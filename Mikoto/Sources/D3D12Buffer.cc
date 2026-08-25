//    Copyright 2025 ケイト
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

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Buffer.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

// D3D12 extension library.
#include <directx/d3d12.h>
#include <directx/d3dx12.h>

#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <wrl.h>

#include <Renderer/Rhi/D3D12/D3D12Buffer.hh>
#include <Renderer/Rhi/D3D12/D3D12Device.hh>
#include <Renderer/Rhi/D3D12/Direct3D12Helpers.hh>
#include <Renderer/Rhi/D3D12/D3D12MemoryAllocator.hh>

namespace mikoto::renderer::d3d12 {

    using namespace mikoto::core;
    using namespace mikoto::renderer::rhi;

    Buffer::Buffer( const rhi::BufferCreateDescription &createInfo, DeviceResources& resources )
        : IBuffer{ createInfo }, mResources{ MKT_ADDRESSOF( resources ) }
    {
    }

    auto Buffer::PersistentMap() -> void* {
        auto allocator{ checked_cast<Device*>( mDevice )->GetAllocator() };
        mMappedAddress = allocator->MapBuffer( mAllocation );

        return mMappedAddress;
    }

    auto Buffer::PersistentUnmap() -> void {
        auto allocator{ checked_cast<Device*>( mDevice )->GetAllocator() };
        allocator->UnmapBuffer(  mAllocation );

        mMappedAddress = nullptr;
    }

    auto Buffer::SetDebugName( eastl::string_view name ) -> void {
        mAllocation.mResource->SetName( string::ToWide( name ).c_str() );
    }

    auto Buffer::GetNativeHandle( rhi::ObjectType type ) -> rhi::Object {
        return IBuffer::GetNativeHandle( type );
    }

    auto Buffer::GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object {
        return IBuffer::GetNativeHandle( type );
    }

    auto Buffer::IsMapped() const -> bool {
        return mMappedAddress != nullptr;
    }

    auto Buffer::GetMappedAddress() -> void * {
        return mMappedAddress;
    }

    auto Buffer::GetMappedAddress() const -> const void * {
        return mMappedAddress;
    }

    auto Buffer::CreateCBV( SIZE_T descriptor, BufferRange range, Format format ) const -> void {
        Device* device{ checked_cast<Device*>( mDevice ) };
        ID3D12Device2* d3d12Device{ device->GetDevice() };

        range.Validate( GetSizeBytes() );

        D3D12_CONSTANT_BUFFER_VIEW_DESC viewDesc{};
        viewDesc.BufferLocation = mAllocation.mResource->GetGPUVirtualAddress() + range.mByteOffset;
        viewDesc.SizeInBytes = core::align( as<UINT>( range.mByteSize ), kConstantBufferOffsetSizeAlignment );
        d3d12Device->CreateConstantBufferView( MKT_ADDRESSOF( viewDesc ), { descriptor } );
    }

    auto Buffer::CreateSRV( SIZE_T descriptor, BufferRange range, ResourceType resourceType, Format format ) const -> void {
        Device* device{ checked_cast<Device*>( mDevice ) };
        ID3D12Device2* d3d12Device{ device->GetDevice() };

        D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc{};
        viewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        Format formatToUse{ mFormat };
        if (format != Format::eUnknown) {
            formatToUse = format;
        }

        ResourceType resourceTypeToUse{ mResourceType };
        if (resourceType != ResourceType::eInvalid) {
            resourceTypeToUse = resourceType;
        }

        range.Validate( GetSizeBytes() );

        switch (resourceTypeToUse) {
            case ResourceType::eStructuredBuffer_SRV:
                MKT_ASSERT(mElementSize != 0, "Structured buffer requires element size");
                viewDesc.Format = DXGI_FORMAT_UNKNOWN;
                viewDesc.Buffer.FirstElement = range.mByteOffset / mElementSize;
                viewDesc.Buffer.NumElements = as<UINT>( range.mByteSize / mElementSize );
                viewDesc.Buffer.StructureByteStride = mElementSize;
                break;

            case ResourceType::eRawBuffer_SRV:
                viewDesc.Format = DXGI_FORMAT_R32_TYPELESS;
                viewDesc.Buffer.FirstElement = range.mByteOffset / 4;
                viewDesc.Buffer.NumElements = as<UINT>( range.mByteSize / 4 );
                viewDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
                break;

            case ResourceType::eTypedBuffer_SRV:
            {
                MKT_ASSERT(formatToUse != Format::eUnknown, "Typed buffer needs a valid format");
                const FormatInfo& formatInfo{ rhi::GetFormatInfo(formatToUse) };

                viewDesc.Format = d3d12::GetFormat( mFormat );
                viewDesc.Buffer.FirstElement = range.mByteOffset / formatInfo.mBytesPerBlock;
                viewDesc.Buffer.NumElements = as<UINT>( range.mByteSize / formatInfo.mBytesPerBlock );
                break;
            }
            default:
                return;
        }

        d3d12Device->CreateShaderResourceView(mAllocation.mResource.Get(), &viewDesc, { descriptor });
    }

    auto Buffer::CreateUAV( SIZE_T descriptor, BufferRange range, ResourceType resourceType, Format format ) const -> void {
        Device* device{ checked_cast<Device*>( mDevice ) };
        ID3D12Device2* d3d12Device{ device->GetDevice() };

        D3D12_UNORDERED_ACCESS_VIEW_DESC viewDesc{};
        viewDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

        Format formatToUse{ mFormat };
        if (format != Format::eUnknown) {
            formatToUse = format;
        }

        ResourceType resourceTypeToUse{ mResourceType };
        if (resourceType != ResourceType::eInvalid) {
            resourceTypeToUse = resourceType;
        }

        range.Validate( GetSizeBytes() );

        switch (resourceTypeToUse) {
            case ResourceType::eStructuredBuffer_UAV:
                MKT_ASSERT(mElementSize != 0, "Structured buffer requires element size");

                viewDesc.Format = DXGI_FORMAT_UNKNOWN;
                viewDesc.Buffer.FirstElement = range.mByteOffset / mElementSize;
                viewDesc.Buffer.NumElements = as<UINT>( range.mByteSize / mElementSize );
                viewDesc.Buffer.StructureByteStride = mElementSize;
                break;

            case ResourceType::eRawBuffer_UAV:
                viewDesc.Format = DXGI_FORMAT_R32_TYPELESS;
                viewDesc.Buffer.FirstElement = range.mByteOffset / 4;
                viewDesc.Buffer.NumElements = as<UINT>( range.mByteSize / 4 );
                viewDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
                break;

            case ResourceType::eTypedBuffer_UAV:
            {
                MKT_ASSERT(formatToUse != Format::eUnknown, "Typed buffer needs a valid format");
                const FormatInfo& formatInfo{ rhi::GetFormatInfo(formatToUse) };

                viewDesc.Format = d3d12::GetFormat( formatToUse );
                viewDesc.Buffer.FirstElement = range.mByteOffset / formatInfo.mBytesPerBlock;
                viewDesc.Buffer.NumElements = (UINT)(range.mByteSize / formatInfo.mBytesPerBlock);
                break;
            }

            default:
                return;
        }

        d3d12Device->CreateUnorderedAccessView(mAllocation.mResource.Get(), nullptr, &viewDesc, { descriptor });
    }

    auto Buffer::CreateNullSRV( SIZE_T descriptor, Format format, ID3D12Device2* device ) -> void {
        const DXGI_FORMAT d3d12Format{ d3d12::GetFormat( format == Format::eUnknown ? Format::eR32_UINT : format ) };

        D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc{};
        viewDesc.Format = d3d12Format;
        viewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        device->CreateShaderResourceView(nullptr, &viewDesc, { descriptor });
    }

    auto Buffer::CreateNullUAV( SIZE_T descriptor, Format format, ID3D12Device2* device ) -> void {
        const DXGI_FORMAT d3d12Format{ d3d12::GetFormat( format == Format::eUnknown ? Format::eR32_UINT : format ) };

        D3D12_UNORDERED_ACCESS_VIEW_DESC viewDesc{};
        viewDesc.Format = d3d12Format;
        viewDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        device->CreateUnorderedAccessView(nullptr, nullptr, &viewDesc, { descriptor });
    }

    Buffer::operator ID3D12Resource*() const {
        return mAllocation.mResource.Get();
    }

    Buffer::~Buffer() {
        if (mIsAllocated) {
            Release();
        }
    }

    auto Buffer::Release() -> void {
        mIsAllocated = false;
    }

    auto Buffer::Initialize() -> void {
        Device* device{ checked_cast<Device*>( mDevice ) };

        // Allocate buffer memory
        mAllocation.mDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        mAllocation.mDesc.Alignment = 0;
        mAllocation.mDesc.Width = mElementCount == 0 ? mElementSize :
            mElementSize * mElementCount;
        mAllocation.mDesc.Height = 1;
        mAllocation.mDesc.DepthOrArraySize = 1;
        mAllocation.mDesc.MipLevels = 1;
        mAllocation.mDesc.Format = DXGI_FORMAT_UNKNOWN;

        mAllocation.mDesc.SampleDesc.Quality = 0;
        mAllocation.mDesc.SampleDesc.Count = 1;
        mAllocation.mDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        mAllocation.mDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        mAllocation.mAllocDesc.HeapType = d3d12::GetHeapType(mHeapType);

        auto* allocator{ device->GetAllocator() };
        ThrowIfFailed( allocator->AllocateBuffer( mAllocation ) );

        if (!mUploadContents.IsEmpty()) {
            u64 fenceValue{ 0 };
            FenceHandle fence{ mDevice->CreateFence( fenceValue++ ) };
            CommandListHandle cmd{ mDevice->CreateCommandList( QueueType::eTransfer ) };
            cmd->Begin( { .mScopeName = string::Format( "Buffer upload: {}", mDebugName ) } );
            cmd->Write( this, mUploadContents->GetData(), mUploadContents->GetSize() );
            cmd->End();

            // Signal fenceValue on one fence on completion of these
            // commands, then we wait for that completion this blocks the caller
            // but client should ideally offload this task to worker threads
            const auto submitInfo{ SubmitInfo{}
                .AddSignal( fence, fenceValue )
                .AddCommandList( cmd ) };
            mDevice->GetQueue( QueueType::eTransfer )->ExecuteCommandLists( submitInfo );
            ( void )fence->Wait( fenceValue, eastl::numeric_limits<u64>::max() );
        }

        if (!mKeepInitializerResources) {
            mUploadContents.Reset();
        }

        mIsAllocated = true;
    }
}

#endif

