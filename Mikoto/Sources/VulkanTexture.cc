/**
 * VulkanTexture2D.cc
 * Created by kate on 7/5/2023.
 * */

// C++ Standard Library
#include <filesystem>
#include <memory>
#include <stdexcept>

// Third-Party Libraries
#include <backends/imgui_impl_vulkan.h>
#include <stb_image.h>
#include <volk.h>
// Project Headers
#include <Common/Common.hh>
#include <Library/Utility/Types.hh>
#include <Renderer/Core/HdriToCubemap.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanDevice.hh>
#include <Renderer/Vulkan/VulkanHelpers.hh>
#include <Renderer/Vulkan/VulkanTexture.hh>

#include "Filesystem/FileService.hh"

namespace Mikoto {

    MKT_NODISCARD static auto ToVkSamplerFilter(SamplerFilter filter) -> VkFilter {
        switch (filter) {
            case SamplerFilter::FILTER_NEAREST:
                return VK_FILTER_NEAREST;
            case SamplerFilter::FILTER_LINEAR:
                return VK_FILTER_LINEAR;
        }
    }

    MKT_NODISCARD static auto ToVkSamplerWarp(SamplerWrapMode wrap) -> VkSamplerAddressMode {
        switch (wrap) {
            case SamplerWrapMode::WRAP_CLAMP_TO_EDGE:
                return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            case SamplerWrapMode::WRAP_CLAMP_TO_BORDER:
                return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
            case SamplerWrapMode::WRAP_REPEAT:
                return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        }
    }

    VulkanSampler::VulkanSampler( const SamplerDescription& info ) {
        // Create a Sampler for the texture we will display in the viewport
        m_CreateInfo = VulkanHelpers::Initializers::SamplerCreateInfo();

        m_CreateInfo.magFilter = ToVkSamplerFilter(info.MagFilter);
        m_CreateInfo.minFilter = ToVkSamplerFilter(info.MinFilter);
        m_CreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

        m_CreateInfo.addressModeU = ToVkSamplerWarp(info.WrapU);
        m_CreateInfo.addressModeV = ToVkSamplerWarp(info.WrapV);
        m_CreateInfo.addressModeW = ToVkSamplerWarp(info.WrapW);

        m_CreateInfo.maxAnisotropy = 1.0f;
        m_CreateInfo.mipLodBias = 0.0f;
        m_CreateInfo.minLod = 0.0f;
        m_CreateInfo.maxLod = info.MipLevels;
        m_CreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;

        if (info.CubeSampler) {
            m_CreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
            m_CreateInfo.anisotropyEnable = VK_TRUE;
        }
    }

    auto VulkanSampler::GetNativeHandle( ObjectType type ) -> Object {
        if ( type != ObjectType::Vk_Sampler ) {
            return Object( nullptr );
        }

        return Object( m_Sampler );
    }

    VulkanSampler::~VulkanSampler() {
        if ( m_IsAllocated ) {
            Release();
        }
    }

    auto VulkanSampler::Release() -> void {
        vkDestroySampler( VK_DEVICE( m_Device ), m_Sampler, nullptr );
        m_IsAllocated = false;
    }

