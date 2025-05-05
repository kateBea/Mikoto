//
// Created by zanet on 3/2/2025.
//

#include <Renderer/Vulkan/VulkanTextureCubeMap.hh>

namespace Mikoto {

    VulkanTextureCubeMap::VulkanTextureCubeMap( const VulkanTextureCubeMapCreateInfo& createInfo )
        : TextureCubeMap{ TextureCubeMapCreateInfo{ .TexturePath{ createInfo.TexturePath } } }
    {}

}
