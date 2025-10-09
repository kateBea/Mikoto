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

    class VulkanFramebuffer final : public Framebuffer {
    public:
        explicit VulkanFramebuffer(const FramebufferDescription& createInfo);

        MKT_NODISCARD auto GetImplHandle() -> VkFramebuffer* { return std::addressof(m_FrameBuffer); }
        MKT_NODISCARD auto GetCreateInfo() const -> const VkFramebufferCreateInfo& { return m_CreateInfo; }

        ~VulkanFramebuffer() override;

    protected:
        auto Allocate() -> void override;
        auto Release() -> void override;

    private:
        VkFramebuffer m_FrameBuffer{};
        VkFramebufferCreateInfo m_CreateInfo{};
    };
}

#endif // MIKOTO_VULKAN_FRAMEBUFFER_HH