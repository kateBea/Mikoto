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
    VulkanCommandPool::VulkanCommandPool( const VkCommandPoolCreateInfo& createInfo ) {
        m_CreateInfo = createInfo;

        if ( vkCreateCommandPool( VulkanContext::GetPrimaryLogicalDevice(), std::addressof( m_CreateInfo ), nullptr, std::addressof( m_CommandPool ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanCommandPool::Create - ailed to create command pool!" );
        }
    }

    auto VulkanCommandPool::Create(const VkCommandPoolCreateInfo& createInfo) -> Ref_T<VulkanCommandPool> {
        auto result{ CreateRef<VulkanCommandPool>( createInfo ) };
        DeletionQueue::Register( result );
        return result;
    }

    auto VulkanCommandPool::Release() -> void {
        DeletionQueue::Push([cmdPool = m_CommandPool]() -> void {
            vkDestroyCommandPool(VulkanContext::GetPrimaryLogicalDevice(), cmdPool, nullptr);
        });
    }
}
