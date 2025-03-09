//
// Created by kate on 11/12/23.
//

#ifndef MIKOTO_VULKAN_PBR_MATERIAL_HH
#define MIKOTO_VULKAN_PBR_MATERIAL_HH

#include <volk.h>

#include <Material/Core/Material.hh>
#include <Material/Material/PBRMaterial.hh>
#include <Models/LightData.hh>
#include <Renderer/Vulkan/VulkanBuffer.hh>
#include <Renderer/Vulkan/VulkanDescriptorManager.hh>
#include <Renderer/Vulkan/VulkanTexture2D.hh>
#include <glm/glm.hpp>

#include "VulkanRenderer.hh"

namespace Mikoto {

    class VulkanPBRMaterial final : public PBRMaterial {
    public:
        explicit VulkanPBRMaterial(const PBRMaterialCreateSpec& spec);

        auto SetView(const glm::mat4& mat) -> void { m_VertexUniformData.View = mat; }
        auto SetProjection(const glm::mat4& mat) -> void { m_VertexUniformData.Projection = mat; }
        auto SetTransform(const glm::mat4& transform) -> void { m_VertexUniformData.Transform = transform; }
        auto SetViewPosition(const glm::vec3& viewPos) -> void { m_FragmentUniformData.ViewPosition = glm::vec4{ viewPos, 1.0f }; }
        auto SetDisplayMode(const glm::vec3& viewPos) -> void { m_FragmentUniformData.ViewPosition = glm::vec4{ viewPos, 1.0f }; }

        auto BindDescriptorSet(const VkCommandBuffer &commandBuffer, const VkPipelineLayout &pipelineLayout) -> void;

        MKT_NODISCARD auto GetPass() const -> MaterialPass { return m_MaterialPass; }

        auto UpdateLightsInfo(const LightData& lightData, LightType type) -> void;
        auto UploadUniformBuffers() -> void;
        auto UpdateDescriptorSets() -> void;

        auto EnableWireframe( const bool enable ) -> void { m_FragmentUniformData.Wireframe = enable; }

        auto ResetLights() -> void;

        auto RemoveMap( MapType type ) -> void override;
        auto SetTexture( Texture* map, MapType type ) -> void override;

        auto SetRenderMode( const Size_T mode ) -> void { m_FragmentUniformData.DisplayMode = static_cast<Int32_T>(mode); }

        MKT_NODISCARD auto HasAlbedoMap() const -> bool override { return m_HasAlbedoTexture; }
        MKT_NODISCARD auto HasNormalMap() const -> bool override { return m_HasNormalTexture; }
        MKT_NODISCARD auto HasMetallicMap() const -> bool override { return m_HasMetallicTexture; }
        MKT_NODISCARD auto HasRoughnessMap() const -> bool override { return m_HasRoughnessTexture; }
        MKT_NODISCARD auto HasAmbientOcclusionMap() const -> bool override { return m_HasAmbientOcclusionTexture; }


    private:
        struct VertexUniformBufferData {
            // Camera
            glm::mat4 View{};
            glm::mat4 Projection{};

            // Object
            glm::mat4 Transform{};
        };

        struct FragmentUniformBufferData {

            SpotLight SpotLights[MAX_LIGHTS_PER_SCENE];
            PointLight PointLights[MAX_LIGHTS_PER_SCENE];
            DirectionalLight DirectionalLights[MAX_LIGHTS_PER_SCENE];

            glm::vec4 ViewPosition{ };

            glm::vec4 Albedo{};
            glm::vec4 Factors{};

            Int32_T HasAlbedo{};
            Int32_T HasNormal{};
            Int32_T HasMetallic{};
            Int32_T HasAmbientOcc{};
            Int32_T HasRoughness{};

            Int32_T DirectionalLightCount{};
            Int32_T PointLightCount{};
            Int32_T SpotLightCount{};

            Int32_T DisplayMode{ DISPLAY_COLOR };

            Int32_T Wireframe{ DISPLAY_COLOR };
        };

    private:

        auto SetupTextures() -> void;
        auto CreateUniformBuffers() -> void;
        auto CreateDescriptorSet() -> void;

    private:
        bool m_HasAlbedoTexture{ true };
        bool m_HasSpecularTexture{ true };
        bool m_HasNormalTexture{ true };
        bool m_HasMetallicTexture{ true };
        bool m_HasRoughnessTexture{ true };
        bool m_HasAmbientOcclusionTexture{ true };

        // One pass for now
        MaterialPass m_MaterialPass{ MATERIAL_PASS_PBR };

        VulkanDescriptorWriter m_DescriptorWriter{};

        // Vertex shader uniform buffer
        Scope_T<VulkanBuffer> m_VertexUniformBuffer{};
        VertexUniformBufferData m_VertexUniformData{};

        // Fragment shader uniform buffer
        Scope_T<VulkanBuffer> m_FragmentUniformBuffer{};
        FragmentUniformBufferData m_FragmentUniformData{};

        // Descriptors
        VkDescriptorSet m_DescriptorSet{};

        bool m_WantDescriptorUpdate{ false };

        Size_T m_UniformDataStructureSize{}; // size of the UniformBufferData structure, with required padding for the device
        Size_T m_FragmentUniformDataStructureSize{}; // size of the UniformBufferData structure, with required padding for the device for fragment shader
    };
}


#endif // MIKOTO_VULKAN_PBR_MATERIAL_HH
