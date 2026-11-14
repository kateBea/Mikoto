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

#include <EASTL/utility.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/string_view.h>

#include <Core/CVar.hh>
#include <Core/Platform.hh>
#include <Core/Profiler.hh>

#include <Renderer/Core/RenderContext.hh>
#include <Renderer/Rhi/Vulkan/VulkanContext.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )
#include <Renderer/Rhi/D3D11/D3D11Context.hh>
#include <Renderer/Rhi/D3D12/D3D12Context.hh>
#endif

#include <Renderer/Rhi/OpenGL/OpenGLContext.hh>

namespace mikoto::renderer {

    using namespace mikoto::renderer::rhi;

    auto RenderContextCreateInfo::SetWindow( platform::IWindow* window ) noexcept -> RenderContextCreateInfo& {
        MKT_BEGIN_PROFILER();
        mWindow = window;
        return *this;
    }

    auto RenderContextCreateInfo::SetRefreshRate( rhi::RefreshRate refreshRate ) noexcept -> RenderContextCreateInfo& {
        MKT_BEGIN_PROFILER();
        mRefreshRate = refreshRate;
        return *this;
    }

    auto RenderContextCreateInfo::SetGraphicsAPI( rhi::GraphicsAPI api ) noexcept -> RenderContextCreateInfo& {
        MKT_BEGIN_PROFILER();
        mApi = api;
        return *this;
    }

    auto RenderContext::GetGpuDevice() -> renderer::rhi::IGpuDevice* {
        MKT_BEGIN_PROFILER();
        return mDevice.get();
    }

    auto RenderContext::GetGpuDevice() const -> const renderer::rhi::IGpuDevice* {
        MKT_BEGIN_PROFILER();
        return mDevice.get();
    }

    auto RenderContext::GetRefreshRate() const -> renderer::rhi::RefreshRate {
        MKT_BEGIN_PROFILER();
        return mRefreshRate;
    }

    auto RenderContext::IsRefreshType( renderer::rhi::RefreshRate type ) const -> bool {
        MKT_BEGIN_PROFILER();
        return mRefreshRate == type;
    }

    auto RenderContext::GetRequestedDeviceFeatures() const -> GpuFeatureSupportFlags {
        MKT_BEGIN_PROFILER();

        GpuFeatureSupportFlags requested{ GpuFeatureSupportFlagsBits::None };

        // Configuration populates the CVar registry before any backend creates its device.
        // Keep the key mapping shared so changing graphics APIs preserves the same requests.
        constexpr eastl::pair<eastl::string_view, GpuFeatureSupportFlags> features[]{
            { "device_feature_hardware_raytracing", GpuFeatureSupportFlagsBits::HardwareRayTracing },
            { "device_feature_texture_compression", GpuFeatureSupportFlagsBits::BCTextureCompression },
            { "device_feature_hardware_wireframe", GpuFeatureSupportFlagsBits::HardwareWireframe },
            { "device_feature_presentation_support", GpuFeatureSupportFlagsBits::WindowPresentation },
            { "device_feature_video_encoding", GpuFeatureSupportFlagsBits::VideoEncoding },
            { "device_feature_video_decoding", GpuFeatureSupportFlagsBits::VideoDecoding }
        };

        if ( const auto* registry{ core::CVarRegistry::GetPtr() } ) {
            for ( const auto& [key, feature]: features ) {
                if ( registry->GetValue<bool>( key, false ) ) {
                    requested |= feature;
                }
            }
        }

        // A window always enables request for presentation
        // capabilities, even when the configured preference is false.
        if ( mWindow ) {
            requested |= GpuFeatureSupportFlagsBits::WindowPresentation;
        }

        return requested;
    }

    auto RenderContext::Create( const RenderContextCreateInfo& config ) -> eastl::unique_ptr<RenderContext> {
        MKT_BEGIN_PROFILER();

        eastl::unique_ptr<RenderContext> result{ nullptr };

        switch ( config.mApi ) {
            case renderer::rhi::GraphicsAPI::eOpenGL:
                result = eastl::make_unique<opengl::Context>( config );
                break;
            case renderer::rhi::GraphicsAPI::eVulkan:
                result = eastl::make_unique<vulkan::Context>( config );
                break;
#if defined( MIKOTO_PLATFORM_WINDOWS )
            case renderer::rhi::GraphicsAPI::eD3D11:
                result = eastl::make_unique<d3d11::Context>( config );
                break;
            case renderer::rhi::GraphicsAPI::eD3D12:
                result = eastl::make_unique<d3d12::Context>( config );
#endif
            default:;
        }

        return result;
    }

    RenderContext::RenderContext( const RenderContextCreateInfo& createInfo )
        : mWindow{ createInfo.mWindow }, mRefreshRate{ createInfo.mRefreshRate } {
    }
}// namespace mikoto::renderer
