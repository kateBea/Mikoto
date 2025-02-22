//
// Created by kate on 11/12/23.
//

#ifndef MIKOTO_VULKAN_PBR_MATERIAL_HH
#define MIKOTO_VULKAN_PBR_MATERIAL_HH

#include <volk.h>
#include <glm/glm.hpp>

#include <Material/Core/Material.hh>
#include <Material/Material/PBRMaterial.hh>
#include <Models/LightData.hh>
#include <Renderer/Vulkan/VulkanBuffer.hh>
#include <Renderer/Vulkan/VulkanDescriptorManager.hh>

#include "VulkanTexture2D.hh"

namespace Mikoto {
    class VulkanPBRMaterial final : public PBRMaterial {
    public:
        explicit VulkanPBRMaterial(const PBRMaterialCreateSpec& spec, std::string_view name = GetName());

        auto SetView(const glm::mat4& mat) -> void { m_VertexUniformData.View = mat; }
        auto SetProjection(const glm::mat4& mat) -> void { m_VertexUniformData.Projection = mat; }
        auto SetTransform(const glm::mat4& transform) -> void { m_VertexUniformData.Transform = transform; }

        auto BindDescriptorSet(const VkCommandBuffer &commandBuffer, const VkPipelineLayout &pipelineLayout) -> void;

        auto UpdateLightsInfo() -> void;
        auto UploadUniformBuffers() -> void;
        auto UpdateDescriptorSets() -> void;

        MKT_NODISCARD static auto GetName() -> std::string_view { return "PBRMaterial"; }

    private:
        struct VertexUniformBufferData {
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
        Size_T m_LightsCount{};
        Size_T m_DirLightIndex{};
        Size_T m_SpotLightIndex{};
        Size_T m_PointLightIndex{};

        VulkanDescriptorWriter m_DescriptorWriter{};

        // Vertex shader uniform buffer
        Scope_T<VulkanBuffer> m_VertexUniformBuffer{};
        VertexUniformBufferData m_VertexUniformData{};

        // Fragment shader uniform buffer
        Scope_T<VulkanBuffer> m_FragmentUniformBuffer{};
        FragmentUniformBufferData m_FragmentUniformLightsData{};

        // Descriptors
        VkDescriptorSet m_DescriptorSet{};

        static inline VulkanTexture2D* s_EmptyTexture{};

        Size_T m_UniformDataStructureSize{}; // size of the UniformBufferData structure, with required padding for the device
        Size_T m_FragmentUniformDataStructureSize{}; // size of the UniformBufferData structure, with required padding for the device for fragment shader
    };
}


#endif // MIKOTO_VULKAN_PBR_MATERIAL_HH
