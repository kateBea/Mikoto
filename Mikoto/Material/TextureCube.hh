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

        auto GetMipLevels() const -> Int32 { return m_MipLevels; }

        ~TextureCube() override = default;

    protected:
        static constexpr UInt32 MAX_CUBE_MAP_FACES{ 6 };

        /**
         * @brief Constructs a 2D texture with specified parameters.
         * @param usage Type of resource usage
         */
        explicit TextureCube(const ResourceUsageType usage)
            : Texture{ TextureType::TEXTURE_CUBE, TextureFormat::TEXTURE_FORMAT_RGBA8_SNORM, 0, 0, 0, usage, TextureUsage::TEXTURE_USAGE_CUBE }
        {}

        // Cube map faces all share same dimensions
        Int32 m_MipLevels{ 1 };
    };
}// namespace Mikoto

#endif//TEXTURECUBEMAP_HH
