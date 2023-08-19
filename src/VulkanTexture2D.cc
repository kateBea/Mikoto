/**
 * VulkanTexture2D.cc
 * Created by kate on 7/5/2023.
 * */

// C++ Standard Library
#include <filesystem>
#include <stdexcept>

// Third-Party Libraries
#include <volk.h>
#include <stb_image.h>

// Project Headers
#include <Utility/Common.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanTexture2D.hh>
#include <Renderer/Vulkan/VulkanRenderer.hh>

namespace Mikoto {
    VulkanTexture2D::VulkanTexture2D(const Path_T& path, bool retainFileData) {
        m_RetainData = retainFileData;

        LoadImageData(path);

        CreateTextureImage();
        CreateTextureImageView();
        CreateTextureSampler();
    }

    auto VulkanTexture2D::CreateTextureImageView() -> void {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_TextureImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(VulkanContext::GetPrimaryLogicalDevice(), &viewInfo, nullptr, &m_TextureImageView) != VK_SUCCESS)
            throw std::runtime_error("failed to create texture image view!");

    }

    auto VulkanTexture2D::CreateTextureSampler() -> void {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
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

        if (vkCreateSampler(VulkanContext::GetPrimaryLogicalDevice(), &samplerInfo, nullptr, &m_TextureSampler) != VK_SUCCESS)
            throw std::runtime_error("failed to create texture sampler!");

    }

    auto VulkanTexture2D::LoadImageData(const Path_T& path) -> void {
        Int32_T width{};
        Int32_T height{};
        Int32_T channels{};
        auto filePath { GetByteChar(path) };

        m_TextureFileData = stbi_load(filePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
        m_Width = width;
        m_Height = height;
        m_Channels = channels;

        constexpr UInt32_T CHANNEL_COUNT{ 4 };
        m_ImageSize = m_Width  * m_Height * CHANNEL_COUNT;

        if (!m_TextureFileData)
            throw std::runtime_error("failed to load texture image!");
    }

    auto VulkanTexture2D::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) -> void {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(VulkanContext::GetPrimaryLogicalDevice(), &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
            throw std::runtime_error("failed to create vertex buffer!");

        VkMemoryRequirements memRequirements{};
        vkGetBufferMemoryRequirements(VulkanContext::GetPrimaryLogicalDevice(), buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = VulkanContext::FindMemoryType(memRequirements.memoryTypeBits, properties, VulkanContext::GetPrimaryPhysicalDevice());

        /**
         * NOTE:
         * It should be noted that in a real world application, you're not supposed to actually call
         * vkAllocateMemory for every individual buffer. The maximum number of simultaneous memory
         * allocations is limited by the maxMemoryAllocationCount physical device limit, which may
         * be as low as 4096 even on high end hardware like an NVIDIA GTX 1080
         * See: https://vulkan-tutorial.com/Vertex_buffers/Staging_buffer
         * */
        if (vkAllocateMemory(VulkanContext::GetPrimaryLogicalDevice(), &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
            throw std::runtime_error("failed to allocate vertex buffer memory!");

        vkBindBufferMemory(VulkanContext::GetPrimaryLogicalDevice(), buffer, bufferMemory, 0);
    }

    auto VulkanTexture2D::CreateTextureImage() -> void {
        VkBuffer stagingBuffer{};
        VkDeviceMemory stagingBufferMemory{};
        CreateBuffer(m_ImageSize,
                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingBuffer, stagingBufferMemory);

        void* data{};
        vkMapMemory(VulkanContext::GetPrimaryLogicalDevice(), stagingBufferMemory, 0, m_ImageSize, 0, &data);
        memcpy(data, m_TextureFileData, static_cast<std::size_t>(m_ImageSize));
        vkUnmapMemory(VulkanContext::GetPrimaryLogicalDevice(), stagingBufferMemory);

        if (!m_RetainData) {
            stbi_image_free(m_TextureFileData);
            m_TextureFileData = nullptr;
        }

        CreateImage(m_Width, m_Height,
                    VK_FORMAT_R8G8B8A8_SRGB,
                    VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    m_TextureImage, m_TextureImageMemory);

        TransitionImageLayout(m_TextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        CopyBufferToImage(stagingBuffer, m_TextureImage, m_Width, m_Height);
        TransitionImageLayout(m_TextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        vkDestroyBuffer(VulkanContext::GetPrimaryLogicalDevice(), stagingBuffer, nullptr);
        vkFreeMemory(VulkanContext::GetPrimaryLogicalDevice(), stagingBufferMemory, nullptr);
    }

    auto VulkanTexture2D::CreateImage(UInt32_T width, UInt32_T height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) -> void {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;

        imageInfo.format = format;
        imageInfo.tiling = tiling;

        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(VulkanContext::GetPrimaryLogicalDevice(), &imageInfo, nullptr, &image) != VK_SUCCESS)
            throw std::runtime_error("failed to create image!");

        VkMemoryRequirements memRequirements{};
        vkGetImageMemoryRequirements(VulkanContext::GetPrimaryLogicalDevice(), image, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = VulkanContext::FindMemoryType(memRequirements.memoryTypeBits, properties, VulkanContext::GetPrimaryPhysicalDevice());

        if (vkAllocateMemory(VulkanContext::GetPrimaryLogicalDevice(), &allocInfo, nullptr, &imageMemory) != VK_SUCCESS)
            throw std::runtime_error("failed to allocate image memory!");

        vkBindImageMemory(VulkanContext::GetPrimaryLogicalDevice(), image, imageMemory, 0);
    }

    auto VulkanTexture2D::TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) -> void {
        VkCommandBuffer commandBuffer{ dynamic_cast<VulkanRenderer*>(Renderer::GetActiveGraphicsAPIPtr())->GetCommandPool().BeginSingleTimeCommands() };

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags sourceStage{};
        VkPipelineStageFlags destinationStage{};

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
            newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
                newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else
            throw std::invalid_argument("unsupported layout transition!");


        vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        dynamic_cast<VulkanRenderer*>(Renderer::GetActiveGraphicsAPIPtr())->GetCommandPool().EndSingleTimeCommands(commandBuffer);
    }

    auto VulkanTexture2D::CopyBufferToImage(VkBuffer buffer, VkImage image, UInt32_T width, UInt32_T height) -> void {
        VkCommandBuffer commandBuffer{ dynamic_cast<VulkanRenderer*>(Renderer::GetActiveGraphicsAPIPtr())->GetCommandPool().BeginSingleTimeCommands() };

        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = { width, height, 1 };

        vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        dynamic_cast<VulkanRenderer*>(Renderer::GetActiveGraphicsAPIPtr())->GetCommandPool().EndSingleTimeCommands(commandBuffer);
    }

    auto VulkanTexture2D::OnRelease() const -> void {
        vkDeviceWaitIdle(VulkanContext::GetPrimaryLogicalDevice());

        vkDestroySampler(VulkanContext::GetPrimaryLogicalDevice(), m_TextureSampler, nullptr);
        vkDestroyImageView(VulkanContext::GetPrimaryLogicalDevice(), m_TextureImageView, nullptr);
        vkDestroyImage(VulkanContext::GetPrimaryLogicalDevice(), m_TextureImage, nullptr);
        vkFreeMemory(VulkanContext::GetPrimaryLogicalDevice(), m_TextureImageMemory, nullptr);
    }
}