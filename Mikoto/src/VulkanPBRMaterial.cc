//
// Created by kate on 11/12/23.
//

#include <Core/System/AssetsSystem.hh>
#include <Core/System/FileSystem.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanPBRMaterial.hh>
#include <Renderer/Vulkan/VulkanRenderer.hh>
#include <cstring>

namespace Mikoto {

    static auto CheckEmptyTexture(Texture2D*& texture, bool& hasTexture) -> void {
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

        if (texture == nullptr) {
            const TextureLoadInfo textureLoadInfo{
                .Path{ emptyTexturePath },
                .Type{ MapType::TEXTURE_2D_DIFFUSE }
            };

            hasTexture = false;
            texture = dynamic_cast<Texture2D *>( assetsSystem.LoadTexture( textureLoadInfo ) );
        }
    }

    VulkanPBRMaterial::VulkanPBRMaterial( const PBRMaterialCreateSpec &spec )
        : PBRMaterial{ spec }
    {
        SetupTextures();

        VulkanDevice& device{ VulkanContext::Get().GetDevice() };

        // UniformBuffer size padded. Vertex shader
        const VkDeviceSize minOffsetAlignment{ device.GetDeviceMinimumOffsetAlignment() };
        const VkDeviceSize paddedSize{ VulkanHelpers::GetUniformBufferPadding(sizeof( m_VertexUniformData ), minOffsetAlignment) };
        m_UniformDataStructureSize = paddedSize;

        // UniformBuffer size padded. Fragment shader
        const VkDeviceSize fragmentPaddedSize{ VulkanHelpers::GetUniformBufferPadding(sizeof(FragmentUniformBufferData), minOffsetAlignment) };
        m_FragmentUniformDataStructureSize = fragmentPaddedSize;

        CreateUniformBuffers();
        CreateDescriptorSet();

        UpdateDescriptorSets();
    }

    auto VulkanPBRMaterial::SetupTextures() -> void {
        CheckEmptyTexture( m_AlbedoMap, m_HasAlbedoTexture );
        CheckEmptyTexture( m_MetallicMap, m_HasMetallicTexture );
        CheckEmptyTexture( m_RoughnessMap, m_HasRoughnessTexture );
        CheckEmptyTexture( m_AmbientOcclusionMap, m_HasAmbientOcclusionTexture );
        CheckEmptyTexture( m_NormalMap, m_HasNormalTexture );
    }

    auto VulkanPBRMaterial::BindDescriptorSet( const VkCommandBuffer &commandBuffer, const VkPipelineLayout &pipelineLayout ) -> void {
        // if necessary update before use
        if (m_WantDescriptorUpdate) {
            UpdateDescriptorSets();
            m_WantDescriptorUpdate = false;
        }

        // It is here that we specify which desc set we bind to, always 0 for now
        constexpr Size_T firstSet{ 0 };
        vkCmdBindDescriptorSets( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, firstSet, 1, &m_DescriptorSet, 0, nullptr );
    }

    auto VulkanPBRMaterial::UpdateLightsInfo(const LightData& lightData, const LightType type) -> void {

        switch ( type ) {

            case LightType::POINT_LIGHT_TYPE:
                m_FragmentUniformData.PointLights[m_FragmentUniformData.PointLightCount++] = lightData.PointLightDat;
            break;
            case LightType::DIRECTIONAL_LIGHT_TYPE:
                m_FragmentUniformData.DirectionalLights[m_FragmentUniformData.DirectionalLightCount++] = lightData.DireLightData;
            break;
            case LightType::SPOT_LIGHT_TYPE:
                m_FragmentUniformData.SpotLights[m_FragmentUniformData.SpotLightCount++] = lightData.SpotLightData;
            break;
        }
    }

    auto VulkanPBRMaterial::UploadUniformBuffers() -> void {
        m_FragmentUniformData.Albedo = m_Color;
        m_FragmentUniformData.Factors.x = GetMetallicFactor();
        m_FragmentUniformData.Factors.y = GetRoughnessFactor();
        m_FragmentUniformData.Factors.z = GetAmbientOcclusionFactor();

        m_FragmentUniformData.HasAlbedo = HasAlbedoMap();
        m_FragmentUniformData.HasNormal = HasNormalMap();
        m_FragmentUniformData.HasMetallic = HasMetallicMap();
        m_FragmentUniformData.HasAmbientOcc = HasAmbientOcclusionMap();
        m_FragmentUniformData.HasRoughness = HasRoughnessMap();

        std::memcpy( m_VertexUniformBuffer->GetMappedPtr(), std::addressof( m_VertexUniformData ), sizeof( m_VertexUniformData ) );
        std::memcpy( m_FragmentUniformBuffer->GetMappedPtr(), std::addressof( m_FragmentUniformData ), sizeof( m_FragmentUniformData ) );
    }

