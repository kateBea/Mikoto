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
#include <Utility/Types.hh>
#include <Renderer/Material/Material.hh>
#include <Renderer/Material/Shader.hh>
#include <Renderer/Vulkan/VulkanPipeline.hh>
#include <Renderer/Vulkan/VulkanShader.hh>
#include <Renderer/Vulkan/VulkanBuffer.hh>
#include <Renderer/Vulkan/VulkanTexture2D.hh>
#include <Renderer/RenderingUtilities.hh>

namespace Mikoto {
    class VulkanStandardMaterial : public Material {
    public:
        explicit VulkanStandardMaterial(std::string_view name = GetStandardMaterialName());

        VulkanStandardMaterial(const VulkanStandardMaterial & other) = default;
        VulkanStandardMaterial(VulkanStandardMaterial && other) = default;

        auto operator=(const VulkanStandardMaterial & other) -> VulkanStandardMaterial & = default;
        auto operator=(VulkanStandardMaterial && other) -> VulkanStandardMaterial & = default;

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
        auto SetView(const glm::mat4& mat) -> void { m_UniformData.View = mat; }
        auto SetProjection(const glm::mat4& mat) -> void { m_UniformData.Projection = mat; }
        /**
         * Sets the value of the transformation matrix
         * @param transform new value for the transformation matrix
         * */
        auto SetTransform(const glm::mat4& transform) -> void;

        auto SetTiltingColor(float red, float green, float blue, float alpha) -> void;

        auto BindDescriptorSet(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout) -> void;

        /**
         * Sends the transform data to the mapped GPU block of memory
         * */
        auto UploadUniformBuffers() -> void;

        MKT_NODISCARD static auto GetStandardMaterialName() -> std::string_view { return "StandardMaterial"; }

        /**
         * Initializes the required shaders for all VulkanStandardMaterials.
         * Must call this function before any VulkanStandardMaterials is created.
         * */
        static auto InitializeRequiredShaders() -> void;

        /**
         * Returns the set of shaders shared amongst all VulkanStandardMaterials
         * @returns VulkanStandardMaterial shaders
         * */
        MKT_NODISCARD static auto GetShaders() -> const std::vector<VulkanShader>& { return s_Shaders; }

    private:
        struct UniformBufferData {
            // Camera
            glm::mat4 View{};
            glm::mat4 Projection{};

            // Object
            glm::mat4 Transform{};
            glm::vec4 Color{};
        };

    private:
        auto CreateUniformBuffer() -> void;
        auto CreateDescriptorPool() -> void;
        auto CreateDescriptorSet() -> void;

    private:
        VulkanBuffer m_UniformBuffer{};
        void* m_UniformBuffersMapped{};

        VkDescriptorPool m_DescriptorPool{};
        VkDescriptorSet m_DescriptorSet{};

        std::shared_ptr<VulkanTexture2D> m_Texture{};

        UniformBufferData m_UniformData{};
        Size_T m_UniformDataStructureSize{}; // size of the UniformBufferData structure, with required padding for the device

        static inline std::vector<VulkanShader> s_Shaders{};
    };
}

#endif // MIKOTO_STANDARD_MATERIAL_HH
