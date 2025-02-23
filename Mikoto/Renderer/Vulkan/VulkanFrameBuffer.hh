/**
 * VulkanFrameBuffer.hh
 * Created by kate on 7/10/2023.
 * */

#ifndef MIKOTO_VULKAN_FRAMEBUFFER_HH
#define MIKOTO_VULKAN_FRAMEBUFFER_HH

// Third-Party Library
#include <volk.h>

// Project Headers
#include <Common/Common.hh>
#include <Renderer/Vulkan/VulkanObject.hh>

namespace Mikoto {
    struct VulkanFrameBufferCreateInfo {
        VkFramebufferCreateInfo CreateInfo{};
    };

    class VulkanFrameBuffer final : public VulkanObject {
    public:
        explicit VulkanFrameBuffer(const VulkanFrameBufferCreateInfo& createInfo);

        MKT_NODISCARD auto Get() const -> const VkFramebuffer& { return m_FrameBuffer; }
        MKT_NODISCARD auto GetCreateInfo() const -> const VkFramebufferCreateInfo& { return m_CreateInfo; }

        MKT_NODISCARD static auto Create(const VulkanFrameBufferCreateInfo& createInfo) -> Scope_T<VulkanFrameBuffer>;

        auto Release() -> void override;

        ~VulkanFrameBuffer() override;

    private:
        VkFramebuffer m_FrameBuffer{};
        VkFramebufferCreateInfo m_CreateInfo{};
    };
}

#endif // MIKOTO_VULKAN_FRAMEBUFFER_HH