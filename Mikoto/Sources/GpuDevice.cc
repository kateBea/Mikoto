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

#include <Core/Platform.hh>

#include <Renderer/Core/GpuDevice.hh>
#include <Renderer/Vulkan/VulkanDevice.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)
#include <Renderer/D3D11/D3D11Device.hh>
#include <Renderer/D3D12/D3D12Device.hh>
#endif

namespace Mikoto {

    auto RenderInfo::Clear() -> void {
        this->ColorRenderTargets.clear();
        this->DepthRenderTarget = TextureHandle::CreateEmpty();
    }

    auto GpuDevice::Create( const GpuDeviceCreateInfo &createInfo ) -> Unique<GpuDevice> {
        switch ( createInfo.Api ) {
            case GraphicsAPI::VULKAN_API:
                return CreateScope<VulkanDevice>( createInfo );
#if defined(MIKOTO_PLATFORM_WINDOWS)
            case GraphicsAPI::DIRECTX_11:
                return CreateScope<D3D11Device>( createInfo );
            case GraphicsAPI::DIRECTX_12:
                return CreateScope<D3D12Device>( createInfo );
#endif
            default:;
        }

        return nullptr;
    }

    GpuDevice::GpuDevice( const GraphicsAPI api )
        : m_Api{ api }
    {}
}