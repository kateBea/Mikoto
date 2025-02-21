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
    }

    VulkanCommandPool::~VulkanCommandPool() {
        if ( !m_IsReleased ) {
            Release();
            Invalidate();
        }
    }

    auto VulkanCommandPool::AllocateCommandBuffer( const VkCommandBufferAllocateInfo& allocateInfo ) -> VkCommandBuffer* {
        VulkanDevice& device{ VulkanContext::Get().GetDevice() };

        VkCommandBuffer commandBuffer{};
        if ( vkAllocateCommandBuffers(
                     device.GetLogicalDevice(),
                     std::addressof( allocateInfo ),
                     std::addressof( commandBuffer ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanCommandPool::AllocateCommandBuffer - Failed to allocate command buffer" );
        }

        m_CommandBuffers.emplace_back( commandBuffer );

        return std::addressof( m_CommandBuffers.back() );
    }

    auto VulkanCommandPool::Release() -> void {
        const VulkanDevice& device{ VulkanContext::Get().GetDevice() };

        device.WaitIdle();

        for ( auto& commandBuffer : m_CommandBuffers ) {
            vkFreeCommandBuffers( device.GetLogicalDevice(), m_CommandPool, 1, std::addressof( commandBuffer ) );
        }

        vkDestroyCommandPool( device.GetLogicalDevice(), m_CommandPool, nullptr );
    }

    auto VulkanCommandPool::Create( const VulkanCommandPoolCreateInfo& createInfo ) -> Scope_T<VulkanCommandPool> {
        return CreateScope<VulkanCommandPool>( createInfo );
    }
}// namespace Mikoto
