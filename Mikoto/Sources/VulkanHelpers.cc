/**
 * VulkanHelpers.hh
 * Created by kate on 8/5/2023.
 * */

// C++ Standard Library
#include <set>
#include <stdexcept>

// Third-Party Libraries
#include "vk_mem_alloc.h"
#include "volk.h"

// Project Header

#include "Renderer/Vulkan/VulkanContext.hh"
#include "Renderer/Vulkan/VulkanHelpers.hh"

namespace Mikoto::VulkanHelpers {

    // Converts VkImageLayout → readable string
    auto ImageLayoutToString(Texture* texture) -> void {
        const auto src{ dynamic_cast<VulkanTexture*>( texture ) };
        VkImageLayout layout{ src->GetCurrentLayout() };
        const char* layoutName = "UNKNOWN_LAYOUT";

        switch (layout)
        {
            case VK_IMAGE_LAYOUT_UNDEFINED:
                layoutName = "VK_IMAGE_LAYOUT_UNDEFINED"; break;
            case VK_IMAGE_LAYOUT_GENERAL:
                layoutName = "VK_IMAGE_LAYOUT_GENERAL"; break;
            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                layoutName = "VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL"; break;
            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                layoutName = "VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL"; break;
            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
                layoutName = "VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL"; break;
            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                layoutName = "VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL"; break;
            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                layoutName = "VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL"; break;
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                layoutName = "VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL"; break;
            case VK_IMAGE_LAYOUT_PREINITIALIZED:
                layoutName = "VK_IMAGE_LAYOUT_PREINITIALIZED"; break;
            case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
                layoutName = "VK_IMAGE_LAYOUT_PRESENT_SRC_KHR"; break;
            case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
                layoutName = "VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL"; break;
            case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
                layoutName = "VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL"; break;
            case VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL:
                layoutName = "VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL"; break;
            case VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL:
                layoutName = "VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL"; break;
            default:
                layoutName = "UNKNOWN_LAYOUT"; break;
        }

        MKT_CORE_LOGGER_DEBUG( "Image layout: {} for texture: {}", layoutName, texture->GetDebugName() );
    }

    auto GetUniformBufferPadding( const VkDeviceSize bufferOriginalSize, const VkDeviceSize deviceMinOffsetAlignment ) -> VkDeviceSize {
        VkDeviceSize alignedSize{ bufferOriginalSize };

        if ( deviceMinOffsetAlignment > 0 )
            alignedSize = ( alignedSize + deviceMinOffsetAlignment - 1 ) & ~( deviceMinOffsetAlignment - 1 );

        return alignedSize;
    }

    auto InferVulkanIndexType( BufferDataType format ) -> VkIndexType {
        switch ( format ) {
            case BufferDataType::BUFFER_DATA_UINT16:
                return VK_INDEX_TYPE_UINT16;
            case BufferDataType::BUFFER_DATA_UINT32:
                return VK_INDEX_TYPE_UINT32;
            default:
                MKT_CORE_LOGGER_ERROR( "VulkanHelpers::InferVulkanIndexType - Invalid index type." );
                return VK_INDEX_TYPE_MAX_ENUM;
        }

        return VK_INDEX_TYPE_MAX_ENUM;
    }

    auto ImageUsageFlagsToString(Texture* texture) -> void {
        const auto src{ dynamic_cast<VulkanTexture*>( texture ) };
        VkImageUsageFlags flags{ src->GetCreateInfo().usage };

        std::ostringstream oss;
        bool first = true;

        auto append = [&](const char* name) {
            if (!first) oss << " | ";
            oss << name;
            first = false;
        };

        if (flags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) append("TRANSFER_SRC");
        if (flags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) append("TRANSFER_DST");
        if (flags & VK_IMAGE_USAGE_SAMPLED_BIT) append("SAMPLED");
        if (flags & VK_IMAGE_USAGE_STORAGE_BIT) append("STORAGE");
        if (flags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) append("COLOR_ATTACHMENT");
        if (flags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) append("DEPTH_STENCIL_ATTACHMENT");
        if (flags & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT) append("TRANSIENT_ATTACHMENT");
        if (flags & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT) append("INPUT_ATTACHMENT");
        if (flags & VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR) append("FRAGMENT_SHADING_RATE_ATTACHMENT_KHR");
        if (flags & VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT) append("FRAGMENT_DENSITY_MAP_EXT");

        if (first)
            oss << "NONE";

        MKT_CORE_LOGGER_DEBUG( "Image usage flags: {} for texture: {}", oss.str(), texture->GetDebugName() );
    }

