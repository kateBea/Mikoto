/**
 * VulkanFrameBuffer.cc
 * Created by kate on 7/10/2023.
 * */

// Third-Party Library
#include <volk.h>

// Project Headers
#include <Common/Common.hh>
#include <Renderer/Vulkan/VulkanDeletionQueue.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanFrameBuffer.hh>

namespace Mikoto {
    VulkanFrameBuffer::VulkanFrameBuffer( const VulkanFrameBufferCreateInfo &createInfo )
        : m_CreateInfo{ createInfo.CreateInfo } {
        VulkanDevice& device{ VulkanContext::Get().GetDevice() };

        if ( vkCreateFramebuffer( device.GetLogicalDevice(), std::addressof( m_CreateInfo ), nullptr, std::addressof( m_FrameBuffer ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "Failed to create framebuffer!" );
        }
    }

    auto VulkanFrameBuffer::Create( const VulkanFrameBufferCreateInfo &createInfo ) -> Scope_T<VulkanFrameBuffer> {
        return CreateScope<VulkanFrameBuffer>( createInfo );
    }

    auto VulkanFrameBuffer::Release() -> void {
        VulkanDevice& device{ VulkanContext::Get().GetDevice() };

        device.WaitIdle();

        vkDestroyFramebuffer( device.GetLogicalDevice(), m_FrameBuffer, nullptr );
    }

    VulkanFrameBuffer::~VulkanFrameBuffer() {
        if (!m_IsReleased) {
            Release();
            Invalidate();
        }
    }
}// namespace Mikoto