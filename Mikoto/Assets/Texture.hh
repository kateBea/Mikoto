//
// Created by zanet on 1/26/2025.
//

#ifndef TEXTURE_HH
#define TEXTURE_HH

#include <string_view>

#include <Common/Common.hh>
#include <Library/Data/ResourcePool.hh>
#include <Library/Filesystem/File.hh>
#include <Library/Random/Random.hh>
#include <Library/Utility/Types.hh>
#include <Renderer/DeviceObject.hh>
#include <Renderer/GpuDevice.hh>

namespace Mikoto {

    /**
    * @struct TextureLoadDescription
    * @brief Holds information for loading a texture.
    *
    * The `TextureLoadInfo` structure stores metadata required to load a texture,
    * including the file path and texture type. It provides a fluent interface
    * for setting its properties.
    */
    struct TextureLoadDescription {
        const File* TextureFile{};
        TextureType Type{ TextureType::TEXTURE_INVALID };

        /**
        * @brief Sets the path of the model.
        * @param file The absolute or relative path to the model file.
        * @return Reference to the modified ModelLoadInfo.
        */
        auto WithFile( const File* file ) -> TextureLoadDescription&;

        /**
         * @brief Sets the texture type.
         * @param type The texture type to be assigned.
         * @return A reference to the modified `TextureLoadInfo` instance.
         */
        auto WithType( TextureType type ) -> TextureLoadDescription&;
    };

    /**
     * @class Texture
     * @brief Represents a texture used in graphics resources.
     *
     * This class encapsulates properties of a texture, including its format, dimensions (width, height),
     * the number of channels (such as RGBA), and its type. Textures are essential for rendering in
     * graphics pipelines and are used to store image data that can be mapped to surfaces.
     */
    class Texture : public DeviceObject {
    public:
        /**
         * @brief Gets the format of the texture.
         *
         * @return The texture format (e.g., RGBA, grayscale).
         */
        MKT_NODISCARD auto GetFormat() const -> TextureFormat {
            return m_Format;
        }

        /**
         * @brief Gets the width of the texture.
         *
         * @return The width of the texture in pixels.
         */
        MKT_NODISCARD auto GetWidth() const -> Int32_T {
            return m_Width;
        }

        /**
         * @brief Gets the height of the texture.
         *
         * @return The height of the texture in pixels.
         */
        MKT_NODISCARD auto GetHeight() const -> Int32_T {
            return m_Height;
        }

        /**
         * @brief Gets the number of channels in the texture.
         *
         * @return The number of channels in the texture (e.g., 4 for RGBA).
         */
        MKT_NODISCARD auto GetChannels() const -> Int32_T {
            return m_Channels;
        }

        /**
         * @brief Gets the type of the texture.
         *
         * @return The texture type (e.g., 2D texture, cube map).
         */
        MKT_NODISCARD auto GetType() const -> TextureType {
            return m_Type;
        }

        MKT_NODISCARD auto GetSampler() const -> SamplerHandle {
            return m_Sampler;
        }

        auto SetSampler(SamplerHandle sampler) -> void {
            m_Sampler = sampler;
        }

        /**
         * @brief Destructor for the Texture class.
         *
         * Ensures proper cleanup of resources when the texture is destroyed.
         */
        ~Texture() override = default;

    protected:
        /**
         * @brief Protected constructor for the Texture class.
         *
         * Initializes a texture with the provided handle, type, format, width, height, and channels.
         *
         * @param type The type of the texture (e.g., 2D, cube map).
         * @param format The format of the texture (e.g., RGBA8, grayscale).
         * @param width The width of the texture in pixels.
         * @param height The height of the texture in pixels.
         * @param channels The number of channels in the texture (e.g., 4 for RGBA).
         * @param usage
         */
        explicit Texture( const TextureType type, const TextureFormat format,
                          const Int32_T width, const Int32_T height, const Int32_T channels, ResourceUsageType usage )
            : DeviceObject{ usage }, m_Type{ type }, m_Format{ format }, m_Width{ width }, m_Height{ height }, m_Channels{ channels } {
        }

    private:
        TextureType m_Type{ TextureType::TEXTURE_INVALID };
        TextureFormat m_Format{ TextureFormat::TEXTURE_FORMAT_INVALID };

        Int32_T m_Width{};
        Int32_T m_Height{};
        Int32_T m_Channels{};

        SamplerHandle m_Sampler{};
    };

}// namespace Mikoto

#endif//TEXTURE_HH
