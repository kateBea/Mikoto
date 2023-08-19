/**
 * VulkanRenderer.hh
 * Created by kate on 7/3/23.
 * */

#ifndef MIKOTO_VULKAN_RENDERER_HH
#define MIKOTO_VULKAN_RENDERER_HH

// C++ Standard Library
#include <array>
#include <filesystem>

// Third-Party Library
#include <volk.h>
#include <glm/glm.hpp>

// Project Headers
#include "VulkanVertexBuffer.hh"
#include <Renderer/Buffers/IndexBuffer.hh>
#include <Renderer/Buffers/VertexBuffer.hh>
#include <Renderer/Material/Material.hh>
#include <Renderer/Model.hh>
#include <Renderer/Renderer.hh>
#include <Renderer/RendererAPI.hh>
#include <Renderer/Vulkan/VulkanCommandBuffer.hh>
#include <Renderer/Vulkan/VulkanCommandPool.hh>
#include <Renderer/Vulkan/VulkanFrameBuffer.hh>
#include <Renderer/Vulkan/VulkanImage.hh>
#include <Renderer/Vulkan/VulkanStandardMaterial.hh>
#include <Utility/Common.hh>

namespace Mikoto {
    class VulkanRenderer : public RendererAPI {
    public:
        explicit VulkanRenderer() = default;

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto EnableWireframeMode() -> void override;
        auto DisableWireframeMode() -> void override;

        auto SetClearColor(const glm::vec4& color) -> void override;
        auto SetClearColor(float red, float green, float blue, float alpha) -> void override;

        auto Draw(const DrawData& data) -> void override;

        auto SetViewport(UInt32_T x, UInt32_T y, UInt32_T width, UInt32_T height) -> void override;

        auto OnEvent(Event& event) -> void override;
        auto OnFramebufferResize(UInt32_T width, UInt32_T height) -> void;

        auto GetCommandPool() -> VulkanCommandPool& { return *m_CommandPool; }

        KT_NODISCARD auto GetDescriptorSet() const -> VkDescriptorSet { return m_OffscreenDescriptorSet; }
        KT_NODISCARD auto GetOffscreenColorAttachmentImage() const -> VkImageView { return m_OffscreenColorAttachment.GetView(); }

        ~VulkanRenderer() override = default;
    private:
        /*************************************************************
        * STRUCTURES
        * ***********************************************************/
        struct SubmitInfo {
            VkSemaphore ImageAvailableSemaphore{};
            VkSemaphore RenderFinishedSemaphore{};
            VkFence Fence{};
        };

    private:
        /*************************************************************
        * HELPERS (Setup offscreen rendering)
        * ***********************************************************/
        auto PrepareOffscreen() -> void;
        auto CreateRenderPass() -> void;
        auto CreateAttachments() -> void;
        auto CreateFrameBuffers() -> void;

        VkExtent2D m_OffscreenExtent{};
        VkRenderPass m_OffscreenMainRenderPass{};
        VulkanFrameBuffer m_OffscreenFrameBuffer{};
        VulkanImage m_OffscreenColorAttachment{};
        VulkanImage m_OffscreenDepthAttachment{};

        // For usage with ImGui
        VkSampler m_OffscreenSampler{};
        VkDescriptorSet m_OffscreenDescriptorSet{};

        VkFormat m_ColorAttachmentFormat{};
        VkFormat m_DepthAttachmentFormat{};

        bool m_OffscreenPrepareFinished{};

    private:
        /*************************************************************
        * HELPERS (For geometry drawing)
        * ***********************************************************/
        auto DrawFrame(const Model &model) -> void;
        auto RecordMainRenderPassCommands(const Mesh &mesh) -> void;

        auto UpdateViewport(UInt32_T x, UInt32_T y, UInt32_T width, UInt32_T height) -> void;
        auto UpdateScissor(Int32_T x, Int32_T y, VkExtent2D extent) -> void;

    private:
        /*************************************************************
        * HELPERS (Renderer Initialization)
        * ***********************************************************/
        auto CreateCommandBuffers() -> void;
        auto SubmitToQueue() -> void;
        auto CreateSynchronizationObjects() -> void;

    private:
        /*************************************************************
        * PRIVATE MEMBERS
        * ***********************************************************/
        std::shared_ptr<VulkanStandardMaterial> m_DefaultMaterial{};
        std::shared_ptr<VulkanCommandPool> m_CommandPool{};

        VulkanCommandBuffer m_DrawCommandBuffer{};

        SubmitInfo m_QueueSubmitData{};

        // Temporary
        // Index 0 represents clear values for color buffer
        // Index 1 represents clear values for depth/stencil
        std::array<VkClearValue, 2> m_ClearValues{};

        VkViewport m_Viewport{};
        VkRect2D m_Scissor{};

        std::vector<VkImage> m_DepthImages{};
        std::vector<VkDeviceMemory> m_DepthImageMemories{};
        std::vector<VkImageView> m_DepthImageViews{};
    };
}


#endif // MIKOTO_VULKAN_RENDERER_HH
