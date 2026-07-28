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

#if defined(MIKOTO_PLATFORM_WINDOWS)

#include <D3D12MemAlloc.h>

#include <Logging/Logger.hh>

#include <Memory/Allocator.hh>

#include <Renderer/D3D12/D3D12Device.hh>
#include <Renderer/D3D12/D3D12Texture.hh>
#include <Renderer/D3D12/Direct3D12Helpers.hh>

namespace mikoto::renderer::d3d12 {

    Texture::Texture( const TextureCreateDescription& desc)
        : ITexture{ desc }
    {

    }

    Texture::~Texture() {
        if (mIsAllocated) {
            Release();
        }
    }

    auto Texture::Initialize() -> void {
        // TODO:
        D3D12_RESOURCE_DESC resourceDesc{};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        resourceDesc.Alignment = 0;
        resourceDesc.Width = 1024;
        resourceDesc.Height = 1024;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.SampleDesc.Quality = 0;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        D3D12MA::ALLOCATION_DESC allocDesc = {};
        allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

        // D3D12Resource* resource;
        // D3D12MA::Allocation* allocation;
        // HRESULT hr = allocator->CreateResource(
        //     &allocDesc, &resourceDesc,
        //     D3D12_RESOURCE_STATE_COPY_DEST, NULL,
        //     &allocation, IID_PPV_ARGS(&resource));

        mIsAllocated = true;
    }

    auto Texture::Release() -> void {
        mIsAllocated = false;
    }
}// namespace mikoto::renderer::d3d12

#endif
