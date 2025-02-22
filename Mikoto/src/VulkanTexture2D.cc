/**
 * VulkanTexture2D.cc
 * Created by kate on 7/5/2023.
 * */

// C++ Standard Library
#include <filesystem>
#include <memory>
#include <stdexcept>

// Third-Party Libraries
#include <volk.h>
#include <stb_image.h>
#include <backends/imgui_impl_vulkan.h>

// Project Headers
#include <Common/Common.hh>
#include <Core/System/FileSystem.hh>
#include <Library/Utility/Types.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanDeletionQueue.hh>
#include <Renderer/Vulkan/VulkanHelpers.hh>
#include <Renderer/Vulkan/VulkanRenderer.hh>
#include <Renderer/Vulkan/VulkanTexture2D.hh>

namespace Mikoto {

    VulkanTexture2D::VulkanTexture2D( const VulkanTexture2DCreateInfo& data )
        : Texture2D{ data.Type }
    {
        try {
            LoadImageData( data.Path );
            CreateImage();
            CreateSampler();

            if ( !data.RetainFileData ) {
                stbi_image_free( m_FileData );
                m_FileData = nullptr;
            }
        } catch (const std::exception& exception) {
            m_FileData = nullptr;
            m_BufferSize = 0;
            m_Image = nullptr;
            m_Sampler = VK_NULL_HANDLE;

            MKT_CORE_LOGGER_ERROR( "VulkanTexture2D::Create - Failed to initialize the Vulkan Texture 2D. exception.what(): {}", exception.what() );
        }

    }

    auto VulkanTexture2D::Create( const VulkanTexture2DCreateInfo& data ) -> Scope_T<VulkanTexture2D> {
        auto result{ CreateScope<VulkanTexture2D>( data ) };

        // Could not create a valid texture
        if (result == nullptr || result->m_Image == nullptr) {
            return nullptr;
        }

        return std::move( result );
    }

    auto VulkanTexture2D::Release() -> void {
        VulkanDevice& device{ VulkanContext::Get().GetDevice() };

        m_BufferSize = 0;
        m_FileData = nullptr;

        vkDestroySampler( device.GetLogicalDevice(), m_Sampler, nullptr );

        m_File = nullptr;

        m_Image = nullptr;
    }

    VulkanTexture2D::~VulkanTexture2D() {
        if (!m_IsReleased) {
            Release();
            Invalidate();
        }
    }

    auto VulkanTexture2D::LoadImageData( const Path_T& path ) -> void {
        FileSystem& fileSystem{ Engine::GetSystem<FileSystem>() };

        const File* textureFile{ nullptr };

        if (path.extension() == ".tif") {
            // Look for the PNG version instead
            Path_T pngVersion{ path };
            pngVersion.replace_extension( ".png" );
            textureFile = fileSystem.LoadFile( pngVersion );
        } else {
            textureFile = fileSystem.LoadFile( path );
        }

        if (textureFile == nullptr) {
            MKT_THROW_RUNTIME_ERROR( "VulkanTexture2D::LoadImageData - Failed to load texture file." );
        }

        m_File = textureFile;

        stbi_set_flip_vertically_on_load( true );

        m_FileData = stbi_load(
            m_File->GetPathCStr(),
            std::addressof( m_Width ),
            std::addressof( m_Height ),
            std::addressof( m_Channels ),
            STBI_rgb_alpha );

        if ( !m_FileData ) {
            MKT_THROW_RUNTIME_ERROR( fmt::format( "VulkanTexture2D - Failed to load texture image! File: [{}]", m_File->GetPathCStr() ) );
        }

        // since we use STBI_rgb_alpha, stb will load the image with four
        // channels which matches the format we are going to be using for now
        constexpr auto channelCount{ 4 };
        m_BufferSize = m_Width * m_Height * channelCount;
    }

    auto VulkanTexture2D::CreateImage() -> void {
        VulkanDevice& device{ VulkanContext::Get().GetDevice() };

        // allocate staging buffer
        VkBufferCreateInfo stagingBufferInfo{ VulkanHelpers::Initializers::BufferCreateInfo() };
        stagingBufferInfo.pNext = nullptr;

        stagingBufferInfo.size = m_BufferSize;
        stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        //let the VMA library know that this data should be on CPU RAM
        VmaAllocationCreateInfo vmaStagingAllocationCreateInfo{};
        vmaStagingAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
        vmaStagingAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        const VulkanBufferCreateInfo stagingBufferBufferCreateInfo{
            .BufferCreateInfo{ stagingBufferInfo },
            .AllocationCreateInfo{ vmaStagingAllocationCreateInfo },
            .WantMapping{ true }
        };

        Scope_T<VulkanBuffer> stagingBuffer{ VulkanBuffer::Create( stagingBufferBufferCreateInfo ) };

        // Copy vertex data to staging buffer
        std::memcpy(stagingBuffer->GetVmaAllocationInfo().pMappedData, m_FileData, m_BufferSize);

        stagingBuffer->PersistentUnmap();

        // Allocate image
        const VkExtent3D extent{ static_cast<UInt32_T>( m_Width ), static_cast<UInt32_T>( m_Height ), 1 };

        VkImageCreateInfo vkImageCreateInfo{ VulkanHelpers::Initializers::ImageCreateInfo() };

        // VK_FORMAT_R8G8B8A8_SRGB matches the image format loaded with stb
        vkImageCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
        vkImageCreateInfo.extent = extent;
        vkImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        vkImageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        vkImageCreateInfo.mipLevels = 1;
        vkImageCreateInfo.arrayLayers = 1;
        vkImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        vkImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        vkImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        // The image will only be used by one queue family: the one that supports graphics (and therefore also) transfer operations.
        vkImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkImageCreateInfo.flags = 0;

        VkImageViewCreateInfo imageViewCreateInfo{ VulkanHelpers::Initializers::ImageViewCreateInfo() };

        imageViewCreateInfo.pNext = nullptr;
        imageViewCreateInfo.flags = 0;
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;

        imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imageViewCreateInfo.subresourceRange.levelCount = 1;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount = 1;

        VulkanImageCreateInfo vulkanImageCreateInfo{
            .Image{ VK_NULL_HANDLE },
            .ImageCreateInfo{ vkImageCreateInfo },
            .ImageViewCreateInfo{ imageViewCreateInfo }
        };

        m_Image = VulkanImage::Create( vulkanImageCreateInfo );

        VulkanContext::Get().ImmediateSubmit( [&]( const VkCommandBuffer& cmd ) -> void {
            m_Image->LayoutTransition( VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, cmd );

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
            vkCmdCopyBufferToImage( cmd, stagingBuffer->Get(), m_Image->Get(), m_Image->GetCurrentLayout(), 1, std::addressof( copyRegion ) );
        } );

        VulkanContext::Get().ImmediateSubmit( [&]( VkCommandBuffer cmd ) -> void {
            // Perform second transition for the descriptor set creation
            m_Image->LayoutTransition(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, cmd);
        } );

        // destroy staging buffer
        stagingBuffer = nullptr;
    }

    auto VulkanTexture2D::CreateSampler() -> void {
        VulkanDevice& device{ VulkanContext::Get().GetDevice() };

        VkSamplerCreateInfo samplerInfo{ VulkanHelpers::Initializers::SamplerCreateInfo() };
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;

        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

        const VkPhysicalDeviceProperties& properties{ device.GetPhysicalDeviceProperties() };

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

        if ( vkCreateSampler( device.GetLogicalDevice(), &samplerInfo, nullptr, &m_Sampler ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "Failed to create texture sampler!" );
        }
    }
}