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
#include <Common/VulkanUtils.hh>

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

        if (vkCreateImageView(VulkanContext::GetPrimaryLogicalDevice(), &m_ImageViewCreateInfo, nullptr, &m_ImageView) != VK_SUCCESS)
            throw std::runtime_error("Failed to create the Vulkan Image View!");

        DeletionQueue::Push([=, this]() -> void {
            vkDestroyImageView(VulkanContext::GetPrimaryLogicalDevice(), m_ImageView, nullptr);
            vmaDestroyImage(VulkanContext::GetDefaultAllocator(), m_AllocInfo.Image, m_AllocInfo.Allocation);
        });
    }
}