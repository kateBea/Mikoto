//
// Created by zanet on 1/29/2025.
//

#include <Library/Utility/Types.hh>
#include <Renderer/RenderContext.hh>
#include <Renderer/RenderService.hh>
#include <Renderer/Vulkan/VulkanContext.hh>

namespace Mikoto {

    auto RenderContext::Create( const RenderContextCreateInfo& config ) -> Unique<RenderContext> {
        switch (RenderService::Get().GetActiveGraphicsApi()) {
            case GraphicsAPI::VULKAN_API:
                return CreateScope<VulkanContext>( config );
            default:;
        }

        return nullptr;
    }
}