    auto ToVkImageUsage( TextureUsage usage ) -> VkImageUsageFlags {
        switch ( usage ) {
            case TextureUsage::TEXTURE_USAGE_COLOR:
                // Textures that I will print to and can copy them to toehr textures like the color image from ImGui Backend
                return VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;// | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

            case TextureUsage::TEXTURE_USAGE_DEPTH:
                return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                       VK_IMAGE_USAGE_SAMPLED_BIT |
                       VK_IMAGE_USAGE_TRANSFER_DST_BIT;

            case TextureUsage::TEXTURE_USAGE_NORMAL:
                // For example textures that I load from disc
                return VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

            case TextureUsage::TEXTURE_USAGE_STORAGE:
                return VK_IMAGE_USAGE_STORAGE_BIT |
                       VK_IMAGE_USAGE_SAMPLED_BIT |
                       VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                       VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

            case TextureUsage::TEXTURE_USAGE_CUBE:
                return VK_IMAGE_USAGE_SAMPLED_BIT |
                       VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                       VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

            case TextureUsage::TEXTURE_USAGE_RENDER_TARGET:
                return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                       VK_IMAGE_USAGE_SAMPLED_BIT |
                       VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                       VK_IMAGE_USAGE_TRANSFER_DST_BIT;

            default:;
        }

        return VK_IMAGE_USAGE_SAMPLED_BIT;
    }

    auto SetupDeviceQueueCreateInfo( const std::set<UInt32>& uniqueQueueFamilies ) -> std::vector<VkDeviceQueueCreateInfo> {
        std::vector<VkDeviceQueueCreateInfo> result{};

        // static because pQueuePriorities is a pointer to a float
        static constexpr float queuePriority{ 1.0f };
        for ( const UInt32 queueFamily: uniqueQueueFamilies ) {
            VkDeviceQueueCreateInfo queueCreateInfo{ Initializers::DeviceQueueCreateInfo() };

            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = std::addressof( queuePriority );

            result.push_back( queueCreateInfo );
        }

        return result;
    }

    auto GetVkStageFromShaderStage( const ShaderStage stage ) -> VkShaderStageFlagBits {
        switch ( stage ) {
            case ShaderStage::VERTEX_STAGE:
                return VK_SHADER_STAGE_VERTEX_BIT;
            case ShaderStage::COMPUTE_STAGE:
                return VK_SHADER_STAGE_COMPUTE_BIT;
            case ShaderStage::FRAGMENT_STAGE:
                return VK_SHADER_STAGE_FRAGMENT_BIT;
            default:
                return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
        }
    }

