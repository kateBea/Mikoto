/**
 * VulkanStandardMaterial.cc
 * Created by kate on 7/10/2023.
 * */

// C++ Standard Library
#include <array>
#include <memory>

// Third-Party Libraries
#include <volk.h>

// Project Headers
#include <Utility/VulkanUtils.hh>
#include <Renderer/Renderer.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanRenderer.hh>
#include <Renderer/Vulkan/VulkanStandardMaterial.hh>

namespace Mikoto {
    VulkanStandardMaterial::VulkanStandardMaterial(std::string_view name)
        :   Material{ name }
    {
        m_Texture = std::make_shared<VulkanTexture2D>("../assets/textures/lava512x512.png");

        // UniformBuffer size padded
        auto minOffsetAlignment{ VulkanUtils::GetDeviceMinimumOffsetAlignment(VulkanContext::GetPrimaryPhysicalDevice()) };
        auto paddedSize{ VulkanUtils::GetUniformBufferPadding(sizeof(m_UniformData), minOffsetAlignment) };
        m_UniformDataStructureSize = paddedSize;

        CreateUniformBuffer();
        CreateDescriptorPool();
        CreateDescriptorSet();
    }

    auto VulkanStandardMaterial::OnRelease() const -> void {
        vkDestroyDescriptorPool(VulkanContext::GetPrimaryLogicalDevice(), m_DescriptorPool, nullptr);

        m_UniformBuffer.OnRelease();
        m_Texture->OnRelease();
    }

    auto VulkanStandardMaterial::BindDescriptorSet(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout) -> void {
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &m_DescriptorSet, 0, nullptr);
    }

    auto VulkanStandardMaterial::CreateUniformBuffer() -> void {
        BufferAllocateInfo allocInfo{};
        allocInfo.Size = m_UniformDataStructureSize;

        allocInfo.BufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        allocInfo.BufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        allocInfo.BufferCreateInfo.size = allocInfo.Size;

        allocInfo.AllocationCreateInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

        m_UniformBuffer.OnCreate(allocInfo);

        if (vmaMapMemory(VulkanContext::GetDefaultAllocator(), m_UniformBuffer.GetVmaAllocation(), &m_UniformBuffersMapped) != VK_SUCCESS)
            throw std::runtime_error("Failed to map memory for uniform buffer in default material!");

        std::memcpy(m_UniformBuffersMapped, &m_UniformData, sizeof(m_UniformData));

        vmaUnmapMemory(VulkanContext::GetDefaultAllocator(), m_UniformBuffer.GetVmaAllocation());
    }

    auto VulkanStandardMaterial::UploadUniformBuffers() -> void {
        if (vmaMapMemory(VulkanContext::GetDefaultAllocator(), m_UniformBuffer.GetVmaAllocation(), &m_UniformBuffersMapped) != VK_SUCCESS)
            throw std::runtime_error("Failed to map memory for uniform buffer in default material!");

        std::memcpy(m_UniformBuffersMapped, &m_UniformData, sizeof(m_UniformData));

        vmaUnmapMemory(VulkanContext::GetDefaultAllocator(), m_UniformBuffer.GetVmaAllocation());
    }

    auto VulkanStandardMaterial::CreateDescriptorPool() -> void {
        std::array<VkDescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = 1;

        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = 1;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<UInt32_T>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = 10;

        if (vkCreateDescriptorPool(VulkanContext::GetPrimaryLogicalDevice(), &poolInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS)
            throw std::runtime_error("failed to create descriptor pool!");
    }

    auto VulkanStandardMaterial::CreateDescriptorSet() -> void {
        auto& standardMaterial {dynamic_cast<VulkanRenderer*>(Renderer::GetActiveGraphicsAPIPtr())->GetMaterialInfo()[std::string(GetStandardMaterialName())] };

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_DescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &standardMaterial.DescriptorSetLayout;

        if (vkAllocateDescriptorSets(VulkanContext::GetPrimaryLogicalDevice(), &allocInfo, &m_DescriptorSet) != VK_SUCCESS)
            throw std::runtime_error("failed to allocate descriptor sets!");

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        imageInfo.imageView = m_Texture->GetImageView();
        imageInfo.sampler = m_Texture->GetImageSampler();

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_UniformBuffer.Get();
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformTransformData);

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = m_DescriptorSet;
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = m_DescriptorSet;
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(VulkanContext::GetPrimaryLogicalDevice(), static_cast<UInt32_T>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }


    auto VulkanStandardMaterial::SetProjectionView(const glm::mat4 &projView) -> void {
        m_UniformData.ProjectionView = projView;
    }

    auto VulkanStandardMaterial::SetTransform(const glm::mat4&transform) -> void {
        m_UniformData.Transform = transform;
    }

    auto VulkanStandardMaterial::SetTiltingColor(float red, float green, float blue, float alpha) -> void {
        m_UniformData.Color.r = red;
        m_UniformData.Color.g = green;
        m_UniformData.Color.b = blue;
        m_UniformData.Color.a = alpha;
    }
}
