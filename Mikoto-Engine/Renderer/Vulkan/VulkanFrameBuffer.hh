/**
 * VulkanFrameBuffer.hh
 * Created by kate on 7/10/2023.
 * */

#ifndef MIKOTO_VULKAN_FRAMEBUFFER_HH
#define MIKOTO_VULKAN_FRAMEBUFFER_HH

// Third-Party Library
#include "volk.h"

// Project Headers
#include "Common/Common.hh"
#include "Renderer/Buffers/FrameBuffer.hh"

namespace Mikoto {
    class VulkanFrameBuffer : public FrameBuffer {
    public:
        explicit VulkanFrameBuffer() = default;

        auto OnCreate(const VkFramebufferCreateInfo& createInfo) -> void;

        auto Resize(UInt32_T width, UInt32_T height) -> void override;
        MKT_NODISCARD auto GetFrameBufferProperties() const -> const FrameBufferCreateInfo& override { return m_CreateInfo; }
        MKT_NODISCARD auto Get() const -> const VkFramebuffer& { return m_FrameBuffer; }

    private:
        FrameBufferCreateInfo m_CreateInfo{};
        VkFramebufferCreateInfo m_VkCreateInfo{};
        VkFramebuffer m_FrameBuffer{};
    };
}

#endif // MIKOTO_VULKAN_FRAMEBUFFER_HH