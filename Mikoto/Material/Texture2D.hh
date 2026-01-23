/**
 * Texture2D.cc
 * Created by kate on 6/8/23.
 * */

#ifndef MIKOTO_TEXTURE2D_HH
#define MIKOTO_TEXTURE2D_HH

// C++ Standard Library
#include <any>
#include <memory>
#include <span>

// Project Headers
#include <Assets/Texture.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {

    /**
    * @class Texture2D
    * @brief Represents a 2D texture.
    *
    * The `Texture2D` class is a specialized texture type for handling 2D images.
    * It inherits from the `Texture` base class and provides constructors
    * for initializing texture properties such as width, height, and channels.
    */
    class Texture2D : public Texture {
    public:
        /**
         * @brief Default destructor.
         */
        ~Texture2D() override = default;

        MKT_NODISCARD auto GetMapType() const -> MapType { return m_MapType; }
        MKT_NODISCARD auto IsMapType(const MapType mapType) const -> bool { return m_MapType == mapType; }

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
        Texture2D( const Int32 width, const Int32 height, const Int32 channels, Byte* data,
                   const ResourceUsageType usage, TextureFormat format = TextureFormat::RGBA8_UNORM, TextureUsage textureUsage = TextureUsage::NORMAL, MapType mapType = MapType::UNDEFINED_TEXTURE)
            : Texture{ TextureType::TEXTURE_2D, format, width, height, channels, usage, textureUsage }, m_Data{ data }, m_MapType{ mapType }
        {}

    protected:
        Byte* m_Data{ nullptr };
        MapType m_MapType{ MapType::UNDEFINED_TEXTURE };
    };
}

#endif // MIKOTO_TEXTURE2D_HH
