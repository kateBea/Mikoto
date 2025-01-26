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
#include <STL/Utility/Types.hh>
#include <Renderer/Vulkan/VulkanUtils.hh>
#include <Material/Texture/Texture2D.hh>

namespace Mikoto {

    struct VulkanTexture2DCreateInfo {
        Path_T Path{};
        MapType Type{};
        bool RetainFileData{ false };
    };

    /**
     * @class VulkanTexture2D
     * @brief Represents a 2D texture used in Vulkan renderer.
     *
     * Extends the Texture2D class for Vulkan-specific texture functionality. It manages loading and handling
     * 2D textures for Vulkan rendering, including creating images, image views, samplers, and descriptor sets.
     * */
    class VulkanTexture2D final : public Texture2D {
    public:

        explicit VulkanTexture2D(const Path_T& path, MapType type, bool retainFileData = false);

        MKT_UNUSED_FUNC MKT_NODISCARD auto GetFileData() const -> stbi_uc* { return m_TextureFileData; }
        MKT_UNUSED_FUNC MKT_NODISCARD auto GetImage() const -> VkImage { return m_ImageInfo.Image; }
        MKT_NODISCARD auto GetImageView() const -> VkImageView { return m_View; }
        MKT_NODISCARD auto GetImageSampler() const -> VkSampler { return m_TextureSampler; }
        MKT_NODISCARD auto GetDescriptorSet() const -> VkDescriptorSet { return m_DescSet; }
        MKT_NODISCARD auto GetImGuiTextureHandle() const -> std::any override { return m_DescSet; }

        ~VulkanTexture2D() override;

    private:
        auto Create(const VulkanTexture2DCreateInfo& data) -> void;
        auto Release() -> void;

    private:
        auto CreateImage() -> void;
        auto CreateImageView() -> void;
        auto CreateSampler() -> void;
        auto LoadImageData(const Path_T& path) -> void;

    private:
        stbi_uc*                        m_TextureFileData{};  /**< Texture file data, represented in STB image format. */
        Size_T                    m_ImageSize{};        /**< Size of the image in Vulkan format. */

        VkSampler                       m_TextureSampler{};   /**< Vulkan sampler associated with the texture. */
        VkImageView                     m_View{};             /**< Vulkan image view associated with the texture. */
        VkDescriptorSet                 m_DescSet{};          /**< Vulkan descriptor set associated with the texture. */
        ImageAllocateInfo               m_ImageInfo{};        /**< Vulkan image information. */
    };
}

#endif // MIKOTO_VULKAN_TEXTURE2D_HH
