/**
 * VulkanTexture2D.hh
 * Created by kate on 7/5/2023.
 * */

#ifndef KATE_ENGINE_VULKAN_TEXTURE2D_HH
#define KATE_ENGINE_VULKAN_TEXTURE2D_HH

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


        /**
         * Returns the identifier of this Texture
         * @return id of this object
         * */
        KT_NODISCARD auto GetId() const -> UInt32_T override { return m_Id; }

        KT_NODISCARD auto GetChannels() const -> UInt32_T override { return m_Channels; }
        KT_NODISCARD auto GetWidth() const -> UInt32_T override { return m_Width; }
        KT_NODISCARD auto GetHeight() const -> UInt32_T override { return m_Height; }

        KT_NODISCARD auto GetFileData() const -> stbi_uc* { return m_TextureFileData; }

        KT_NODISCARD auto GetImageView() const -> VkImageView { return m_TextureImageView; }
        KT_NODISCARD auto GetImageSampler() const -> VkSampler { return m_TextureSampler; }
        KT_NODISCARD auto GetImage() const -> VkImage { return m_TextureImage; }

        auto Bind(UInt32_T slot) -> void override {}

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
        UInt32_T    m_Id{};
        UInt32_T    m_Width{};
        UInt32_T    m_Height{};
        UInt32_T    m_Channels{};
        stbi_uc*    m_TextureFileData{};

        VkImage         m_TextureImage{};
        VkDeviceMemory  m_TextureImageMemory{};
        VkDeviceSize    m_ImageSize{};
        VkImageView     m_TextureImageView{};
        VkSampler       m_TextureSampler{};

        bool m_RetainData{};
    };
}


#endif //KATE_ENGINE_VULKAN_TEXTURE2D_HH
