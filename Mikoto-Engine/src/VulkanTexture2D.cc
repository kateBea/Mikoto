/**
 * VulkanTexture2D.cc
 * Created by kate on 7/5/2023.
 * */

// C++ Standard Library
#include <memory>
#include <filesystem>
#include <stdexcept>

// Third-Party Libraries
#include "backends/imgui_impl_vulkan.h"
#include "stb_image.h"
#include "volk.h"

// Project Headers
#include "../Common/Common.hh"
#include "../Common/Types.hh"
#include <Common/VulkanUtils.hh>

#include <Renderer/Vulkan/DeletionQueue.hh>
#include "Renderer/Vulkan/VulkanBuffer.hh"
#include "Renderer/Vulkan/VulkanContext.hh"
#include "Renderer/Vulkan/VulkanRenderer.hh"
#include "Renderer/Vulkan/VulkanTexture2D.hh"

namespace Mikoto {
    VulkanTexture2D::VulkanTexture2D(const Path_T& path, MapType type, bool retainFileData)
        :   Texture2D{ type }
    {
        LoadImageData(path);

        CreateImage();

        CreateImageView();

        CreateSampler();

        // TODO: proper descriptor sets and pools management
        // https://vkguide.dev/docs/extra-chapter/abstracting_descriptors/
        m_DescSet = (VkDescriptorSet)ImGui_ImplVulkan_AddTexture(m_TextureSampler,
                                                                  m_View,
                                                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        DeletionQueue::Push([imageHandle = m_ImageInfo.Image, allocation = m_ImageInfo.Allocation, textureDescSet = m_DescSet]() -> void {
            ImGui_ImplVulkan_RemoveTexture(textureDescSet);
            vmaDestroyImage(VulkanContext::GetDefaultAllocator(), imageHandle, allocation);
        });

        if (!retainFileData) {
            stbi_image_free(m_TextureFileData);
            m_TextureFileData = nullptr;
        }
    }

    auto VulkanTexture2D::CreateDescriptorPool() -> void {
        std::array<VkDescriptorPoolSize, 1> poolSizes{};

        poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[0].descriptorCount = 1;

        VkDescriptorPoolCreateInfo poolInfo{ VulkanUtils::Initializers::DescriptorPoolCreateInfo() };
        poolInfo.poolSizeCount = static_cast<UInt32_T>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = 4000; // TODO: WILL FAIL IF TRY TO ALLOCATE MORE

        if (vkCreateDescriptorPool(VulkanContext::GetPrimaryLogicalDevice(), std::addressof(poolInfo), nullptr, std::addressof(s_DescriptorPool)) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("Failed to create descriptor pool for Texture2D!");
        }
    }

    auto VulkanTexture2D::CreateDescriptorSet() -> void {
        // UNUSED FOR NOW. NEED BETTER DESCRIPTOR SET/POOL MANAGEMENT
        auto& singleTextureSetLayout { dynamic_cast<VulkanRenderer*>(Renderer::GetActiveGraphicsAPIPtr())->GetSingleTextureSetLayout() };

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = s_DescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &singleTextureSetLayout;

        if (vkAllocateDescriptorSets(VulkanContext::GetPrimaryLogicalDevice(), &allocInfo, &m_DescSet) != VK_SUCCESS)
            MKT_THROW_RUNTIME_ERROR("failed to allocate descriptor sets!");

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        imageInfo.imageView = GetImageView();
        imageInfo.sampler = GetImageSampler();

        std::array<VkWriteDescriptorSet, 1> descriptorWrites{};

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = m_DescSet;
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(VulkanContext::GetPrimaryLogicalDevice(), static_cast<UInt32_T>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }

    auto VulkanTexture2D::LoadImageData(const Path_T& path) -> void {
        auto filePath { GetByteChar(path) };
        stbi_set_flip_vertically_on_load(true);
        m_TextureFileData = stbi_load(filePath.c_str(), std::addressof(m_Width), std::addressof(m_Height), std::addressof(m_Channels), STBI_rgb_alpha);

        // since we use STBI_rgb_alpha, stb will load the image with four
        // channels which matches the format we are going to be using for now
        constexpr UInt32_T channelCount{ 4 };

        m_ImageSize = m_Width  * m_Height * channelCount;

        if (!m_TextureFileData) {
            MKT_THROW_RUNTIME_ERROR(fmt::format("Failed to load texture image! Error file [{}]", path.string()));
        }
    }

    auto VulkanTexture2D::CreateImage() -> void {
        auto& vmaAllocator{ VulkanContext::GetDefaultAllocator() };

        VkBufferCreateInfo stagingBufferInfo{ VulkanUtils::Initializers::BufferCreateInfo() };
        stagingBufferInfo.pNext = nullptr;
        stagingBufferInfo.size = m_ImageSize;
        stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        //let the VMA library know that this data should be on CPU RAM
        VmaAllocationCreateInfo vmaStagingAllocationCreateInfo{};
        vmaStagingAllocationCreateInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

        BufferAllocateInfo stagingBuffer{};

        // Allocate the buffer
        if (vmaCreateBuffer(vmaAllocator,
                            std::addressof(stagingBufferInfo),
                            std::addressof(vmaStagingAllocationCreateInfo),
                            std::addressof(stagingBuffer.Buffer),
                            std::addressof(stagingBuffer.Allocation),
                            nullptr) != VK_SUCCESS)
        {
            MKT_THROW_RUNTIME_ERROR("Failed to create VMA staging buffer for Vulkan vertex buffer");
        }

        // Copy vertex data to staging buffer
        void* stagingBufferData{};
        vmaMapMemory(vmaAllocator, stagingBuffer.Allocation, std::addressof(stagingBufferData));
        std::memcpy(stagingBufferData, static_cast<const void*>(m_TextureFileData), m_ImageSize);
        vmaUnmapMemory(vmaAllocator, stagingBuffer.Allocation);

        // Allocate image
        const VkExtent3D extent{ static_cast<UInt32_T>(m_Width), static_cast<UInt32_T>(m_Height), static_cast<UInt32_T>(1) };

        m_ImageInfo.ImageCreateInfo = VulkanUtils::Initializers::ImageCreateInfo();

        // VK_FORMAT_R8G8B8A8_SRGB matches the image format loaded with stb
        m_ImageInfo.ImageCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
        m_ImageInfo.ImageCreateInfo.extent = extent;
        m_ImageInfo.ImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        m_ImageInfo.ImageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        m_ImageInfo.ImageCreateInfo.mipLevels = 1;
        m_ImageInfo.ImageCreateInfo.arrayLayers = 1;
        m_ImageInfo.ImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        m_ImageInfo.ImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;

        m_ImageInfo.AllocationCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        m_ImageInfo.AllocationCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        VulkanUtils::AllocateImage(m_ImageInfo);

        VulkanContext::ImmediateSubmit([&](VkCommandBuffer cmd) -> void {
            VulkanUtils::PerformImageLayoutTransition(m_ImageInfo.Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, cmd);

            // Copy from staging buffer to image buffer
            VkBufferImageCopy copyRegion{};
            copyRegion.bufferOffset = 0;
            copyRegion.bufferRowLength = 0;
            copyRegion.bufferImageHeight = 0;

            copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.imageSubresource.mipLevel = 0;
            copyRegion.imageSubresource.baseArrayLayer = 0;
            copyRegion.imageSubresource.layerCount = 1;
            copyRegion.imageExtent = extent;

            //copy the buffer into the image
            vkCmdCopyBufferToImage(cmd, stagingBuffer.Buffer, m_ImageInfo.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, std::addressof(copyRegion));
        });

        VulkanContext::ImmediateSubmit([&](VkCommandBuffer cmd) -> void {
            // Perform second transition for the descriptor set creation
            VulkanUtils::PerformImageLayoutTransition(m_ImageInfo.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, cmd);
        });

        // destroy staging buffer
        vmaDestroyBuffer(vmaAllocator, stagingBuffer.Buffer, stagingBuffer.Allocation);

        MKT_CORE_LOGGER_DEBUG("Texture loaded successfully");
    }

    auto VulkanTexture2D::CreateImageView() -> void {
        VkImageViewCreateInfo imageViewCreateInfo{ VulkanUtils::Initializers::ImageViewCreateInfo() };

        imageViewCreateInfo.pNext = nullptr;
        imageViewCreateInfo.flags = 0;
        imageViewCreateInfo.image = m_ImageInfo.Image;
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format = m_ImageInfo.ImageCreateInfo.format;

        imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imageViewCreateInfo.subresourceRange.levelCount = 1;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(VulkanContext::GetPrimaryLogicalDevice(), std::addressof(imageViewCreateInfo), nullptr, std::addressof(m_View)) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("Failed to create Vulkan image view!");
        }

        DeletionQueue::Push([imageView = m_View]() -> void {
            vkDestroyImageView(VulkanContext::GetPrimaryLogicalDevice(), imageView, nullptr);
        });

        MKT_CORE_LOGGER_DEBUG("Vulkan image view created successfully");
    }

    auto VulkanTexture2D::CreateSampler() -> void {
        VkSamplerCreateInfo samplerInfo{ VulkanUtils::Initializers::SamplerCreateInfo() };
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;

        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

        auto properties{ VulkanContext::GetPrimaryLogicalDeviceProperties() };
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;

        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

        samplerInfo.unnormalizedCoordinates = VK_FALSE;

        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        if (vkCreateSampler(VulkanContext::GetPrimaryLogicalDevice(), &samplerInfo, nullptr, &m_TextureSampler) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("Failed to create texture sampler!");
        }

        DeletionQueue::Push([sampler = m_TextureSampler]() -> void {
            vkDestroySampler(VulkanContext::GetPrimaryLogicalDevice(), sampler, nullptr);
        });
    }
}