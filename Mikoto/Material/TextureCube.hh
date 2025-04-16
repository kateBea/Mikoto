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
         * @param format Texture format
         * @param usage Type of resource usage
         */
        TextureCube(const TextureType type, const Int32_T width, const Int32_T height, const Int32_T channels, Byte_T* data,
                   const ResourceUsageType usage)
            : Texture{ type, InferFormatFromChannels(channels), width, height, channels, usage }, m_Data{ data }
        {}

    protected:
        Byte_T* m_Data{ nullptr };
    };
}// namespace Mikoto

#endif//TEXTURECUBEMAP_HH
