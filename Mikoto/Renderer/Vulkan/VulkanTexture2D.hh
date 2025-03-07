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
#include <Library/Utility/Types.hh>
#include <Library/Filesystem/File.hh>
#include <Renderer/Vulkan/VulkanHelpers.hh>
#include <Renderer/Vulkan/VulkanImage.hh>
#include <Renderer/Vulkan/VulkanObject.hh>
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
    class VulkanTexture2D final : public VulkanObject, public Texture2D {
    public:

        explicit VulkanTexture2D(const VulkanTexture2DCreateInfo& data);

        MKT_NODISCARD auto GetFileData() const -> stbi_uc* { return m_FileData; }
        MKT_NODISCARD auto GetImage() -> VulkanImage& { return *m_Image; }
        MKT_NODISCARD auto GetImage() const -> const VulkanImage& { return *m_Image; }
        MKT_NODISCARD auto GetSampler() const -> const VkSampler& { return m_Sampler; }

        auto Release() -> void override;

        ~VulkanTexture2D() override;

        static auto Create(const VulkanTexture2DCreateInfo& data) -> Scope_T<VulkanTexture2D>;

    private:
        auto CreateImage() -> void;
        auto CreateSampler() -> void;
        auto LoadImageData(const Path_T& path) -> void;

    private:
        Size_T m_BufferSize{ 0 };
        stbi_uc* m_FileData{ nullptr };

        VkSampler m_Sampler{ VK_NULL_HANDLE };

        Scope_T<VulkanImage> m_Image{ nullptr };

        // Made private if need to defer the destruction or need a host visible block of memory
        Scope_T<VulkanBuffer> m_StagingBuffer{ nullptr };
    };
}

#endif // MIKOTO_VULKAN_TEXTURE2D_HH
