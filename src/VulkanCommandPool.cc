/**
 * VulkanCommandPool.cc
 * Created by kate on 7/4/2023.
 * */

// C++ Standard Library
#include <stdexcept>

// Third-Party Libraries
#include <volk.h>

// Project Headers
#include <Utility/VulkanUtils.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanCommandPool.hh>

namespace Mikoto {
    auto VulkanCommandPool::OnCreate(VkCommandPoolCreateInfo createInfo) -> void {
        auto queueFamilyData{ VulkanContext::FindQueueFamilies(VulkanContext::GetPrimaryPhysicalDevice()) };

        VkCommandPoolCreateInfo poolInfo{ VulkanUtils::Initializers::CommandPoolCreateInfo() };
        poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyData.GraphicsFamilyIndex;

        if (vkCreateCommandPool(VulkanContext::GetPrimaryLogicalDevice(), &poolInfo, nullptr, &m_CommandPool) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("Failed to create command pool!");
        }
    }

    auto VulkanCommandPool::BeginSingleTimeCommands() const -> VkCommandBuffer {
        VkCommandBufferAllocateInfo allocInfo{ VulkanUtils::Initializers::CommandBufferAllocateInfo() };
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = m_CommandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer{};
        vkAllocateCommandBuffers(VulkanContext::GetPrimaryLogicalDevice(), &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo{ VulkanUtils::Initializers::CommandBufferBeginInfo() };
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);
        return commandBuffer;
    }

    auto VulkanCommandPool::EndSingleTimeCommands(VkCommandBuffer commandBuffer) const -> void {
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(VulkanContext::GetPrimaryLogicalDeviceGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(VulkanContext::GetPrimaryLogicalDeviceGraphicsQueue());
        vkFreeCommandBuffers(VulkanContext::GetPrimaryLogicalDevice(), m_CommandPool, 1, &commandBuffer);
    }

    auto VulkanCommandPool::OnRelease() const -> void {
        vkDestroyCommandPool(VulkanContext::GetPrimaryLogicalDevice(), m_CommandPool, nullptr);
    }
}
