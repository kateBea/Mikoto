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

#include <EASTL/unique_ptr.h>

#include <Core/Platform.hh>
#include <Renderer/Core/GpuDevice.hh>

#include <Renderer/Vulkan/VulkanDevice.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)
#include <Renderer/D3D11/D3D11Device.hh>
#include <Renderer/D3D12/D3D12Device.hh>
#endif

namespace mikoto::renderer {

    auto GpuDevice::GetGraphicsApi() const -> GraphicsAPI {
        return mApi;
    }

    auto GpuDevice::IsInitialized() const -> bool {
        return mIsInitialized;
    }

    auto GpuDevice::IsGraphicsApi( GraphicsAPI api ) const -> bool {
        return mApi == api;
    }

    auto GpuDevice::GetDeviceName() const -> eastl::string_view {
        return mName;
    }

    auto GpuDevice::Create( const GpuDeviceCreateInfo &createInfo ) -> eastl::unique_ptr<GpuDevice> {
        switch ( createInfo.mApi ) {
            case GraphicsAPI::eVulkan:
                return eastl::make_unique<vulkan::Device>( createInfo );
#if defined(MIKOTO_PLATFORM_WINDOWS)
            case GraphicsAPI::eD3D11:
                return eastl::make_unique<d3d11::Device>( createInfo );
            case GraphicsAPI::eD3D12:
                return eastl::make_unique<d3d12::Device>( createInfo );
#endif
            default:;
        }

        return nullptr;
    }

    GpuDevice::GpuDevice( const GraphicsAPI api, const GpuFeatureSupport& featuresSupport )
        : mApi{ api }, mFeaturesSupport{ featuresSupport }
    {}
}