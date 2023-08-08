/**
 * VulkanRenderer.cc
 * Created by kate on 7/3/23.
 * */
#include <memory>
#include <array>

#include <volk.h>

#include <Utility/Common.hh>
#include <Utility/VulkanUtils.hh>

#include <Core/Assert.hh>
#include <Core/Application.hh>

#include <Renderer/Vulkan/VulkanCommandPool.hh>
#include <Renderer/Vulkan/VulkanFrameBuffer.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanIndexBuffer.hh>
#include <Renderer/Vulkan/VulkanRenderer.hh>
#include <Renderer/Vulkan/VulkanStandardMaterial.hh>
#include <Renderer/Vulkan/VulkanVertexBuffer.hh>

namespace kaTe {
    auto VulkanRenderer::Init() -> void {
        m_CommandPool = std::make_shared<VulkanCommandPool>();
        KT_ASSERT(m_CommandPool, "Command Pool pointer is NULL");
        m_CommandPool->OnCreate(VkCommandPoolCreateInfo());

        m_ClearValues[0].color = { 0.2f, 0.2f, 0.2f, 1.0f };
        m_ClearValues[1].depthStencil = { 1.0f, 0 };

        m_DefaultMaterial = std::make_shared<VulkanStandardMaterial>();

        CreateRenderPass();
        CreateImages();
        CreateDepthResources();
        CreateFrameBuffers();

        CreateDescriptorLayout();
        CreateDescriptorPool();
        CreateDescriptorSet();

        CreateCommandBuffers();
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
        m_Viewport = {
                .x = static_cast<float>(x),
                .y = static_cast<float>(y),
                .width = static_cast<float>(width),
                .height = static_cast<float>(height),
                .minDepth = 0.0f,
                .maxDepth = 1.0f,
        };
    }

    auto VulkanRenderer::Shutdown() -> void {
        vkDeviceWaitIdle(VulkanContext::GetPrimaryLogicalDevice());
        m_CommandPool->OnRelease();
        m_DefaultMaterial->OnRelease();
    }

    auto VulkanRenderer::CreateCommandBuffers() -> void {
        constexpr UInt32_T COMMAND_BUFFERS_COUNT{ 1 };
        m_CommandBuffers.resize(COMMAND_BUFFERS_COUNT);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = m_CommandPool->GetCommandPool();
        allocInfo.commandBufferCount = static_cast<UInt32_T>(m_CommandBuffers.size());

        if (vkAllocateCommandBuffers(VulkanContext::GetPrimaryLogicalDevice(), &allocInfo, m_CommandBuffers.data()) != VK_SUCCESS)
            throw std::runtime_error("Failed to allocate command buffers");
    }

    auto VulkanRenderer::DrawFrame(const Model& model) -> void {
        UInt32_T imageIndex{};

        auto result{ VulkanContext::GetSwapChain()->AcquireNextImage(&imageIndex) };
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            VulkanContext::RecreateSwapChain();
            return;
        }

        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
            throw std::runtime_error("failed to acquire swap chain image!");


        m_DefaultMaterial->UpdateUniformBuffers(VulkanContext::GetSwapChain()->GetCurrentFrame());

        for (const auto& mesh : model.GetMeshes())
            RecordCommandBuffers(imageIndex, mesh);

        result = VulkanContext::GetSwapChain()->SubmitCommandBuffers(&m_CommandBuffers[imageIndex], imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            VulkanContext::RecreateSwapChain();
            return;
        }

