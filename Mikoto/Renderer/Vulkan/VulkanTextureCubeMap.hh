//
// Created by zanet on 3/2/2025.
//

#ifndef VULKANTEXTURECUBEMAP_HH
#define VULKANTEXTURECUBEMAP_HH

#include <Material/Texture/TextureCubeMap.hh>

namespace Mikoto {
    struct VulkanTextureCubeMapCreateInfo {
        Path_T TexturePath{};
    };

    class VulkanTextureCubeMap final : public TextureCubeMap {
    public:

        explicit VulkanTextureCubeMap( const VulkanTextureCubeMapCreateInfo& createInfo );

    private:
    };

}



#endif //VULKANTEXTURECUBEMAP_HH
