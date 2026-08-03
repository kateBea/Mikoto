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

#include <Renderer/Core/Rhi.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

// D3D12 extension library.
#include <directx/d3d12.h>

#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <wrl.h>

#include <Renderer/D3D12/D3D12Buffer.hh>
#include <Renderer/D3D12/D3D12Device.hh>
#include <Renderer/D3D12/Direct3D12Helpers.hh>
#include <Renderer/D3D12/D3D12MemoryAllocator.hh>

namespace mikoto::renderer::d3d12 {

    Buffer::Buffer( const rhi::BufferCreateDescription &createInfo, DeviceResources& resources )
        : IBuffer{ createInfo }, mResources{ MKT_ADDRESSOF( resources ) }
    {
    }

    auto Buffer::PersistentMap() -> void {
        auto allocator{ checked_cast<Device*>( mDevice )->GetAllocator() };
        mMappedAddress = allocator->MapBuffer( mAllocation );
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
        ID3D12Device1* d3d12Device{ device->GetDevice() };

        // Create descriptor heap if it's going to be used in shaders
        if (d3d12::IsDescriptorHeapRequired( mResourceType )) {

        }

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

        mIsAllocated = true;
    }
}

#endif