    auto ToVkFormat( TextureFormat format ) -> VkFormat {
        switch ( format ) {
            case TextureFormat::TEXTURE_FORMAT_RGBA8_SNORM:
                return VK_FORMAT_R8G8B8A8_SNORM;
            case TextureFormat::TEXTURE_FORMAT_R8_UNORM:
                return VK_FORMAT_R8_UNORM;
            case TextureFormat::TEXTURE_FORMAT_RG8_UNORM:
                return VK_FORMAT_R8G8_UNORM;
            case TextureFormat::TEXTURE_FORMAT_RGB8_UNORM:
                return VK_FORMAT_R8G8B8_UNORM;
            case TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM:
                return VK_FORMAT_R8G8B8A8_UNORM;
            case TextureFormat::TEXTURE_FORMAT_RG8_SNORM:
                return VK_FORMAT_R8G8B8_SNORM;
            case TextureFormat::TEXTURE_FORMAT_SRGB8:
                return VK_FORMAT_R8G8B8_SRGB;
            case TextureFormat::TEXTURE_FORMAT_SRGB8_ALPHA8:
                return VK_FORMAT_R8G8B8A8_SRGB;
            case TextureFormat::TEXTURE_FORMAT_R16_FLOAT:
                return VK_FORMAT_R16_SFLOAT;
            case TextureFormat::TEXTURE_FORMAT_RG16_FLOAT:
                return VK_FORMAT_R16G16_SFLOAT;
            case TextureFormat::TEXTURE_FORMAT_RGB16_FLOAT:
                return VK_FORMAT_R16G16B16_SFLOAT;
            case TextureFormat::TEXTURE_FORMAT_RGBA16_FLOAT:
                return VK_FORMAT_R16G16B16A16_SFLOAT;
            case TextureFormat::TEXTURE_FORMAT_R32_FLOAT:
                return VK_FORMAT_R32_SFLOAT;
            case TextureFormat::TEXTURE_FORMAT_RG32_FLOAT:
                return VK_FORMAT_R32G32_SFLOAT;
            case TextureFormat::TEXTURE_FORMAT_RGB32_FLOAT:
                return VK_FORMAT_R32G32B32_SFLOAT;
            case TextureFormat::TEXTURE_FORMAT_RGBA32_FLOAT:
                return VK_FORMAT_R32G32B32A32_SFLOAT;
            case TextureFormat::TEXTURE_FORMAT_D16_UNORM:
                return VK_FORMAT_D16_UNORM;
            case TextureFormat::TEXTURE_FORMAT_D24_UNORM_S8_UINT:
                return VK_FORMAT_D24_UNORM_S8_UINT;
            case TextureFormat::TEXTURE_FORMAT_D32_FLOAT:
                return VK_FORMAT_D32_SFLOAT;
            case TextureFormat::TEXTURE_FORMAT_D32_FLOAT_S8_UINT:
                return VK_FORMAT_D32_SFLOAT_S8_UINT;
            default:
                return VK_FORMAT_UNDEFINED;
        }
    }

    auto ToMikotoFormat( VkFormat format ) -> TextureFormat {
        switch ( format ) {
            // --- 8-bit normalized ---
            case VK_FORMAT_R8_UNORM:
                return TextureFormat::TEXTURE_FORMAT_R8_UNORM;
            case VK_FORMAT_R8G8_UNORM:
                return TextureFormat::TEXTURE_FORMAT_RG8_UNORM;
            case VK_FORMAT_R8G8B8_UNORM:
                return TextureFormat::TEXTURE_FORMAT_RGB8_UNORM;
            case VK_FORMAT_R8G8B8A8_UNORM:
                return TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM;

            // --- 8-bit signed normalized ---
            case VK_FORMAT_R8_SNORM:
                return TextureFormat::TEXTURE_FORMAT_R8_SNORM;
            case VK_FORMAT_R8G8_SNORM:
                return TextureFormat::TEXTURE_FORMAT_RG8_SNORM;
            case VK_FORMAT_R8G8B8_SNORM:
                return TextureFormat::TEXTURE_FORMAT_RGB8_SNORM;
            case VK_FORMAT_R8G8B8A8_SNORM:
                return TextureFormat::TEXTURE_FORMAT_RGBA8_SNORM;

            // --- 16-bit normalized ---
            case VK_FORMAT_R16_UNORM:
                return TextureFormat::TEXTURE_FORMAT_R16_UNORM;
            case VK_FORMAT_R16G16_UNORM:
                return TextureFormat::TEXTURE_FORMAT_RG16_UNORM;
            case VK_FORMAT_R16G16B16_UNORM:
                return TextureFormat::TEXTURE_FORMAT_RGB16_UNORM;
            case VK_FORMAT_R16G16B16A16_UNORM:
                return TextureFormat::TEXTURE_FORMAT_RGBA16_UNORM;

            // --- 16-bit float ---
            case VK_FORMAT_R16_SFLOAT:
                return TextureFormat::TEXTURE_FORMAT_R16_FLOAT;
            case VK_FORMAT_R16G16_SFLOAT:
                return TextureFormat::TEXTURE_FORMAT_RG16_FLOAT;
            case VK_FORMAT_R16G16B16_SFLOAT:
                return TextureFormat::TEXTURE_FORMAT_RGB16_FLOAT;
            case VK_FORMAT_R16G16B16A16_SFLOAT:
                return TextureFormat::TEXTURE_FORMAT_RGBA16_FLOAT;

            // --- 32-bit float ---
            case VK_FORMAT_R32_SFLOAT:
                return TextureFormat::TEXTURE_FORMAT_R32_FLOAT;
            case VK_FORMAT_R32G32_SFLOAT:
                return TextureFormat::TEXTURE_FORMAT_RG32_FLOAT;
            case VK_FORMAT_R32G32B32_SFLOAT:
                return TextureFormat::TEXTURE_FORMAT_RGB32_FLOAT;
            case VK_FORMAT_R32G32B32A32_SFLOAT:
                return TextureFormat::TEXTURE_FORMAT_RGBA32_FLOAT;

            // --- sRGB ---
            case VK_FORMAT_R8G8B8_SRGB:
                return TextureFormat::TEXTURE_FORMAT_SRGB8;
            case VK_FORMAT_R8G8B8A8_SRGB:
                return TextureFormat::TEXTURE_FORMAT_SRGB8_ALPHA8;

            // --- Depth / Stencil ---
            case VK_FORMAT_D16_UNORM:
                return TextureFormat::TEXTURE_FORMAT_D16_UNORM;
            case VK_FORMAT_D24_UNORM_S8_UINT:
                return TextureFormat::TEXTURE_FORMAT_D24_UNORM_S8_UINT;
            case VK_FORMAT_D32_SFLOAT:
                return TextureFormat::TEXTURE_FORMAT_D32_FLOAT;
            case VK_FORMAT_D32_SFLOAT_S8_UINT:
                return TextureFormat::TEXTURE_FORMAT_D32_FLOAT_S8_UINT;

            default:;
        }

        return TextureFormat::TEXTURE_FORMAT_INVALID;
    }

