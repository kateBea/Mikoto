/**
 * VulkanRenderPass.hh
 * Created by kate on 8/9/2023.
 * */

#ifndef KATE_ENGINE_VULKAN_RENDER_PASS_HH
#define KATE_ENGINE_VULKAN_RENDER_PASS_HH

// Third-Party Libraries
#include <volk.h>

// Project Headers
#include <Utility/Common.hh>

namespace Mikoto {
    class VulkanRenderPass {
    public:
        explicit VulkanRenderPass() = default;

        auto OnCreate(VkRenderPassCreateInfo *createInfo, VkRenderPass *renderPass, bool useDefaultRenderPass) -> void;
        auto OnRelease() const -> void;

        auto BeginRender() -> void;
        auto EndRender() -> void;

        KT_NODISCARD static auto GetDefaultRenderPassConfig() -> VkRenderPassCreateInfo;
        KT_NODISCARD auto Get() const -> const VkRenderPass& { return m_RenderPass; }
        KT_NODISCARD auto GetBeginInfo() const -> const VkRenderPassBeginInfo& { return m_RenderPassInfo; }
        KT_NODISCARD auto GetCreateInfo() const -> const VkRenderPassCreateInfo& { return m_CreateInfo; }

        auto UpdateBeginPassInfo(const VkRenderPassBeginInfo& passInfo) -> void { m_RenderPassInfo = passInfo; }

    private:
        VkRenderPass m_RenderPass{};
        VkRenderPassBeginInfo m_RenderPassInfo{};
        VkRenderPassCreateInfo m_CreateInfo{};
    };
}


#endif // KATE_ENGINE_VULKAN_RENDER_PASS_HH
