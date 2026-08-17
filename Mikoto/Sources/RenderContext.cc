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
#include <Renderer/Core/RenderContext.hh>

#include <Renderer/Rhi/Vulkan/VulkanContext.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)
#include <Renderer/Rhi/D3D11/D3D11Context.hh>
#include <Renderer/Rhi/D3D12/D3D12Context.hh>
#endif

namespace mikoto::renderer {

    using namespace mikoto::renderer::rhi;

    auto RenderContextCreateInfo::SetWindow( platform::Window* window ) noexcept -> RenderContextCreateInfo& {
        mWindow = window;
        return *this;
    }

    auto RenderContextCreateInfo::SetRefreshRate( rhi::RefreshRate refreshRate ) noexcept -> RenderContextCreateInfo& {
        mRefreshRate = refreshRate;
        return *this;
    }

    auto RenderContextCreateInfo::SetGraphicsAPI( rhi::GraphicsAPI api ) noexcept -> RenderContextCreateInfo& {
        mApi = api;
        return *this;
    }

    auto RenderContext::GetGpuDevice() -> IGpuDevice* {
        return mDevice.get();
    }

    auto RenderContext::GetGpuDevice() const -> const IGpuDevice* {
        return mDevice.get();
    }

    auto RenderContext::GetRefreshRate() const -> RefreshRate {
        return mRefreshRate;
    }

    auto RenderContext::IsRefreshType( RefreshRate type ) const -> bool {
        return mRefreshRate == type;
    }

    auto RenderContext::Create( const RenderContextCreateInfo& config ) -> eastl::unique_ptr<RenderContext> {
        eastl::unique_ptr<RenderContext> result{ nullptr };

        switch (config.mApi) {
            case GraphicsAPI::eVulkan:
                result = eastl::make_unique<vulkan::Context>( config );
                break;
#if defined(MIKOTO_PLATFORM_WINDOWS)
            case GraphicsAPI::eD3D11:
                result = eastl::make_unique<d3d11::Context>( config );
                break;
            case GraphicsAPI::eD3D12:
                result = eastl::make_unique<d3d12::Context>( config );
#endif
            default:;
        }

        return result;
    }

    RenderContext::RenderContext( const RenderContextCreateInfo& createInfo )
        : mWindow{ createInfo.mWindow }, mRefreshRate{ createInfo.mRefreshRate }
    {}
}// namespace mikoto::renderer