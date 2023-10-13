/**
 * VulkanRenderer.hh
 * Created by kate on 7/3/23.
 * */

#ifndef MIKOTO_VULKAN_RENDERER_HH
#define MIKOTO_VULKAN_RENDERER_HH

// C++ Standard Library
#include <array>
#include <vector>
#include <filesystem>
#include <unordered_map>

// Third-Party Library
#include <volk.h>
#include <glm/glm.hpp>

// Project Headers
#include <Renderer/Buffers/IndexBuffer.hh>
#include <Renderer/Buffers/VertexBuffer.hh>
#include <Renderer/Material/Material.hh>
#include <Renderer/Model.hh>
#include <Renderer/Renderer.hh>
#include <Renderer/RendererBackend.hh>
#include <Renderer/Vulkan/VulkanCommandBuffer.hh>
#include <Renderer/Vulkan/VulkanCommandPool.hh>
#include <Renderer/Vulkan/VulkanFrameBuffer.hh>
#include <Renderer/Vulkan/VulkanImage.hh>
#include <Renderer/Vulkan/VulkanStandardMaterial.hh>
#include <Utility/Common.hh>
#include <Utility/VulkanUtils.hh>

namespace Mikoto {
    /**
     * This structure will hold data that is necessary for the different types of
     * materials, but is not necessary to have a duplicate for each one of them. Fo example,
     * the pipeline for the default material will be shared amongst all default material since
     * pipelines are expensive to create and makes no sense to have one per material
     * */
    struct MaterialSharedSpecificData {
        VkPipelineLayout MaterialPipelineLayout{};
        VkDescriptorSetLayout DescriptorSetLayout{};
        std::shared_ptr<VulkanPipeline> Pipeline{};
    };

    class VulkanRenderer : public RendererBackend {
    public:
        explicit VulkanRenderer() = default;
        ~VulkanRenderer() override = default;

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto EnableWireframeMode() -> void override;
        auto DisableWireframeMode() -> void override;

        auto SetClearColor(const glm::vec4& color) -> void override;
        auto SetClearColor(float red, float green, float blue, float alpha) -> void override;
        auto SetViewport(float x, float y, float width, float height) -> void override;

        auto Flush() -> void override;

        auto Draw() -> void override;
        auto QueueForDrawing(std::shared_ptr<DrawData> data) -> void override;

        auto OnFramebufferResize(UInt32_T width, UInt32_T height) -> void;

        MKT_NODISCARD auto GetCommandPool() -> VulkanCommandPool& { return *m_CommandPool; }
        MKT_NODISCARD auto GetOffscreenColorAttachmentImage() const -> VkImageView { return m_OffscreenColorAttachment.GetView(); }
        MKT_NODISCARD auto GetMaterialInfo() -> std::unordered_map<std::string, MaterialSharedSpecificData>& { return m_MaterialInfo; }

    private:
        enum ClearValueIndex {
            COLOR_BUFFER = 0,
            DEPTH_BUFFER = 1,
            CLEAR_COUNT,
        };

    private:
        auto PrepareOffscreen() -> void;
        auto CreateRenderPass() -> void;
        auto CreateAttachments() -> void;
        auto CreateFrameBuffers() -> void;
        auto InitializeMaterialSpecificData() -> void;

        VkExtent2D m_OffscreenExtent{};
        VkRenderPass m_OffscreenMainRenderPass{};
        VulkanFrameBuffer m_OffscreenFrameBuffer{};
        VulkanImage m_OffscreenColorAttachment{};
        VulkanImage m_OffscreenDepthAttachment{};

        VkFormat m_ColorAttachmentFormat{};
        VkFormat m_DepthAttachmentFormat{};
        VkViewport m_OffscreenViewport{};
        VkRect2D m_OffscreenScissor{};

        std::array<VkClearValue, CLEAR_COUNT> m_ClearValues{};

        bool m_OffscreenPrepareFinished{};

    private:
        static auto InitializeMaterialShaders() -> void;
        auto CreateGlobalDescriptorSetLayoutForStandardMaterial() -> void;

        std::unordered_map<std::string, MaterialSharedSpecificData> m_MaterialInfo{};

        std::shared_ptr<VulkanStandardMaterial> m_ActiveDefaultMaterial{};
        std::shared_ptr<VulkanStandardMaterial> m_ActiveWireframeMaterial{};

    private:
        auto DrawFrame(const Model &model) -> void;
        auto RecordMeshDrawCommands(const Mesh &mesh) -> void;

        /**
         * Changes the values of the Offscreen Viewport to the values passed in
         * */
        auto UpdateViewport(float x, float y, float width, float height) -> void;

        /**
         * Updates the values of the Offscreen Scissor to the values passed in
         * */
        auto UpdateScissor(Int32_T x, Int32_T y, VkExtent2D extent) -> void;

    private:
        auto CreateCommandBuffers() -> void;
        auto SubmitToQueue() -> void;

    private:
        std::vector<std::shared_ptr<DrawData>> m_DrawQueue{};
        std::shared_ptr<VulkanCommandPool> m_CommandPool{};
        VulkanCommandBuffer m_DrawCommandBuffer{};
        std::vector<VkImage> m_DepthImages{};
        std::vector<VkDeviceMemory> m_DepthImageMemories{};
        std::vector<VkImageView> m_DepthImageViews{};

        bool m_UseWireframe{};
    };
}

#endif // MIKOTO_VULKAN_RENDERER_HH