/**
 * VulkanImage.cc
 * Created by kate on 8/9/2023.
 * */

#ifndef MIKOTO_VULKAN_IMAGE_HH
#define MIKOTO_VULKAN_IMAGE_HH

// Third-Party Libraries
#include <volk.h>

// Project Headers
#include <Utility/Common.hh>
#include <Utility/VulkanUtils.hh>

namespace Mikoto {
    struct ImageCreateInfo {
        VkImageCreateInfo ImageCreateData{};
        VkImageViewCreateInfo ImageViewCreateData{};
    };

    class VulkanImage {
    public:
        explicit VulkanImage() = default;

        auto OnCreate(const ImageCreateInfo& createInfo) -> void;
        auto OnRelease() const -> void;

        MKT_NODISCARD auto Get() const -> const VkImage& { return m_AllocInfo.Image; }
        MKT_NODISCARD auto GetView() const -> const VkImageView& { return m_ImageView; }
        MKT_NODISCARD auto GetCreateInfo() const -> const VkImageCreateInfo& { return m_AllocInfo.ImageCreateInfo; }
        MKT_NODISCARD auto GetViewCreateInfo() const -> const VkImageViewCreateInfo& { return m_ImageViewCreateInfo; }

    private:
        ImageAllocateInfo m_AllocInfo{};
        VkImageView m_ImageView{};
        VkImageViewCreateInfo m_ImageViewCreateInfo{};
    };
}


#endif // MIKOTO_VULKAN_IMAGE_HH
