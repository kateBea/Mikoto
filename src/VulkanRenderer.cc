/**
 * VulkanRenderer.cc
 * Created by kate on 7/3/23.
 * */

 // C++ Standard Library
#include <memory>
#include <array>
#include <limits>

// Third-Party Libraries
#include <volk.h>
#include <backends/imgui_impl_vulkan.h>

// Project Headers
#include <Utility/Common.hh>
#include <Utility/VulkanUtils.hh>
#include <Core/Assert.hh>
#include <Core/Application.hh>
#include <Renderer/Vulkan/VulkanImage.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanRenderer.hh>
#include <Renderer/Vulkan/VulkanFrameBuffer.hh>
#include <Renderer/Vulkan/VulkanCommandPool.hh>
#include <Renderer/Vulkan/VulkanIndexBuffer.hh>
#include <Renderer/Vulkan/VulkanVertexBuffer.hh>
#include <Renderer/Vulkan/VulkanStandardMaterial.hh>

namespace Mikoto {
    auto VulkanRenderer::Init() -> void {
        m_CommandPool = std::make_shared<VulkanCommandPool>();
        KT_ASSERT(m_CommandPool, "Command Pool pointer is NULL");
        m_CommandPool->OnCreate(VkCommandPoolCreateInfo());

        m_ClearValues[0].color = { 0.2f, 0.2f, 0.2f, 1.0f };
        m_ClearValues[1].depthStencil = { 1.0f, 0 };

        m_DefaultMaterial = std::make_shared<VulkanStandardMaterial>();

        PrepareOffscreen();
        CreateCommandBuffers();

        VulkanStandardMaterialCreateInfo createInfo{};
        createInfo.RenderPass = m_OffscreenMainRenderPass;

        m_DefaultMaterial->OnCreate(createInfo);
    }

    auto VulkanRenderer::EnableWireframeMode() -> void {
        m_DefaultMaterial->EnableWireframe();
    }

    auto VulkanRenderer::DisableWireframeMode() -> void {
        m_DefaultMaterial->DisableWireframe();
    }

    auto VulkanRenderer::SetClearColor(const glm::vec4 &color) -> void {
        m_ClearValues[0].color = { color.r, color.g, color.b, color.a };
    }

    auto VulkanRenderer::SetClearColor(float red, float green, float blue, float alpha) -> void {
        m_ClearValues[0].color = { red, green, blue, alpha };
    }

    auto VulkanRenderer::SetViewport(UInt32_T x, UInt32_T y, UInt32_T width, UInt32_T height) -> void {
        UpdateViewport(x, y, width, height);
    }

    auto VulkanRenderer::Shutdown() -> void {
        VulkanUtils::WaitIdle(VulkanContext::GetPrimaryLogicalDevice());

        m_CommandPool->OnRelease();
        m_DefaultMaterial->OnRelease();
    }

    auto VulkanRenderer::CreateCommandBuffers() -> void {
        // Right now there's only one command buffer which
        constexpr UInt32_T COMMAND_BUFFERS_COUNT{ 1 };

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = m_CommandPool->GetCommandPool();
        allocInfo.commandBufferCount = COMMAND_BUFFERS_COUNT;

        m_DrawCommandBuffer.OnCreate(allocInfo);
    }

    auto VulkanRenderer::DrawFrame(const Model& model) -> void {
        m_DefaultMaterial->UploadUniformBuffers();

        for (const auto& mesh : model.GetMeshes())
            RecordMainRenderPassCommands(mesh);

        SubmitToQueue();
    }

