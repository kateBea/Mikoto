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

    auto GetUniformBufferPadding( const VkDeviceSize bufferOriginalSize, const VkDeviceSize deviceMinOffsetAlignment) -> VkDeviceSize {
        VkDeviceSize alignedSize{ bufferOriginalSize };

        if (deviceMinOffsetAlignment > 0)
            alignedSize = (alignedSize + deviceMinOffsetAlignment - 1) & ~(deviceMinOffsetAlignment - 1);

        return alignedSize;
    }

    auto SetupDeviceQueueCreateInfo(const std::set<UInt32_T>& uniqueQueueFamilies) -> std::vector<VkDeviceQueueCreateInfo> {
        std::vector<VkDeviceQueueCreateInfo> result{};

        // static because pQueuePriorities is a pointer to a float
        static constexpr float queuePriority{ 1.0f };
        for ( const UInt32_T queueFamily : uniqueQueueFamilies ) {
            VkDeviceQueueCreateInfo queueCreateInfo{ Initializers::DeviceQueueCreateInfo() };

            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = std::addressof(queuePriority);

            result.push_back( queueCreateInfo );
        }

        return result;
    }

    auto GetVkStageFromShaderStage( const ShaderStage stage) -> VkShaderStageFlagBits {
        switch (stage) {
            case VERTEX_STAGE:
                return VK_SHADER_STAGE_VERTEX_BIT;
            case COMPUTE_STAGE:
                return VK_SHADER_STAGE_COMPUTE_BIT;
            case FRAGMENT_STAGE:
                return VK_SHADER_STAGE_FRAGMENT_BIT;
            case GEOMETRY_STAGE:
            case TESSELATION_STAGE:
            default:
                return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
        }
    }

    auto GetSwapChainSupport( const VkPhysicalDevice& device, const VkSurfaceKHR& surface ) -> SwapChainSupportDetails {
        SwapChainSupportDetails details{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR( device, surface, std::addressof( details.Capabilities ) );

        UInt32_T formatCount{};
        vkGetPhysicalDeviceSurfaceFormatsKHR( device, surface, std::addressof( formatCount ), nullptr );
        if ( formatCount != 0 ) {
            details.Formats.resize( formatCount );
            vkGetPhysicalDeviceSurfaceFormatsKHR( device, surface, std::addressof( formatCount ), details.Formats.data() );
        }

        UInt32_T presentModeCount{};
        vkGetPhysicalDeviceSurfacePresentModesKHR( device, surface, std::addressof( presentModeCount ), nullptr );
        if ( presentModeCount != 0 ) {
            details.PresentModes.resize( presentModeCount );
            vkGetPhysicalDeviceSurfacePresentModesKHR( device, surface, std::addressof( presentModeCount ), details.PresentModes.data() );
        }

        return details;
    }

    auto HasGraphicsQueue( const VkQueueFamilyProperties& queueFamily ) -> bool {
        return queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT;
    }

    auto HasComputeQueue( const VkQueueFamilyProperties& queueFamily ) -> bool {
        return queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT;
    }

    auto HasPresentQueue( const VkPhysicalDevice& device, const UInt32_T queueFamilyIndex,  const VkSurfaceKHR& surface, const VkQueueFamilyProperties& queueFamilyProperties  ) -> bool {
        VkBool32 presentSupport{ VK_FALSE };
        if (vkGetPhysicalDeviceSurfaceSupportKHR( device, queueFamilyIndex, surface, std::addressof( presentSupport ) ) != VK_SUCCESS) {
            MKT_CORE_LOGGER_ERROR( "VulkanHelpers::HasPresentQueue - Failed to get Physical device surface support." );
            return false;
        }

        // Has present queues and at least one of them available
        if ( queueFamilyProperties.queueCount > 0 && presentSupport == VK_TRUE ) {
            return true;
        }

        return false;
    }

    auto GetVulkanAttributeDataType(ShaderDataType type) -> VkFormat {
        switch(type) {
            case ShaderDataType::FLOAT_TYPE: return VK_FORMAT_R32_SFLOAT;
            case ShaderDataType::FLOAT2_TYPE: return VK_FORMAT_R32G32_SFLOAT;
            case ShaderDataType::FLOAT3_TYPE: return VK_FORMAT_R32G32B32_SFLOAT;
            case ShaderDataType::FLOAT4_TYPE: return VK_FORMAT_R32G32B32A32_SFLOAT;

            case ShaderDataType::MAT3_TYPE:
            case ShaderDataType::MAT4_TYPE: return VK_FORMAT_UNDEFINED; //temporary

            case ShaderDataType::INT_TYPE:  return VK_FORMAT_R32_SINT;
            case ShaderDataType::INT2_TYPE: return VK_FORMAT_R32G32_SINT;
            case ShaderDataType::INT3_TYPE: return VK_FORMAT_R32G32B32_SINT;
            case ShaderDataType::INT4_TYPE: return VK_FORMAT_R32G32B32A32_SINT;
            case ShaderDataType::BOOL_TYPE: return VK_FORMAT_R32_SINT;

            case ShaderDataType::NONE:
            case ShaderDataType::COUNT: [[fallthrough]];
            default:
                MKT_ASSERT(false, "Invalid shader data type");
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

        vkCmdBlitImage2(cmd, &blitInfo);
    }
}