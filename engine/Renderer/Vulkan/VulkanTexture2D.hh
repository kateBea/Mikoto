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
#include <Utility/Common.hh>
#include <Renderer/Material/Texture2D.hh>

namespace Mikoto {
    class VulkanTexture2D : public Texture2D {
    public:
        /**
         * Creates a new Texture object and fills it with the data
         * from Texture file in path. If no data is provided it simply creates
         * a valid Texture object with a valid id
         * @param path the path to the Texture file
         * @param retainFileData whether we want to keep the texture data
         * */
        explicit VulkanTexture2D(const Path_T& path, bool retainFileData = false);


        MKT_NODISCARD auto GetFileData() const -> stbi_uc* { return m_TextureFileData; }

        MKT_NODISCARD auto GetImageView() const -> VkImageView { return m_TextureImageView; }
        MKT_NODISCARD auto GetImageSampler() const -> VkSampler { return m_TextureSampler; }
        MKT_NODISCARD auto GetImage() const -> VkImage { return m_TextureImage; }

        auto OnRelease() const -> void;
    private:
        auto LoadImageData(const Path_T& path) -> void;
        auto CreateTextureImage() -> void;
        auto CreateTextureImageView() -> void;
        auto CreateTextureSampler() -> void;

        static auto TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) -> void;
        static auto CopyBufferToImage(VkBuffer buffer, VkImage image, UInt32_T width, UInt32_T height) -> void;

        static auto CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) -> void;
        static auto CreateImage(UInt32_T width, UInt32_T height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
                                          VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) -> void;
    private:
        stbi_uc*    m_TextureFileData{};
        VkImage         m_TextureImage{};
        VkDeviceMemory  m_TextureImageMemory{};
        VkDeviceSize    m_ImageSize{};
        VkImageView     m_TextureImageView{};
        VkSampler       m_TextureSampler{};

        bool m_RetainData{};
    };
}

#endif // MIKOTO_VULKAN_TEXTURE2D_HH
