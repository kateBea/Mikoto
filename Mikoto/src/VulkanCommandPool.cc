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
#include <Renderer/Vulkan/VulkanCommandPool.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanDeletionQueue.hh>
#include <Renderer/Vulkan/VulkanHelpers.hh>

namespace Mikoto {
    VulkanCommandPool::VulkanCommandPool( const VulkanCommandPoolCreateInfo& createInfo )
        : m_CreateInfo{ createInfo.CreateInfo }
    {
        const VulkanDevice& device{ VulkanContext::Get().GetDevice() };

        if ( vkCreateCommandPool( device.GetLogicalDevice(), std::addressof( m_CreateInfo ), nullptr, std::addressof( m_CommandPool ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanCommandPool::Create - ailed to create command pool!" );
        }

        VulkanDeletionQueue::Push( [device = device.GetLogicalDevice(), pool = m_CommandPool]() -> void {
            vkDestroyCommandPool(device, pool, nullptr);
        } );
    }

    VulkanCommandPool::~VulkanCommandPool() {
        if (!m_IsReleased) {
            Release();
            Invalidate();
        }
    }

    auto VulkanCommandPool::Release() -> void {
        const VulkanDevice& device{ VulkanContext::Get().GetDevice() };

        vkDestroyCommandPool( device.GetLogicalDevice(), m_CommandPool, nullptr );
    }

    auto VulkanCommandPool::Create( const VulkanCommandPoolCreateInfo& createInfo ) -> Scope_T<VulkanCommandPool> {
        return CreateScope<VulkanCommandPool>( createInfo );
    }
}// namespace Mikoto
