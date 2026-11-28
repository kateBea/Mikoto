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
#include <Core/Profiler.hh>

#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Rhi/Vulkan/VulkanDevice.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )
#include <Renderer/Rhi/D3D11/D3D11Device.hh>
#include <Renderer/Rhi/D3D12/D3D12Device.hh>

#endif

#include <Renderer/Rhi/OpenGL/OpenGLDevice.hh>

namespace mikoto::renderer {

    using namespace mikoto::renderer::rhi;

    auto rhi::IGpuDevice::GetDenoiserInputSizes( const DenoiserDescription &description ) -> DenoiserInputSizes {
        return {
            .mOptimal = description.mOutputSize,
            .mMinimum = description.mOutputSize,
            .mMaximum = description.mOutputSize,
        };
    }

    auto rhi::IGpuDevice::CreateDenoiser( const DenoiserDescription &description ) -> DenoiserHandle {
        auto denoiser{ DenoiserHandle::New( description ) };
        denoiser->Initialize( this );
        return denoiser;
    }

    auto rhi::IGpuDevice::CreateVideoDecoder( const VideoDecoderDescription& ) -> VideoDecoderHandle {
        return {};
    }

    auto renderer::rhi::IGpuDevice::GetMaxSceneMultisampling() const -> Multisampling {
        for ( auto index{ core::as<core::u32>( Multisampling::eMsaaX16 ) }; index > 0; --index ) {
            const auto samples{ core::as<Multisampling>( index ) };

            if ( SupportsSceneMultisampling( samples ) ) {
                return samples;
            }
        }

        return Multisampling::eMsaaX1;
    }

    auto renderer::rhi::IGpuDevice::Create( const renderer::rhi::GpuDeviceCreateInfo &createInfo ) -> eastl::unique_ptr<renderer::rhi::IGpuDevice> {
        MKT_BEGIN_PROFILER();

        switch ( createInfo.mApi ) {
            case renderer::rhi::GraphicsAPI::eOpenGL:
                return eastl::make_unique<opengl::Device>( createInfo );
            case renderer::rhi::GraphicsAPI::eVulkan:
                return eastl::make_unique<vulkan::Device>( createInfo );
#if defined( MIKOTO_PLATFORM_WINDOWS )
            case renderer::rhi::GraphicsAPI::eD3D11:
                return eastl::make_unique<d3d11::Device>( createInfo );
            case renderer::rhi::GraphicsAPI::eD3D12:
                return eastl::make_unique<d3d12::Device>( createInfo );
#endif
            default:;
        }

        return nullptr;
    }
}
