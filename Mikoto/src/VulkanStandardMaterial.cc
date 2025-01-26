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
#include <Core/FileManager.hh>
#include <Material/Core/Shader.hh>
#include <Renderer/Core/Renderer.hh>
#include <Renderer/Vulkan/DeletionQueue.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanRenderer.hh>
#include <Renderer/Vulkan/VulkanStandardMaterial.hh>
#include <Renderer/Vulkan/VulkanUtils.hh>
#include <STL/Filesystem/PathBuilder.hh>

#define LIGHT_HAS_SPECULAR_MAP      1
#define LIGHT_HAS_NO_SPECULAR_MAP   0

#define LIGHT_HAS_DIFFUSE_MAP      1
#define LIGHT_HAS_NO_DIFFUSE_MAP   0

namespace Mikoto {
    VulkanStandardMaterial::VulkanStandardMaterial(const StandardMaterialCreateData& spec, std::string_view name)
        :   StandardMaterial{ name }
    {
        // Create shared empty texture
        // is just a placeholder for when a mesh has no specific map
        if (!s_EmptyTexture) {
            s_EmptyTexture = std::dynamic_pointer_cast<VulkanTexture2D>( Texture2D::Create(
                PathBuilder()
                .WithPath( FileManager::Assets::GetRootPath().string() )
                .WithPath( "Icons" )
                .WithPath( "emptyTexture.png" )
                .Build(), MapType::TEXTURE_2D_DIFFUSE ) );
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

    auto VulkanStandardMaterial::BindDescriptorSet(const VkCommandBuffer& commandBuffer, const VkPipelineLayout& pipelineLayout) const -> void {
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &m_DescriptorSet, 0, nullptr);
    }

    auto VulkanStandardMaterial::CreateUniformBuffers() -> void {
        // NOTE: maxUniformBufferRange is the maximum value that can be specified in
        // the range member of a VkDescriptorBufferInfo structure passed to vkUpdateDescriptorSets
        // for descriptors of type VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER or VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC.
        // If we want to keep adding more lights we can't just increase the max lights. Most of devices support 65536 at best:
        // https://vulkan.gpuinfo.org/displaydevicelimit.php?name=maxUniformBufferRange

        // [Vertex shader uniform buffer]
        BufferAllocateInfo allocInfo{};
        allocInfo.Size = m_UniformDataStructureSize;

        allocInfo.BufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        allocInfo.BufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        allocInfo.BufferCreateInfo.size = allocInfo.Size;

        allocInfo.AllocationCreateInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        allocInfo.WantMapping = true;

        m_VertexUniformBuffer.OnCreate(allocInfo);

        // [Fragment shader uniform buffer]
        BufferAllocateInfo fragmentAllocInfo{};
        fragmentAllocInfo.Size = m_FragmentUniformDataStructureSize;

        fragmentAllocInfo.BufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        fragmentAllocInfo.BufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        fragmentAllocInfo.BufferCreateInfo.size = fragmentAllocInfo.Size;

        fragmentAllocInfo.AllocationCreateInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        fragmentAllocInfo.WantMapping = true;

        m_FragmentUniformBuffer.OnCreate(fragmentAllocInfo);
    }

    auto VulkanStandardMaterial::UploadUniformBuffers() -> void {
        // upload the color
        m_VertexUniformData.Color = m_Color;

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

        std::memcpy( m_VertexUniformBuffer.GetMappedPtr(), &m_VertexUniformData, sizeof( m_VertexUniformData ) );
        std::memcpy( m_FragmentUniformBuffer.GetMappedPtr(), &m_FragmentUniformLightsData, sizeof( m_FragmentUniformLightsData ) );
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
            MKT_THROW_RUNTIME_ERROR("VulkanStandardMaterial - Failed to create descriptor pool!");
        }

        DeletionQueue::Push([descPool = m_DescriptorPool]() -> void {
            vkDestroyDescriptorPool(VulkanContext::GetPrimaryLogicalDevice(), descPool, nullptr);
        });
    }

    auto VulkanStandardMaterial::CreateDescriptorSet() -> void {
        // FIXME: This is a temporary solution. We need to find a way to get the material info from the renderer
        auto& standardMaterial { dynamic_cast<VulkanRenderer*>(Renderer::GetActiveGraphicsAPIPtr())->GetMaterialInfo()[std::string( GetName())] };

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_DescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &standardMaterial.DescriptorSetLayout;

        if (vkAllocateDescriptorSets(VulkanContext::GetPrimaryLogicalDevice(), &allocInfo, &m_DescriptorSet) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("failed to allocate descriptor sets!");
        }

        DeletionQueue::Push([descPool = m_DescriptorPool, descSet = m_DescriptorSet]() -> void {
            std::array descSets{ descSet };
            vkFreeDescriptorSets(VulkanContext::GetPrimaryLogicalDevice(), descPool, static_cast<UInt32_T>(descSets.size()), descSets.data());
        });

        UpdateDescriptorSets();
    }

    auto VulkanStandardMaterial::UpdateDescriptorSets() const -> void {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = std::dynamic_pointer_cast<VulkanTexture2D>(m_DiffuseTexture)->GetImageView();
        imageInfo.sampler = std::dynamic_pointer_cast<VulkanTexture2D>(m_DiffuseTexture)->GetImageSampler();

        VkDescriptorImageInfo specularImageInfo{};
        specularImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        specularImageInfo.imageView = std::dynamic_pointer_cast<VulkanTexture2D>(m_SpecularTexture)->GetImageView();
        specularImageInfo.sampler = std::dynamic_pointer_cast<VulkanTexture2D>(m_SpecularTexture)->GetImageSampler();

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

    auto VulkanStandardMaterial::UpdateLightsInfo() -> void {
        m_FragmentUniformLightsData.ViewPosition = Renderer::GetCameraPosition();

        const auto& lights{ Renderer::GetLightObjects() };

        Size_T pointLightIndex{};
        Size_T dirLightIndex{};
        Size_T spotLightIndex{};

        for ( const auto& lightInfo: lights | std::views::values ) {

            if (lightInfo.IsActive) {
                switch ( lightInfo.Type ) {
                    case LightType::DIRECTIONAL_LIGHT_TYPE:
                        m_FragmentUniformLightsData.DirectionalLights[dirLightIndex++] = lightInfo.Data.DireLightData;
                    break;
                    case LightType::POINT_LIGHT_TYPE:
                        m_FragmentUniformLightsData.PointLights[pointLightIndex++] = lightInfo.Data.PointLightDat;
                    break;
                    case LightType::SPOT_LIGHT_TYPE:
                        m_FragmentUniformLightsData.SpotLights[spotLightIndex++] = lightInfo.Data.SpotLightData;
                    break;
                }
            }
        }

        m_FragmentUniformLightsData.LightMeta.x = static_cast<float>(dirLightIndex + dirLightIndex + spotLightIndex);

        m_FragmentUniformLightsData.LightTypesCount.x = static_cast<float>(dirLightIndex);
        m_FragmentUniformLightsData.LightTypesCount.y = static_cast<float>(pointLightIndex);
        m_FragmentUniformLightsData.LightTypesCount.z = static_cast<float>(spotLightIndex);
    }
}
