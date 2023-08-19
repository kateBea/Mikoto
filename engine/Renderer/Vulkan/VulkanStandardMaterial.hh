/**
 * VulkanStandardMaterial.hh
 * Created by kate on 6/30/23.
 * */

#ifndef MIKOTO_STANDARD_MATERIAL_HH
#define MIKOTO_STANDARD_MATERIAL_HH

// C++ Standard Library
#include <string>
#include <string_view>
#include <memory>

// Third-Party Libraries
#include <glm/glm.hpp>

// Project Headers
#include <Utility/Common.hh>
#include <Renderer/Material/Material.hh>
#include <Renderer/Material/Shader.hh>
#include <Renderer/Material/Texture.hh>
#include <Renderer/Vulkan/VulkanPipeline.hh>
#include <Renderer/Vulkan/VulkanShader.hh>
#include <Renderer/Vulkan/VulkanBuffer.hh>
#include <Renderer/Vulkan/VulkanTexture2D.hh>
#include <Renderer/RenderingUtilities.hh>

namespace Mikoto {
    struct VulkanStandardMaterialCreateInfo {
        VkRenderPass RenderPass{};
    };

    class VulkanStandardMaterial : public Material {
    public:
        explicit VulkanStandardMaterial(std::string_view name = "VulkanStandardMaterial");

        VulkanStandardMaterial(const VulkanStandardMaterial & other) = default;
        VulkanStandardMaterial(VulkanStandardMaterial && other) = default;

        auto operator=(const VulkanStandardMaterial & other) -> VulkanStandardMaterial & = default;
        auto operator=(VulkanStandardMaterial && other) -> VulkanStandardMaterial & = default;

        /**
         * Initializes this material
         * */
        auto OnCreate(const VulkanStandardMaterialCreateInfo& createInfo) -> void;

        /**
         * Releases the resources held by this material
         * */
        auto OnRelease() const -> void;

        /**
         * Returns the texture
         * @returns texture from this material
         * */
        auto GetTexture() -> std::shared_ptr<VulkanTexture2D>& { return m_Texture; }

        /**
         * Sets the value of the projection and view matrix
         * @param projView new value for the the projection and view matrix
         * */
        auto SetProjectionView(const glm::mat4& projView) -> void;
        /**
         * Sets the value of the transformation matrix
         * @param transform new value for the transformation matrix
         * */
        auto SetTransform(const glm::mat4& transform) -> void;

        auto SetTiltingColor(float red, float green, float blue, float alpha) -> void;
        auto SetTiltingColor(const glm::vec4& color) -> void;

        auto GetPipeline() -> VulkanPipeline& { return *m_Pipeline; }
        auto BindDescriptorSets(VkCommandBuffer commandBuffer) -> void;
        /**
         * Sends the transform data to the mapped GPU block of memory
         * */
        auto UploadUniformBuffers() -> void;

        // SEPARATE TO A DIFERENT PIPELINE THAT WILL BE PART OF THE RENDERER
        auto EnableWireframe() -> void;
        auto DisableWireframe() -> void;

    private:
        /*************************************************************
        * HELPERS
        * ********************************************************+ */
        auto CreateUniformBuffer() -> void;
        auto CreateDescriptorPool() -> void;
        auto CreateDescriptorSet() -> void;

        auto CreateDescriptorSetLayout() -> void;
        auto CreatePipelineLayout() -> void;
        auto CreatePipeline() -> void;

    private:
        VulkanBuffer m_UniformBuffer{};
        void* m_UniformBuffersMapped{};

        VkPipelineLayout m_PipelineLayout{};
        VkDescriptorSetLayout m_DescriptorSetLayout{};
        VkDescriptorPool m_DescriptorPool{};
        VkDescriptorSet m_DescriptorSet{};
        VkCommandBuffer m_CommandBuffers{};
        VkRenderPass m_RenderPass{};

        std::shared_ptr<VulkanPipeline> m_Pipeline{};
        std::shared_ptr<VulkanShader> m_Shader{};
        std::shared_ptr<VulkanTexture2D> m_Texture{};
        UniformTransformData m_Transform{};
    };
}


#endif // MIKOTO_STANDARD_MATERIAL_HH
