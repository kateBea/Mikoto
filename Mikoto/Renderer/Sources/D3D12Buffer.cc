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
#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Logging/Assert.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Buffer.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )

// D3D12 extension library.
#include <Platform/Windows/Windows.hh>

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

    Buffer::Buffer( const rhi::BufferCreateDescription& createInfo, DeviceResources& resources )
        : mUploadContents{ createInfo.mSpanHandle }, mElementCount{ createInfo.mElementCount }, mElementSize{ createInfo.mElementSize }, mDataType{ createInfo.mDataType }, mUsage{ createInfo.mUsageFlags }, mInitialState{ createInfo.mInitialState }, mIsVolatile{ createInfo.mIsVolatile }, mMaxVersions{ createInfo.mMaxVersions }, mFormat{ createInfo.mFormat }, mResources{ MKT_ADDRESSOF( resources ) } {
        mHeapType = createInfo.mHeapType;
        mResourceType = createInfo.mResourceType;
    }

    auto Buffer::PersistentMap() -> void* {
        MKT_BEGIN_PROFILER();
        auto allocator{ core::checked_cast<Device*>( mDevice )->GetAllocator() };
        mMappedAddress = allocator->MapBuffer( mAllocation );

        return mMappedAddress;
    }

    auto Buffer::PersistentUnmap() -> void {
        MKT_BEGIN_PROFILER();
        auto allocator{ core::checked_cast<Device*>( mDevice )->GetAllocator() };
        allocator->UnmapBuffer( mAllocation );

        mMappedAddress = nullptr;
    }

    auto Buffer::SetDebugName( eastl::string_view name ) -> void {
        MKT_BEGIN_PROFILER();

        if ( name.empty() ) {
            return;
        }

        mDebugName = name;
        mAllocation.mResource->SetName( string::ToWide( name ).c_str() );
    }

    auto Buffer::GetGpuDeviceAddress() -> rhi::DeviceAddress {
        MKT_BEGIN_PROFILER();
        return ( renderer::rhi::DeviceAddress )mAllocation.mResource->GetGPUVirtualAddress();
    }

    auto Buffer::GetShaderDeviceAddress() -> rhi::DeviceAddress {
        if ( mSrvDescriptorIndex == kInvalidDescriptorIndex ) {
            MKT_THROW_RUNTIME_ERROR( "D3D12 buffer has no shader descriptor." );
        }

        if ( GetSizeBytes() > ( core::u64{ 1 } << 32 ) ) {
            MKT_THROW_RUNTIME_ERROR( "D3D12 shader buffer references support at most 4 GiB per buffer." );
        }

        return ( static_cast<core::u64>( mSrvDescriptorIndex ) + 1 ) << 32;
    }

    auto Buffer::GetNativeHandle( rhi::ObjectType type ) -> rhi::Object {
        MKT_BEGIN_PROFILER();
        return renderer::rhi::IBuffer::GetNativeHandle( type );
    }

    auto Buffer::GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object {
        MKT_BEGIN_PROFILER();
        return renderer::rhi::IBuffer::GetNativeHandle( type );
    }

    auto Buffer::IsMapped() const -> bool {
        MKT_BEGIN_PROFILER();
        return mMappedAddress != nullptr;
    }

    auto Buffer::GetMappedAddress() -> void* {
        MKT_BEGIN_PROFILER();
        return mMappedAddress;
    }

    auto Buffer::GetMappedAddress() const -> const void* {
        MKT_BEGIN_PROFILER();
        return mMappedAddress;
    }

    auto Buffer::GetResourceIndex() const -> DescriptorIndex {
        MKT_BEGIN_PROFILER();
        return mShaderResourceIndex;
    }

    auto Buffer::CreateCBV( core::usize descriptor, renderer::rhi::BufferRange range, renderer::rhi::Format format ) const -> void {
        MKT_BEGIN_PROFILER();
        Device* device{ core::checked_cast<Device*>( mDevice ) };
        ID3D12Device2* d3d12Device{ device->GetDevice() };

        range.Validate( GetSizeBytes() );

        D3D12_CONSTANT_BUFFER_VIEW_DESC viewDesc{};
        viewDesc.BufferLocation = mAllocation.mResource->GetGPUVirtualAddress() + range.mByteOffset;
        viewDesc.SizeInBytes = core::align( core::as<UINT>( range.mByteSize ), kConstantBufferOffsetSizeAlignment );
        d3d12Device->CreateConstantBufferView( MKT_ADDRESSOF( viewDesc ), { descriptor } );
    }

    auto Buffer::CreateSRV( core::usize descriptor, renderer::rhi::BufferRange range, renderer::rhi::ResourceType resourceType, renderer::rhi::Format format ) const -> void {
        MKT_BEGIN_PROFILER();
        Device* device{ core::checked_cast<Device*>( mDevice ) };
        ID3D12Device2* d3d12Device{ device->GetDevice() };

        D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc{};
        viewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        renderer::rhi::Format formatToUse{ mFormat };

        if ( format != renderer::rhi::Format::eUnknown ) {
            formatToUse = format;
        }

        renderer::rhi::ResourceType resourceTypeToUse{ mResourceType };

        if ( resourceType != renderer::rhi::ResourceType::eInvalid ) {
            resourceTypeToUse = resourceType;
        }

        range.Validate( GetSizeBytes() );

        switch ( resourceTypeToUse ) {
            case renderer::rhi::ResourceType::eStructuredBuffer_SRV:
                MKT_ASSERT( mElementSize != 0, "Structured buffer requires element size" );
                viewDesc.Format = DXGI_FORMAT_UNKNOWN;
                viewDesc.Buffer.FirstElement = range.mByteOffset / mElementSize;
                viewDesc.Buffer.NumElements = core::as<UINT>( range.mByteSize / mElementSize );
                viewDesc.Buffer.StructureByteStride = mElementSize;
                break;

            case renderer::rhi::ResourceType::eRawBuffer_SRV:
                viewDesc.Format = DXGI_FORMAT_R32_TYPELESS;
                viewDesc.Buffer.FirstElement = range.mByteOffset / 4;
                viewDesc.Buffer.NumElements = core::as<UINT>( range.mByteSize / 4 );
                viewDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
                break;

            case renderer::rhi::ResourceType::eTypedBuffer_SRV: {
                MKT_ASSERT( formatToUse != renderer::rhi::Format::eUnknown, "Typed buffer needs a valid format" );
                const renderer::rhi::FormatInfo& formatInfo{ rhi::GetFormatInfo( formatToUse ) };

                viewDesc.Format = d3d12::GetFormat( formatToUse );
                viewDesc.Buffer.FirstElement = range.mByteOffset / formatInfo.mBytesPerBlock;
                viewDesc.Buffer.NumElements = core::as<UINT>( range.mByteSize / formatInfo.mBytesPerBlock );
                break;
            }

            default:
                return;
        }

        d3d12Device->CreateShaderResourceView( mAllocation.mResource.Get(), &viewDesc, { descriptor } );
    }

    auto Buffer::CreateUAV( core::usize descriptor, renderer::rhi::BufferRange range, renderer::rhi::ResourceType resourceType, renderer::rhi::Format format ) const -> void {
        MKT_BEGIN_PROFILER();
        Device* device{ core::checked_cast<Device*>( mDevice ) };
        ID3D12Device2* d3d12Device{ device->GetDevice() };

        D3D12_UNORDERED_ACCESS_VIEW_DESC viewDesc{};
        viewDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

        renderer::rhi::Format formatToUse{ mFormat };

        if ( format != renderer::rhi::Format::eUnknown ) {
            formatToUse = format;
        }

        renderer::rhi::ResourceType resourceTypeToUse{ mResourceType };

        if ( resourceType != renderer::rhi::ResourceType::eInvalid ) {
            resourceTypeToUse = resourceType;
        }

        range.Validate( GetSizeBytes() );

        switch ( resourceTypeToUse ) {
            case renderer::rhi::ResourceType::eStructuredBuffer_UAV:
                MKT_ASSERT( mElementSize != 0, "Structured buffer requires element size" );

                viewDesc.Format = DXGI_FORMAT_UNKNOWN;
                viewDesc.Buffer.FirstElement = range.mByteOffset / mElementSize;
                viewDesc.Buffer.NumElements = core::as<UINT>( range.mByteSize / mElementSize );
                viewDesc.Buffer.StructureByteStride = mElementSize;
                break;

            case renderer::rhi::ResourceType::eRawBuffer_UAV:
                viewDesc.Format = DXGI_FORMAT_R32_TYPELESS;
                viewDesc.Buffer.FirstElement = range.mByteOffset / 4;
                viewDesc.Buffer.NumElements = core::as<UINT>( range.mByteSize / 4 );
                viewDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
                break;

            case renderer::rhi::ResourceType::eTypedBuffer_UAV: {
                MKT_ASSERT( formatToUse != renderer::rhi::Format::eUnknown, "Typed buffer needs a valid format" );
                const renderer::rhi::FormatInfo& formatInfo{ rhi::GetFormatInfo( formatToUse ) };

                viewDesc.Format = d3d12::GetFormat( formatToUse );
                viewDesc.Buffer.FirstElement = range.mByteOffset / formatInfo.mBytesPerBlock;
                viewDesc.Buffer.NumElements = ( UINT )( range.mByteSize / formatInfo.mBytesPerBlock );
                break;
            }

            default:
                return;
        }

        d3d12Device->CreateUnorderedAccessView( mAllocation.mResource.Get(), nullptr, &viewDesc, { descriptor } );
    }

    auto Buffer::CreateNullSRV( core::usize descriptor, renderer::rhi::Format format, ID3D12Device2* device ) -> void {
        MKT_BEGIN_PROFILER();
        const DXGI_FORMAT d3d12Format{ d3d12::GetFormat( format == renderer::rhi::Format::eUnknown ? renderer::rhi::Format::eR32_UINT : format ) };

        D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc{};
        viewDesc.Format = d3d12Format;
        viewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        device->CreateShaderResourceView( nullptr, &viewDesc, { descriptor } );
    }

    auto Buffer::CreateNullUAV( core::usize descriptor, renderer::rhi::Format format, ID3D12Device2* device ) -> void {
        MKT_BEGIN_PROFILER();
        const DXGI_FORMAT d3d12Format{ d3d12::GetFormat( format == renderer::rhi::Format::eUnknown ? renderer::rhi::Format::eR32_UINT : format ) };

        D3D12_UNORDERED_ACCESS_VIEW_DESC viewDesc{};
        viewDesc.Format = d3d12Format;
        viewDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        device->CreateUnorderedAccessView( nullptr, nullptr, &viewDesc, { descriptor } );
    }

    Buffer::operator ID3D12Resource*() const {
        MKT_BEGIN_PROFILER();
        return mAllocation.mResource.Get();
    }

    Buffer::~Buffer() {
        MKT_BEGIN_PROFILER();

        if ( mAllocation.mAllocation ) {
            Destroy();
        }
    }

    auto Buffer::Destroy() -> void {
        MKT_BEGIN_PROFILER();

        if ( mSrvDescriptorIndex != kInvalidDescriptorIndex ) {
            mResources->mResourceDescriptorHeap->ReleaseDescriptors( mSrvDescriptorIndex, 2 );
            mSrvDescriptorIndex = kInvalidDescriptorIndex;
        }

        if ( mMappedAddress ) {
            PersistentUnmap();
        }

        core::checked_cast<Device*>( mDevice )->GetAllocator()->FreeBuffer( mAllocation );
        mIsAllocated = false;
    }

    auto Buffer::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        Device* device{ core::checked_cast<Device*>( mDevice ) };

        // https://logins.github.io/graphics/2020/07/31/DX12ResourceHandling.html
        // We can also use D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT constant
        // instead of specifying buffer and base textures alignment size for the
        // functions that require it and D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT
        // for MSAA textures, but we can also set the field to 0, and that will
        // be automatically resolved by the SDK depending on the resource.
        // Still those values represent the minimum size for a resource heap memory allocation.

        // Allocate buffer memory
        mAllocation.mDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        mAllocation.mDesc.Alignment = 0;
        mAllocation.mDesc.Width = mElementCount == 0 ? mElementSize : mElementSize * mElementCount;
        mAllocation.mDesc.Height = 1;
        mAllocation.mDesc.DepthOrArraySize = 1;
        mAllocation.mDesc.MipLevels = 1;
        mAllocation.mDesc.Format = DXGI_FORMAT_UNKNOWN;

        mAllocation.mDesc.SampleDesc.Quality = 0;
        mAllocation.mDesc.SampleDesc.Count = 1;
        mAllocation.mDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        mAllocation.mDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        if ( ( mHeapType == renderer::rhi::HeapType::eDeviceLocal &&
                 ( mUsage.Has( rhi::BufferUsageFlagsBits::Structured ) ||
                     mUsage.Has( rhi::BufferUsageFlagsBits::AccelStructureStorage ) ||
                     mUsage.Has( rhi::BufferUsageFlagsBits::AccelStructureScratch ) ) ) ||
             mResourceType == renderer::rhi::ResourceType::eRawBuffer_UAV ||
             mResourceType == renderer::rhi::ResourceType::eTypedBuffer_UAV ||
             mResourceType == renderer::rhi::ResourceType::eStructuredBuffer_UAV ) {
            mAllocation.mDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        }

        // Constant buffer needs to be 256 bytes at least
        if ( mResourceType == renderer::rhi::ResourceType::eConstantBuffer ) {
            mAllocation.mDesc.Width = core::align( mAllocation.mDesc.Width, static_cast<core::u64>( d3d12::kConstantBufferOffsetSizeAlignment ) );
        }

        // Allocator creates them in common state
        mResourceState = renderer::rhi::ResourceStates::eCommon;

        mAllocation.mAllocDesc.HeapType = d3d12::GetHeapType( mHeapType );
        mAllocation.mAllocDesc.ExtraHeapFlags |= D3D12_HEAP_FLAG_CREATE_NOT_ZEROED;

        if ( mHeapType == renderer::rhi::HeapType::eUpload && mUsage.Has( rhi::BufferUsageFlagsBits::VideoDecodeSource ) ) {
            mAllocation.mAllocDesc.CustomPool = device->GetAllocator()->GetVideoBitstreamPool();
            mAllocation.mInitialState = D3D12_RESOURCE_STATE_VIDEO_DECODE_READ;
            mResourceState = renderer::rhi::ResourceStates::eVideoDecodeReference;
        }
        else if ( mHeapType == renderer::rhi::HeapType::eUpload ) {
            mAllocation.mInitialState = D3D12_RESOURCE_STATE_GENERIC_READ;
            // RHI tracks the intended access; the native upload heap remains GENERIC_READ.
            mResourceState = mInitialState;
        }
        else if ( mHeapType == renderer::rhi::HeapType::eReadback ) {
            mAllocation.mInitialState = D3D12_RESOURCE_STATE_COPY_DEST;
            mResourceState = renderer::rhi::ResourceStates::eCopyDest;
        }
        else if ( mUsage.Has( rhi::BufferUsageFlagsBits::AccelStructureStorage ) ) {
            // DXR result buffers remain in this state for their entire lifetime.
            mAllocation.mInitialState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
            mResourceState = rhi::ResourceStates::eAccelStructRead;
        }
        else if ( mUsage.Has( rhi::BufferUsageFlagsBits::AccelStructureScratch ) ) {
            mAllocation.mInitialState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            mResourceState = rhi::ResourceStates::eUnorderedAccess;
        }
        else if ( mInitialState != renderer::rhi::ResourceStates::eCommon ) {
            mAllocation.mInitialState = d3d12::GetResourceState( mInitialState );
            mResourceState = mInitialState;
        }

        auto* allocator{ device->GetAllocator() };
        ThrowIfFailed( allocator->AllocateBuffer( mAllocation ) );

        if ( mHeapType != renderer::rhi::HeapType::eReadback && !mUsage.Has( rhi::BufferUsageFlagsBits::AccelStructureStorage ) ) {
            // Paired raw views give shader references a stable read/write identity.
            // The buffer is retained by submissions, so these slots cannot be
            // recycled until its last GPU use completes.
            mSrvDescriptorIndex = mResources->mResourceDescriptorHeap->AllocateDescriptors( 2 );
            auto cpu{ mResources->mResourceDescriptorHeap->GetCpuHandle( mSrvDescriptorIndex ) };
            CreateSRV( cpu.ptr, {}, renderer::rhi::ResourceType::eRawBuffer_SRV );

            if ( mAllocation.mDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS ) {
                cpu = mResources->mResourceDescriptorHeap->GetCpuHandle( mSrvDescriptorIndex + 1 );
                CreateUAV( cpu.ptr, {}, renderer::rhi::ResourceType::eRawBuffer_UAV );
            }
        }

        if ( !mUploadContents.IsEmpty() ) {
            core::u64 fenceValue{ 0 };
            renderer::rhi::FenceHandle fence{ mDevice->CreateFence( fenceValue++ ) };
            renderer::rhi::CommandListHandle cmd{ mDevice->CreateCommandList( renderer::rhi::QueueType::eTransfer ) };
            cmd->Open( { .mScopeName = string::Format( "Buffer upload: {}", mDebugName ) } );
            cmd->Write( this, mUploadContents->GetData(), mUploadContents->GetSize() );
            cmd->Close();

            // Signal fenceValue on one fence on completion of these
            // commands, then we wait for that completion this blocks the caller
            // but client should ideally offload this task to worker threads
            const auto submitInfo{ renderer::rhi::SubmitInfo{}
                .AddSignal( fence, fenceValue )
                .AddCommandList( cmd ) };
            mDevice->GetQueue( renderer::rhi::QueueType::eTransfer )->ExecuteCommandLists( submitInfo );
            ( void )fence->Wait( fenceValue, eastl::numeric_limits<core::u64>::max() );
        }

        if ( !mKeepInitializerResources ) {
            mUploadContents.Reset();
        }

        mIsAllocated = true;
    }

    auto Buffer::GetUsage() const -> rhi::BufferUsageFlags {
        return mUsage;
    }

    auto Buffer::GetDataType() const -> rhi::BufferDataType {
        return mDataType;
    }

    auto Buffer::GetData() const -> memory::BufferSpanHandle {
        return mUploadContents;
    }

    auto Buffer::GetSizeBytes() const -> core::usize {
        return mElementCount == 0 ? mElementSize : mElementCount * mElementSize;
    }

    auto Buffer::GetFormat() const -> rhi::Format {
        return mFormat;
    }

    auto Buffer::GetCount() const -> core::usize {
        return mElementCount == 0 ? rhi::InferElementCount( mFormat, mElementSize ) : mElementCount;
    }
}
#endif
