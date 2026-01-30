//
// Created by zanet on 3/25/2025.
//

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
    {
    }
}