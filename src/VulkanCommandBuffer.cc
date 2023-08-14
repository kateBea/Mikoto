/**
* VulkanCommandBuffer.cc
* Created by kate on 8/8/2023.
* */

// C++ Standard Libraries
#include <stdexcept>

// Third-Party Libraries
#include <volk.h>

// Project Headers
#include <Utility/Common.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanCommandBuffer.hh>

namespace Mikoto {
    auto VulkanCommandBuffer::OnCreate(const VkCommandBufferAllocateInfo& allocateInfo) -> void {
        m_AllocInfo = allocateInfo;
        if (vkAllocateCommandBuffers(VulkanContext::GetPrimaryLogicalDevice(), &m_AllocInfo, &m_CommandBuffer) != VK_SUCCESS)
            throw std::runtime_error("Failed to allocate command buffer");
    }

    auto VulkanCommandBuffer::BeginRecording() const -> void {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(m_CommandBuffer, &beginInfo) != VK_SUCCESS)
            throw std::runtime_error("Failed to begin recording to command buffer");
    }

    auto VulkanCommandBuffer::EndRecording() const -> void {
        if (vkEndCommandBuffer(m_CommandBuffer) != VK_SUCCESS)
            throw std::runtime_error("Failed to record command buffer");
    }
}