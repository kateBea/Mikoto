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
            m_AllocInfo = {};
            m_AllocInfo.ImageCreateInfo = createInfo.ImageCreateInfo;

            m_AllocInfo.AllocationCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
            m_AllocInfo.AllocationCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

            device.AllocateImage( m_AllocInfo );
            m_Image = m_AllocInfo.Image;
            m_ImageViewCreateInfo.image = m_AllocInfo.Image;

        } else {
            m_Image = createInfo.Image;
            m_IsImageExternal = true;
        }

        // Here we always create the image view;
        // the caller can optionally pass a valid image because this VulkanImage is supposed be
        // usable for the swapchain as well, however, if the latter is the case,
        // we are responsible for releasing the image views, not the actual images.
        if ( vkCreateImageView( device.GetLogicalDevice(), std::addressof( m_ImageViewCreateInfo ), nullptr, std::addressof( m_ImageView ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanImage::VulkanImage - Failed to create the Vulkan Image View!" );
        }

        VulkanDeletionQueue::Push( [&]() -> void {
            vkDestroyImageView( device.GetLogicalDevice(), m_ImageView, nullptr );

            if ( !m_IsImageExternal ) {
                vmaDestroyImage( device.GetAllocator(), m_AllocInfo.Image, m_AllocInfo.Allocation );
            }
        } );
    }

    auto VulkanImage::Create( const VulkanImageCreateInfo& createInfo ) -> Scope_T<VulkanImage> {
        return CreateScope<VulkanImage>( createInfo );
    }

    auto VulkanImage::Release() -> void {
        VulkanDevice& device{ VulkanContext::Get().GetDevice() };

        vkDestroyImageView( device.GetLogicalDevice(), m_ImageView, nullptr );

        if (!m_IsImageExternal) {
            vmaDestroyImage( device.GetAllocator(), m_AllocInfo.Image, m_AllocInfo.Allocation );
        }
    }

    VulkanImage::~VulkanImage() {
        if (!m_IsReleased) {
            Release();
            Invalidate();
        }
    }
}// namespace Mikoto