    auto VulkanSampler::Initialize() -> void {
        m_CreateInfo.maxAnisotropy = TO_VK_DEVICE( m_Device )->GetPhysicalDeviceProperties().limits.maxSamplerAnisotropy;

        if ( vkCreateSampler( VK_DEVICE( m_Device ), std::addressof( m_CreateInfo ), nullptr, std::addressof( m_Sampler ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanSampler::Initialize - Failed to create sampler!" );
        }

        m_IsAllocated = true;
    }

    VulkanTexture::VulkanTexture( const TextureDescription& data )
        : Texture2D{ data.Width, data.Height, data.ChannelCount, data.Data, data.UsageType, data.Format, data.Usage, data.Map } {
        m_ImageSize = m_Width * m_Height * m_Channels;

        m_ExternalBufferSize = data.BufferSize;

        m_Multisampling = data.MSAA;

        m_IsHDR = data.IsHDR;

        if ( data.TextureFile ) {
            m_DebugName = fmt::format( "Mikoto Texture. Id: {}, Loaded from {}", GetHandle(), data.TextureFile->GetPathCStr() );
        } else {
            m_DebugName = fmt::format( "Mikoto Texture. Id: {}", GetHandle() );
        }

        if (data.TextureFile) {
            SetTextureUri( data.TextureFile->GetPathCStr() );
            SetTextureName( data.TextureFile->GetName() );
        }
    }

    VulkanTexture::VulkanTexture( const VkImageViewCreateInfo& viewCreateInfo, VkExtent2D extent )
        : Texture2D{ static_cast<Int32>( extent.width ), static_cast<Int32>( extent.height ), 0, nullptr,
                     ResourceUsageType::RESOURCE_USAGE_STATIC,
                     VulkanHelpers::ToTextureFormat( viewCreateInfo.format ) },
          m_IsImageExternal{ true },
          m_ImageViewCreateInfo{ viewCreateInfo } {
        m_ImageAllocation.Image = viewCreateInfo.image;

        m_ImageSize = m_Width * m_Height * m_Channels;

        m_DebugName = fmt::format( "Mikoto Swap chain Texture. Id:", GetHandle() );
    }

    auto VulkanTexture::Release() -> void {
        if ( !m_IsAllocated ) {
            return;
        }

        TO_VK_DEVICE( m_Device )->WaitIdle();

        vkDestroyImageView( VK_DEVICE( m_Device ), m_ImageView, nullptr );

        if ( !m_IsImageExternal ) {
            auto* allocator{ MKT_VMA_ALLOC_PTR( m_Device ) };
            allocator->FreeImage( m_ImageAllocation );
        }

        m_IsAllocated = false;
    }

    auto VulkanTexture::SetDebugInfo() -> void {
        if ( m_DebugName == GetDefaultDebugName() ) {
            m_DebugName = fmt::format( "MikotoVulkanTexture (Loaded Text: '{}') Image: {}, ImageView: {}, Pool ID: {}", GetTextureUri(), reinterpret_cast<UInt64>( m_ImageAllocation.Image ), reinterpret_cast<UInt64>( m_ImageView ), GetHandle() );
        }

        VulkanHelpers::SetObjectDebugName( VK_DEVICE( m_Device ), VK_OBJECT_TYPE_IMAGE, reinterpret_cast<UInt64>( m_ImageAllocation.Image ), m_DebugName.c_str() );
        VulkanHelpers::SetObjectDebugName( VK_DEVICE( m_Device ), VK_OBJECT_TYPE_IMAGE_VIEW, reinterpret_cast<UInt64>( m_ImageView ), m_DebugName.c_str() );
    }

    VulkanTextureCube::VulkanTextureCube( const TextureCubeCreateDescription& data )
        : TextureCube{ data.ResourceUsage, data.MipLevels } {

        m_TextureFaces = data.Faces;
        m_IsHDR = data.IsHdrMap;

        m_Format = data.Format;

        m_Width = data.Dimensions;
        m_Height = data.Dimensions;

        m_Multisampling = data.MSAA;

        m_TextureUsage = data.Usage;

        m_IsHDR = data.IsHdrMap;

        if (!data.Faces.empty() && data.Faces[0]) {
            SetTextureUri( data.Faces[0]->GetPathCStr() );
            SetTextureName( data.Faces[0]->GetName() );
        }
    }

    auto VulkanTextureCube::GetNativeHandle( ObjectType type ) -> Object {
        switch ( type ) {
            case ObjectType::Vk_Image:
                return Object( m_ImageAllocation.Image );

            case ObjectType::Vk_ImageView:
                return Object( m_ImageView );

            case ObjectType::Vk_Format:
                return Object( std::addressof( m_ImageAllocation.ImageCreateInfo.format ) );

            default:;
        }

        return Object( nullptr );
    }

    auto VulkanTextureCube::GetNativeHandle( ObjectType type ) const -> Object {
        return const_cast<VulkanTextureCube*>(this)->GetNativeHandle( type );
    }

    auto VulkanTextureCube::GetCurrentLayout() const -> VkImageLayout {
        return m_CurrentLayout;
    }

    auto VulkanTextureCube::SetDebugName( std::string_view name ) -> void {
        m_DebugName = name;

        VulkanHelpers::SetObjectDebugName( VK_DEVICE( m_Device ), VK_OBJECT_TYPE_IMAGE, reinterpret_cast<UInt64>( m_ImageAllocation.Image ), m_DebugName.c_str() );
        VulkanHelpers::SetObjectDebugName( VK_DEVICE( m_Device ), VK_OBJECT_TYPE_IMAGE_VIEW, reinterpret_cast<UInt64>( m_ImageView ), m_DebugName.c_str() );
    }

    auto VulkanTextureCube::GetCreateInfo() const -> const VkImageCreateInfo& {
        return m_ImageAllocation.ImageCreateInfo;
    }

    auto VulkanTextureCube::GetViewCreateInfo() const -> const VkImageViewCreateInfo& {
        return m_ImageViewCreateInfo;
    }

    auto VulkanTextureCube::SetCurrentLayout( VkImageLayout layout ) -> void {
        m_CurrentLayout = layout;
    }

    auto VulkanTextureCube::SubmitLayoutTransition( VkImageLayout newLayout, VkCommandBuffer cmd ) -> void {
        if (m_CurrentLayout == newLayout) {
            return;
        }

        VkImageSubresourceRange subresourceRange{};
        subresourceRange.aspectMask = VulkanHelpers::GetAspectMask(VulkanHelpers::ToVkFormat( GetFormat() ));;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = m_MipLevels;
        subresourceRange.layerCount = 6;

        // Create an image barrier object
        VkImageMemoryBarrier imageMemoryBarrier{};
        imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        imageMemoryBarrier.oldLayout = m_CurrentLayout;
        imageMemoryBarrier.newLayout = newLayout;
        imageMemoryBarrier.image = m_ImageAllocation.Image;
        imageMemoryBarrier.subresourceRange = subresourceRange;

        // Source layouts (old)
        // Source access mask controls actions that have to be finished on the old layout
        // before it will be transitioned to the new layout
        switch ( m_CurrentLayout ) {
            case VK_IMAGE_LAYOUT_UNDEFINED:
                // Image layout is undefined (or does not matter)
                // Only valid as initial layout
                // No flags required, listed only for completeness
                imageMemoryBarrier.srcAccessMask = 0;
                break;

            case VK_IMAGE_LAYOUT_PREINITIALIZED:
                // Image is preinitialized
                // Only valid as initial layout for linear images, preserves memory contents
                // Make sure host writes have been finished
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                // Image is a color attachment
                // Make sure any writes to the color buffer have been finished
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                // Image is a depth/stencil attachment
                // Make sure any writes to the depth/stencil buffer have been finished
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                // Image is a transfer source
                // Make sure any reads from the image have been finished
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                break;

            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                // Image is a transfer destination
                // Make sure any writes to the image have been finished
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                // Image is read by a shader
                // Make sure any shader reads from the image have been finished
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
                break;
            default:
                // Other source layouts aren't handled (yet)
                break;
        }

        // Target layouts (new)
        // Destination access mask controls the dependency for the new image layout
        switch ( newLayout ) {
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                // Image will be used as a transfer destination
                // Make sure any writes to the image have been finished
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                // Image will be used as a transfer source
                // Make sure any reads from the image have been finished
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                break;

            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                // Image will be used as a color attachment
                // Make sure any writes to the color buffer have been finished
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                // Image layout will be used as a depth/stencil attachment
                // Make sure any writes to depth/stencil buffer have been finished
                imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                // Image will be read in a shader (sampler, input attachment)
                // Make sure any writes to the image have been finished
                if ( imageMemoryBarrier.srcAccessMask == 0 ) {
                    imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
                }
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                break;
            default:
                // Other source layouts aren't handled (yet)
                break;
        }

        // Put barrier inside setup command buffer
        vkCmdPipelineBarrier(
                cmd,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &imageMemoryBarrier );

        // Update the current layout
        m_CurrentLayout = newLayout;
    }

    VulkanTextureCube::~VulkanTextureCube() {
        if ( m_IsAllocated ) {
            Release();
        }
    }

    auto VulkanTextureCube::Initialize() -> void {
        if (IsTextureUsage( TextureUsage::RENDER_TARGET )) {
            CreateImageResource();
        }

        if (IsTextureUsage( TextureUsage::CUBE )) {
            LoadCubeFaces();
            CreateImageResource();

            CommandListHandle cmd{ m_Device->CreateCommandList( QueueType::TRANSFER_QUEUE, true ) };
            cmd->Begin();
            cmd->FillTexture( m_StagingBuffer.GetRaw(), this );
            cmd->End();
            m_Device->SubmitCommands( cmd );
        }

        SetDebugInfo();

        m_IsAllocated = true;

        // We no longer need it
        m_StagingBuffer.Reset();
    }

    auto VulkanTextureCube::Release() -> void {
        dynamic_cast<VulkanDevice*>( m_Device )->WaitIdle();

        vkDestroyImageView( VK_DEVICE( m_Device ), m_ImageView, nullptr );

        auto* allocator{ MKT_VMA_ALLOC_PTR(m_Device) };
        allocator->FreeImage( m_ImageAllocation );

        m_IsAllocated = false;
    }

    auto VulkanTextureCube::CreateImageResource() -> void {
        const VkExtent3D extent{
            static_cast<UInt32>( m_Width ),
            static_cast<UInt32>( m_Height ),
            1
        };

        m_ImageAllocation.ImageCreateInfo = VulkanHelpers::Initializers::ImageCreateInfo();
        m_ImageAllocation.ImageCreateInfo.format = VulkanHelpers::ToVkFormat( m_Format );
        m_ImageAllocation.ImageCreateInfo.extent = extent;

        // This texture is a 2D image always
        m_ImageAllocation.ImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        m_ImageAllocation.ImageCreateInfo.usage = VulkanHelpers::ToVkImageUsage( m_TextureUsage );

        if (m_TextureUsage == TextureUsage::RENDER_TARGET) {
            m_ImageAllocation.ImageCreateInfo.usage = VulkanHelpers::ToVkImageUsage( TextureUsage::CUBE );
        }

        m_ImageAllocation.ImageCreateInfo.mipLevels = m_MipLevels;
        m_ImageAllocation.ImageCreateInfo.arrayLayers = 6;// Cube
        m_ImageAllocation.ImageCreateInfo.samples = VulkanHelpers::ToVkRasterSamples( m_Multisampling );
        m_ImageAllocation.ImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        m_ImageAllocation.ImageCreateInfo.initialLayout = m_CurrentLayout;

        // The image will only be used by one queue family:
        // the one that supports transfer operations, often graphics one suffices.
        m_ImageAllocation.ImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        m_ImageAllocation.ImageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

        auto* allocator{ MKT_VMA_ALLOC_PTR(m_Device) };
        if ( const VkResult result{ allocator->AllocateImage( m_ImageAllocation ) }; result != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanTextureCube::CreateImageResource - Failed to allocate Vulkan cube image!" );
        }

        m_SizeBytes = m_ImageSize;

        // Prepare for view creation
        m_ImageViewCreateInfo = VulkanHelpers::Initializers::ImageViewCreateInfo();
        m_ImageViewCreateInfo.image = m_ImageAllocation.Image;
        m_ImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        m_ImageViewCreateInfo.format = m_ImageAllocation.ImageCreateInfo.format;

        m_ImageViewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        m_ImageViewCreateInfo.subresourceRange.layerCount = 6;
        m_ImageViewCreateInfo.subresourceRange.levelCount = m_MipLevels;
        m_ImageViewCreateInfo.subresourceRange.aspectMask = VulkanHelpers::GetAspectMask(VulkanHelpers::ToVkFormat( GetFormat() ));

        if ( vkCreateImageView( VK_DEVICE( m_Device ), std::addressof( m_ImageViewCreateInfo ), nullptr, std::addressof( m_ImageView ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanTextureCube::CreateImageResource - Failed to create the Vulkan Image View!" );
        }
    }

    auto VulkanTextureCube::LoadCubeFaces() -> void {
        std::vector<StbImage> images{};

        if ( m_IsHDR ) {
            constexpr bool linearFilter{ true };
            const Int32 cubeMapResolution{ m_Width };

            HdriToCubemap<unsigned char> hdriToCube_hdr( m_TextureFaces[0]->GetPathCStr(), cubeMapResolution, linearFilter );

            const std::string basePath{ Path{ m_TextureFaces[0]->GetPathCStr() }.remove_filename().string() };
            const std::string hdrFile{ Path{ m_TextureFaces[0]->GetPathCStr() }.stem().string() };

            const std::string emitFolder{ fmt::format( "{}{}", basePath, hdrFile ) };

            // Ensure folder exists
            if ( !std::filesystem::exists( emitFolder ) ) {
                std::filesystem::create_directory( emitFolder );
            }

            hdriToCube_hdr.writeCubemap( emitFolder );

            std::vector<std::string> filenames{ "right", "left", "down", "up", "front", "back" };

            const std::string extension{ hdriToCube_hdr.isHdri() ? "hdr" : "png" };

            for ( const auto& filename: filenames ) {
                const File* file{ FileService::Get()->LoadFile( fmt::format( "{}/{}.{}", emitFolder, filename, extension ) ) };
                images.emplace_back( file );
            }
        } else {
            for ( const File* file: m_TextureFaces ) {
                images.emplace_back( file );
            }
        }

        // Calculate the image size and the layer size.
        // CubeMaps share dimensions
        const StbImage& frontImage{ images.front() };
        m_Width = frontImage.GetWidth();
        m_Height = frontImage.GetHeight();

        constexpr VkDeviceSize layerCount{ 6 };

        // Multiply it by 6 because the image must have 6 layers for a cube.
        m_ImageSize = m_Width * m_Height * frontImage.GetChannels() * layerCount;

        // This is just the size of each layer.
        const VkDeviceSize layerSize{ m_ImageSize / 6 };

        // Allocate staging buffer to copy over the texture data
        BufferDescription stagingDesc{};
        stagingDesc.WithData( nullptr )
                .WithUsage( BufferUsage::STAGING )
                .WithSizeBytes( m_ImageSize );

        m_StagingBuffer = m_Device->CreateBuffer( stagingDesc );

        Size layerIndex{};
        for ( StbImage& image: images ) {
            const Size offset{ layerIndex * layerSize };

            m_StagingBuffer->CopyToDevice( image.GetData(), layerSize, offset );
            layerIndex++;
        }
    }

    auto VulkanTextureCube::SetDebugInfo() -> void {
        if ( m_DebugName == GetDefaultDebugName() ) {
            m_DebugName = fmt::format( "MikotoVulkanTextureCube (Loaded Text: '{}') Image: {}, ImageView: {}, Pool ID: {}",
                                       GetTextureUri(), reinterpret_cast<UInt64>( m_ImageAllocation.Image ), reinterpret_cast<UInt64>( m_ImageView ), GetHandle() );
        }

        VulkanHelpers::SetObjectDebugName( VK_DEVICE( m_Device ), VK_OBJECT_TYPE_IMAGE, reinterpret_cast<UInt64>( m_ImageAllocation.Image ), m_DebugName.c_str() );
        VulkanHelpers::SetObjectDebugName( VK_DEVICE( m_Device ), VK_OBJECT_TYPE_IMAGE_VIEW, reinterpret_cast<UInt64>( m_ImageView ), m_DebugName.c_str() );
    }

    VulkanTexture::~VulkanTexture() {
        if ( m_IsAllocated ) {
            Release();
        }
    }


    auto VulkanTexture::HasExternalImage() const -> bool {
        return m_IsImageExternal;
    }

    auto VulkanTexture::GetCurrentLayout() const -> VkImageLayout {
        return m_CurrentLayout;
    }

    auto VulkanTexture::GetCreateInfo() const -> const VkImageCreateInfo& {
        return m_ImageAllocation.ImageCreateInfo;
    }

    auto VulkanTexture::GetViewCreateInfo() const -> const VkImageViewCreateInfo& {
        return m_ImageViewCreateInfo;
    }

    auto VulkanTexture::SetDebugName( std::string_view name ) -> void {
        m_DebugName = name;

        VulkanHelpers::SetObjectDebugName( VK_DEVICE( m_Device ), VK_OBJECT_TYPE_IMAGE, reinterpret_cast<UInt64>( m_ImageAllocation.Image ), m_DebugName.c_str() );
        VulkanHelpers::SetObjectDebugName( VK_DEVICE( m_Device ), VK_OBJECT_TYPE_IMAGE_VIEW, reinterpret_cast<UInt64>( m_ImageView ), m_DebugName.c_str() );
    }

    auto VulkanTexture::IsSwapChainImage() const -> bool {
        return m_IsImageExternal;
    }

    auto VulkanTexture::SetCurrentLayout( VkImageLayout layout ) -> void {
        m_CurrentLayout = layout;
    }

    auto VulkanTexture::GetNativeHandle( ObjectType type ) -> Object {
        switch ( type ) {
            case ObjectType::Vk_Image:
                return Object( m_ImageAllocation.Image );

            case ObjectType::Vk_ImageView:
                return Object( m_ImageView );

            case ObjectType::Vk_Format:
                return Object( std::addressof( m_ImageAllocation.ImageCreateInfo.format ) );

            default:;
        }

        return Object( nullptr );
    }

    auto VulkanTexture::GetNativeHandle( ObjectType type ) const -> Object {
        return const_cast<VulkanTexture*>(this)->GetNativeHandle( type );
    }

    auto VulkanTexture::SubmitLayoutTransition( const VkImageLayout newLayout, const VkCommandBuffer cmd ) -> void {
        if (m_CurrentLayout == newLayout) {
            return;
        }

        VkImageSubresourceRange subresourceRange{};
        subresourceRange.aspectMask = VulkanHelpers::GetAspectMask(VulkanHelpers::ToVkFormat( GetFormat() ));;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = 1;
        subresourceRange.layerCount = 1;

        // Create an image barrier object
        VkImageMemoryBarrier imageMemoryBarrier{};
        imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        imageMemoryBarrier.oldLayout = m_CurrentLayout;
        imageMemoryBarrier.newLayout = newLayout;
        imageMemoryBarrier.image = m_ImageAllocation.Image;
        imageMemoryBarrier.subresourceRange = subresourceRange;

        // Source layouts (old)
        // Source access mask controls actions that have to be finished on the old layout
        // before it will be transitioned to the new layout
        switch ( m_CurrentLayout ) {
            case VK_IMAGE_LAYOUT_UNDEFINED:
                // Image layout is undefined (or does not matter)
                // Only valid as initial layout
                // No flags required, listed only for completeness
                imageMemoryBarrier.srcAccessMask = 0;
                break;

            case VK_IMAGE_LAYOUT_PREINITIALIZED:
                // Image is preinitialized
                // Only valid as initial layout for linear images, preserves memory contents
                // Make sure host writes have been finished
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                // Image is a color attachment
                // Make sure any writes to the color buffer have been finished
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                // Image is a depth/stencil attachment
                // Make sure any writes to the depth/stencil buffer have been finished
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                // Image is a transfer source
                // Make sure any reads from the image have been finished
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                break;

            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                // Image is a transfer destination
                // Make sure any writes to the image have been finished
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                // Image is read by a shader
                // Make sure any shader reads from the image have been finished
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
                break;
            default:
                // Other source layouts aren't handled (yet)
                break;
        }

        // Target layouts (new)
        // Destination access mask controls the dependency for the new image layout
        switch ( newLayout ) {
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                // Image will be used as a transfer destination
                // Make sure any writes to the image have been finished
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                // Image will be used as a transfer source
                // Make sure any reads from the image have been finished
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                break;

            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                // Image will be used as a color attachment
                // Make sure any writes to the color buffer have been finished
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                // Image layout will be used as a depth/stencil attachment
                // Make sure any writes to depth/stencil buffer have been finished
                imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                // Image will be read in a shader (sampler, input attachment)
                // Make sure any writes to the image have been finished
                if ( imageMemoryBarrier.srcAccessMask == 0 ) {
                    imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
                }
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                break;
            default:
                // Other source layouts aren't handled (yet)
                break;
        }

        // Put barrier inside setup command buffer
        vkCmdPipelineBarrier(
                cmd,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &imageMemoryBarrier );

        // Update the current layout
        m_CurrentLayout = newLayout;
    }

    auto VulkanTexture::Initialize() -> void {
        // The case for non-swap chain images
        if ( m_ImageAllocation.Image == VK_NULL_HANDLE ) {

            m_ImageAllocation.ImageCreateInfo = VulkanHelpers::Initializers::ImageCreateInfo();

            const VkExtent3D extent{
                static_cast<UInt32>( m_Width ),
                static_cast<UInt32>( m_Height ),
                1
            };

            m_ImageAllocation.ImageCreateInfo.format = VulkanHelpers::ToVkFormat( m_Format );
            m_ImageAllocation.ImageCreateInfo.extent = extent;

            // This texture is a 2D image always
            m_ImageAllocation.ImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
            m_ImageAllocation.ImageCreateInfo.usage = VulkanHelpers::ToVkImageUsage( m_TextureUsage );

            m_ImageAllocation.ImageCreateInfo.mipLevels = 1;
            m_ImageAllocation.ImageCreateInfo.arrayLayers = 1;
            m_ImageAllocation.ImageCreateInfo.samples = VulkanHelpers::ToVkRasterSamples( m_Multisampling );;
            m_ImageAllocation.ImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            m_ImageAllocation.ImageCreateInfo.initialLayout = m_CurrentLayout;

            // The image will only be used by one queue family:
            // the one that supports transfer operations, often graphics one suffices.
            m_ImageAllocation.ImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            m_ImageAllocation.ImageCreateInfo.flags = 0;

            auto* allocator{ MKT_VMA_ALLOC_PTR(m_Device) };
            if ( const VkResult result{ allocator->AllocateImage( m_ImageAllocation ) }; result != VK_SUCCESS ) {
                MKT_THROW_RUNTIME_ERROR( "VulkanTexture::Initialize - Failed to allocate Vulkan image!" );
            }

            m_SizeBytes = m_ImageSize;

            // This could be a texture that we want to fill from
            // an image loaded from disc or a texture we will write to as a color image
            // for the later we do not want to copy anything to it
            if ( m_Data ) {
                // Allocate staging buffer to copy over the texture data
                BufferDescription stagingDesc{};
                stagingDesc.WithData( nullptr )
                        .WithUsage( BufferUsage::STAGING )
                        .WithSizeBytes( m_ExternalBufferSize == 0 ? m_ImageSize : m_ExternalBufferSize );

                m_StagingBuffer = m_Device->CreateBuffer( stagingDesc );

                m_StagingBuffer->CopyToDevice( m_Data, m_ExternalBufferSize == 0 ? m_ImageSize : m_ExternalBufferSize );

                //Specify optional type operation so we return for instance
                //a command list to be submitted in transfer queue
                CommandListHandle cmd{ m_Device->CreateCommandList( QueueType::TRANSFER_QUEUE, true ) };
                cmd->Begin();

                cmd->FillTexture( m_StagingBuffer.GetRaw(), this );

                cmd->End();
                m_Device->SubmitCommands( cmd );
            }

            // Prepare for view creation
            m_ImageViewCreateInfo = VulkanHelpers::Initializers::ImageViewCreateInfo();
            m_ImageViewCreateInfo.pNext = nullptr;
            m_ImageViewCreateInfo.flags = 0;
            m_ImageViewCreateInfo.image = m_ImageAllocation.Image;
            m_ImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            m_ImageViewCreateInfo.format = m_ImageAllocation.ImageCreateInfo.format;

            VkImageAspectFlags aspectFlags{ VK_IMAGE_ASPECT_COLOR_BIT };
            if ( m_TextureUsage == TextureUsage::DEPTH ) {
                aspectFlags = m_ImageAllocation.ImageCreateInfo.format < VK_FORMAT_D16_UNORM_S8_UINT ? VK_IMAGE_ASPECT_DEPTH_BIT : ( VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT );
            }

            m_ImageViewCreateInfo.subresourceRange.aspectMask = aspectFlags;
            m_ImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
            m_ImageViewCreateInfo.subresourceRange.levelCount = 1;
            m_ImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
            m_ImageViewCreateInfo.subresourceRange.layerCount = 1;
        }

        // the caller can optionally pass a valid image because this VulkanTexture is supposed be
        // usable for the swapchain as well, however, if the latter is the case,
        // we are responsible for releasing the image views, not the actual images.
        if ( vkCreateImageView( VK_DEVICE( m_Device ), std::addressof( m_ImageViewCreateInfo ), nullptr, std::addressof( m_ImageView ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanTexture::Allocate - Failed to create the Vulkan Image View!" );
        }

        SetDebugInfo();

        m_IsAllocated = true;

        // We no longer need it
        m_StagingBuffer.Reset();
    }

    VulkanSwapChain::VulkanSwapChain( const VulkanSwapChainCreateInfo& createInfo )
        : m_Extent{ createInfo.Extent },
          m_Surface{ createInfo.Surface },
          m_IsVsyncEnabled{ createInfo.EnableVsync } {
    }

    auto VulkanSwapChain::Initialize() -> void {
        if ( m_Surface == nullptr ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanSwapChain::Initialize - Error the surface for the swapchain is null." );
        }

        /**
         * [00:11:36] CORE LOG [thread 10211] Validation layer: Validation Error: [ VUID-VkSwapchainCreateInfoKHR-imageExtent-01274 ] Object 0:
         * handle = 0x62e000018450, type = VK_OBJECT_TYPE_DEVICE; | MessageID = 0x7cd0911d | vkCreateSwapchainKHR() called with imageExtent = (1494,921),
         * which is outside the bounds returned by vkGetPhysicalDeviceSurfaceCapabilitiesKHR(): currentExtent = (1495,925), minImageExtent = (1495,925),
         * maxImageExtent = (1495,925). The Vulkan spec states: imageExtent must be between minImageExtent and maxImageExtent, inclusive, where
         * minImageExtent and maxImageExtent are members of the VkSurfaceCapabilitiesKHR structure returned by vkGetPhysicalDeviceSurfaceCapabilitiesKHR
         * for the surface (https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/vkspec.html#VUID-VkSwapchainCreateInfoKHR-imageExtent-01274)
         *
         * this validation error is triggered at times when resizing the main window (GLFW window)
         * */
        CreateSwpChain();

        GetImages();

        m_IsAllocated = true;
    }

    auto VulkanSwapChain::CreateSwpChain() -> void {
        const auto [Capabilities, Formats, PresentModes]{
            VulkanHelpers::GetSwapChainSupport( TO_VK_DEVICE( m_Device )->GetPhysicalDevice(), *m_Surface )
        };

        const auto [format, colorSpace]{ ChooseSurfaceFormat( Formats ) };
        const VkPresentModeKHR presentMode{ ChoosePresentMode( PresentModes ) };
        const VkExtent2D extent{ ChooseExtent( Capabilities ) };

        // Save for later use
        m_Format = format;
        m_PresentMode = presentMode;

        /**
         * We may sometimes have to wait on the driver to complete internal operations
         * before we can acquire another image to render to. Therefore, it is recommended
         * to request at least one more image, hence why we add 1. Likely the image count
         * results in the maximum swap chain image count so we do the check and clamp the resulting image count
         * */
        UInt32 imageCount{ Capabilities.minImageCount + 1 };
        if ( Capabilities.maxImageCount > 0 && imageCount > Capabilities.maxImageCount ) {
            imageCount = Capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{ VulkanHelpers::Initializers::SwapchainCreateInfoKHR() };
        createInfo.surface = *m_Surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = format;
        createInfo.imageColorSpace = colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;

        // Swap chain images are used for drawing or copying to it (in case we render first to a texture and copy it to a swap chain image)
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        const auto& [Present, Graphics, Compute]{
            TO_VK_DEVICE( m_Device )->GetLogicalDeviceQueues()
        };

        // Let swapchain to share images between queues or not. We need to account for it
        // in the case the present queue and the graphics queue are not actually the same
        const std::array queueFamilyIndices{ Graphics->FamilyIndex, Present->FamilyIndex };
        if ( Graphics->FamilyIndex != Present->FamilyIndex ) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = queueFamilyIndices.size();
            createInfo.pQueueFamilyIndices = queueFamilyIndices.data();
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0;
            createInfo.pQueueFamilyIndices = nullptr;
        }

        createInfo.preTransform = Capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;// no blending
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;// TODO: pass old swap chain (need debug currently old swapchain becoming retired which can't be passed here)

        if ( vkCreateSwapchainKHR( VK_DEVICE( m_Device ), std::addressof( createInfo ), nullptr, std::addressof( m_Swapchain ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanSwapChain::CreateSwapChain - Failed to create swap chain." );
        }
    }

    auto VulkanSwapChain::GetImages() -> void {
        static VulkanDevice& device{ *TO_VK_DEVICE( m_Device ) };

        UInt32 imageCount{};

        // We only specified a minimum number of images in the swap chain, even though the implementation is
        // allowed to create a swap chain with more. That's why we'll first query the final number of
        // images with vkGetSwapchainImagesKHR with the last parameter as nullptr, then resize the container and finally call it again to
        // retrieve the handles.
        vkGetSwapchainImagesKHR( device.GetLogicalDevice(), m_Swapchain, std::addressof( imageCount ), nullptr );

        auto images{ std::vector<VkImage>( imageCount ) };
        vkGetSwapchainImagesKHR( device.GetLogicalDevice(), m_Swapchain, std::addressof( imageCount ), images.data() );

        UInt32 imageIndex{ 0 };
        for ( const VkImage image: images ) {
            auto& insertedImg{ m_Images.emplace_back( device.CreateSwapChainTextures( ConstructImgViewInfo( image, m_Format ), m_Extent ) ) };
            insertedImg->SetDebugName( fmt::format( "Swapchain Img - {}", imageIndex++ ) );
        }
    }

    auto VulkanSwapChain::ConstructImgViewInfo( VkImage image, const VkFormat& format ) -> VkImageViewCreateInfo {
        VkImageViewCreateInfo createInfo{ VulkanHelpers::Initializers::ImageViewCreateInfo() };
        createInfo.image = image;
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = format;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.layerCount = 1;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.baseMipLevel = 0;

        return createInfo;
    }

    auto VulkanSwapChain::GetNextRenderableImageIndex( UInt32& imageIndex, const VkSemaphore imageAvailable ) const -> VkResult {
        return vkAcquireNextImageKHR( VK_DEVICE( m_Device ), m_Swapchain, ( std::numeric_limits<UInt64>::max )(), imageAvailable, VK_NULL_HANDLE, std::addressof( imageIndex ) );
    }

    auto VulkanSwapChain::OnResize( const VkExtent2D newDimensions, const bool vsync ) -> void {
        m_Extent = newDimensions;
        m_IsVsyncEnabled = vsync;

        TO_VK_DEVICE( m_Device )->WaitIdle();
        m_Images.clear();

        vkDestroySwapchainKHR( VK_DEVICE( m_Device ), m_Swapchain, nullptr );
        m_OldSwapchain = m_Swapchain;

        m_Images.clear();

        Initialize();
    }

    auto VulkanSwapChain::Present( const UInt32 imageIndex, const VkSemaphore& renderFinished ) const -> VkResult {
        const VulkanDevice& device{ *TO_VK_DEVICE( m_Device ) };

        const std::array swapChains{ m_Swapchain };
        const std::array waitSemaphores{ renderFinished };

        VkPresentInfoKHR presentInfo{ VulkanHelpers::Initializers::PresentInfoKHR() };

        presentInfo.swapchainCount = static_cast<UInt32>(swapChains.size());
        presentInfo.pSwapchains = swapChains.data();

        presentInfo.pImageIndices = &imageIndex;

        // specifies the semaphores to wait for before issuing the present request.
        presentInfo.waitSemaphoreCount = waitSemaphores.size();
        presentInfo.pWaitSemaphores = waitSemaphores.data();

        const auto& [Present, Graphics, Compute]{ device.GetLogicalDeviceQueues() };
        VkQueue presentQueue{ VK_NULL_HANDLE };

        if ( Present.has_value() && Present->Queue != VK_NULL_HANDLE ) {
            presentQueue = Present->Queue;
        } else if ( Graphics.has_value() && Graphics->Queue != VK_NULL_HANDLE ) {
            // Not all hardware guarantees this, some allow graphics queue to be used for presentation
            presentQueue = Graphics->Queue;
        } else {
            MKT_CORE_LOGGER_ERROR( "VulkanSwapChain::Present - No presentation queue available." );
        }

        return vkQueuePresentKHR( presentQueue, std::addressof( presentInfo ) );
    }

    auto VulkanSwapChain::GetImageCount() const -> Size {
        return m_Images.size();
    }

    auto VulkanSwapChain::GetImage( const Size index ) -> TextureHandle {
        return m_Images[index];
    }
    auto VulkanSwapChain::GetExtent() const -> VkExtent2D {
        return m_Extent;
    }

    auto VulkanSwapChain::IsVsyncEnabled() const -> bool {
        return m_IsVsyncEnabled;
    }

    auto VulkanSwapChain::ChooseSurfaceFormat( const std::vector<VkSurfaceFormatKHR>& availableFormats ) -> VkSurfaceFormatKHR {
        // NOTE: if we only have one format, and it is VK_FORMAT_UNDEFINED, it means the surface supports all formats
        for ( const auto& availableFormat: availableFormats ) {
            if ( availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR ) {
                return availableFormat;
            }
        }

        return availableFormats[0];
    }

    auto VulkanSwapChain::ChoosePresentMode( const std::vector<VkPresentModeKHR>& availablePresentModes ) const -> VkPresentModeKHR {
        if ( m_IsVsyncEnabled ) {
            return VK_PRESENT_MODE_FIFO_KHR;
        }

        for ( const auto& availablePresentMode: availablePresentModes ) {
            if ( availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR || availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR ) {
                return availablePresentMode;
            }
        }

        // We return this one because if we do not find the present mode we are looking for,
        // VK_PRESENT_MODE_FIFO_KHR is guaranteed to be always available
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    auto VulkanSwapChain::ChooseExtent( const VkSurfaceCapabilitiesKHR& capabilities ) const -> VkExtent2D {
        // Determine if the currentExtent is set to a special value indicating that the surface size is undefined.
        // This special value is (std::numeric_limits<UInt32_T>::max)(). If the currentExtent width is equal to
        // this value, it means the surface size can be defined by the application, otherwise, the currentExtent
        // provided by the surface capabilities should be used.

        // If capabilities.currentExtent.width is not equal to the maximum unsigned integer, it means the surface
        // size is defined, and you should use currentExtent.
        // If it is equal to the maximum unsigned integer, you need to define the extent yourself within the bounds
        // of minImageExtent and maxImageExtent.

        if ( capabilities.currentExtent.width != ( std::numeric_limits<UInt32>::max )() ) {
            return capabilities.currentExtent;
        }

        const VkExtent2D actualExtent{
            .width{ ( std::max )( capabilities.minImageExtent.width, std::min( capabilities.maxImageExtent.width, m_Extent.width ) ) },
            .height{ ( std::max )( capabilities.minImageExtent.height, std::min( capabilities.maxImageExtent.height, m_Extent.height ) ) },
        };

        return actualExtent;
    }

    VulkanSwapChain::~VulkanSwapChain() {
        if ( m_IsAllocated ) {
            Release();
        }
    }

    auto VulkanSwapChain::Release() -> void {

        // Wait on outstanding queue operations because there might be some objects still in use by the GPU
        TO_VK_DEVICE( m_Device )->WaitIdle();

        m_Surface = nullptr;

        m_Images.clear();

        // Destroy handles
        // The device is owned by the context and is destroyed before the instance and after any object is
        // created from it has finished being used
        vkDestroySwapchainKHR( VK_DEVICE( m_Device ), m_Swapchain, nullptr );

        m_IsAllocated = false;
    }

}// namespace Mikoto