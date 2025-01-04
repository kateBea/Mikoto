//
// Created by kate on 11/12/23.
//

#ifndef MIKOTO_VULKAN_PBR_MATERIAL_HH
#define MIKOTO_VULKAN_PBR_MATERIAL_HH

#include <volk.h>

#include <Common/RenderingUtils.hh>
#include <Material/Core/Material.hh>
#include <Material/Material/PhysicallyBasedMaterial.hh>
#include <Renderer/Vulkan/VulkanBuffer.hh>
#include <glm/glm.hpp>

#include "VulkanTexture2D.hh"
#include <Models/LightTypeData.hh>
#include <Models/LightRenderData.hh>

namespace Mikoto {
    class VulkanPBRMaterial final : public PhysicallyBasedMaterial {
    public:
        explicit VulkanPBRMaterial(const PBRMaterialCreateSpec& spec, std::string_view name = GetName());

        VulkanPBRMaterial(const VulkanPBRMaterial& other) = default;
        VulkanPBRMaterial(VulkanPBRMaterial && other) = default;

        auto operator=(const VulkanPBRMaterial & other) -> VulkanPBRMaterial & = default;
        auto operator=(VulkanPBRMaterial && other) -> VulkanPBRMaterial & = default;

        auto SetView(const glm::mat4& mat) -> void { m_VertexUniformData.View = mat; }
        auto SetProjection(const glm::mat4& mat) -> void { m_VertexUniformData.Projection = mat; }
        auto SetTransform(const glm::mat4& transform) -> void { m_VertexUniformData.Transform = transform; }

        auto BindDescriptorSet(const VkCommandBuffer &commandBuffer, const VkPipelineLayout &pipelineLayout) -> void;

        auto UpdateLightsInfo() -> void;
        auto UploadUniformBuffers() -> void;
        auto UpdateDescriptorSets() -> void;

        MKT_NODISCARD static auto GetName() -> std::string_view { return "PBRMaterial"; }

    private:
        struct VertexUniformBuffer {
            // Camera
            glm::mat4 View{};
            glm::mat4 Projection{};

            // Object
            glm::mat4 NormalMat{};
            glm::mat4 Transform{};
            glm::vec4 Color{};
        };

        struct FragmentUniformBufferData {
            /** Lights information */
            SpotLight SpotLights[MAX_LIGHTS_PER_SCENE];
            PointLight PointLights[MAX_LIGHTS_PER_SCENE];
            DirectionalLight DirectionalLights[MAX_LIGHTS_PER_SCENE];

            /** represents the camera position */
            glm::vec4 ViewPosition;

            /**
             * x = Has/hasn't albedo map
             * y = Has/hasn't normal map
             * z = Has/hasn't metallic map
             * w = Has/hasn't roughness map
             * */
            glm::vec4 TextureMapInfo;

            /**
             * x = Has/hasn't ao map
             * */
            glm::vec4 TextureMapInfo2;

            /**
             * Represents the count of lights for each type
             *
             * x = Directional lights count
             * y = point lights count
             * z = spot lights count
             * */
            glm::vec4 LightTypesCount;


            /** Material parameters */
            glm::vec4 Albedo;

            /**
             * x = Metallic
             * y = Roughness
             * z = ao
             * w = unused
             * */
            glm::vec4 MaterialParams;
        };

    private:
        auto CreateUniformBuffers() -> void;
        auto CreateDescriptorPool() -> void;
        auto CreateDescriptorSet() -> void;

    private:
        static inline std::shared_ptr<VulkanTexture2D> s_EmptyTexture{ nullptr };

        // Vertex shader uniform buffers
        VulkanBuffer m_VertexUniformBuffer{};
        Size_T m_VertexShaderUniformPaddedSize{};
        VertexUniformBuffer m_VertexUniformData{};

        // Fragment shader uniform buffers
        VulkanBuffer m_FragmentUniformBuffer{};
        Size_T m_FragmentShaderUniformPaddedSize{};
        FragmentUniformBufferData m_FragmentUniformData{};

        // Descriptors
        VkDescriptorPool m_DescriptorPool{};
        VkDescriptorSet m_DescriptorSet{};
    };
}


#endif // MIKOTO_VULKAN_PBR_MATERIAL_HH
