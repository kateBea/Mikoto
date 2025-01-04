/**
 * VulkanImage.cc
 * Created by kate on 8/9/2023.
 * */

// C++ Standard Library
#include <stdexcept>

// Third-Party Libraries
#include "vk_mem_alloc.h"
#include "volk.h"

// Project Headers
#include <Renderer/Vulkan/VulkanUtils.hh>

#include <Renderer/Vulkan/VulkanImage.hh>
#include <Renderer/Vulkan/DeletionQueue.hh>
#include <Renderer/Vulkan/VulkanContext.hh>

namespace Mikoto {
    auto VulkanImage::OnCreate(const ImageCreateInfo& createInfo) -> void {
        m_AllocInfo = {};
        m_AllocInfo.ImageCreateInfo = createInfo.ImageCreateData;

        m_AllocInfo.AllocationCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        m_AllocInfo.AllocationCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        m_ImageViewCreateInfo = createInfo.ImageViewCreateData;

        VulkanUtils::AllocateImage(m_AllocInfo);
        m_ImageViewCreateInfo.image = m_AllocInfo.Image;

        if (vkCreateImageView(VulkanContext::GetPrimaryLogicalDevice(), &m_ImageViewCreateInfo, nullptr, &m_ImageView) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("Failed to create the Vulkan Image View!");
        }

        DeletionQueue::Push([imageView = m_ImageView, imageHandle = m_AllocInfo.Image, allocation = m_AllocInfo.Allocation]() -> void {
            vkDestroyImageView(VulkanContext::GetPrimaryLogicalDevice(), imageView, nullptr);
            vmaDestroyImage(VulkanContext::GetDefaultAllocator(), imageHandle, allocation);
        });
    }
}