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

#include <Logging/Logger.hh>

#include <Memory/Allocator.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )

#include <Renderer/Rhi/D3D11/D3D11Buffer.hh>
#include <Renderer/Rhi/D3D11/D3D11Device.hh>
#include <Renderer/Rhi/D3D11/Direct3D11Helpers.hh>

namespace mikoto::renderer::d3d11 {

    using namespace mikoto::core;
    using namespace mikoto::memory;
    using namespace mikoto::renderer::rhi;

    Buffer::Buffer( const renderer::rhi::BufferCreateDescription &description )
        : mUploadContents{ description.mSpanHandle }, mElementCount{ description.mElementCount }, mElementSize{ description.mElementSize }, mDataType{ description.mDataType }, mUsage{ description.mUsageFlags }, mInitialState{ description.mInitialState }, mIsVolatile{ description.mIsVolatile }, mMaxVersions{ description.mMaxVersions }, mFormat{ description.mFormat } {
        mHeapType = description.mHeapType;
        mResourceType = description.mResourceType;
    }

    auto Buffer::GetShaderDeviceAddress() -> rhi::DeviceAddress {
        return GetGpuDeviceAddress();
    }

    auto Buffer::GetGpuDeviceAddress() -> renderer::rhi::DeviceAddress {
        MKT_BEGIN_PROFILER();
        return renderer::rhi::IBuffer::kNullDeviceAddress;
    }

    auto Buffer::GetNativeHandle( rhi::ObjectType type ) -> rhi::Object {
        MKT_BEGIN_PROFILER();

        switch ( type ) {
            case rhi::ObjectType::D3D11_Buffer:
                return rhi::Object( mBuffer.Get() );

            default:
                return rhi::Object( nullptr );
        }
    }

    auto Buffer::GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object {
        MKT_BEGIN_PROFILER();

        switch ( type ) {
            case rhi::ObjectType::D3D11_Buffer:
                return rhi::Object( mBuffer.Get() );

            default:
                return rhi::Object( nullptr );
        }
    }

    Buffer::~Buffer() {
        MKT_BEGIN_PROFILER();

        if ( mIsAllocated ) {
            Destroy();
        }
    }

    auto Buffer::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        D3D11_BUFFER_DESC desc{};

        desc.BindFlags = GetBindFlags( mUsage );
        desc.Usage = GetUsageFromHeapType( mHeapType );

        // CPU access derived from usage
        switch ( desc.Usage ) {
            case D3D11_USAGE_DYNAMIC:
                desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
                break;

            case D3D11_USAGE_STAGING:
                desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                desc.BindFlags = 0;// staging can't be bound
                break;

            default:
                desc.CPUAccessFlags = 0;
                break;
        }

        // When the buffer is typed we must have specified the element count
        // otherwise mElementSize the total size in bytes of the buffer
        if ( mElementCount != 0 ) {
            desc.ByteWidth = mElementSize * mElementCount;
        }
        else {
            desc.ByteWidth = mElementSize;
        }

        // Structured buffer
        if ( mUsage.Has( renderer::rhi::BufferUsageFlagsBits::Structured ) ) {
            desc.StructureByteStride = mElementSize;
            desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        }

        // Initial data
        D3D11_SUBRESOURCE_DATA initData{};
        D3D11_SUBRESOURCE_DATA *pInitData{ nullptr };

        if ( !mUploadContents.IsEmpty() ) {
            initData.pSysMem = mUploadContents->GetData();
            pInitData = &initData;
        }

        HRESULT result{ core::checked_cast<Device *>( mDevice )->GetDevice()->CreateBuffer( &desc, pInitData, &mBuffer ) };

        if ( SUCCEEDED( result ) ) {
            mIsAllocated = true;
        }
        else {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate D3D11 buffer" );
        }

        // Release initial data
        mUploadContents.Reset();
    }

    auto Buffer::Destroy() -> void {
        MKT_BEGIN_PROFILER();

        mIsAllocated = false;
    }

    auto Buffer::SetDebugName( const eastl::string_view name ) -> void {
        MKT_BEGIN_PROFILER();
        mBuffer->SetPrivateData( WKPDID_D3DDebugObjectName, core::as<UINT>( name.size() ), name.data() );
    }

    Buffer::operator ID3D11Resource *() const {
        MKT_BEGIN_PROFILER();
        return mBuffer.Get();
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
