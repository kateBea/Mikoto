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
#include <Common/VulkanUtils.hh>

#include "Renderer/Renderer.hh"
#include "Renderer/Material/Shader.hh"

#include <Renderer/Vulkan/DeletionQueue.hh>
#include "Renderer/Vulkan/VulkanContext.hh"
#include "Renderer/Vulkan/VulkanRenderer.hh"
#include "Renderer/Vulkan/VulkanStandardMaterial.hh"

#define LIGHT_HAS_SPECULAR_MAP      1
#define LIGHT_HAS_NO_SPECULAR_MAP   0

#define LIGHT_HAS_DIFFUSE_MAP      1
#define LIGHT_HAS_NO_DIFFUSE_MAP   0

namespace Mikoto {
    VulkanStandardMaterial::VulkanStandardMaterial(const DefaultMaterialCreateSpec& spec, std::string_view name)
        :   Material{ name, Type::MATERIAL_TYPE_STANDARD }
    {
        // Create shared empty texture
        // is just a placeholder for when a mesh has no specific map
        if (!s_EmptyTexture) {
            s_EmptyTexture = std::dynamic_pointer_cast<VulkanTexture2D>( Texture2D::Create( "../Assets/Icons/emptyTexture.png", MapType::TEXTURE_2D_DIFFUSE ) );
        }

        m_HasSpecular = spec.SpecularMap != nullptr;
        m_HasDiffuse = spec.DiffuseMap != nullptr;

        m_SpecularTexture = std::dynamic_pointer_cast<VulkanTexture2D>( HasSpecularMap() ? spec.SpecularMap : s_EmptyTexture );
        m_DiffuseTexture = std::dynamic_pointer_cast<VulkanTexture2D>(HasDiffuseMap() ? spec.DiffuseMap : s_EmptyTexture);

        // UniformBuffer size padded. Vertex shader
        const auto minOffsetAlignment{ VulkanUtils::GetDeviceMinimumOffsetAlignment(VulkanContext::GetPrimaryPhysicalDevice()) };
        auto paddedSize{ VulkanUtils::GetUniformBufferPadding(sizeof( m_VertexUniformData ), minOffsetAlignment) };
        m_UniformDataStructureSize = paddedSize;

        // UniformBuffer size padded. Fragment shader
        auto fragmentPaddedSize{ VulkanUtils::GetUniformBufferPadding(sizeof(LightsUniformData), minOffsetAlignment) };
        m_FragmentUniformDataStructureSize = fragmentPaddedSize;

        CreateUniformBuffers();
        CreateDescriptorPool();
        CreateDescriptorSet();
    }

