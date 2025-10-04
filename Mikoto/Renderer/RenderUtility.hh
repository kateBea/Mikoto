//
// Created by zanet on 4/7/2025.
//

#ifndef RENDERUTILITY_HH
#define RENDERUTILITY_HH

#include <stb_image.h>

#include <Common/Common.hh>
#include <Library/IO/File.hh>

namespace Mikoto {
    enum class GraphicsAPI {
        INVALID_API = -1,
        VULKAN_API,
        UNKNOWN,
    };

    // By default, textures are loaded with rgba format which is supported by most of gpus
    MKT_NODISCARD auto LoadImageFromFile( const File* textureFile, Int32& outWidth, Int32& outHeight, Int32& outChannels ) -> stbi_uc*;

    MKT_NODISCARD auto FreeImageData( Byte* data ) -> void;


    class StbImage final {
    public:
        explicit StbImage( const File* textureFile );

        ~StbImage();

        DISABLE_COPY_FOR( StbImage );

        StbImage( StbImage&& other ) noexcept;

        auto operator=( StbImage&& other ) noexcept -> StbImage&;

        MKT_NODISCARD auto GetData() const -> Byte* { return m_Data; }
        MKT_NODISCARD auto GetWidth() const -> Int32 { return m_Width; }
        MKT_NODISCARD auto GetHeight() const -> Int32 { return m_Height; }
        MKT_NODISCARD auto GetChannels() const -> Int32 { return m_Channels; }

        MKT_NODISCARD auto IsValid() const -> bool { return m_Data != nullptr; }

    private:
        Int32 m_Width{};
        Int32 m_Height{};
        Int32 m_Channels{};
        Byte* m_Data{ nullptr };
    };

    /**
    * @struct FontLoadDescription
    * @brief Holds information for loading a font.
    *
    * The `FontLoadInfo` structure stores metadata required to load a font,
    * It is simply a fluent interface for setting up properties to construct a font.
    */
    struct FontLoadDescription {
        const File* FontFile{};
        float PixelSize{ 48 };

        /**
        * @brief Sets the path of the model.
        * @param file The absolute or relative path to the model file.
        * @return Reference to the modified ModelLoadInfo.
        */
        auto WithFile( const File* file ) -> FontLoadDescription&;

        /**
         * @brief Sets the pixel size of the font.
         * @param pixelSize The desired pixel size.
         * @return Reference to the modified FontLoadInfo.
         */
        auto WithPixelSize( float pixelSize ) -> FontLoadDescription&;
    };
}     // namespace Mikoto
#endif//RENDERUTILITY_HH
