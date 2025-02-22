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
#include <Core/System/AssetsSystem.hh>
#include <Core/System/FileSystem.hh>
#include <Library/Filesystem/PathBuilder.hh>
#include <Material/Core/Shader.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanRenderer.hh>
#include <Renderer/Vulkan/VulkanStandardMaterial.hh>

namespace Mikoto {
    VulkanStandardMaterial::VulkanStandardMaterial(const StandardMaterialCreateInfo& spec)
        :   StandardMaterial{ spec }
    {
        SetupTextures();

        VulkanDevice& device{ VulkanContext::Get().GetDevice() };

        // UniformBuffer size padded. Vertex shader
        const VkDeviceSize minOffsetAlignment{ device.GetDeviceMinimumOffsetAlignment() };
        const VkDeviceSize paddedSize{ VulkanHelpers::GetUniformBufferPadding(sizeof( m_VertexUniformData ), minOffsetAlignment) };
        m_UniformDataStructureSize = paddedSize;

        // UniformBuffer size padded. Fragment shader
        const VkDeviceSize fragmentPaddedSize{ VulkanHelpers::GetUniformBufferPadding(sizeof(LightsUniformData), minOffsetAlignment) };
        m_FragmentUniformDataStructureSize = fragmentPaddedSize;

        CreateUniformBuffers();
        CreateDescriptorSet();

        UpdateDescriptorSets();
    }

    auto VulkanStandardMaterial::SetupTextures() -> void {
        AssetsSystem& assetsSystem{ Engine::GetSystem<AssetsSystem>() };
        FileSystem& fileSystem{ Engine::GetSystem<FileSystem>() };

        // Create shared empty texture
        // is just a placeholder for when a mesh has no specific map
        Path_T emptyTexturePath{
            PathBuilder()
                .WithPath( fileSystem.GetIconsRootPath().string() )
                .WithPath( "emptyTexture.png" )
                .Build()
        };

        if (m_DiffuseTexture == nullptr) {
            const TextureLoadInfo textureLoadInfo{
                .Path{ emptyTexturePath },
                .Type{ MapType::TEXTURE_2D_DIFFUSE }
            };

            m_HasDiffuseTexture = false;
            m_DiffuseTexture = dynamic_cast<Texture2D *>( assetsSystem.LoadTexture( textureLoadInfo ) );
        }

        if (m_SpecularTexture == nullptr) {
            const TextureLoadInfo textureLoadInfo{
                .Path{ emptyTexturePath },
                .Type{ MapType::TEXTURE_2D_DIFFUSE }
            };

            m_HasSpecularTexture = false;
            m_SpecularTexture = dynamic_cast<Texture2D *>( assetsSystem.LoadTexture( textureLoadInfo ) );
        }
    }

    auto VulkanStandardMaterial::BindDescriptorSet( const VkCommandBuffer& commandBuffer, const VkPipelineLayout& pipelineLayout ) -> void {
        // if needed update before use
        if (m_WantDescriptorUpdate) {
            UpdateDescriptorSets();
            m_WantDescriptorUpdate = false;
        }

        // It is here that we specify which desc set we bind to, always 0 for now
        constexpr Size_T firstSet{ 0 };
        vkCmdBindDescriptorSets( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, firstSet, 1, &m_DescriptorSet, 0, nullptr );
    }

    auto VulkanStandardMaterial::CreateUniformBuffers() -> void {
        // NOTE: maxUniformBufferRange is the maximum value that can be specified in
        // the range member of a VkDescriptorBufferInfo structure passed to vkUpdateDescriptorSets
        // for descriptors of type VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER or VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC.
        // If we want to keep adding more lights we can't just increase the max lights. Most of devices support 65536 at best:
        // https://vulkan.gpuinfo.org/displaydevicelimit.php?name=maxUniformBufferRange

        // [Vertex shader uniform buffer]
        VulkanBufferCreateInfo vertexAllocInfo{};

        vertexAllocInfo.BufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        vertexAllocInfo.BufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        vertexAllocInfo.BufferCreateInfo.size = m_UniformDataStructureSize;

        vertexAllocInfo.AllocationCreateInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        vertexAllocInfo.WantMapping = true;

        m_VertexUniformBuffer = CreateScope<VulkanBuffer>( vertexAllocInfo );

        // [Fragment shader uniform buffer]
        VulkanBufferCreateInfo fragmentAllocInfo{};

        fragmentAllocInfo.BufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        fragmentAllocInfo.BufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        fragmentAllocInfo.BufferCreateInfo.size = m_FragmentUniformDataStructureSize;

        fragmentAllocInfo.AllocationCreateInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        fragmentAllocInfo.WantMapping = true;

        m_FragmentUniformBuffer = CreateScope<VulkanBuffer>( fragmentAllocInfo );
    }

