/**
 * Created by kate on 7/3/23.
 * */

#ifndef KATE_ENGINE_VULKAN_RENDERER_HH
#define KATE_ENGINE_VULKAN_RENDERER_HH

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
#include <Renderer/Buffers/IndexBuffer.hh>
#include <Renderer/Buffers/VertexBuffer.hh>
#include <Renderer/Vulkan/VulkanCommandPool.hh>
#include <Renderer/Vulkan/VulkanFrameBuffer.hh>
#include <Renderer/Vulkan/VulkanStandardMaterial.hh>

namespace kaTe {
    class VulkanRenderer : public RendererAPI {
    public:
        explicit VulkanRenderer() = default;

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto EnableWireframeMode() -> void override;
        auto DisableWireframeMode() -> void override;

        auto SetClearColor(const glm::vec4& color) -> void override;
        auto SetClearColor(float red, float green, float blue, float alpha) -> void override;

        auto Draw(const DrawData& data) -> void;
        auto Draw(const RenderingData &data) -> void override;

        auto SetViewport(UInt32_T x, UInt32_T y, UInt32_T width, UInt32_T height) -> void override;
        auto OnEvent(Event& event) -> void override;

        auto GetCommandPool() -> VulkanCommandPool& { return *m_CommandPool; }

        ~VulkanRenderer() override = default;
    private:
        friend class VulkanStandardMaterial;
        friend class Application;

    private:
        /*************************************************************
        * HELPERS
        * ********************************************************+ */
        auto CreateRenderPass() -> void;
        auto CreateImages() -> void;
        auto CreateDepthResources() -> void;
        auto CreateFrameBuffers() -> void;
        auto CreateCommandBuffers() -> void;

        auto CreateDescriptorLayout() -> void;
        auto CreateDescriptorPool() -> void;
        auto CreateDescriptorSet() -> void;

        auto DrawFrame(const Model &model) -> void;
        auto RecordCommandBuffers(UInt32_T imageIndex, const Mesh& model) -> void;

        auto UpdateViewport(UInt32_T xVal, UInt32_T yVal, UInt32_T widthVal, UInt32_T heightVal) -> void;
        auto UpdateScissor(Int32_T x, Int32_T y, VkExtent2D extent) -> void;

    private:
        /*************************************************************
        * PRIVATE MEMBERS
        * ********************************************************+ */
        std::shared_ptr<VulkanStandardMaterial> m_DefaultMaterial{};
        std::shared_ptr<VulkanCommandPool> m_CommandPool{};
        std::vector<VkCommandBuffer> m_CommandBuffers{};

        std::vector<VulkanFrameBuffer> m_FrameBuffers{};

        // Temporary
        // Index 0 represents clear values for color buffer
        // Index 1 represents clear values for depth/stencil
        std::array<VkClearValue, 2> m_ClearValues{};

        VkViewport m_Viewport{};
        VkRect2D m_Scissor{};

        VkDescriptorPool m_DescriptorPool{};
        VkDescriptorSet m_DescriptorSet{};
        VkDescriptorSetLayout m_DescriptorSetLayout{};
        VkRenderPass m_MainRenderPass{};

        std::vector<VkImage> m_Images{};
        std::vector<VkImageView> m_ImageViews{};

        std::vector<VkImage> m_DepthImages{};
        std::vector<VkDeviceMemory> m_DepthImageMemories{};
        std::vector<VkImageView> m_DepthImageViews{};
    };
}


#endif //KATE_ENGINE_VULKAN_RENDERER_HH
