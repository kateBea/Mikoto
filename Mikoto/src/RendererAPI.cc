/**
 * RendererAPI.cc
 * Created by kate on 6/9/23.
 * */

// C++ Standard Library
#include <new>

// Project Headers
#include <Core/Logger.hh>
#include <Renderer/RendererBackend.hh>
#include <Renderer/Vulkan/VulkanRenderer.hh>

namespace Mikoto {

    auto RendererBackend::Create( const GraphicsAPI backend ) -> RendererBackend * {
        switch ( backend ) {
            case GraphicsAPI::VULKAN_API:
                return new ( std::nothrow ) VulkanRenderer();
            default:
                MKT_CORE_LOGGER_CRITICAL( "Unsupported renderer API!" );
                break;
        }

        return nullptr;
    }
}// namespace Mikoto