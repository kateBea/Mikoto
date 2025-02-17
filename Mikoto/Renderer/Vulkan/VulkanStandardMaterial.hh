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
#include "glm/glm.hpp"

// Project Headers
#include <Common/Common.hh>
#include <Library/Utility/Types.hh>
#include <Material/Material/StandardMaterial.hh>
#include <Renderer/Vulkan/VulkanBuffer.hh>
#include <Renderer/Vulkan/VulkanDescriptorManager.hh>
#include <Renderer/Vulkan/VulkanTexture2D.hh>

namespace Mikoto {

    class VulkanStandardMaterial final : public StandardMaterial {
    public:
        explicit VulkanStandardMaterial(const StandardMaterialCreateInfo& spec);

        auto UpdateDescriptorSets() -> void;

        auto SetView(const glm::mat4& mat) -> void { m_VertexUniformData.View = mat; }
        auto SetProjection(const glm::mat4& mat) -> void { m_VertexUniformData.Projection = mat; }
        auto SetTransform(const glm::mat4& transform) -> void { m_VertexUniformData.Transform = transform; }

        auto BindDescriptorSet(const VkCommandBuffer &commandBuffer, const VkPipelineLayout &pipelineLayout) const -> void;

        auto UploadUniformBuffers() -> void;

        auto ResetLights() -> void;
        auto UpdateLightsInfo(const LightData& lightData, LightType type) -> void;

    private:
        struct UniformBufferData {
            // Camera
            glm::mat4 View{};
            glm::mat4 Projection{};

            // Object
            glm::mat4 Transform{};
            glm::vec4 Color{};
        };

        struct LightsUniformData {
            SpotLight SpotLights[MAX_LIGHTS_PER_SCENE];
            PointLight PointLights[MAX_LIGHTS_PER_SCENE];
            DirectionalLight DirectionalLights[MAX_LIGHTS_PER_SCENE];

            glm::vec4 ViewPosition{};

            // x = Total number of lights
            // y = Has a diffuse map (1 if true, 0 if false)
            // z = Has a specular map (1 if true, 0 if false)
            // w = Shininess factor
            glm::vec4 LightMeta{};

            // x = Number of directional lights
            // y = Number of point lights
            // z = Number of spotlights
            // w = unused component
            glm::vec4 LightTypesCount{};
        };

    private:
        auto SetupTextures() -> void;
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
        UniformBufferData m_VertexUniformData{};

        // Fragment shader uniform buffer
        Scope_T<VulkanBuffer> m_FragmentUniformBuffer{};
        LightsUniformData m_FragmentUniformLightsData{};

        // Descriptors
        VkDescriptorSet m_DescriptorSet{};

        static inline VulkanTexture2D* s_EmptyTexture{};

        Size_T m_UniformDataStructureSize{}; // size of the UniformBufferData structure, with required padding for the device
        Size_T m_FragmentUniformDataStructureSize{}; // size of the UniformBufferData structure, with required padding for the device for fragment shader
    };
}

#endif // MIKOTO_STANDARD_MATERIAL_HH