    auto VulkanPBRMaterial::UpdateDescriptorSets() -> void {
        const VulkanDevice& device{ VulkanContext::Get().GetDevice() };

        VulkanTexture2D* albedo{ dynamic_cast<VulkanTexture2D *>( m_AlbedoMap ) };
        VulkanTexture2D* metallic{ dynamic_cast<VulkanTexture2D *>( m_MetallicMap ) };
        VulkanTexture2D* normal{ dynamic_cast<VulkanTexture2D *>( m_NormalMap ) };
        VulkanTexture2D* roughness{ dynamic_cast<VulkanTexture2D *>( m_RoughnessMap ) };
        VulkanTexture2D* ambientOcclusion{ dynamic_cast<VulkanTexture2D *>( m_AmbientOcclusionMap ) };

        m_DescriptorWriter
            .WriteImage( 1, albedo->GetImage().GetView(), albedo->GetSampler(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER )
            .WriteImage( 2, normal->GetImage().GetView(), normal->GetSampler(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER )
            .WriteImage( 3, metallic->GetImage().GetView(), metallic->GetSampler(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER )
            .WriteImage( 4, roughness->GetImage().GetView(), roughness->GetSampler(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER )
            .WriteImage( 5, ambientOcclusion->GetImage().GetView(), ambientOcclusion->GetSampler(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER )
            .WriteBuffer( 0, m_VertexUniformBuffer->Get(), m_VertexUniformBuffer->GetSize(), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
            .WriteBuffer( 6, m_FragmentUniformBuffer->Get(), m_FragmentUniformBuffer->GetSize(), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
        .UpdateSet( device.GetLogicalDevice(), m_DescriptorSet );
    }

    auto VulkanPBRMaterial::ResetLights() -> void {
        m_FragmentUniformData.DirectionalLightCount = 0;
        m_FragmentUniformData.PointLightCount = 0;
        m_FragmentUniformData.SpotLightCount = 0;
    }

    auto VulkanPBRMaterial::RemoveMap( MapType type ) -> void {
        FileSystem& fileSystem{ Engine::GetSystem<FileSystem>() };
        AssetsSystem& assetsSystem{ Engine::GetSystem<AssetsSystem>() };

        Path_T emptyTexturePath{
            PathBuilder()
                .WithPath( fileSystem.GetIconsRootPath().string() )
                .WithPath( "emptyTexture.png" )
                .Build()
        };

        const TextureLoadInfo textureLoadInfo{
            .Path{ emptyTexturePath },
            .Type{ MapType::TEXTURE_2D_DIFFUSE }
        };

        Texture2D* emptyTexturePlaceholder{ dynamic_cast<Texture2D *>( assetsSystem.LoadTexture( textureLoadInfo ) ) };

        switch ( type ) {
            case MapType::TEXTURE_2D_DIFFUSE:
                m_AlbedoMap = emptyTexturePlaceholder;
                m_HasAlbedoTexture = false;
            break;

            case MapType::TEXTURE_2D_NORMAL:
                m_NormalMap = emptyTexturePlaceholder;
                m_HasNormalTexture = false;
            break;

            case MapType::TEXTURE_2D_METALLIC:
                m_MetallicMap = emptyTexturePlaceholder;
                m_HasMetallicTexture = false;
                break;

            case MapType::TEXTURE_2D_ROUGHNESS:
                m_RoughnessMap = emptyTexturePlaceholder;
                m_HasRoughnessTexture = false;
                break;

            case MapType::TEXTURE_2D_AMBIENT_OCCLUSION:
                m_AmbientOcclusionMap = emptyTexturePlaceholder;
                m_HasAmbientOcclusionTexture = false;
                break;

            default:
                break;
        }

        // Deffer descriptor set update until we bound them again
        m_WantDescriptorUpdate = true;
    }

    auto VulkanPBRMaterial::SetTexture( Texture *map, MapType type ) -> void {
        if ( map ) {

            switch ( type ) {
                case MapType::TEXTURE_2D_DIFFUSE:
                    m_AlbedoMap = dynamic_cast<Texture2D *>(map);
                    m_HasAlbedoTexture = true;
                    break;

                case MapType::TEXTURE_2D_NORMAL:
                    m_NormalMap = dynamic_cast<Texture2D *>(map);
                    m_HasNormalTexture = true;
                    break;

                case MapType::TEXTURE_2D_METALLIC:
                    m_MetallicMap = dynamic_cast<Texture2D *>(map);
                    m_HasMetallicTexture = true;
                    break;

                case MapType::TEXTURE_2D_ROUGHNESS:
                    m_RoughnessMap = dynamic_cast<Texture2D *>(map);
                    m_HasRoughnessTexture = true;
                    break;

                case MapType::TEXTURE_2D_AMBIENT_OCCLUSION:
                    m_AmbientOcclusionMap = dynamic_cast<Texture2D *>(map);
                    m_HasAmbientOcclusionTexture = true;
                    break;
                default:
                    break;
            }

            // Deffer descriptor set update until we bound them again
            m_WantDescriptorUpdate = true;
        }
    }

    auto VulkanPBRMaterial::CreateUniformBuffers() -> void {
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

    auto VulkanPBRMaterial::CreateDescriptorSet() -> void {
        const VkDescriptorSetLayout& descriptorSetLayout{ VulkanContext::Get().GetDescriptorSetLayouts( DESCRIPTOR_SET_LAYOUT_PBR_SHADER ) };

        const VulkanDevice& device{ VulkanContext::Get().GetDevice() };
        VulkanDescriptorAllocator& descriptorAllocator{ VulkanContext::Get().GetDescriptorAllocator() };

        m_DescriptorSet = *descriptorAllocator.Allocate( device.GetLogicalDevice(), descriptorSetLayout );
    }
}// namespace Mikoto