//
// Created by zanet on 1/26/2025.
//

#ifndef TEXTURE_HH
#define TEXTURE_HH

#include <string_view>

#include <Common/Common.hh>
#include <Library/Utility/Types.hh>
#include <Renderer/DeviceObject.hh>
#include <Common/ReferenceCounted.hh>
#include <Renderer/GpuUtility.hh>

namespace Mikoto {

    /**
    * @brief Represents a sampler object used for texture sampling.
    *
    * This class encapsulates the functionality of a sampler, allowing for
    * texture sampling with various filtering and wrapping modes.
    */
    class Sampler : public DeviceObject {
    public:


    protected:
        SamplerFilter m_Filter{ SamplerFilter::FILTER_LINEAR };
        SamplerWrapMode m_Wrap{ SamplerWrapMode::WRAP_CLAMP_TO_EDGE };
    };

    using SamplerHandle = Ref<Sampler>;

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
        MKT_NODISCARD auto GetWidth() const -> Int32 {
            return m_Width;
        }

        /**
         * @brief Gets the height of the texture.
         *
         * @return The height of the texture in pixels.
         */
        MKT_NODISCARD auto GetHeight() const -> Int32 {
            return m_Height;
        }

        /**
         * @brief Gets the number of channels in the texture.
         *
         * @return The number of channels in the texture (e.g., 4 for RGBA).
         */
        MKT_NODISCARD auto GetChannels() const -> Int32 {
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

        MKT_NODISCARD auto GetTextureUsage() const -> TextureUsage {
            return m_TextureUsage;
        }

        MKT_NODISCARD auto GetSampler() const -> SamplerHandle {
            return SamplerHandle::CreateEmpty();
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
                          const Int32 width, const Int32 height, const Int32 channels, ResourceUsageType usage, TextureUsage textureUsage )
            : DeviceObject{ usage }, m_Type{ type }, m_Format{ format }, m_TextureUsage{ textureUsage }, m_Width{ width }, m_Height{ height }, m_Channels{ channels } {
        }

    protected:
        TextureType m_Type{ TextureType::TEXTURE_INVALID };
        TextureFormat m_Format{ TextureFormat::TEXTURE_FORMAT_INVALID };
        TextureUsage m_TextureUsage{ TextureUsage::TEXTURE_USAGE_NORMAL };

        Int32 m_Width{};
        Int32 m_Height{};
        Int32 m_Channels{};

        SamplerHandle m_Sampler{};
    };

    using TextureHandle = Ref<Texture>;

}// namespace Mikoto

#endif//TEXTURE_HH