    auto VulkanStandardMaterial::UploadUniformBuffers() -> void {
        // upload the color
        m_VertexUniformData.Color = m_Color;

        m_FragmentUniformLightsData.LightMeta.x = static_cast<float>( m_LightsCount );
        m_FragmentUniformLightsData.LightMeta.y = HasDiffuseMap() ? 1 : 0;
        m_FragmentUniformLightsData.LightMeta.z = HasSpecularMap() ? 1 : 0;
        m_FragmentUniformLightsData.LightMeta.w = m_Shininess;

        std::memcpy( m_VertexUniformBuffer->GetMappedPtr(), std::addressof( m_VertexUniformData ), sizeof( m_VertexUniformData ) );
        std::memcpy( m_FragmentUniformBuffer->GetMappedPtr(), std::addressof( m_FragmentUniformLightsData ), sizeof( m_FragmentUniformLightsData ) );
    }

    auto VulkanStandardMaterial::ResetLights() -> void {
        m_LightsCount = 0;
        m_DirLightIndex = 0;
        m_PointLightIndex = 0;
        m_SpotLightIndex = 0;
    }

    auto VulkanStandardMaterial::CreateDescriptorSet() -> void {
        const VkDescriptorSetLayout& descriptorSetLayout{ VulkanContext::Get().GetDescriptorSetLayouts( DESCRIPTOR_SET_LAYOUT_BASE_SHADER ) };

        const VulkanDevice& device{ VulkanContext::Get().GetDevice() };
        VulkanDescriptorAllocator& descriptorAllocator{ VulkanContext::Get().GetDescriptorAllocator() };

        m_DescriptorSet = *descriptorAllocator.Allocate( device.GetLogicalDevice(), descriptorSetLayout );
    }

    auto VulkanStandardMaterial::SetDiffuseMap( Texture2D* map, const MapType type ) -> void {
        if ( map ) {

            switch ( type ) {
                case MapType::TEXTURE_2D_DIFFUSE:
                    m_DiffuseTexture = map;
                    m_HasDiffuseTexture = true;
                    break;
                case MapType::TEXTURE_2D_SPECULAR:
                    m_SpecularTexture = map;
                    m_HasSpecularTexture = true;
                    break;
                default:
                    break;
            }

            // Deffer descriptor set update until we bound them again
            m_WantDescriptorUpdate = true;
        }
    }

    auto VulkanStandardMaterial::RemoveMap( MapType type ) -> void {

    }

    auto VulkanStandardMaterial::UpdateDescriptorSets() -> void {

        const VulkanDevice& device{ VulkanContext::Get().GetDevice() };

        VulkanTexture2D* diffuse{ dynamic_cast<VulkanTexture2D *>( m_DiffuseTexture ) };
        VulkanTexture2D* specular{ dynamic_cast<VulkanTexture2D *>( m_SpecularTexture ) };

        m_DescriptorWriter
            .WriteImage( 1, diffuse->GetImage().GetView(), diffuse->GetSampler(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER )
            .WriteImage( 2, specular->GetImage().GetView(), specular->GetSampler(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER )
            .WriteBuffer( 0, m_VertexUniformBuffer->Get(), m_VertexUniformBuffer->GetSize(), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
            .WriteBuffer( 3, m_FragmentUniformBuffer->Get(), m_FragmentUniformBuffer->GetSize(), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
        .UpdateSet( device.GetLogicalDevice(), m_DescriptorSet );
    }

    auto VulkanStandardMaterial::UpdateLightsInfo(const LightData& lightData, const LightType type) -> void {
        m_LightsCount += 1;

        switch ( type ) {
            case LightType::DIRECTIONAL_LIGHT_TYPE:
                m_FragmentUniformLightsData.DirectionalLights[m_DirLightIndex++] = lightData.DireLightData;
            break;

            case LightType::POINT_LIGHT_TYPE:
                m_FragmentUniformLightsData.PointLights[m_PointLightIndex++] = lightData.PointLightDat;
            break;

            case LightType::SPOT_LIGHT_TYPE:
                m_FragmentUniformLightsData.SpotLights[m_SpotLightIndex++] = lightData.SpotLightData;
            break;
        }

        m_FragmentUniformLightsData.LightTypesCount.x = static_cast<float>(m_DirLightIndex);
        m_FragmentUniformLightsData.LightTypesCount.y = static_cast<float>(m_PointLightIndex);
        m_FragmentUniformLightsData.LightTypesCount.z = static_cast<float>(m_SpotLightIndex);
    }
}
