/**
 * VulkanStandardMaterial.hh
 * Created by kate on 6/30/23.
 * */

#ifndef KATE_ENGINE_STANDARD_MATERIAL_HH
#define KATE_ENGINE_STANDARD_MATERIAL_HH

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
#include <Renderer/Vulkan/VulkanTexture2D.hh>

namespace Mikoto {
    class VulkanStandardMaterial : public Material {
    public:
        explicit VulkanStandardMaterial(std::string_view name = "VulkanStandardMaterial");

        VulkanStandardMaterial(const VulkanStandardMaterial & other) = default;
        VulkanStandardMaterial(VulkanStandardMaterial && other) = default;

        auto operator=(const VulkanStandardMaterial & other) -> VulkanStandardMaterial & = default;
        auto operator=(VulkanStandardMaterial && other) -> VulkanStandardMaterial & = default;

        auto OnRelease() const -> void;

        auto GetTexture() -> std::shared_ptr<VulkanTexture2D>& { return m_Texture; }

        auto SetModelMatrix(const glm::mat4& model) -> void;
        auto SetViewMatrix(const glm::mat4& view) -> void;
        auto SetProjectionMatrix(const glm::mat4& proj) -> void;

        auto SetTexture(const std::shared_ptr<Texture>& texture) -> void { m_Texture = std::dynamic_pointer_cast<VulkanTexture2D>(texture); }

        auto GetPipeline() -> VulkanPipeline& { return *m_Pipeline; }
        auto BindDescriptorSets(VkCommandBuffer commandBuffer) -> void;
        auto UpdateUniformBuffers(UInt32_T frame) -> void;

        auto EnableWireframe() -> void;
        auto DisableWireframe() -> void;

    private:
        auto CreateUniformBuffers() -> void;
        auto CreateDescriptorPool() -> void;
        auto CreateDescriptorSets() -> void;

        auto CreateDescriptorSetLayout() -> void;
        auto CreatePipelineLayout() -> void;
        auto CreatePipeline() -> void;

        static auto CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) -> void;
    private:
        struct UniformBufferObject {
            glm::mat4 Model{};
            glm::mat4 View{};
            glm::mat4 Projection{};
        };


        std::vector<VkBuffer> m_UniformBuffers;
        std::vector<VkDeviceMemory> m_UniformBuffersMemory;
        std::vector<void*> m_UniformBuffersMapped;

        VkPipelineLayout                    m_PipelineLayout{};
        VkDescriptorSetLayout               m_DescriptorSetLayout{};
        VkDescriptorPool                    m_DescriptorPool{};
        std::vector<VkDescriptorSet>        m_DescriptorSets{};
        std::vector<VkCommandBuffer>        m_CommandBuffers{};
        std::shared_ptr<VulkanPipeline>     m_Pipeline{};

        std::shared_ptr<VulkanShader> m_Shader{};
        std::shared_ptr<VulkanTexture2D> m_Texture{};
        UniformBufferObject m_Transform{};
    };
}


#endif//KATE_ENGINE_STANDARD_MATERIAL_HH
