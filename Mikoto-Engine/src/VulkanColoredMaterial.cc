//
// Created by kate on 10/29/23.
//

#include <Common/VulkanUtils.hh>

#include <Renderer/Renderer.hh>
#include <Renderer/Material/Shader.hh>
#include <Renderer/Vulkan/DeletionQueue.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanRenderer.hh>
#include <Renderer/Vulkan/VulkanColoredMaterial.hh>

namespace Mikoto {
    VulkanColoredMaterial::VulkanColoredMaterial(std::string_view name)
        :   Material{ name, Type::MATERIAL_TYPE_COLORED }
    {
        // UniformBuffer size padded
        auto minOffsetAlignment{ VulkanUtils::GetDeviceMinimumOffsetAlignment(VulkanContext::GetPrimaryPhysicalDevice()) };
        auto paddedSize{ VulkanUtils::GetUniformBufferPadding(sizeof(m_UniformData), minOffsetAlignment) };
        m_UniformDataStructureSize = paddedSize;

        CreateUniformBuffer();
        CreateDescriptorPool();
        CreateDescriptorSet();
    }

    auto VulkanColoredMaterial::BindDescriptorSet(const VkCommandBuffer& commandBuffer, const VkPipelineLayout& pipelineLayout) -> void {
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &m_DescriptorSet, 0, nullptr);
    }

    auto VulkanColoredMaterial::CreateUniformBuffer() -> void {
        BufferAllocateInfo allocInfo{};
        allocInfo.Size = m_UniformDataStructureSize;

        allocInfo.BufferCreateInfo = VulkanUtils::Initializers::BufferCreateInfo();
        allocInfo.BufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        allocInfo.BufferCreateInfo.size = allocInfo.Size;

        allocInfo.AllocationCreateInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        allocInfo.IsMapped = true;

        m_UniformBuffer.OnCreate(allocInfo);

        if (vmaMapMemory(VulkanContext::GetDefaultAllocator(), m_UniformBuffer.GetVmaAllocation(), &m_UniformBuffersMapped) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("Failed to map memory for uniform buffer in default material!");
        }
    }

    auto VulkanColoredMaterial::UploadUniformBuffers() -> void {
        std::memcpy(m_UniformBuffersMapped, &m_UniformData, sizeof(m_UniformData));
    }

    auto VulkanColoredMaterial::CreateDescriptorPool() -> void {
        std::array<VkDescriptorPoolSize, 1> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = 1;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<UInt32_T>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = 1000;

        if (vkCreateDescriptorPool(VulkanContext::GetPrimaryLogicalDevice(), &poolInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("Failed to create descriptor pool!");
        }

        DeletionQueue::Push([descPool = m_DescriptorPool]() -> void {
            vkDestroyDescriptorPool(VulkanContext::GetPrimaryLogicalDevice(), descPool, nullptr);
        });
    }

    auto VulkanColoredMaterial::CreateDescriptorSet() -> void {
        auto& standardMaterial{ dynamic_cast<VulkanRenderer*>(Renderer::GetActiveGraphicsAPIPtr())->GetMaterialInfo()[std::string(GetColoredMaterialName())] };

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_DescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &standardMaterial.DescriptorSetLayout;

        if (vkAllocateDescriptorSets(VulkanContext::GetPrimaryLogicalDevice(), &allocInfo, &m_DescriptorSet) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("failed to allocate descriptor sets!");
        }

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_UniformBuffer.Get();
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(m_UniformData);

        std::array<VkWriteDescriptorSet, 1> descriptorWrites{};
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = m_DescriptorSet;
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(VulkanContext::GetPrimaryLogicalDevice(), static_cast<UInt32_T>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

        DeletionQueue::Push([descPool = m_DescriptorPool, descSet = m_DescriptorSet]() -> void {
            // The descriptor pool is pushed before to the deleting queue
            // because it must be freed after the descriptor set. The deleting queue
            // runs the deletors in reversed order, newer objects get deleted first
            std::array<VkDescriptorSet, 1> descSets{ descSet };
            vkFreeDescriptorSets(VulkanContext::GetPrimaryLogicalDevice(), descPool, static_cast<UInt32_T>(descSets.size()), descSets.data());
        });
    }

    auto VulkanColoredMaterial::SetTransform(const glm::mat4&transform) -> void {
        m_UniformData.Transform = transform;
    }

    auto VulkanColoredMaterial::SetColor(float red, float green, float blue, float alpha) -> void {
        m_UniformData.Color.r = red;
        m_UniformData.Color.g = green;
        m_UniformData.Color.b = blue;
        m_UniformData.Color.a = alpha;
    }
}