//
// Created by zanet on 3/25/2025.
//

#include <Renderer/Core/GpuDevice.hh>
#include <Renderer/Vulkan/VulkanDevice.hh>

namespace Mikoto {

    auto GpuDevice::Create( const GpuDeviceCreateInfo &createInfo ) -> Unique<GpuDevice> {
        switch ( createInfo.Api ) {
            case GraphicsAPI::VULKAN_API:
                return CreateScope<VulkanDevice>( createInfo );
            default:;
        }

        return nullptr;
    }

    GpuDevice::GpuDevice( const GraphicsAPI api )
        : m_Api{ api }
    {
    }
}// namespace Mikoto
