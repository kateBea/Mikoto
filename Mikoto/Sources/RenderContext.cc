//
// Created by zanet on 1/29/2025.
//

#include <Library/Utility/Types.hh>
#include <Renderer/RenderContext.hh>
#include <Renderer/RenderService.hh>
#include <Renderer/Vulkan/VulkanContext.hh>

namespace Mikoto {

    auto RenderContext::Create( const RenderContextCreateInfo& config ) -> Unique<RenderContext> {
        Unique<RenderContext> result{ nullptr };
        switch (config.Api) {
            case GraphicsAPI::VULKAN_API:
                result = CreateScope<VulkanContext>( config );
            default:;
        }

        return result;
    }
}