/**
 * VulkanFramebuffer.hh
 * Created by kate on 7/10/2023.
 * */

#ifndef MIKOTO_VULKAN_FRAMEBUFFER_HH
#define MIKOTO_VULKAN_FRAMEBUFFER_HH

// Third-Party Library
#include <volk.h>

// Project Headers
#include <Common/Common.hh>
#include <Renderer/Framebuffer.hh>

namespace Mikoto {
    struct VulkanFramebufferDescription {
        VkFramebufferCreateInfo CreateInfo{};
    };

    class VulkanFramebuffer final : public Framebuffer {
    public:
        explicit VulkanFramebuffer(const VulkanFramebufferDescription& createInfo);

        MKT_NODISCARD auto Get() const -> const VkFramebuffer& { return m_FrameBuffer; }
        MKT_NODISCARD auto GetCreateInfo() const -> const VkFramebufferCreateInfo& { return m_CreateInfo; }

        auto Release() -> void override;

        ~VulkanFramebuffer() override;

    protected:
        auto Allocate() -> void override;

    private:
        VulkanDevice* m_Device{ nullptr };

        VkFramebuffer m_FrameBuffer{};
        VkFramebufferCreateInfo m_CreateInfo{};
    };
}

#endif // MIKOTO_VULKAN_FRAMEBUFFER_HH