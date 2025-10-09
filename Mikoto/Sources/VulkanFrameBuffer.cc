/**
 * VulkanFrameBuffer.cc
 * Created by kate on 7/10/2023.
 * */

// Third-Party Library
#include <volk.h>

// Project Headers
#include <Common/Common.hh>
#include <Renderer/Vulkan/VulkanDevice.hh>
#include <Renderer/Vulkan/VulkanFrameBuffer.hh>

namespace Mikoto {
    VulkanFramebuffer::VulkanFramebuffer( const FramebufferDescription &createInfo )
        : Framebuffer{} {
    }

    auto VulkanFramebuffer::Release() -> void {
        TO_VK_DEVICE( m_Device )->WaitIdle();
        vkDestroyFramebuffer( VK_DEVICE(m_Device), m_FrameBuffer, nullptr );
    }

    VulkanFramebuffer::~VulkanFramebuffer() {
        if ( !m_IsAllocated ) {
            return;
        }

        Release();
    }

    auto VulkanFramebuffer::Allocate() -> void {
        if ( vkCreateFramebuffer( VK_DEVICE(m_Device), std::addressof( m_CreateInfo ), nullptr, std::addressof( m_FrameBuffer ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "Failed to create framebuffer!" );
        }
    }
}// namespace Mikoto