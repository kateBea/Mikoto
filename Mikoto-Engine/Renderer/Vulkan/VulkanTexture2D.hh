/**
 * VulkanTexture2D.hh
 * Created by kate on 7/5/2023.
 * */

#ifndef MIKOTO_VULKAN_TEXTURE2D_HH
#define MIKOTO_VULKAN_TEXTURE2D_HH

// C++ Standard Library
#include <filesystem>

// Third-Party Libraries
#include <stb_image.h>

// Project Headers
#include <Common/Common.hh>
#include <Common/Types.hh>
#include <Common/VulkanUtils.hh>
#include <Renderer/Material/Texture2D.hh>

namespace Mikoto {
    /**
     * @class VulkanTexture2D
     * @brief Represents a 2D texture used in Vulkan renderer.
     *
     * Extends the Texture2D class for Vulkan-specific texture functionality. It manages loading and handling
     * 2D textures for Vulkan rendering, including creating images, image views, samplers, and descriptor sets.
     * */
    class VulkanTexture2D : public Texture2D {
    public:
        /**
         * @brief Constructs a Vulkan 2D texture object.
         *
         * @param path The path to the image file.
         * @param type The mapping type of the texture.
         * @param retainFileData Flag indicating whether to retain file data (if true) in CPU memory after loading.
         * @throws std::runtime_error If the path is not valid.
         * */
        explicit VulkanTexture2D(const Path_T& path, MapType type, bool retainFileData = false);

        /**
         * @brief Retrieves the texture file data (STB image data).
         *
         * @return The file data of the texture.
         * */
        MKT_UNUSED_FUNC MKT_NODISCARD auto GetFileData() const -> stbi_uc* { return m_TextureFileData; }

        /**
         * @brief Retrieves the Vulkan image.
         *
         * @return The Vulkan image associated with the texture.
         * */
        MKT_UNUSED_FUNC MKT_NODISCARD auto GetImage() const -> VkImage { return m_ImageInfo.Image; }

        /**
         * @brief Retrieves the Vulkan image view.
         *
         * @return The Vulkan image view.
         * */
        MKT_NODISCARD auto GetImageView() const -> VkImageView { return m_View; }

        /**
         * @brief Retrieves the Vulkan image sampler.
         *
         * @return The Vulkan image sampler.
         * */
        MKT_NODISCARD auto GetImageSampler() const -> VkSampler { return m_TextureSampler; }

        /**
         * @brief Retrieves the Vulkan descriptor set.
         *
         * @return The Vulkan descriptor set.
         * */
        MKT_NODISCARD auto GetDescriptorSet() const -> VkDescriptorSet { return m_DescSet; }

        /**
         * @brief Retrieves the ImGui texture handle for this texture.
         *
         * @return The ImGui texture handle (descriptor set) for ImGui rendering.
         * */
        MKT_NODISCARD auto GetImGuiTextureHandle() const -> std::any override { return m_DescSet; }

        ~VulkanTexture2D() override = default;

    private:
        /**
         * @brief Creates the Vulkan image for this texture.
         * */
        auto CreateImage() -> void;

        /**
         * @brief Creates the Vulkan image view for this texture.
         * */
        auto CreateImageView() -> void;

        /**
         * @brief Creates the Vulkan sampler for this texture.
         * */
        auto CreateSampler() -> void;

        /**
         * @brief Loads the image data from the provided path.
         *
         * @param path The path to the image file.
         * */
        auto LoadImageData(const Path_T& path) -> void;

    private:
        stbi_uc*                        m_TextureFileData{};  /**< Texture file data, represented in STB image format. */
        VkDeviceSize                    m_ImageSize{};        /**< Size of the image in Vulkan format. */

        VkSampler                       m_TextureSampler{};   /**< Vulkan sampler associated with the texture. */
        VkImageView                     m_View{};             /**< Vulkan image view associated with the texture. */
        VkDescriptorSet                 m_DescSet{};          /**< Vulkan descriptor set associated with the texture. */
        ImageAllocateInfo               m_ImageInfo{};        /**< Vulkan image information. */
    };
}

#endif // MIKOTO_VULKAN_TEXTURE2D_HH
