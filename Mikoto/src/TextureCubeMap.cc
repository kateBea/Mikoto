//
// Created by zanet on 3/2/2025.
//

#include <Core/Logging/Logger.hh>
#include <Core/System/RenderSystem.hh>
#include <Renderer/Vulkan/VulkanTextureCubeMap.hh>
#include <Material/Texture/TextureCubeMap.hh>

namespace Mikoto {

    TextureCubeMap::TextureCubeMap( const TextureCubeMapCreateInfo& createInfo )
        : m_FilePath{ createInfo.TexturePath }
    {}

    auto TextureCubeMap::Create( const TextureCubeMapCreateInfo& createInfo ) -> Scope_T<TextureCubeMap> {
        switch (Engine::GetSystem<RenderSystem>().GetDefaultApi()) {
            case GraphicsAPI::VULKAN_API:
                return CreateScope<VulkanTextureCubeMap>(VulkanTextureCubeMapCreateInfo{
                    .TexturePath{ createInfo.TexturePath }
                });
            default:
                MKT_CORE_LOGGER_ERROR( "TextureCubeMap::Create - Trying to create cube map for non-available graphics API." );
        }

        return nullptr;
    }
}