    auto GetVkFormatFromTextureFormat( TextureFormat format, TextureUsage usage, VkPhysicalDevice device ) -> VkFormat {
        VkFormat result{ ToVkFormat( format ) };

        const std::initializer_list<const VkFormat> targetColorFormats{
            VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_D32_SFLOAT, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SRGB
        };

        const std::initializer_list<const VkFormat> targetDepthFormats{
            VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT
        };

        if ( usage == TextureUsage::TEXTURE_USAGE_COLOR ) {
            result = FindSupportedFormat(
                    device,
                    targetColorFormats,
                    VK_IMAGE_TILING_OPTIMAL,
                    VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT );
        }

        if ( usage == TextureUsage::TEXTURE_USAGE_DEPTH ) {
            result = FindSupportedFormat(
                    device,
                    targetDepthFormats,
                    VK_IMAGE_TILING_OPTIMAL,
                    VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT );
        }

        return result;
    }

    auto GetSwapChainSupport( const VkPhysicalDevice& device, const VkSurfaceKHR& surface ) -> SwapChainSupportDetails {
        SwapChainSupportDetails details{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR( device, surface, std::addressof( details.Capabilities ) );

        UInt32 formatCount{};
        vkGetPhysicalDeviceSurfaceFormatsKHR( device, surface, std::addressof( formatCount ), nullptr );
        if ( formatCount != 0 ) {
            details.Formats.resize( formatCount );
            vkGetPhysicalDeviceSurfaceFormatsKHR( device, surface, std::addressof( formatCount ), details.Formats.data() );
        }

        UInt32 presentModeCount{};
        vkGetPhysicalDeviceSurfacePresentModesKHR( device, surface, std::addressof( presentModeCount ), nullptr );
        if ( presentModeCount != 0 ) {
            details.PresentModes.resize( presentModeCount );
            vkGetPhysicalDeviceSurfacePresentModesKHR( device, surface, std::addressof( presentModeCount ), details.PresentModes.data() );
        }

        return details;
    }

    auto FindSupportedFormat( VkPhysicalDevice device, std::span<const VkFormat> candidates, VkImageTiling tiling, VkFormatFeatureFlags features ) -> VkFormat {
        for ( const VkFormat format: candidates ) {
            VkFormatProperties props{};
            vkGetPhysicalDeviceFormatProperties( device, format, std::addressof( props ) );

            if ( ( tiling == VK_IMAGE_TILING_LINEAR && ( props.linearTilingFeatures & features ) == features ) ||
                 ( tiling == VK_IMAGE_TILING_OPTIMAL && ( props.optimalTilingFeatures & features ) == features ) ) {
                return format;
            }
        }

        return VK_FORMAT_UNDEFINED;
    }

    auto HasGraphicsQueue( const VkQueueFamilyProperties& queueFamily ) -> bool {
        return queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT;
    }

    auto HasComputeQueue( const VkQueueFamilyProperties& queueFamily ) -> bool {
        return queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT;
    }

    auto HasPresentQueue( const VkPhysicalDevice& device, const UInt32 queueFamilyIndex, const VkSurfaceKHR& surface, const VkQueueFamilyProperties& queueFamilyProperties ) -> bool {
        VkBool32 presentSupport{ VK_FALSE };
        if ( vkGetPhysicalDeviceSurfaceSupportKHR( device, queueFamilyIndex, surface, std::addressof( presentSupport ) ) != VK_SUCCESS ) {
            MKT_CORE_LOGGER_ERROR( "VulkanHelpers::HasPresentQueue - Failed to get Physical device surface support." );
            return false;
        }

        // Has present queues and at least one of them available
        if ( queueFamilyProperties.queueCount > 0 && presentSupport == VK_TRUE ) {
            return true;
        }

        return false;
    }

    auto GetVulkanAttributeDataType( ShaderDataType type ) -> VkFormat {
        switch ( type ) {
            case ShaderDataType::FLOAT_TYPE:
                return VK_FORMAT_R32_SFLOAT;
            case ShaderDataType::FLOAT2_TYPE:
                return VK_FORMAT_R32G32_SFLOAT;
            case ShaderDataType::FLOAT3_TYPE:
                return VK_FORMAT_R32G32B32_SFLOAT;
            case ShaderDataType::FLOAT4_TYPE:
                return VK_FORMAT_R32G32B32A32_SFLOAT;

            case ShaderDataType::MAT3_TYPE:
            case ShaderDataType::MAT4_TYPE:
                return VK_FORMAT_UNDEFINED;//temporary

            case ShaderDataType::INT_TYPE:
                return VK_FORMAT_R32_SINT;
            case ShaderDataType::INT2_TYPE:
                return VK_FORMAT_R32G32_SINT;
            case ShaderDataType::INT3_TYPE:
                return VK_FORMAT_R32G32B32_SINT;
            case ShaderDataType::INT4_TYPE:
                return VK_FORMAT_R32G32B32A32_SINT;
            case ShaderDataType::BOOL_TYPE:
                return VK_FORMAT_R32_SINT;

            case ShaderDataType::NONE:
            case ShaderDataType::COUNT:
                [[fallthrough]];
            default:
                MKT_ASSERT( false, "Invalid shader data type" );
        }
    }

    auto CopyImageToImage( const VkCommandBuffer cmd, const VkImage source, const VkImage destination, const VkExtent3D imageSize ) -> void {
        VkImageBlit2 blitRegion{};
        blitRegion.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
        blitRegion.pNext = nullptr;

        blitRegion.srcOffsets[1].x = imageSize.width;
        blitRegion.srcOffsets[1].y = imageSize.height;
        blitRegion.srcOffsets[1].z = 1;

        blitRegion.dstOffsets[1].x = imageSize.width;
        blitRegion.dstOffsets[1].y = imageSize.height;
        blitRegion.dstOffsets[1].z = 1;

        blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blitRegion.srcSubresource.baseArrayLayer = 0;
        blitRegion.srcSubresource.layerCount = 1;
        blitRegion.srcSubresource.mipLevel = 0;

        blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blitRegion.dstSubresource.baseArrayLayer = 0;
        blitRegion.dstSubresource.layerCount = 1;
        blitRegion.dstSubresource.mipLevel = 0;

        VkBlitImageInfo2 blitInfo{};
        blitInfo.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
        blitInfo.pNext = nullptr;
        blitInfo.dstImage = destination;
        blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        blitInfo.srcImage = source;
        blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        blitInfo.filter = VK_FILTER_NEAREST;
        blitInfo.regionCount = 1;
        blitInfo.pRegions = &blitRegion;

        vkCmdBlitImage2( cmd, &blitInfo );
    }
}// namespace Mikoto::VulkanHelpers