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
#include "VulkanCommandPool.hh"
#include "VulkanFrameBuffer.hh"
#include "VulkanImage.hh"
#include "VulkanStandardMaterial.hh"
#include <Renderer/Vulkan/VulkanPBRMaterial.hh>

namespace Mikoto {
    /**
     * This structure will hold data that is necessary for the different types of
     * materials, but is not necessary to have a duplicate for each one of them. For example,
     * the pipeline for the default material will be shared amongst all default material since
     * pipelines are expensive to create and makes no sense to have one per material
     * */
    struct PipelineInfo {
        VulkanPipeline Pipeline{};
        VkPipelineLayout MaterialPipelineLayout{};
        VkDescriptorSetLayout DescriptorSetLayout{};

        explicit PipelineInfo() = default;
        PipelineInfo( PipelineInfo&& ) = default;
        auto operator=( PipelineInfo&& ) noexcept -> PipelineInfo& = default;
    };

    class VulkanRenderer : public RendererBackend {
    public:
        /**
         * Default constructs this renderer.
         * */
        explicit VulkanRenderer() = default;


        /**
         * @brief Default destructor
         * */
        ~VulkanRenderer() override = default;


        /**
         * Initializes the Vulkan renderer by setting up necessary components and
         * preparing the rendering environment. This function creates the command pool,
         * command buffers, adjusts clearing colors, prepares for offscreen rendering,
         * and initializes material-specific data structures required for the rendering pipeline.
         * Must call once after creating an instance of this object and before any call to any other function.
         * */
        auto Init() -> void override;


        /**
         * Shuts down the Vulkan renderer releasing all of its resources.
         * */
        auto Shutdown() -> void override;


        /**
         * Enables wireframe mode for rendering.
         * */
        auto EnableWireframeMode() -> void override;


        /**
         * Disables wireframe mode for rendering.
         * */
        auto DisableWireframeMode() -> void override;


        /**
         * Sets the color to clear the color buffer.
         * @param color The color vector to set as the clear color.
         * */
        auto SetClearColor(const glm::vec4& color) -> void override;


        /**
         * Sets the color to clear the color buffer.
         * @param red The red component of the clear color.
         * @param green The green component of the clear color.
         * @param blue The blue component of the clear color.
         * @param alpha The alpha component of the clear color.
         * */
        auto SetClearColor(float red, float green, float blue, float alpha) -> void override;


        /**
         * Sets the viewport for rendering.
         * @param x The x-coordinate of the viewport.
         * @param y The y-coordinate of the viewport.
         * @param width The width of the viewport.
         * @param height The height of the viewport.
         * */
        auto SetViewport(float x, float y, float width, float height) -> void override;


        /**
         * Flushes the rendering queue. Records the appropriate commands
         * to draw currently enqueued objects.
         * */
        auto Flush() -> void override;


        /**
         * Queues provided data for drawing.
         * @param data The shared pointer to the DrawData to be queued.
         * */
        auto QueueForDrawing(std::shared_ptr<DrawData>&& data) -> void override;


        /**
         * Retrieves the Vulkan image representing the final output.
         * @return VulkanImage representing the final color attachment.
         * */
        MKT_NODISCARD auto GetFinalImage() const -> VulkanImage {
            return m_OffscreenColorAttachment;
        }


        /**
         * Retrieves a reference to the material-specific data information maintained by the renderer.
         * @return Reference to an unordered map containing the material-specific data for rendering.
         * */
        MKT_NODISCARD auto GetMaterialInfo() -> std::unordered_map<std::string, PipelineInfo>& {
            return m_MaterialInfo;
        }


    private:
        /**
         * @brief Signals the type of buffer we want to clear.
         * Serves as an index to the array containing the clear values.
         * */
        enum ClearValueIndex {
            COLOR_BUFFER,
            DEPTH_BUFFER,
            CLEAR_COUNT,
        };

    private:
        /**
         * Draws the scene. Responsible for recording draw commands.
         * */
        auto Draw() -> void override;


        /**
         * Prepares the offscreen rendering by creating necessary render passes, attachments, and frame buffers.
         * */
        auto PrepareOffscreen() -> void;


        /**
         * Creates the render pass necessary for this Vulkan renderer.
         * */
        auto CreateOffscreenRenderPass() -> void;


        /**
         * Creates attachments required for rendering.
         * */
        auto CreateOffscreenAttachments() -> void;


        /**
         * Creates frame buffers to be used for rendering.
         * */
        auto CreateOffscreenFramebuffers() -> void;


        /**
         * Creates command buffers for drawing.
         * */
        auto CreateCommandBuffers() -> void;


        /**
         * Initializes the wireframe pipeline for rendering.
         * */
        auto InitializeWireFramePipeline() -> void;


        /**
         * Initializes the pipeline for rendering a single texture.
         * */
        auto InitializeDefaultPipeline() -> void;


        /**
         * Initializes the data necessary for rendering materials.
         * */
        auto InitializePipelinesData() -> void;


        /**
         * Updates the viewport dimensions for rendering.
         * @param x X-coordinate of the viewport.
         * @param y Y-coordinate of the viewport.
         * @param width Width of the viewport.
         * @param height Height of the viewport.
         * */
        auto UpdateViewport(float x, float y, float width, float height) -> void;


        /**
         * @brief Updates the scissor region for rendering.
         * @param x X-coordinate of the scissor.
         * @param y Y-coordinate of the scissor.
         * @param extent The extent of the scissor region.
         */
        auto UpdateScissor(Int32_T x, Int32_T y, VkExtent2D extent) -> void;


        /**
         * @brief Submits recorded commands to be executed.
         * For now the recorded commands are submitted to
         * the graphics queue of the main logical device.
         * */
        auto SubmitToQueue() -> void;


    private:
        VkRenderPass m_OffscreenMainRenderPass{};
        VulkanImage m_OffscreenColorAttachment{};
        VulkanImage m_OffscreenDepthAttachment{};
        VulkanFrameBuffer m_OffscreenFrameBuffer{};

        VkFormat m_ColorAttachmentFormat{};
        VkFormat m_DepthAttachmentFormat{};

        VkExtent2D m_OffscreenExtent{};
        VkViewport m_OffscreenViewport{};
        VkRect2D m_OffscreenScissor{};
        std::array<VkClearValue, CLEAR_COUNT> m_ClearValues{};

        std::unordered_map<std::string, PipelineInfo> m_MaterialInfo{};
        VulkanStandardMaterial* m_ActiveDefaultMaterial{};

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