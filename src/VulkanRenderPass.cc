/**
 * VulkanRenderPass.cc
 * Created by kate on 8/9/2023.
 * */

// Third-Party Libraries
#include <volk.h>

// Project Headers
#include "Renderer/Vulkan/VulkanContext.hh"
#include "Renderer/Vulkan/VulkanRenderPass.hh"
#include "Utility/Common.hh"

namespace Mikoto {

    auto VulkanRenderPass::OnCreate(VkRenderPassCreateInfo* createInfo, VkRenderPass* renderPass, bool useDefaultRenderPass) -> void {
        //Unused param
        (void)createInfo;

        if (useDefaultRenderPass)
            m_CreateInfo = GetDefaultRenderPassConfig();
        else
            m_CreateInfo = *createInfo;

        if (vkCreateRenderPass(VulkanContext::GetPrimaryLogicalDevice(), &m_CreateInfo, nullptr, renderPass) != VK_SUCCESS)
            throw std::runtime_error("Failed to create render pass");
    }

    auto VulkanRenderPass::OnRelease() const -> void {

    }

    auto VulkanRenderPass::BeginRender() -> void {

    }

    auto VulkanRenderPass::EndRender() -> void {

    }

    auto VulkanRenderPass::GetDefaultRenderPassConfig() -> VkRenderPassCreateInfo {
        VkAttachmentDescription attachment{};
        attachment.format = VK_FORMAT_B8G8R8A8_UNORM;
        attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;// or VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        info.attachmentCount = 1;
        info.pAttachments = &attachment;
        info.subpassCount = 1;
        info.pSubpasses = &subpass;
        info.dependencyCount = 1;
        info.pDependencies = &dependency;

        return info;
    }
}
