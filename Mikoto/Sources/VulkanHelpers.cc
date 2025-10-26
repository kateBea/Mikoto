/**
 * VulkanHelpers.hh
 * Created by kate on 8/5/2023.
 * */

// C++ Standard Library
#include <set>
#include <stdexcept>

// Third-Party Libraries
#include <spirv_reflect.h>

#include "vk_mem_alloc.h"
#include "volk.h"

// Project Header

#include <Renderer/Vulkan/VulkanDevice.hh>
#include <Renderer/Vulkan/VulkanRenderer.hh>

#include "Renderer/Vulkan/VulkanContext.hh"
#include "Renderer/Vulkan/VulkanHelpers.hh"

namespace Mikoto::VulkanHelpers {

    // Converts VkImageLayout → readable string
    auto ImageLayoutToString( Texture* texture ) -> void {
        const auto src{ dynamic_cast<VulkanTexture*>( texture ) };
        VkImageLayout layout{ src->GetCurrentLayout() };
        const char* layoutName = "UNKNOWN_LAYOUT";

        switch ( layout ) {
            case VK_IMAGE_LAYOUT_UNDEFINED:
                layoutName = "VK_IMAGE_LAYOUT_UNDEFINED";
                break;
            case VK_IMAGE_LAYOUT_GENERAL:
                layoutName = "VK_IMAGE_LAYOUT_GENERAL";
                break;
            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                layoutName = "VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL";
                break;
            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                layoutName = "VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL";
                break;
            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
                layoutName = "VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL";
                break;
            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                layoutName = "VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL";
                break;
            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                layoutName = "VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL";
                break;
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                layoutName = "VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL";
                break;
            case VK_IMAGE_LAYOUT_PREINITIALIZED:
                layoutName = "VK_IMAGE_LAYOUT_PREINITIALIZED";
                break;
            case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
                layoutName = "VK_IMAGE_LAYOUT_PRESENT_SRC_KHR";
                break;
            case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
                layoutName = "VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL";
                break;
            case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
                layoutName = "VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL";
                break;
            case VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL:
                layoutName = "VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL";
                break;
            case VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL:
                layoutName = "VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL";
                break;
            default:
                layoutName = "UNKNOWN_LAYOUT";
                break;
        }

        MKT_CORE_LOGGER_DEBUG( "Image layout: {} for texture: {}", layoutName, texture->GetDebugName() );
    }
    auto VkResultToString( VkResult result ) -> const char* {
        switch (result) {
            case VK_SUCCESS: return "VK_SUCCESS";
            case VK_NOT_READY: return "VK_NOT_READY";
            case VK_TIMEOUT: return "VK_TIMEOUT";
            case VK_EVENT_SET: return "VK_EVENT_SET";
            case VK_EVENT_RESET: return "VK_EVENT_RESET";
            case VK_INCOMPLETE: return "VK_INCOMPLETE";
            case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
            case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
            case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
            case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
            case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
            case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
            case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
            case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
            case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
            case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
            case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
            case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
            case VK_ERROR_UNKNOWN: return "VK_ERROR_UNKNOWN";
            default: return "UNKNOWN_VK_RESULT";
        }
    }

    auto GetUniformBufferPadding( const VkDeviceSize bufferOriginalSize, const VkDeviceSize deviceMinOffsetAlignment ) -> VkDeviceSize {
        VkDeviceSize alignedSize{ bufferOriginalSize };

        if ( deviceMinOffsetAlignment > 0 )
            alignedSize = ( alignedSize + deviceMinOffsetAlignment - 1 ) & ~( deviceMinOffsetAlignment - 1 );

        return alignedSize;
    }

