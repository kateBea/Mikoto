//
// Created by zanet on 3/2/2025.
//

#ifndef TEXTURECUBEMAP_HH
#define TEXTURECUBEMAP_HH

#include <Common/Common.hh>
#include <Library/Utility/Types.hh>
#include <Assets/Texture.hh>

namespace Mikoto {
    struct TextureCubeMapCreateInfo {
        Path_T TexturePath{};
    };

    class TextureCubeMap : public Texture {
    public:
        explicit TextureCubeMap( const TextureCubeMapCreateInfo& createInfo );

        MKT_NODISCARD static auto Create( const TextureCubeMapCreateInfo& createInfo ) -> Scope_T<TextureCubeMap>;

    private:
        Path_T m_FilePath{};
    };
}// namespace Mikoto

#endif//TEXTURECUBEMAP_HH
