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
#include <Utility/Common.hh>
#include <Utility/VulkanUtils.hh>
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

    class VulkanRenderer : public RendererAPI {
    public:
        explicit VulkanRenderer() = default;

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto EnableWireframeMode() -> void override;
        auto DisableWireframeMode() -> void override;

        auto SetClearColor(const glm::vec4& color) -> void override;
        auto SetClearColor(float red, float green, float blue, float alpha) -> void override;
        auto SetViewport(UInt32_T x, UInt32_T y, UInt32_T width, UInt32_T height) -> void override;

        auto Flush() -> void override;

        auto Draw() -> void override;
        auto QueueForDrawing(std::shared_ptr<DrawData>) -> void override;

        auto OnEvent(Event& event) -> void override;
        auto OnFramebufferResize(UInt32_T width, UInt32_T height) -> void;

        MKT_NODISCARD auto GetCommandPool() -> VulkanCommandPool& { return *m_CommandPool; }
        MKT_NODISCARD auto GetOffscreenColorAttachmentImage() const -> VkImageView { return m_OffscreenColorAttachment.GetView(); }
        MKT_NODISCARD auto GetMaterialInfo() -> std::unordered_map<std::string, MaterialSharedSpecificData>& { return m_MaterialInfo; }

        ~VulkanRenderer() override = default;
    private:
        /*************************************************************
        * STRUCTURES
        * ***********************************************************/
        enum ClearValueIndex {
            COLOR_BUFFER = 0,
            DEPTH_BUFFER = 1,
            CLEAR_COUNT,
        };

    private:
        /*************************************************************
        * HELPERS (Setup for offscreen rendering)
        * ***********************************************************/
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
        /*************************************************************
        * MATERIALS
        * ***********************************************************/
        auto CreateGlobalDescriptorSetLayoutForStandardMaterial() -> void;

        std::unordered_map<std::string, MaterialSharedSpecificData> m_MaterialInfo{};

        std::shared_ptr<VulkanStandardMaterial> m_ActiveDefaultMaterial{};
        std::shared_ptr<VulkanStandardMaterial> m_ActiveWireframeMaterial{};

    private:
        /*************************************************************
        * HELPERS (For geometry drawing)
        * ***********************************************************/
        auto DrawFrame(const Model &model) -> void;
        auto RecordMeshDrawCommands(const Mesh &mesh) -> void;

        auto UpdateViewport(UInt32_T x, UInt32_T y, UInt32_T width, UInt32_T height) -> void;
        auto UpdateScissor(Int32_T x, Int32_T y, VkExtent2D extent) -> void;

    private:
        /*************************************************************
        * HELPERS (Renderer Initialization)
        * ***********************************************************/
        auto CreateCommandBuffers() -> void;
        auto SubmitToQueue() -> void;

    private:
        /*************************************************************
        * PRIVATE MEMBERS
        * ***********************************************************/
        std::vector<std::shared_ptr<DrawData>> m_DrawQueue{};
        std::shared_ptr<VulkanCommandPool> m_CommandPool{};
        VulkanCommandBuffer m_DrawCommandBuffer{};
        std::vector<VkImage> m_DepthImages{};
        std::vector<VkDeviceMemory> m_DepthImageMemories{};
        std::vector<VkImageView> m_DepthImageViews{};
    };
}

#endif // MIKOTO_VULKAN_RENDERER_HH