    auto InferVulkanIndexType( const BufferDataType format ) -> VkIndexType {
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

    auto ImageUsageFlagsToString( Texture* texture ) -> void {
        const auto src{ dynamic_cast<VulkanTexture*>( texture ) };
        VkImageUsageFlags flags{ src->GetCreateInfo().usage };

        std::ostringstream oss;
        bool first = true;

        auto append = [&]( const char* name ) {
            if ( !first ) oss << " | ";
            oss << name;
            first = false;
        };

        if ( flags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT ) append( "TRANSFER_SRC" );
        if ( flags & VK_IMAGE_USAGE_TRANSFER_DST_BIT ) append( "TRANSFER_DST" );
        if ( flags & VK_IMAGE_USAGE_SAMPLED_BIT ) append( "SAMPLED" );
        if ( flags & VK_IMAGE_USAGE_STORAGE_BIT ) append( "STORAGE" );
        if ( flags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT ) append( "COLOR_ATTACHMENT" );
        if ( flags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT ) append( "DEPTH_STENCIL_ATTACHMENT" );
        if ( flags & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT ) append( "TRANSIENT_ATTACHMENT" );
        if ( flags & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT ) append( "INPUT_ATTACHMENT" );
        if ( flags & VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR ) append( "FRAGMENT_SHADING_RATE_ATTACHMENT_KHR" );
        if ( flags & VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT ) append( "FRAGMENT_DENSITY_MAP_EXT" );

        if ( first )
            oss << "NONE";

        MKT_CORE_LOGGER_DEBUG( "Image usage flags: {} for texture: {}", oss.str(), texture->GetDebugName() );
    }

    auto ToVkImageUsage( const TextureUsage usage ) -> VkImageUsageFlags {
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

    auto ToVkStage( const ShaderStage stage ) -> VkShaderStageFlagBits {
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

    auto ToVkFormat( const TextureFormat format ) -> VkFormat {
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

    auto ToTextureFormat( VkFormat format ) -> TextureFormat {
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

    auto ToVkShaderDataType( const ShaderDataType type ) -> VkFormat {
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

namespace Mikoto::VulkanHelpers::Reflection {
    auto ToVkDescriptorType( SpvReflectDescriptorType type ) -> VkDescriptorType {
        switch ( type ) {
            case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
                return VK_DESCRIPTOR_TYPE_SAMPLER;
            case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
                return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
            case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
                return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
            case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
                return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
#if defined( VK_KHR_acceleration_structure )
            case SPV_REFLECT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
                return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
#endif
            default:
                return VK_DESCRIPTOR_TYPE_MAX_ENUM;
        }
    }

    auto MergePushConstantRange( std::vector<VkPushConstantRange>& dst, const UInt32 targetOffset, const UInt32 targetSize, const VkShaderStageFlags flags ) -> void {
        for ( auto& [stageFlags, offset, size]: dst ) {
            if ( offset == targetOffset && size == targetSize ) {
                stageFlags |= flags;
                return;
            }
        }

        dst.push_back( VkPushConstantRange{ flags, targetOffset, targetSize } );
    }

    MKT_NODISCARD static auto IsBindlessEnabled() -> bool {
#if defined(MKT_USE_VULKAN_BINDLESS)
        return true;
#else
        return false;
#endif

    }

    auto ReflectSPIRV(VkDevice device, const std::vector<std::vector<UInt32>>& spirvModules, ReflectedData& out) -> VkResult {
        out = {};

        std::map<UInt32, std::unordered_map<UInt32, VkDescriptorSetLayoutBinding>> sets{};
        std::vector<VkPushConstantRange> pushConstants;

        for ( const auto& moduleData: spirvModules ) {
            if ( moduleData.empty() ) {
                continue;
            }

            SpvReflectShaderModule mod{};
            if ( spvReflectCreateShaderModule( moduleData.size() * sizeof( UInt32 ), moduleData.data(), &mod ) != SPV_REFLECT_RESULT_SUCCESS )
                return VK_ERROR_INITIALIZATION_FAILED;

            // The reason why we do this cast is that they are a map 1 to 1 (the enums values)
            auto stage{ static_cast<VkShaderStageFlagBits>( mod.shader_stage) };

            // === DESCRIPTOR SETS ===
            UInt32 setCount{};
            spvReflectEnumerateDescriptorSets( &mod, &setCount, nullptr );
            std::vector<SpvReflectDescriptorSet*> reflSets( setCount );
            spvReflectEnumerateDescriptorSets( &mod, &setCount, reflSets.data() );

            for ( auto* reflectDs: reflSets ) {
                for ( UInt32 setBinding{}; setBinding < reflectDs->binding_count; ++setBinding ) {
                    auto* reflectedBinding = reflectDs->bindings[setBinding];
                    UInt32 set{ reflectDs->set };
                    auto& bindingMap{ sets[set] };

                    if ( auto it{ bindingMap.find( reflectedBinding->binding ) }; it == bindingMap.end() ) {
                        VkDescriptorSetLayoutBinding bindingInfo{};
                        bindingInfo.binding = reflectedBinding->binding;
                        bindingInfo.descriptorType = ToVkDescriptorType( reflectedBinding->descriptor_type );

                        if (IsBindlessEnabled() && bindingInfo.descriptorType == SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER && reflectedBinding->type_description->op == SpvOpTypeRuntimeArray || reflectedBinding->type_description->op == SpvOpTypeArray ) {
                            bindingInfo.descriptorCount = std::max( 1u, VulkanRenderer::GetMaxBindlessTextureCount() );

                        } else {
                            bindingInfo.descriptorCount = std::max( 1u, reflectedBinding->count );
                        }

                        bindingInfo.stageFlags = stage;
                        bindingMap[bindingInfo.binding] = bindingInfo;

                        out.bindingMap[{ set, bindingInfo.binding }] = ReflectedBindingInfo{
                            reflectedBinding->name, set, bindingInfo.binding, bindingInfo.descriptorType, bindingInfo.descriptorCount, static_cast<VkShaderStageFlags>( stage)
                        };
                    } else {
                        it->second.stageFlags |= stage;
                        out.bindingMap[{ reflectDs->set, it->second.binding }].stageFlags |= stage;
                    }
                }
            }

            // === PUSH CONSTANTS ===
            UInt32 pcCount{};
            spvReflectEnumeratePushConstantBlocks( &mod, &pcCount, nullptr );
            std::vector<SpvReflectBlockVariable*> pcs( pcCount );
            spvReflectEnumeratePushConstantBlocks( &mod, &pcCount, pcs.data() );
            for ( auto* pc: pcs )
                MergePushConstantRange( pushConstants, pc->offset, pc->size, stage );

            // === VERTEX INPUTS (vertex stage only) ===
            if ( stage == VK_SHADER_STAGE_VERTEX_BIT ) {
                UInt32 inputCount{};
                spvReflectEnumerateInputVariables( &mod, &inputCount, nullptr );
                std::vector<SpvReflectInterfaceVariable*> inputs( inputCount );
                spvReflectEnumerateInputVariables( &mod, &inputCount, inputs.data() );

                // Collect attributes
                UInt32 binding{};
                for ( auto* v: inputs ) {
                    if ( v->decoration_flags & SPV_REFLECT_DECORATION_BUILT_IN ) {
                        // skip built-ins like gl_VertexIndex
                        continue;
                    }

                    VkVertexInputAttributeDescription attr{};
                    attr.binding = binding;
                    attr.location = v->location;
                    attr.offset = 0;

                    switch ( v->format ) {
                        case SPV_REFLECT_FORMAT_R32_SFLOAT:
                            attr.format = VK_FORMAT_R32_SFLOAT;
                            break;
                        case SPV_REFLECT_FORMAT_R32G32_SFLOAT:
                            attr.format = VK_FORMAT_R32G32_SFLOAT;
                            break;
                        case SPV_REFLECT_FORMAT_R32G32B32_SFLOAT:
                            attr.format = VK_FORMAT_R32G32B32_SFLOAT;
                            break;
                        case SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT:
                            attr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
                            break;
                        default:
                            attr.format = VK_FORMAT_UNDEFINED;
                            break;
                    }

                    out.vertexAttributes.push_back( attr );
                }

                if ( !out.vertexAttributes.empty() ) {
                    VkVertexInputBindingDescription bind{};
                    bind.binding = 0;
                    bind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

                    // user-defined later
                    // SPIR-V reflection can tell us which vertex attributes the shader expects
                    // (e.g. positions, normals, UVs, etc.), along with their formats and offsets,
                    // but it CANNOT tell us the total stride (the size of one vertex in bytes).
                    bind.stride = 0;

                    out.vertexBindings.push_back( bind );
                }
            }

            spvReflectDestroyShaderModule( &mod );
        }

        // === CREATE DESCRIPTOR SET LAYOUTS ===
        for (auto& [setIdx, bindings] : sets) {
            std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
            layoutBindings.reserve(bindings.size());

            for (auto& [_, b] : bindings) {
                layoutBindings.push_back(b);
            }

            VkDescriptorSetLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
            layoutInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
            layoutInfo.pBindings = layoutBindings.data();

            std::vector<VkDescriptorBindingFlags> bindingFlags(layoutBindings.size(), 0);

            // Attach the binding flags info via pNext
            VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{};
            flagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
            flagsInfo.bindingCount = static_cast<UInt32>(bindingFlags.size());
            flagsInfo.pBindingFlags = bindingFlags.data();


            if (IsBindlessEnabled()) {
                // Enable bindless flags for each binding
                UInt8 index{ 0 };
                for (auto& flags : bindingFlags) {

                    // Image samplers are in the form of array by convention
                    // we dont bind them one by one but instead provide an array of combined image samplers
                    if (bindings[index++].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
                        flags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
                            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                           VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
                    } else {
                        flags = 0;
                    }
                }

                layoutInfo.pNext = &flagsInfo;
                layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
            }

            VkDescriptorSetLayout layoutHandle{};
            if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &layoutHandle) != VK_SUCCESS) {
                return VK_ERROR_INITIALIZATION_FAILED;
            }

            out.setLayouts.push_back(layoutHandle);
        }

        // === PIPELINE LAYOUT ===
        VkPipelineLayoutCreateInfo plInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };

        plInfo.setLayoutCount = static_cast<UInt32>( out.setLayouts.size() );
        plInfo.pSetLayouts = out.setLayouts.data();

        plInfo.pushConstantRangeCount = static_cast<uint32_t>( pushConstants.size() );
        plInfo.pPushConstantRanges = pushConstants.data();

        if ( vkCreatePipelineLayout( device, &plInfo, nullptr, &out.pipelineLayout ) != VK_SUCCESS )
            return VK_ERROR_INITIALIZATION_FAILED;

        out.pushConstantRanges = std::move( pushConstants );

        return VK_SUCCESS;
    }

    auto DestroyReflectedPipeline( const VkDevice device, ReflectedData& reflected ) -> void {
        if ( reflected.pipelineLayout ) {
            vkDestroyPipelineLayout( device, reflected.pipelineLayout, nullptr );
            reflected.pipelineLayout = VK_NULL_HANDLE;
        }

        for ( const auto dsLayout: reflected.setLayouts ) {
            vkDestroyDescriptorSetLayout( device, dsLayout, nullptr );
        }

        reflected.setLayouts.clear();
        reflected.vertexBindings.clear();
        reflected.vertexAttributes.clear();
        reflected.bindingMap.clear();
        reflected.pushConstantRanges.clear();
    }

}