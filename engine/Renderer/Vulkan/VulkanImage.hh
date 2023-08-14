/**
 * VulkanImage.cc
 * Created by kate on 8/9/2023.
 * */

#ifndef KATE_ENGINE_VULKAN_IMAGE_HH
#define KATE_ENGINE_VULKAN_IMAGE_HH

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

        KT_NODISCARD auto Get() const -> const VkImage& { return m_AllocInfo.Image; }
        KT_NODISCARD auto GetView() const -> const VkImageView& { return m_ImageView; }
        KT_NODISCARD auto GetCreateInfo() const -> const VkImageCreateInfo& { return m_AllocInfo.ImageCreateInfo; }
        KT_NODISCARD auto GetViewCreateInfo() const -> const VkImageViewCreateInfo& { return m_ImageViewCreateInfo; }

    private:
        ImageAllocateInfo m_AllocInfo{};
        VkImageView m_ImageView{};
        VkImageViewCreateInfo m_ImageViewCreateInfo{};
    };
}


#endif // KATE_ENGINE_VULKAN_IMAGE_HH
