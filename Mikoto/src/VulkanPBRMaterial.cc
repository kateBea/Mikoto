//
// Created by kate on 11/12/23.
//

#include <cstring>

#include <Core/FileManager.hh>

#include <Renderer/Vulkan/VulkanRenderer.hh>
#include <Renderer/Vulkan/DeletionQueue.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanPBRMaterial.hh>

/** Albedo flags */
#define LIGHT_HAS_ALBEDO_MAP      1
#define LIGHT_HAS_NO_ALBEDO_MAP   0

/** Normal flags */
#define LIGHT_HAS_NORMAL_MAP       1
#define LIGHT_HAS_NO_NORMAL_MAP    0

/** Metallic flags */
#define LIGHT_HAS_METALLIC_MAP       1
#define LIGHT_HAS_NO_METALLIC_MAP    0

/** Roughness flags */
#define LIGHT_HAS_ROUGHNESS_MAP       1
#define LIGHT_HAS_NO_ROUGHNESS_MAP    0

/** AO flags */
#define LIGHT_HAS_AO_MAP       1
#define LIGHT_HAS_NO_AO_MAP    0

namespace Mikoto {

    VulkanPBRMaterial::VulkanPBRMaterial( const PBRMaterialCreateSpec &spec, std::string_view name )
        :   PhysicallyBasedMaterial{ name }
    {
        if (!s_EmptyTexture) {
            s_EmptyTexture = std::dynamic_pointer_cast<VulkanTexture2D>( Texture2D::Create( FileManager::Assets::GetRootPath() / "Icons/emptyTexture.png", MapType::TEXTURE_2D_DIFFUSE ) );
        }

        // [Setup flags]
        m_HasAlbedoMap      = spec.AlbedoMap != nullptr;
        m_HasNormalMap      = spec.NormalMap != nullptr;
        m_HasMetallicMap    = spec.MetallicMap != nullptr;
        m_HasRoughnessMap   = spec.RoughnessMap != nullptr;
        m_HasAoMap          = spec.AmbientOcclusionMap != nullptr;

        // [Setup maps]
        m_AlbedoMap = std::dynamic_pointer_cast<VulkanTexture2D>( HasAlbedoMap() ? spec.AlbedoMap : s_EmptyTexture );
        m_NormalMap = std::dynamic_pointer_cast<VulkanTexture2D>( HasNormalMap() ? spec.NormalMap : s_EmptyTexture );
        m_MetallicMap = std::dynamic_pointer_cast<VulkanTexture2D>( HasMetallicMap() ? spec.MetallicMap : s_EmptyTexture );
        m_RoughnessMap = std::dynamic_pointer_cast<VulkanTexture2D>( HasRoughnessMap() ? spec.RoughnessMap : s_EmptyTexture );
        m_AmbientOcclusionMap = std::dynamic_pointer_cast<VulkanTexture2D>( HasAmbientOcclusionMap() ? spec.AmbientOcclusionMap : s_EmptyTexture );

        // [Setup padded sizes]

        // vertex shader uniform buffer size padded
        const auto minOffsetAlignment{ VulkanUtils::GetDeviceMinimumOffsetAlignment(VulkanContext::GetPrimaryPhysicalDevice()) };
        auto paddedSize{ VulkanUtils::GetUniformBufferPadding(sizeof( VertexUniformBuffer ), minOffsetAlignment) };
        m_VertexShaderUniformPaddedSize = paddedSize;

        // fragment shader  uniform buffer size padded
        auto fragmentPaddedSize{ VulkanUtils::GetUniformBufferPadding(sizeof(FragmentUniformBufferData), minOffsetAlignment) };
        m_FragmentShaderUniformPaddedSize = fragmentPaddedSize;

        CreateUniformBuffers();
        CreateDescriptorPool();
        CreateDescriptorSet();
    }


