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
#include "Common/Common.hh"
#include "Common/Types.hh"
#include "Common/RenderingUtils.hh"
#include "Renderer/Material/Material.hh"
#include "Renderer/Material/Shader.hh"
#include "VulkanBuffer.hh"
#include "VulkanPipeline.hh"
#include "VulkanShader.hh"
#include "VulkanTexture2D.hh"

namespace Mikoto {
    class VulkanStandardMaterial : public Material {
    public:
        explicit VulkanStandardMaterial(const DefaultMaterialCreateSpec& spec, std::string_view name = GetStandardMaterialName());

        VulkanStandardMaterial(const VulkanStandardMaterial& other) = default;
        VulkanStandardMaterial(VulkanStandardMaterial && other) = default;

        auto operator=(const VulkanStandardMaterial & other) -> VulkanStandardMaterial & = default;
        auto operator=(VulkanStandardMaterial && other) -> VulkanStandardMaterial & = default;


        MKT_NODISCARD auto GetDiffuseMap() -> std::shared_ptr<VulkanTexture2D> { return m_DiffuseTexture; }


        auto UpdateDescriptorSets() -> void;

        /**
         * Sets the value of the projection and view matrix
         * @param projView new value for the the projection and view matrix
         * */
        auto SetView(const glm::mat4& mat) -> void { m_VertexUniformData.View = mat; }
        auto SetProjection(const glm::mat4& mat) -> void { m_VertexUniformData.Projection = mat; }
        /**
         * Sets the value of the transformation matrix
         * @param transform new value for the transformation matrix
         * */
        auto SetTransform(const glm::mat4& transform) -> void;


        auto SetTiltingColor(float red, float green, float blue, float alpha) -> void;

        auto BindDescriptorSet(const VkCommandBuffer &commandBuffer, const VkPipelineLayout &pipelineLayout) -> void;

        /**
         * Sends the transform data to the mapped GPU block of memory
         * */
        auto UploadUniformBuffers() -> void;

        MKT_NODISCARD static auto GetStandardMaterialName() -> std::string_view { return "StandardMaterial"; }



        // Temporary for lights
        auto SetLights( PointLight* lights, Size_T count) -> void;
        auto SetViewPosition(const glm::vec4& pos) -> void { m_FragmentUniformLightsData.ViewPosition = pos; }

        MKT_NODISCARD auto HasSpecularMap() const -> bool { return m_HasSpecular; }
        MKT_NODISCARD auto HasDiffuseMap() const -> bool { return m_HasDiffuse; }



    private:
        // stick to mat4s and vec4s for now for simplicity
        struct UniformBufferData {
            // Camera
            glm::mat4 View{};
            glm::mat4 Projection{};

            // Object
            glm::mat4 Transform{};
            glm::vec4 Color{};

            // Lights
            glm::vec4 LightPosition{};
            glm::vec4 LightColor{};
        };

        struct LightsUniformData {
            PointLight PointLights[5];

            glm::vec4 ViewPosition{};

            // Stores x=lights count, y=has diffuse, z=has specular, w=shininess
            glm::vec4 LightMeta{};
        };

    private:
        auto CreateUniformBuffers() -> void;
        auto CreateDescriptorPool() -> void;
        auto CreateDescriptorSet() -> void;

    private:
        // Vertex shader uniform buffer
        VulkanBuffer m_VertexUniformBuffer{};
        void* m_VertexUniformBuffersMapped{};
        UniformBufferData m_VertexUniformData{};

        // Fragment shader uniform buffer
        VulkanBuffer m_FragmentUniformBuffer{};
        void* m_FragmentUniformBuffersMapped{};
        LightsUniformData m_FragmentUniformLightsData{};

        // Descriptors
        VkDescriptorPool m_DescriptorPool{};
        VkDescriptorSet m_DescriptorSet{};

        std::shared_ptr<VulkanTexture2D> m_DiffuseTexture{};
        std::shared_ptr<VulkanTexture2D> m_SpecularTexture{};


        static inline std::shared_ptr<VulkanTexture2D> s_EmptyTexture{};

        Size_T m_UniformDataStructureSize{}; // size of the UniformBufferData structure, with required padding for the device
        Size_T m_FragmentUniformDataStructureSize{}; // size of the UniformBufferData structure, with required padding for the device for fragment shader

        bool m_HasDiffuse{ false };
        bool m_HasSpecular{ false };
    };
}

#endif // MIKOTO_STANDARD_MATERIAL_HH