    auto VulkanRenderer::RecordMainRenderPassCommands(const Mesh &mesh) -> void {
        m_DrawCommandBuffer.BeginRecording();

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = m_OffscreenMainRenderPass;
        renderPassInfo.framebuffer = m_OffscreenFrameBuffer.Get();
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = m_OffscreenExtent;

        renderPassInfo.clearValueCount = static_cast<UInt32_T>(m_ClearValues.size());
        renderPassInfo.pClearValues = m_ClearValues.data();

        // Begin Render pass commands recording
        vkCmdBeginRenderPass(m_DrawCommandBuffer.Get(), &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        // Set Viewport and Scissor
        vkCmdSetViewport(m_DrawCommandBuffer.Get(), 0, 1, &m_Viewport);
        vkCmdSetScissor(m_DrawCommandBuffer.Get(), 0, 1, &m_Scissor);

        // Bind material Pipeline and Descriptors
        m_DefaultMaterial->GetPipeline().Bind(m_DrawCommandBuffer.Get());
        m_DefaultMaterial->BindDescriptorSets(m_DrawCommandBuffer.Get());

        // Bind vertex and index buffers
        std::dynamic_pointer_cast<VulkanIndexBuffer>(mesh.GetIndexBuffer())->Bind(m_DrawCommandBuffer.Get());
        std::dynamic_pointer_cast<VulkanVertexBuffer>(mesh.GetVertexBuffer())->Bind(m_DrawCommandBuffer.Get());

        // Draw call command
        vkCmdDrawIndexed(m_DrawCommandBuffer.Get(), std::dynamic_pointer_cast<VulkanIndexBuffer>(mesh.GetIndexBuffer())->GetCount(), 1, 0, 0, 0);

        // End Render pass commands recording
        vkCmdEndRenderPass(m_DrawCommandBuffer.Get());

        // End command buffer recording
        m_DrawCommandBuffer.EndRecording();
    }

    auto VulkanRenderer::Draw(const DrawData & data) -> void {
        m_DefaultMaterial->SetProjectionView(data.TransformData.ProjectionView);
        m_DefaultMaterial->SetTransform(data.TransformData.Transform);

        if (data.ModelData)
            DrawFrame(*data.ModelData);
        else {
            m_DrawCommandBuffer.BeginRecording();

            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = m_OffscreenMainRenderPass;
            renderPassInfo.framebuffer = m_OffscreenFrameBuffer.Get();
            renderPassInfo.renderArea.offset = { 0, 0 };
            renderPassInfo.renderArea.extent = m_OffscreenExtent;

            renderPassInfo.clearValueCount = static_cast<UInt32_T>(m_ClearValues.size());
            renderPassInfo.pClearValues = m_ClearValues.data();

            // Begin Render pass commands recording
            vkCmdBeginRenderPass(m_DrawCommandBuffer.Get(), &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            // Set Viewport and Scissor
            vkCmdSetViewport(m_DrawCommandBuffer.Get(), 0, 1, &m_Viewport);
            vkCmdSetScissor(m_DrawCommandBuffer.Get(), 0, 1, &m_Scissor);

            // Bind material Pipeline and Descriptors
            m_DefaultMaterial->GetPipeline().Bind(m_DrawCommandBuffer.Get());
            m_DefaultMaterial->BindDescriptorSets(m_DrawCommandBuffer.Get());

            // End Render pass commands recording
            vkCmdEndRenderPass(m_DrawCommandBuffer.Get());

            // End command buffer recording
            m_DrawCommandBuffer.EndRecording();

            SubmitToQueue();
        }
    }

    auto VulkanRenderer::OnEvent(Event& event) -> void {

    }

    auto VulkanRenderer::CreateRenderPass() -> void {
        // Color Attachment
        VkAttachmentDescription colorAttachmentDesc{};
        colorAttachmentDesc.format = m_ColorAttachmentFormat;
        colorAttachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // Depth Attachment
        VkAttachmentDescription depthAttachmentDesc{};
        depthAttachmentDesc.flags = 0;
        depthAttachmentDesc.format = m_DepthAttachmentFormat;
        depthAttachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthAttachmentRef{};
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;

        VkSubpassDependency colorAttachmentDependency{};
        colorAttachmentDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        colorAttachmentDependency.dstSubpass = 0;
        colorAttachmentDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        colorAttachmentDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        colorAttachmentDependency.srcAccessMask = 0;
        colorAttachmentDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        // Add a new subpass dependency that synchronizes accesses to depth attachments.
        // This dependency tells Vulkan that the depth attachment in a renderpass cannot be used before previous renderpasses have finished using it.
        VkSubpassDependency deptAttachmentDependency{};
        deptAttachmentDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        deptAttachmentDependency.dstSubpass = 0;
        deptAttachmentDependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        deptAttachmentDependency.srcAccessMask = 0;
        deptAttachmentDependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        deptAttachmentDependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        std::array<VkSubpassDependency, 2> attachmentDependencies{ colorAttachmentDependency, deptAttachmentDependency };
        std::array<VkAttachmentDescription, 2> attachmentDescriptions{ colorAttachmentDesc, depthAttachmentDesc };

        VkRenderPassCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        info.attachmentCount = static_cast<UInt32_T>(attachmentDescriptions.size());
        info.pAttachments = attachmentDescriptions.data();

        info.dependencyCount = static_cast<UInt32_T>(attachmentDependencies.size());
        info.pDependencies = attachmentDependencies.data();

        info.subpassCount = 1;
        info.pSubpasses = &subpass;

        if (vkCreateRenderPass(VulkanContext::GetPrimaryLogicalDevice(), &info, nullptr, &m_OffscreenMainRenderPass) != VK_SUCCESS)
            throw std::runtime_error("Failed to create render pass for the Vulkan Renderer!");
    }

    auto VulkanRenderer::CreateFrameBuffers() -> void {
        std::array<VkImageView, 2> attachments{ m_OffscreenColorAttachment.GetView(), m_OffscreenDepthAttachment.GetView() };

        VkFramebufferCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfo.pNext = nullptr;
        createInfo.renderPass = m_OffscreenMainRenderPass;

        createInfo.width = DEFAULT_FRAMEBUFFER_WIDTH;
        createInfo.height = DEFAULT_FRAMEBUFFER_HEIGHT;
        createInfo.layers = 1;

        createInfo.attachmentCount = static_cast<UInt32_T>(attachments.size());
        createInfo.pAttachments = attachments.data();

        m_OffscreenFrameBuffer.OnCreate(createInfo);
    }

    auto VulkanRenderer::UpdateViewport(UInt32_T x, UInt32_T y, UInt32_T width, UInt32_T height) -> void {
        m_Viewport = {
            .x = static_cast<float>(x),
            .y = static_cast<float>(y),
            .width = static_cast<float>(width),
            .height = static_cast<float>(height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
    }

    auto VulkanRenderer::UpdateScissor(Int32_T x, Int32_T y, VkExtent2D extent) -> void {
        m_Scissor = {
            .offset{ x, y },
            .extent{ extent },
        };
    }

    auto VulkanRenderer::CreateAttachments() -> void {
        m_OffscreenExtent.width = DEFAULT_FRAMEBUFFER_WIDTH;
        m_OffscreenExtent.height = DEFAULT_FRAMEBUFFER_HEIGHT;

        // Color Buffer attachment
        VkImageCreateInfo colorAttachmentCreateInfo{};
        colorAttachmentCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        colorAttachmentCreateInfo.pNext = nullptr;
        colorAttachmentCreateInfo.flags = 0;
        colorAttachmentCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        colorAttachmentCreateInfo.format = m_ColorAttachmentFormat;
        colorAttachmentCreateInfo.extent.width = m_OffscreenExtent.width;
        colorAttachmentCreateInfo.extent.height = m_OffscreenExtent.height;
        colorAttachmentCreateInfo.extent.depth = 1;
        colorAttachmentCreateInfo.mipLevels = 1;
        colorAttachmentCreateInfo.arrayLayers = 1;
        colorAttachmentCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachmentCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        colorAttachmentCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        colorAttachmentCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkImageViewCreateInfo colorAttachmentViewCreateInfo{};
        colorAttachmentViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        colorAttachmentViewCreateInfo.pNext = nullptr;
        colorAttachmentViewCreateInfo.flags = 0;
        colorAttachmentViewCreateInfo.image = m_OffscreenColorAttachment.Get();
        colorAttachmentViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        colorAttachmentViewCreateInfo.format = colorAttachmentCreateInfo.format; // match formats for simplicity

        colorAttachmentViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
        colorAttachmentViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
        colorAttachmentViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
        colorAttachmentViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;

        colorAttachmentViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        colorAttachmentViewCreateInfo.subresourceRange.baseMipLevel = 0;
        colorAttachmentViewCreateInfo.subresourceRange.levelCount = 1;
        colorAttachmentViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        colorAttachmentViewCreateInfo.subresourceRange.layerCount = 1;

        m_OffscreenColorAttachment.OnCreate({ colorAttachmentCreateInfo , colorAttachmentViewCreateInfo });

        // Depth attachment
        VkImageCreateInfo depthAttachmentCreateInfo{};
        depthAttachmentCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        depthAttachmentCreateInfo.pNext = nullptr;

        depthAttachmentCreateInfo.imageType = VK_IMAGE_TYPE_2D;

        depthAttachmentCreateInfo.extent.width = m_OffscreenExtent.width;
        depthAttachmentCreateInfo.extent.height = m_OffscreenExtent.height;
        depthAttachmentCreateInfo.extent.depth = 1;
        depthAttachmentCreateInfo.format = m_DepthAttachmentFormat;

        depthAttachmentCreateInfo.mipLevels = 1;
        depthAttachmentCreateInfo.arrayLayers = 1;
        depthAttachmentCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachmentCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        depthAttachmentCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        VkImageViewCreateInfo depthAttachmentViewCreateInfo{};
        depthAttachmentViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        depthAttachmentViewCreateInfo.pNext = nullptr;
        
        depthAttachmentViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        depthAttachmentViewCreateInfo.image = m_OffscreenDepthAttachment.Get();
        depthAttachmentViewCreateInfo.format = depthAttachmentCreateInfo.format;

        depthAttachmentViewCreateInfo.subresourceRange.baseMipLevel = 0;
        depthAttachmentViewCreateInfo.subresourceRange.levelCount = 1;
        depthAttachmentViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        depthAttachmentViewCreateInfo.subresourceRange.layerCount = 1;
        depthAttachmentViewCreateInfo.subresourceRange.aspectMask =
                depthAttachmentCreateInfo.format < VK_FORMAT_D16_UNORM_S8_UINT ? VK_IMAGE_ASPECT_DEPTH_BIT : (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);

        m_OffscreenDepthAttachment.OnCreate({ depthAttachmentCreateInfo , depthAttachmentViewCreateInfo });
    }

    auto VulkanRenderer::PrepareOffscreen() -> void {
        m_ColorAttachmentFormat = VulkanContext::FindSupportedFormat(
                VulkanContext::GetPrimaryPhysicalDevice(),
                { VK_FORMAT_D32_SFLOAT, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SRGB },
                VK_IMAGE_TILING_OPTIMAL,
                VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT);

        m_DepthAttachmentFormat = VulkanContext::FindSupportedFormat(
                VulkanContext::GetPrimaryPhysicalDevice(),
                { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
                VK_IMAGE_TILING_OPTIMAL,
                VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

        CreateRenderPass();
        CreateAttachments();
        CreateFrameBuffers();

        UpdateViewport(0, 0, DEFAULT_FRAMEBUFFER_WIDTH, DEFAULT_FRAMEBUFFER_HEIGHT);
        UpdateScissor(0, 0, { DEFAULT_FRAMEBUFFER_WIDTH, DEFAULT_FRAMEBUFFER_HEIGHT });

        CreateSynchronizationObjects();
    }

    auto VulkanRenderer::SubmitToQueue() -> void {
        //if (vkWaitForFences(VulkanContext::GetPrimaryLogicalDevice(), 1, &m_QueueSubmitData.Fence, VK_TRUE, std::numeric_limits<UInt64_T>::max()) != VK_SUCCESS)
          //  throw std::runtime_error("Failed to wait for fences in renderer queue submission!");

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        std::array<VkSemaphore, 1> waitSemaphores{ m_QueueSubmitData.RenderFinishedSemaphore };
        std::array<VkSemaphore, 1> signalSemaphores{ m_QueueSubmitData.RenderFinishedSemaphore };
        std::array<VkPipelineStageFlags, 1> waitStages{ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        std::array<VkCommandBuffer, 1> commandBuffers{ m_DrawCommandBuffer.Get() };

        // Wait semaphores
        //submitInfo.waitSemaphoreCount = waitSemaphores.size();
        //submitInfo.pWaitSemaphores = waitSemaphores.data();
        //submitInfo.pWaitDstStageMask = waitStages.data();

        // Signal semaphores
        //submitInfo.signalSemaphoreCount = signalSemaphores.size();
        //submitInfo.pSignalSemaphores = signalSemaphores.data();

        // Command buffers to be submitted
        submitInfo.commandBufferCount = static_cast<UInt32_T>(commandBuffers.size());
        submitInfo.pCommandBuffers = commandBuffers.data();

        //vkResetFences(VulkanContext::GetPrimaryLogicalDevice(), 1, &m_QueueSubmitData.Fence);

        if (vkQueueSubmit(VulkanContext::GetPrimaryLogicalDeviceGraphicsQueue(), 1, &submitInfo, nullptr) != VK_SUCCESS)
            throw std::runtime_error("failed to submit draw command buffer!");

        VulkanUtils::WaitIdle(VulkanContext::GetPrimaryLogicalDevice());
    }

    auto VulkanRenderer::CreateSynchronizationObjects() -> void {
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        if (vkCreateSemaphore(VulkanContext::GetPrimaryLogicalDevice(), &semaphoreInfo, nullptr, &m_QueueSubmitData.ImageAvailableSemaphore) != VK_SUCCESS ||
            vkCreateSemaphore(VulkanContext::GetPrimaryLogicalDevice(), &semaphoreInfo, nullptr, &m_QueueSubmitData.RenderFinishedSemaphore) != VK_SUCCESS ||
            vkCreateFence(VulkanContext::GetPrimaryLogicalDevice(), &fenceInfo, nullptr, &m_QueueSubmitData.Fence) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create synchronization objects");
        }
    }
}