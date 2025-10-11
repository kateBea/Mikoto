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
        : Framebuffer{ createInfo } {
    }

    auto VulkanFramebuffer::Release() -> void {
        TO_VK_DEVICE( m_Device )->WaitIdle();
        vkDestroyFramebuffer( VK_DEVICE(m_Device), m_FrameBuffer, nullptr );

        m_IsAllocated = false;
    }

    VulkanFramebuffer::~VulkanFramebuffer() {
        if ( m_IsAllocated ) {
            Release();
        }
    }

    auto VulkanFramebuffer::Allocate() -> void {
        m_CreateInfo = VulkanHelpers::Initializers::FramebufferCreateInfo();

        VkRenderPass renderPass{ VK_NULL_HANDLE };
        if (m_Spec.NativeHandleSpec.has_value()) {
            renderPass = std::any_cast<VkFramebufferCreateInfo>( m_Spec.NativeHandleSpec ).renderPass;
        }

        m_CreateInfo.pNext = nullptr;
        m_CreateInfo.renderPass = renderPass;

        m_CreateInfo.width = m_Spec.Width;
        m_CreateInfo.height = m_Spec.Height;
        m_CreateInfo.layers = 1;

        std::vector<VkImageView> attachments{};
        for (const auto& texture : m_Spec.ColorAttachments) {
            attachments.emplace_back( *dynamic_cast<const VulkanTexture*>(texture.GetRaw())->GetView() );
        }

        for (const auto& texture : m_Spec.DepthAttachment) {
            attachments.emplace_back( *dynamic_cast<const VulkanTexture*>(texture.GetRaw())->GetView() );
        }

        m_CreateInfo.attachmentCount = static_cast<UInt32>( attachments.size() );
        m_CreateInfo.pAttachments = attachments.data();

        if ( vkCreateFramebuffer( VK_DEVICE(m_Device), std::addressof( m_CreateInfo ), nullptr, std::addressof( m_FrameBuffer ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "Failed to create framebuffer!" );
        }

        m_IsAllocated = true;
    }
}// namespace Mikoto