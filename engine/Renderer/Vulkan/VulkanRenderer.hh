/**
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
#include <Utility/Common.hh>
#include <Renderer/Model.hh>
#include <Renderer/Renderer.hh>
#include <Renderer/RendererAPI.hh>
#include <Renderer/Material/Material.hh>
#include <Renderer/Vulkan/VulkanImage.hh>
#include <Renderer/Buffers/IndexBuffer.hh>
#include <Renderer/Buffers/VertexBuffer.hh>
#include <Renderer/Vulkan/VulkanCommandPool.hh>
#include <Renderer/Vulkan/VulkanFrameBuffer.hh>
#include <Renderer/Vulkan/VulkanCommandBuffer.hh>
#include <Renderer/Vulkan/VulkanStandardMaterial.hh>

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

        auto GetCommandPool() -> VulkanCommandPool& { return *m_CommandPool; }

        KT_NODISCARD auto GetDescriptorSet() const -> VkDescriptorSet { return m_OffscreenDescriptorSet; }
        KT_NODISCARD auto GetOffscreenColorAttachmentImage() const -> VkImageView { return m_OffscreenColorAttachment.GetView(); }

        ~VulkanRenderer() override = default;

    private:
        /*************************************************************
        * HELPERS (Setup offscreen rendering)
        * ********************************************************+ */
        auto PrepareOffscreen() -> void;
        auto CreateRenderPass() -> void;
        auto CreateAttachments() -> void;
        auto CreateFrameBuffers() -> void;

        // These are the default values for the frame buffers we will be rendering into
        // Although, it is more appropriate to adjust them, so they match the viewport values
        static constexpr UInt32_T DEFAULT_FRAMEBUFFER_WIDTH{ 1280 };
        static constexpr UInt32_T DEFAULT_FRAMEBUFFER_HEIGHT{ 720 };

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

    private:
        /*************************************************************
        * HELPERS (For geometry drawing)
        * ********************************************************+ */
        auto DrawFrame(const Model &model) -> void;
        auto RecordMainRenderPassCommands(const Mesh &mesh) -> void;

        auto UpdateViewport(UInt32_T x, UInt32_T y, UInt32_T width, UInt32_T height) -> void;
        auto UpdateScissor(Int32_T x, Int32_T y, VkExtent2D extent) -> void;

    private:
        /*************************************************************
        * HELPERS (Renderer Initialization)
        * ********************************************************+ */
        auto CreateCommandBuffers() -> void;

    private:
        /*************************************************************
        * PRIVATE MEMBERS
        * ********************************************************+ */
        std::shared_ptr<VulkanStandardMaterial> m_DefaultMaterial{};
        std::shared_ptr<VulkanCommandPool> m_CommandPool{};

        std::vector<VulkanCommandBuffer> m_CommandBuffers{};

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