        if (result != VK_SUCCESS)
            throw std::runtime_error("failed to present swap chain image!");
    }

    auto VulkanRenderer::RecordCommandBuffers(UInt32_T imageIndex, const Mesh& mesh) -> void {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(m_CommandBuffers[imageIndex], &beginInfo) != VK_SUCCESS)
            throw std::runtime_error("Failed to begin recording command buffer");

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = VulkanContext::GetSwapChain()->GetRenderPass();
        renderPassInfo.framebuffer = VulkanContext::GetSwapChain()->GetFrameBuffer(imageIndex);
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = VulkanContext::GetSwapChain()->GetSwapChainExtent();

        renderPassInfo.clearValueCount = static_cast<UInt32_T>(m_ClearValues.size());
        renderPassInfo.pClearValues = m_ClearValues.data();

        vkCmdBeginRenderPass(m_CommandBuffers[imageIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdSetViewport(m_CommandBuffers[imageIndex], 0, 1, &m_Viewport);
        vkCmdSetScissor(m_CommandBuffers[imageIndex], 0, 1, &m_Scissor);

        m_DefaultMaterial->GetPipeline().Bind(m_CommandBuffers[imageIndex]);
        m_DefaultMaterial->BindDescriptorSets(m_CommandBuffers[imageIndex]);

        std::dynamic_pointer_cast<VulkanIndexBuffer>(mesh.GetIndexBuffer())->Bind(m_CommandBuffers[imageIndex]);
        std::dynamic_pointer_cast<VulkanVertexBuffer>(mesh.GetVertexBuffer())->Submit(m_CommandBuffers[imageIndex]);
        vkCmdDrawIndexed(m_CommandBuffers[imageIndex], std::dynamic_pointer_cast<VulkanIndexBuffer>(mesh.GetIndexBuffer())->GetCount(), 1, 0, 0, 0);

        vkCmdEndRenderPass(m_CommandBuffers[imageIndex]);

        if (vkEndCommandBuffer(m_CommandBuffers[imageIndex]) != VK_SUCCESS)
            throw std::runtime_error("Failed to record command buffer");
    }

    auto VulkanRenderer::Draw(const DrawData &data) -> void {
        m_DefaultMaterial->SetModelMatrix(data.Model);
        m_DefaultMaterial->SetProjectionMatrix(data.Projection);
        m_DefaultMaterial->SetViewMatrix(data.View);

        DrawFrame(*data.ModelData);
    }

    auto VulkanRenderer::OnEvent(Event &event) -> void {

    }

    auto VulkanRenderer::Draw(const RenderingData &data) -> void {

    }

    auto VulkanRenderer::CreateRenderPass() -> void {
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

        if (vkCreateRenderPass(VulkanContext::GetPrimaryLogicalDevice(), &info, nullptr, &m_MainRenderPass) != VK_SUCCESS)
            throw std::runtime_error("Failed to create render pass for the Vulkan Renderer!");
    }

    auto VulkanRenderer::CreateFrameBuffers() -> void {
        constexpr UInt32_T FRAMEBUFFER_COUNT{ 1 };

        // These are the default values for the frame buffers we will be rendering into
        // Although, it is more appropriate to adjust them, so they match the viewport values
        constexpr UInt32_T DEFAULT_FRAMEBUFFER_WIDTH{ 1280 };
        constexpr UInt32_T DEFAULT_FRAMEBUFFER_HEIGHT{ 720 };

        VkFramebufferCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfo.pNext = nullptr;
        createInfo.renderPass = m_MainRenderPass;

        createInfo.width = DEFAULT_FRAMEBUFFER_WIDTH;
        createInfo.height = DEFAULT_FRAMEBUFFER_HEIGHT;
        createInfo.layers = 1;

        createInfo.attachmentCount = 0;
        createInfo.pAttachments = VK_NULL_HANDLE; /* Image Views */

        for (Size_T count{}; count < FRAMEBUFFER_COUNT; ++count)
            m_FrameBuffers.emplace_back(createInfo);
    }

    auto VulkanRenderer::UpdateViewport(UInt32_T xVal, UInt32_T yVal, UInt32_T widthVal, UInt32_T heightVal) -> void {
        m_Viewport = {
            .x = static_cast<float>(xVal),
            .y = static_cast<float>(yVal),
            .width = static_cast<float>(widthVal),
            .height = static_cast<float>(heightVal),
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

    auto VulkanRenderer::CreateImages() -> void {

    }

    auto VulkanRenderer::CreateDepthResources() -> void {

    }

    auto VulkanRenderer::CreateDescriptorLayout() -> void {


    }

    auto VulkanRenderer::CreateDescriptorPool() -> void {

    }

    auto VulkanRenderer::CreateDescriptorSet() -> void {

    }
}