    auto VulkanStandardMaterial::BindDescriptorSet(const VkCommandBuffer& commandBuffer, const VkPipelineLayout& pipelineLayout) -> void {
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &m_DescriptorSet, 0, nullptr);
    }

    auto VulkanStandardMaterial::CreateUniformBuffers() -> void {
        // [Vertex shader uniform buffer]
        BufferAllocateInfo allocInfo{};
        allocInfo.Size = m_UniformDataStructureSize;

        allocInfo.BufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        allocInfo.BufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        allocInfo.BufferCreateInfo.size = allocInfo.Size;

        allocInfo.AllocationCreateInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        allocInfo.IsMapped = true; // using vmaMapMemory

        m_VertexUniformBuffer.OnCreate(allocInfo);

        if (vmaMapMemory(VulkanContext::GetDefaultAllocator(), m_VertexUniformBuffer.GetVmaAllocation(), &m_VertexUniformBuffersMapped ) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("Failed to map memory for uniform buffer in default material!");
        }


        // [Fragment shader uniform buffer]
        BufferAllocateInfo fragmentAllocInfo{};
        fragmentAllocInfo.Size = m_FragmentUniformDataStructureSize;

        fragmentAllocInfo.BufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        fragmentAllocInfo.BufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        fragmentAllocInfo.BufferCreateInfo.size = fragmentAllocInfo.Size;

        fragmentAllocInfo.AllocationCreateInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        fragmentAllocInfo.IsMapped = true; // using vmaMapMemory

        m_FragmentUniformBuffer.OnCreate(fragmentAllocInfo);

        if (vmaMapMemory(VulkanContext::GetDefaultAllocator(), m_FragmentUniformBuffer.GetVmaAllocation(), &m_FragmentUniformBuffersMapped) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("Failed to map memory for uniform buffer in default material!");
        }
    }

    auto VulkanStandardMaterial::UploadUniformBuffers() -> void {
        if ( HasDiffuseMap() ) {
            m_FragmentUniformLightsData.LightMeta.y = LIGHT_HAS_DIFFUSE_MAP;
        }
        else {
            m_FragmentUniformLightsData.LightMeta.y = LIGHT_HAS_NO_DIFFUSE_MAP;
        }

        if ( HasSpecularMap() ) {
            m_FragmentUniformLightsData.LightMeta.z = LIGHT_HAS_SPECULAR_MAP;
        }
        else {
            m_FragmentUniformLightsData.LightMeta.z = LIGHT_HAS_NO_SPECULAR_MAP;
        }

        // TODO: rework. The shininess is an material/object property that specifies how an objects should reflect the light or scatter it over its surface
        m_FragmentUniformLightsData.LightMeta.w = 32.0f;

        std::memcpy( m_VertexUniformBuffersMapped, &m_VertexUniformData, sizeof( m_VertexUniformData ) );
        std::memcpy( m_FragmentUniformBuffersMapped, &m_FragmentUniformLightsData, sizeof( m_FragmentUniformLightsData ) );
    }

    auto VulkanStandardMaterial::CreateDescriptorPool() -> void {
        std::array<VkDescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = 1;

        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = 1;

        VkDescriptorPoolCreateInfo poolInfo{ VulkanUtils::Initializers::DescriptorPoolCreateInfo() };
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
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

    auto VulkanStandardMaterial::CreateDescriptorSet() -> void {
        auto& standardMaterial {dynamic_cast<VulkanRenderer*>(Renderer::GetActiveGraphicsAPIPtr())->GetMaterialInfo()[std::string(GetStandardMaterialName())] };

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_DescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &standardMaterial.DescriptorSetLayout;

        if (vkAllocateDescriptorSets(VulkanContext::GetPrimaryLogicalDevice(), &allocInfo, &m_DescriptorSet) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("failed to allocate descriptor sets!");
        }

        DeletionQueue::Push([descPool = m_DescriptorPool, descSet = m_DescriptorSet]() -> void {
            std::array<VkDescriptorSet, 1> descSets{ descSet };
            vkFreeDescriptorSets(VulkanContext::GetPrimaryLogicalDevice(), descPool, static_cast<UInt32_T>(descSets.size()), descSets.data());
        });

        UpdateDescriptorSets();
    }

    auto VulkanStandardMaterial::UpdateDescriptorSets() -> void {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = m_DiffuseTexture->GetImageView();
        imageInfo.sampler = m_DiffuseTexture->GetImageSampler();

        VkDescriptorImageInfo specularImageInfo{};
        specularImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        specularImageInfo.imageView = m_SpecularTexture->GetImageView();
        specularImageInfo.sampler = m_SpecularTexture->GetImageSampler();

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_VertexUniformBuffer.Get();
        bufferInfo.offset = 0;
        bufferInfo.range = m_VertexUniformBuffer.GetSize();

        VkDescriptorBufferInfo fragmentBufferInfo{};
        fragmentBufferInfo.buffer = m_FragmentUniformBuffer.Get();
        fragmentBufferInfo.offset = 0;
        fragmentBufferInfo.range = m_FragmentUniformBuffer.GetSize();

        std::array<VkWriteDescriptorSet, 4> descriptorWrites{};
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

        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = m_DescriptorSet;
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pBufferInfo = &fragmentBufferInfo;

        descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[3].dstSet = m_DescriptorSet;
        descriptorWrites[3].dstBinding = 3;
        descriptorWrites[3].dstArrayElement = 0;
        descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[3].descriptorCount = 1;
        descriptorWrites[3].pImageInfo = &specularImageInfo;

        vkUpdateDescriptorSets(VulkanContext::GetPrimaryLogicalDevice(), static_cast<UInt32_T>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }

    auto VulkanStandardMaterial::SetTransform(const glm::mat4&transform) -> void {
        m_VertexUniformData.Transform = transform;
    }

    auto VulkanStandardMaterial::SetLights( PointLight* lights, Size_T count) -> void {
        Size_T index{};

        for ( ; index < count; ++index) {
            m_FragmentUniformLightsData.PointLights[index] = lights[index];
        }

        m_FragmentUniformLightsData.LightMeta.x = count;
    }

    auto VulkanStandardMaterial::SetTiltingColor(float red, float green, float blue, float alpha) -> void {
        m_VertexUniformData.Color.r = red;
        m_VertexUniformData.Color.g = green;
        m_VertexUniformData.Color.b = blue;
        m_VertexUniformData.Color.a = alpha;
    }
}
