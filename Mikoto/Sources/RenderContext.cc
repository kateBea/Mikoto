//
// Created by zanet on 1/29/2025.
//

#include <Library/Utility/Types.hh>
#include <Renderer/Vulkan/VulkanContext.hh>

namespace Mikoto {

    auto RenderContext::Create( const RenderContextCreateInfo& config ) -> Scope_T<RenderContext> {
        switch (config.Backend) {
            case GraphicsAPI::VULKAN_API:
                return CreateScope<VulkanContext>( config );
            default:;
        }

        return nullptr;
    }
}