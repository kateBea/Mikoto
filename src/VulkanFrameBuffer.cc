/**
 * VulkanFrameBuffer.cc
 * Created by kate on 7/10/2023.
 * */

// C++ Standard Library

// Third-Party Library
#include <volk.h>

// Project Headers
#include <Utility/Common.hh>

#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanFrameBuffer.hh>

namespace kaTe {
    VulkanFrameBuffer::VulkanFrameBuffer(const FrameBufferCreateInfo& createInfo)
        :   m_CreateInfo{ createInfo }
    {
        m_VkCreateInfo = {};

        m_VkCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        m_VkCreateInfo.pNext = nullptr;
        m_VkCreateInfo.renderPass = VK_NULL_HANDLE; // TODO: reorganize
        m_VkCreateInfo.width = createInfo.width;
        m_VkCreateInfo.height = createInfo.height;
        m_VkCreateInfo.layers = 1;

        m_VkCreateInfo.attachmentCount = 0;
        m_VkCreateInfo.pAttachments = VK_NULL_HANDLE; /* Image Views */

        OnCreate(m_VkCreateInfo, m_FrameBuffer);
    }

    VulkanFrameBuffer::VulkanFrameBuffer(const VkFramebufferCreateInfo& createInfo)
        :   m_VkCreateInfo{ createInfo }
    {
        OnCreate(m_VkCreateInfo, m_FrameBuffer);
    }

    auto VulkanFrameBuffer::OnCreate(VkFramebufferCreateInfo createInfo, VkFramebuffer& fb) -> void {
        if (vkCreateFramebuffer(VulkanContext::GetPrimaryLogicalDevice(), &createInfo, nullptr, &fb) != VK_SUCCESS)
            throw std::runtime_error("failed to create framebuffer!");
    }

    auto VulkanFrameBuffer::OnRelease() const -> void {
        vkDestroyFramebuffer(VulkanContext::GetPrimaryLogicalDevice(), m_FrameBuffer, nullptr);
    }

    auto VulkanFrameBuffer::Resize(UInt32_T width, UInt32_T height) -> void {

    }
}