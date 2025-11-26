//
// Created by zanet on 3/2/2025.
//

#ifndef TEXTURECUBEMAP_HH
#define TEXTURECUBEMAP_HH

#include <Common/Common.hh>
#include <Library/Utility/Types.hh>
#include <Assets/Texture.hh>

namespace Mikoto {

    class TextureCube : public Texture {
    public:

        ~TextureCube() override = default;

    protected:
        /**
         * @brief Constructs a 2D texture with specified parameters.
         * @param type Type of the texture.
         * @param width Width of the texture in pixels.
         * @param height Height of the texture in pixels.
         * @param channels Number of color channels in the texture.
         * @param data Pointer to the texture data
         * @param usage Type of resource usage
         * @param textureUsage Usage of this texture
         */
        TextureCube(const Int32 width, const Int32 height, const Int32 channels, Byte* data,
                   const ResourceUsageType usage, TextureUsage textureUsage)
            : Texture{ TextureType::TEXTURE_CUBE, TextureFormat::TEXTURE_FORMAT_RGBA8_SNORM, width, height, channels, usage, textureUsage }, m_Data{ data }
        {}

    protected:
        Byte* m_Data{ nullptr };
    };
}// namespace Mikoto

#endif//TEXTURECUBEMAP_HH