    auto VulkanPBRMaterial::BindDescriptorSet( VkCommandBuffer const &commandBuffer, VkPipelineLayout const &pipelineLayout ) -> void {
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &m_DescriptorSet, 0, nullptr);
    }


    auto VulkanPBRMaterial::UpdateLightsInfo() -> void {

    }


    auto VulkanPBRMaterial::UploadUniformBuffers() -> void {
        std::memcpy(m_VertexUniformBuffer.GetMappedPtr(), static_cast<const void*>(std::addressof(m_VertexUniformData)), sizeof(m_VertexUniformData) );
        std::memcpy(m_FragmentUniformBuffer.GetMappedPtr(), static_cast<const void*>(std::addressof(m_FragmentUniformData)), sizeof(m_FragmentUniformData) );
    }

    auto VulkanPBRMaterial::CreateUniformBuffers() -> void {
        // NOTE: maxUniformBufferRange is the maximum value that can be specified in
        // the range member of a VkDescriptorBufferInfo structure passed to vkUpdateDescriptorSets
        // for descriptors of type VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER or VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC.
        // If we want to keep adding more lights we can't just increase the max lights. Most of devices support 65536 at best:
        // https://vulkan.gpuinfo.org/displaydevicelimit.php?name=maxUniformBufferRange

        // [Vertex shader uniform buffer]
        BufferAllocateInfo allocInfo{};
        allocInfo.Size = m_VertexShaderUniformPaddedSize;

        allocInfo.BufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        allocInfo.BufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        allocInfo.BufferCreateInfo.size = allocInfo.Size;

        allocInfo.AllocationCreateInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        allocInfo.WantMapping = true;

        m_VertexUniformBuffer.OnCreate(allocInfo);

        // [Fragment shader uniform buffer]
        BufferAllocateInfo fragmentAllocInfo{};
        fragmentAllocInfo.Size = m_FragmentShaderUniformPaddedSize;

        fragmentAllocInfo.BufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        fragmentAllocInfo.BufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        fragmentAllocInfo.BufferCreateInfo.size = fragmentAllocInfo.Size;

        fragmentAllocInfo.AllocationCreateInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        fragmentAllocInfo.WantMapping = true; // using vmaMapMemory

        m_FragmentUniformBuffer.OnCreate(fragmentAllocInfo);
    }

    auto VulkanPBRMaterial::CreateDescriptorPool() -> void {
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

    auto VulkanPBRMaterial::CreateDescriptorSet() -> void {
        auto&pbrMaterialInfo{ dynamic_cast<VulkanRenderer*>(Renderer::GetActiveGraphicsAPIPtr())->GetMaterialInfo()[std::string(GetName())] };

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_DescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &pbrMaterialInfo.DescriptorSetLayout;

        if (vkAllocateDescriptorSets(VulkanContext::GetPrimaryLogicalDevice(), &allocInfo, &m_DescriptorSet) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("failed to allocate descriptor sets!");
        }

        DeletionQueue::Push([descPool = m_DescriptorPool, descSet = m_DescriptorSet]() -> void {
            std::array<VkDescriptorSet, 1> descSets{ descSet };
            vkFreeDescriptorSets(VulkanContext::GetPrimaryLogicalDevice(), descPool, static_cast<UInt32_T>(descSets.size()), descSets.data());
        });

        UpdateDescriptorSets();
    }

    auto VulkanPBRMaterial::UpdateDescriptorSets() -> void {
        std::array<VkWriteDescriptorSet, 7> descriptorWrites{};

        VkDescriptorImageInfo albedo{};
        albedo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        albedo.imageView = std::dynamic_pointer_cast<VulkanTexture2D>(m_AlbedoMap)->GetImageView();
        albedo.sampler = std::dynamic_pointer_cast<VulkanTexture2D>(m_AlbedoMap)->GetImageSampler();
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = m_DescriptorSet;
        descriptorWrites[0].dstBinding = 2;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pImageInfo = &albedo;


        VkDescriptorImageInfo normal{};
        normal.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        normal.imageView = std::dynamic_pointer_cast<VulkanTexture2D>(m_NormalMap)->GetImageView();
        normal.sampler = std::dynamic_pointer_cast<VulkanTexture2D>(m_NormalMap)->GetImageSampler();
        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = m_DescriptorSet;
        descriptorWrites[1].dstBinding = 3;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo = &normal;

        VkDescriptorImageInfo metallic{};
        metallic.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        metallic.imageView = std::dynamic_pointer_cast<VulkanTexture2D>(m_MetallicMap)->GetImageView();
        metallic.sampler = std::dynamic_pointer_cast<VulkanTexture2D>(m_MetallicMap)->GetImageSampler();
        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = m_DescriptorSet;
        descriptorWrites[2].dstBinding = 4;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pImageInfo = &metallic;

        VkDescriptorImageInfo roughness{};
        roughness.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        roughness.imageView = std::dynamic_pointer_cast<VulkanTexture2D>(m_RoughnessMap)->GetImageView();
        roughness.sampler = std::dynamic_pointer_cast<VulkanTexture2D>(m_RoughnessMap)->GetImageSampler();
        descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[3].dstSet = m_DescriptorSet;
        descriptorWrites[3].dstBinding = 5;
        descriptorWrites[3].dstArrayElement = 0;
        descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[3].descriptorCount = 1;
        descriptorWrites[3].pImageInfo = &roughness;


        VkDescriptorImageInfo ao{};
        ao.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        ao.imageView = std::dynamic_pointer_cast<VulkanTexture2D>(m_AmbientOcclusionMap)->GetImageView();
        ao.sampler = std::dynamic_pointer_cast<VulkanTexture2D>(m_AmbientOcclusionMap)->GetImageSampler();
        descriptorWrites[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[4].dstSet = m_DescriptorSet;
        descriptorWrites[4].dstBinding = 6;
        descriptorWrites[4].dstArrayElement = 0;
        descriptorWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[4].descriptorCount = 1;
        descriptorWrites[4].pImageInfo = &ao;


        VkDescriptorBufferInfo vertexUbo{};
        vertexUbo.buffer = m_VertexUniformBuffer.Get();
        vertexUbo.offset = 0;
        vertexUbo.range = m_VertexUniformBuffer.GetSize();
        descriptorWrites[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[5].dstSet = m_DescriptorSet;
        descriptorWrites[5].dstBinding = 0;
        descriptorWrites[5].dstArrayElement = 0;
        descriptorWrites[5].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[5].descriptorCount = 1;
        descriptorWrites[5].pBufferInfo = &vertexUbo;

        VkDescriptorBufferInfo fragmentUbo{};
        fragmentUbo.buffer = m_FragmentUniformBuffer.Get();
        fragmentUbo.offset = 0;
        fragmentUbo.range = m_FragmentUniformBuffer.GetSize();
        descriptorWrites[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[6].dstSet = m_DescriptorSet;
        descriptorWrites[6].dstBinding = 1;
        descriptorWrites[6].dstArrayElement = 0;
        descriptorWrites[6].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[6].descriptorCount = 1;
        descriptorWrites[6].pBufferInfo = &fragmentUbo;

        vkUpdateDescriptorSets(VulkanContext::GetPrimaryLogicalDevice(), static_cast<UInt32_T>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }
}