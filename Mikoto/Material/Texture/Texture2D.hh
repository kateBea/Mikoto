/**
 * Texture2D.cc
 * Created by kate on 6/8/23.
 * */

#ifndef MIKOTO_TEXTURE2D_HH
#define MIKOTO_TEXTURE2D_HH

// C++ Standard Library
#include <any>
#include <memory>

// Project Headers
#include <STL/Random/Random.hh>

#include "Common/Common.hh"
#include "STL/Utility/Types.hh"

namespace Mikoto {
    enum class MapType {
        TEXTURE_2D_INVALID,
        TEXTURE_2D_DIFFUSE,
        TEXTURE_2D_SPECULAR,
        TEXTURE_2D_EMISSIVE,
        TEXTURE_2D_NORMAL,
        TEXTURE_2D_ROUGHNESS,
        TEXTURE_2D_METALLIC,
        TEXTURE_2D_AMBIENT_OCCLUSION,
        TEXTURE_2D_COUNT,
    };

    enum class TextureFileType {
        UNKNOWN_IMAGER_TYPE,
        PNG_IMAGE_TYPE,
        JPEG_IMAGE_TYPE,
        JPG_IMAGE_TYPE,
    };

    class Texture2D {
    public:
        MKT_NODISCARD auto GetChannels() const -> Int32_T { return m_Channels; }
        MKT_NODISCARD auto GetWidth() const -> Int32_T  { return m_Width; }
        MKT_NODISCARD auto GetHeight() const -> Int32_T { return m_Height; }
        MKT_NODISCARD auto GetType() const -> MapType { return m_Type; }
        MKT_NODISCARD auto GetFileType() const -> TextureFileType { return m_FileType; }
        MKT_NODISCARD auto GetSize() const -> double { return m_Size; }
        MKT_NODISCARD auto GetID() const -> UUID { return m_UUID; }

        MKT_NODISCARD virtual auto GetImGuiTextureHandle() const -> std::any = 0;

        /**
         * @brief Creates a new texture
         * Creates a Texture2D based on the active graphics API (Vulkan or OpenGL) from the provided file path and MapType.
         * @param path The path to the texture file.
         * @param type The MapType for the texture.
         * @return A shared pointer to the created Texture2D. If creation fails, returns a null pointer.
         * */
        static auto Create(const Path_T& path, MapType type) -> std::shared_ptr<Texture2D>;

        static constexpr auto GetFileTypeStr(TextureFileType type) -> std::string_view {
            switch ( type ) {
                case TextureFileType::UNKNOWN_IMAGER_TYPE:  return "Unknown";
                case TextureFileType::PNG_IMAGE_TYPE:       return "PNG Image";
                case TextureFileType::JPEG_IMAGE_TYPE:      return "JPEG Image";
                case TextureFileType::JPG_IMAGE_TYPE:       return "JPG Image";
            }
        }
        virtual ~Texture2D() = default;
    protected:
        explicit Texture2D( const MapType map)
            :   m_Width{}, m_Height{}, m_Channels{}, m_Type{ map }, m_UUID{ GenerateGUID() }
        {

        }

        Texture2D( const Int32_T width, const Int32_T height, const Int32_T channels)
            :   m_Width{ width }, m_Height{ height }, m_Channels{ channels }
        {

        }

    protected:
        Int32_T m_Width{};
        Int32_T m_Height{};
        Int32_T m_Channels{};
        MapType m_Type{};

        UUID m_UUID{};

        double m_Size{}; // in MB

        TextureFileType m_FileType{ TextureFileType::UNKNOWN_IMAGER_TYPE };
    };
}

#endif // MIKOTO_TEXTURE2D_HH
