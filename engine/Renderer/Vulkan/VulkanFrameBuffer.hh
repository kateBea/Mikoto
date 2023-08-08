/**
 * VulkanFrameBuffer.hh
 * Created by kate on 7/10/2023.
 * */

#ifndef KATE_ENGINE_VULKAN_FRAMEBUFFER_HH
#define KATE_ENGINE_VULKAN_FRAMEBUFFER_HH

// Third-Party Library
#include <volk.h>

// Project Headers
#include <Renderer/Buffers/FrameBuffer.hh>

namespace kaTe {
    class VulkanFrameBuffer : public FrameBuffer {
    public:
        explicit VulkanFrameBuffer(const FrameBufferCreateInfo& createInfo);
        explicit VulkanFrameBuffer(const VkFramebufferCreateInfo& createInfo);

        auto Resize(UInt32_T width, UInt32_T height) -> void override;
        KT_NODISCARD auto GetFrameBufferProperties() const -> const FrameBufferCreateInfo& override { return m_CreateInfo; }

        auto OnRelease() const -> void;
    private:
        static auto OnCreate(VkFramebufferCreateInfo createInfo, VkFramebuffer &fb) -> void;
    private:
        FrameBufferCreateInfo m_CreateInfo{};
        VkFramebufferCreateInfo m_VkCreateInfo{};
        VkFramebuffer m_FrameBuffer{};
    };
}


#endif //KATE_ENGINE_VULKAN_FRAMEBUFFER_HH
