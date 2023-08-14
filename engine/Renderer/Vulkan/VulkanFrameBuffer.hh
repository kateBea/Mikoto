/**
 * VulkanFrameBuffer.hh
 * Created by kate on 7/10/2023.
 * */

#ifndef KATE_ENGINE_VULKAN_FRAMEBUFFER_HH
#define KATE_ENGINE_VULKAN_FRAMEBUFFER_HH

// Third-Party Library
#include <volk.h>

// Project Headers
#include <Utility/Common.hh>
#include <Renderer/Buffers/FrameBuffer.hh>

namespace Mikoto {
    class VulkanFrameBuffer : public FrameBuffer {
    public:
        explicit VulkanFrameBuffer() = default;

        auto OnCreate(const VkFramebufferCreateInfo& createInfo) -> void;
        auto OnRelease() const -> void;

        auto Resize(UInt32_T width, UInt32_T height) -> void override;
        KT_NODISCARD auto GetFrameBufferProperties() const -> const FrameBufferCreateInfo& override { return m_CreateInfo; }
        KT_NODISCARD auto Get() const -> const VkFramebuffer& { return m_FrameBuffer; }

    private:
        FrameBufferCreateInfo m_CreateInfo{};
        VkFramebufferCreateInfo m_VkCreateInfo{};
        VkFramebuffer m_FrameBuffer{};
    };
}


#endif //KATE_ENGINE_VULKAN_FRAMEBUFFER_HH