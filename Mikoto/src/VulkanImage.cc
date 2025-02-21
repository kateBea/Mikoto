/**
 * VulkanImage.cc
 * Created by kate on 8/9/2023.
 * */

// C++ Standard Library
#include <stdexcept>

// Third-Party Libraries
#include <volk.h>
#include <vk_mem_alloc.h>

// Project Headers
#include <Renderer/Vulkan/VulkanHelpers.hh>

#include <Renderer/Vulkan/VulkanImage.hh>
#include <Renderer/Vulkan/VulkanDeletionQueue.hh>
#include <Renderer/Vulkan/VulkanContext.hh>

namespace Mikoto {

    VulkanImage::VulkanImage( const VulkanImageCreateInfo& createInfo ) {
        VulkanDevice& device{ VulkanContext::Get().GetDevice() };

        m_ImageViewCreateInfo = createInfo.ImageViewCreateInfo;

        // The image has not been allocated, and we need to allocate it on the given device
        if ( createInfo.Image == VK_NULL_HANDLE ) {

            VmaAllocationCreateInfo allocCreateInfo{
                .usage = VMA_MEMORY_USAGE_GPU_ONLY,
                .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            };

            device.CreateImage( createInfo, allocCreateInfo, m_Image, m_Allocation, m_AllocationInfo );
        } else {
            m_Image = createInfo.Image;
            m_IsImageExternal = true;
        }

        // Save the created image into the view create info
        // required to create the image view
        m_ImageViewCreateInfo.image = m_Image;

        // Here we always create the image view;
        // the caller can optionally pass a valid image because this VulkanImage is supposed be
        // usable for the swapchain as well, however, if the latter is the case,
        // we are responsible for releasing the image views, not the actual images.
        if ( vkCreateImageView( device.GetLogicalDevice(), std::addressof( m_ImageViewCreateInfo ), nullptr, std::addressof( m_ImageView ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanImage::VulkanImage - Failed to create the Vulkan Image View!" );
        }
    }

    auto VulkanImage::LayoutTransition( const VkImageLayout newLayout, const VkCommandBuffer cmd ) -> void {
        VkImageMemoryBarrier2 imageBarrier {};
        imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        imageBarrier.pNext = nullptr;

        imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
        imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

        imageBarrier.oldLayout = m_CurrentLayout;
        imageBarrier.newLayout = newLayout;

        VkImageAspectFlags aspectMask = (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        imageBarrier.subresourceRange = VulkanHelpers::Initializers::ImageSubresourceRange(aspectMask);
        imageBarrier.image = m_Image;

        VkDependencyInfo depInfo {};
        depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        depInfo.pNext = nullptr;

        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers = std::addressof(imageBarrier);

        vkCmdPipelineBarrier2(cmd, std::addressof(depInfo));

        // Update the current layout
        m_CurrentLayout = newLayout;
    }

    auto VulkanImage::Create( const VulkanImageCreateInfo& createInfo ) -> Scope_T<VulkanImage> {
        return CreateScope<VulkanImage>( createInfo );
    }

    auto VulkanImage::Release() -> void {
        VulkanDevice& device{ VulkanContext::Get().GetDevice() };

        device.WaitIdle();

        vkDestroyImageView( device.GetLogicalDevice(), m_ImageView, nullptr );

        if (!m_IsImageExternal) {
            vmaDestroyImage( device.GetAllocator(), m_Image, m_Allocation );
        }
    }

    VulkanImage::~VulkanImage() {
        if (!m_IsReleased) {
            Release();
            Invalidate();
        }
    }
}// namespace Mikoto