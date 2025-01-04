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
#include <Renderer/Vulkan/VulkanUtils.hh>
#include <Renderer/Vulkan/DeletionQueue.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanCommandPool.hh>

namespace Mikoto {
    auto VulkanCommandPool::Create(const VkCommandPoolCreateInfo& createInfo) -> void {
        m_CreateInfo = createInfo;

        if (vkCreateCommandPool(VulkanContext::GetPrimaryLogicalDevice(), std::addressof(m_CreateInfo), nullptr, std::addressof(m_Obj)) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("Failed to create command pool!");
        }

        DeletionQueue::Push([cmdPool = m_Obj]() -> void {
            vkDestroyCommandPool(VulkanContext::GetPrimaryLogicalDevice(), cmdPool, nullptr);
        });
    }

    auto VulkanCommandPool::Release() -> void {

    }
}
