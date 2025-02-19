/**
 * VulkanImage.cc
 * Created by kate on 8/9/2023.
 * */

#ifndef MIKOTO_VULKAN_IMAGE_HH
#define MIKOTO_VULKAN_IMAGE_HH

// Third-Party Libraries
#include <volk.h>

// Project Headers
#include <Common/Common.hh>
#include <Renderer/Vulkan/VulkanHelpers.hh>
#include <Renderer/Vulkan/VulkanDevice.hh>

namespace Mikoto {
    struct VulkanImageCreateInfo {
        // We can create an image passing in a valid VkImage handle
        // this could be the case for swapchain images in which case this should not be a null handle
        VkImage Image{ VK_NULL_HANDLE };

        // Fill imageCreateInfo and imageViewCreateInfo if we do not pass a valid VkImage
        VkImageCreateInfo ImageCreateInfo{};
        VkImageViewCreateInfo ImageViewCreateInfo{};
    };

    class VulkanImage final : VulkanObject {
    public:
        explicit VulkanImage(const VulkanImageCreateInfo& createInfo);

        MKT_NODISCARD auto Get() const -> VkImage { return m_Image; }
        MKT_NODISCARD auto GetView() const -> VkImageView { return m_ImageView; }

        MKT_NODISCARD auto HasExternalImage() const -> bool { return m_IsImageExternal; }

        MKT_NODISCARD auto GetCurrentLayout() const -> VkImageLayout { return m_CurrentLayout; }
        MKT_NODISCARD auto GetCreateInfo() const -> const VkImageCreateInfo& { return m_AllocInfo.ImageCreateInfo; }
        MKT_NODISCARD auto GetViewCreateInfo() const -> const VkImageViewCreateInfo& { return m_ImageViewCreateInfo; }

        auto LayoutTransition( VkImageLayout newLayout, VkCommandBuffer cmd ) -> void;

        MKT_NODISCARD static auto Create(const VulkanImageCreateInfo& createInfo) -> Scope_T<VulkanImage>;

        auto Release() -> void override;
        ~VulkanImage() override;

    private:
        bool m_IsImageExternal{ false };

        VkImage m_Image{};
        VkImageView m_ImageView{};

        VkImageLayout m_CurrentLayout{ };

        ImageAllocateInfo m_AllocInfo{};
        VkImageViewCreateInfo m_ImageViewCreateInfo{};
    };
}


#endif // MIKOTO_VULKAN_IMAGE_HH
