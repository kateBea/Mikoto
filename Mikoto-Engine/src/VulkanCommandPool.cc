/**
 * VulkanCommandPool.cc
 * Created by kate on 7/4/2023.
 * */

// C++ Standard Library
#include <memory>
#include <stdexcept>

// Third-Party Libraries
#include <volk.h>

// Project Headers
#include <Common/VulkanUtils.hh>
#include <Renderer/Vulkan/DeletionQueue.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanCommandPool.hh>

namespace Mikoto {
    auto VulkanCommandPool::OnCreate(const VkCommandPoolCreateInfo& createInfo) -> void {
        m_CreateInfo = createInfo;

        if (vkCreateCommandPool(VulkanContext::GetPrimaryLogicalDevice(), std::addressof(m_CreateInfo), nullptr, &m_CommandPool) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("Failed to create command pool!");
        }

        DeletionQueue::Push([cmdPool = m_CommandPool]() -> void {
            vkDestroyCommandPool(VulkanContext::GetPrimaryLogicalDevice(), cmdPool, nullptr);
        });
    }
}
