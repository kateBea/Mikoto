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

#include <cstring>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Memory/Allocator.hh>

#include <Renderer/D3D11/D3D11Buffer.hh>
#include <Renderer/D3D11/D3D11Device.hh>
#include <Renderer/D3D11/Direct3D11Helpers.hh>

namespace mikoto::renderer::d3d11 {

    using namespace mikoto::core;
    using namespace mikoto::memory;
    using namespace mikoto::renderer::rhi;

    Buffer::Buffer( const BufferCreateDescription &description )
        : IBuffer{ description }
    {}

    auto Buffer::GetNativeHandle(rhi::ObjectType type) -> rhi::Object {
        switch (type) {
            case rhi::ObjectType::D3D11_Buffer:
                return rhi::Object(mBuffer.Get());

            default:
                return rhi::Object(nullptr);
        }
    }

    auto Buffer::GetNativeHandle(rhi::ObjectType type) const -> rhi::Object {
        switch (type) {
            case rhi::ObjectType::D3D11_Buffer:
                return rhi::Object(mBuffer.Get());

            default:
                return rhi::Object(nullptr);
        }
    }

    Buffer::~Buffer() {
        if (mIsAllocated) {
            Release();
        }
    }

    auto Buffer::Initialize() -> void {
        D3D11_BUFFER_DESC desc{};

        desc.BindFlags = GetBindFlags(mUsage);
        desc.Usage = GetUsageFromHeapType(mHeapType);

        // CPU access derived from usage
        switch (desc.Usage) {
            case D3D11_USAGE_DYNAMIC:
                desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
                break;

            case D3D11_USAGE_STAGING:
                desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                desc.BindFlags = 0; // staging can't be bound
                break;

            default:
                desc.CPUAccessFlags = 0;
                break;
        }

        // When the buffer is typed we must have specified the element count
        // otherwise mElementSize the total size in bytes of the buffer
        if (mElementCount != 0) {
            desc.ByteWidth = mElementSize * mElementCount;
        } else {
            desc.ByteWidth = mElementSize;
        }

        // Structured buffer
        if (mUsage.Has( BufferUsageFlagsBits::kStorage )) {
            desc.StructureByteStride = mElementSize;
            desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        }

        // Initial data
        D3D11_SUBRESOURCE_DATA initData{};
        D3D11_SUBRESOURCE_DATA* pInitData{ nullptr };

        if (!mSpan.IsEmpty()) {
            initData.pSysMem = mSpan->GetData();
            pInitData = &initData;
        }

        HRESULT result{ as<Device*>(mDevice)->GetDevice()->CreateBuffer(&desc, pInitData, &mBuffer) };
        if (SUCCEEDED( result )) {
            mIsAllocated = true;
        } else {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate D3D11 buffer" );
        }

        // Release initial data
        mSpan.Reset();
    }

    auto Buffer::Release() -> void {

        mIsAllocated = false;
    }

    auto Buffer::SetDebugName( const eastl::string_view name ) -> void {
        mBuffer->SetPrivateData(WKPDID_D3DDebugObjectName,as<UINT>(name.size()), name.data() );
    }
}// namespace mikoto::renderer::d3d11
