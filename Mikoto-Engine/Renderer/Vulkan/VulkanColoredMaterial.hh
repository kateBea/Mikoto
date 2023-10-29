//
// Created by kate on 10/29/23.
//

#ifndef MIKOTO_VULKAN_COLORED_MATERIAL_HH
#define MIKOTO_VULKAN_COLORED_MATERIAL_HH

#include <string_view>

#include <volk.h>
#include <glm/glm.hpp>

#include <Common/Types.hh>
#include <Common/Common.hh>

#include <Renderer/Material/Material.hh>
#include <Renderer/Vulkan/VulkanBuffer.hh>

namespace Mikoto {
    class VulkanColoredMaterial : public Material {
    public:
        explicit VulkanColoredMaterial(std::string_view name = GetColoredMaterialName());

        VulkanColoredMaterial(VulkanColoredMaterial& other) = default;
        VulkanColoredMaterial(VulkanColoredMaterial && other) = default;

        auto operator=(const VulkanColoredMaterial & other) -> VulkanColoredMaterial& = default;
        auto operator=(VulkanColoredMaterial && other) -> VulkanColoredMaterial& = default;

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

        auto SetColor(float red, float green, float blue, float alpha) -> void;

        auto BindDescriptorSet(const VkCommandBuffer &commandBuffer, const VkPipelineLayout &pipelineLayout) -> void;

        /**
         * Sends the transform data to the mapped GPU block of memory
         * */
        auto UploadUniformBuffers() -> void;

        MKT_NODISCARD static auto GetColoredMaterialName() -> std::string_view { return "ColoredMaterial"; }


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

        UniformBufferData m_UniformData{};
        Size_T m_UniformDataStructureSize{}; // size of the UniformBufferData structure, with required padding for the device
    };
}


#endif//MIKOTO_VULKAN_COLORED_MATERIAL_HH
