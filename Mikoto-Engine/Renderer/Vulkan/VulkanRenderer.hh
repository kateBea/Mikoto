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
#include "glm/glm.hpp"
#include "volk.h"

// Project Headers
#include "Common/Common.hh"
#include "Common/VulkanUtils.hh"
#include "Renderer/Buffers/IndexBuffer.hh"
#include "Renderer/Buffers/VertexBuffer.hh"
#include "Renderer/Material/Material.hh"
#include "Renderer/Model.hh"
#include "Renderer/Renderer.hh"
#include "Renderer/RendererBackend.hh"
#include "VulkanColoredMaterial.hh"
#include "VulkanCommandPool.hh"
#include "VulkanFrameBuffer.hh"
#include "VulkanImage.hh"
#include "VulkanStandardMaterial.hh"

namespace Mikoto {
    /**
     * This structure will hold data that is necessary for the different types of
     * materials, but is not necessary to have a duplicate for each one of them. For example,
     * the pipeline for the default material will be shared amongst all default material since
     * pipelines are expensive to create and makes no sense to have one per material
     * */
    struct MaterialSharedSpecificData {
        VulkanPipeline Pipeline{};
        VkPipelineLayout MaterialPipelineLayout{};
        VkDescriptorSetLayout DescriptorSetLayout{};

        explicit MaterialSharedSpecificData() = default;
        MaterialSharedSpecificData(MaterialSharedSpecificData&&) = default;
        auto operator=(MaterialSharedSpecificData&&) noexcept -> MaterialSharedSpecificData& = default;
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
        auto QueueForDrawing(std::shared_ptr<DrawData> &&data) -> void override;


        // TODO: temporary
        auto GetSingleTextureSetLayout() -> VkDescriptorSetLayout& { return m_MaterialInfo[std::string(VulkanStandardMaterial::GetStandardMaterialName())].DescriptorSetLayout; }

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
        auto InitializeColorPipeline() -> void;
        auto InitializeSingleTexturePipeline() -> void;
        auto InitializeWireFramePipeline() -> void;

        auto InitializeMaterialSpecificData() -> void;

        // TODO: refactor
        std::unordered_map<std::string, MaterialSharedSpecificData> m_MaterialInfo{};

        VulkanStandardMaterial* m_ActiveDefaultMaterial{};
        VulkanColoredMaterial* m_ActiveColoredMaterial{};
        VulkanStandardMaterial* m_ActiveWireframeMaterial{};

    private:
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
        VulkanCommandPool m_CommandPool{};

        VkCommandBuffer m_DrawCommandBuffer{};
        std::vector<DrawData> m_DrawQueue{};
        std::vector<VkImage> m_DepthImages{};
        std::vector<VkDeviceMemory> m_DepthImageMemories{};
        std::vector<VkImageView> m_DepthImageViews{};

        bool m_UseWireframe{};
    };
}

#endif // MIKOTO_VULKAN_RENDERER_HH