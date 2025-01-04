/**
 * VulkanFrameBuffer.cc
 * Created by kate on 7/10/2023.
 * */

// Third-Party Library
#include "volk.h"

// Project Headers
#include "Common/Common.hh"
#include "STL/Utility/Types.hh"

#include <Renderer/Vulkan/DeletionQueue.hh>
#include "Renderer/Vulkan/VulkanContext.hh"
#include "Renderer/Vulkan/VulkanFrameBuffer.hh"

namespace Mikoto {
    auto VulkanFrameBuffer::OnCreate(const VkFramebufferCreateInfo& createInfo) -> void {
        m_VkCreateInfo = createInfo;

        if (vkCreateFramebuffer(VulkanContext::GetPrimaryLogicalDevice(), std::addressof(createInfo), nullptr, std::addressof(m_FrameBuffer)) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("Failed to create framebuffer!");
        }

        DeletionQueue::Push([frameBufferHandle = m_FrameBuffer]() -> void {
            vkDestroyFramebuffer(VulkanContext::GetPrimaryLogicalDevice(), frameBufferHandle, nullptr);
        });
    }

    auto VulkanFrameBuffer::Resize(UInt32_T width, UInt32_T height) -> void {
        vkDestroyFramebuffer(VulkanContext::GetPrimaryLogicalDevice(), m_FrameBuffer, nullptr);

        m_VkCreateInfo.width = width;
        m_VkCreateInfo.height = height;

        m_CreateInfo.width = (Int32_T)width;
        m_CreateInfo.height = (Int32_T)height;

        OnCreate(m_VkCreateInfo);
    }
}