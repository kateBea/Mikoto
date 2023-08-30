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
        MKT_ASSERT(m_CommandPool, "Command Pool pointer is NULL");
        m_CommandPool->OnCreate(VkCommandPoolCreateInfo());

        m_ClearValues[COLOR_BUFFER].color = { { 0.2f, 0.2f, 0.2f, 1.0f } };
        m_ClearValues[DEPTH_BUFFER].depthStencil = { 1.0f, 0 };

        PrepareOffscreen();
        CreateCommandBuffers();
    }

    auto VulkanRenderer::EnableWireframeMode() -> void {

    }

    auto VulkanRenderer::DisableWireframeMode() -> void {

    }

    auto VulkanRenderer::SetClearColor(const glm::vec4 &color) -> void {
        m_ClearValues[COLOR_BUFFER].color = { { color.r, color.g, color.b, color.a } };
    }

    auto VulkanRenderer::SetClearColor(float red, float green, float blue, float alpha) -> void {
        m_ClearValues[COLOR_BUFFER].color = { { red, green, blue, alpha } };
    }

    auto VulkanRenderer::SetViewport(UInt32_T x, UInt32_T y, UInt32_T width, UInt32_T height) -> void {
        UpdateViewport(x, y, width, height);
    }

    auto VulkanRenderer::Shutdown() -> void {
        VulkanUtils::WaitOnDevice(VulkanContext::GetPrimaryLogicalDevice());

        m_CommandPool->OnRelease();

        auto standardMatInfo{ m_MaterialInfo[std::string(VulkanStandardMaterial::GetStandardMaterialName())] };
        vkDestroyDescriptorSetLayout(VulkanContext::GetPrimaryLogicalDevice(), standardMatInfo.DescriptorSetLayout, nullptr);

        // Release material data
        for (auto& [materialName, materialData] : m_MaterialInfo) {
            vkDestroyPipelineLayout(VulkanContext::GetPrimaryLogicalDevice(), materialData.MaterialPipelineLayout, nullptr);
            materialData.Pipeline->OnRelease();
        }
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
        // choose material
        m_ActiveDefaultMaterial->UploadUniformBuffers();

        // build commands
        for (const auto& mesh : model.GetMeshes())
            RecordMeshDrawCommands(mesh);

        SubmitToQueue();
    }

    auto VulkanRenderer::RecordMeshDrawCommands(const Mesh &mesh) -> void {
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
        vkCmdSetViewport(m_DrawCommandBuffer.Get(), 0, 1, &m_OffscreenViewport);
        vkCmdSetScissor(m_DrawCommandBuffer.Get(), 0, 1, &m_OffscreenScissor);

        // Bind material Pipeline and Descriptors
        m_MaterialInfo[m_ActiveDefaultMaterial->GetName()].Pipeline->Bind(m_DrawCommandBuffer.Get());
        m_ActiveDefaultMaterial->BindDescriptorSet(m_DrawCommandBuffer.Get(), m_MaterialInfo[m_ActiveDefaultMaterial->GetName()].MaterialPipelineLayout);

        // Bind vertex and index buffers
        std::dynamic_pointer_cast<VulkanIndexBuffer>(mesh.GetIndexBuffer())->Bind(m_DrawCommandBuffer.Get());
        std::dynamic_pointer_cast<VulkanVertexBuffer>(mesh.GetVertexBuffer())->Bind(m_DrawCommandBuffer.Get());

        // Draw call command
        UInt32_T indexCount{ (UInt32_T)std::dynamic_pointer_cast<VulkanIndexBuffer>(mesh.GetIndexBuffer())->GetCount() };
        vkCmdDrawIndexed(m_DrawCommandBuffer.Get(), indexCount, 1, 0, 0, 0);

        // End Render pass commands recording
        vkCmdEndRenderPass(m_DrawCommandBuffer.Get());

        // End command buffer recording
        m_DrawCommandBuffer.EndRecording();

    }

    auto VulkanRenderer::Draw() -> void {

        m_DrawCommandBuffer.BeginRecording();

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = m_OffscreenMainRenderPass;
        renderPassInfo.framebuffer = m_OffscreenFrameBuffer.Get();
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = m_OffscreenExtent;

        renderPassInfo.clearValueCount = static_cast<UInt32_T>(m_ClearValues.size());
        renderPassInfo.pClearValues = m_ClearValues.data();

        // Set Viewport and Scissor
        vkCmdSetViewport(m_DrawCommandBuffer.Get(), 0, 1, &m_OffscreenViewport);
        vkCmdSetScissor(m_DrawCommandBuffer.Get(), 0, 1, &m_OffscreenScissor);

        // Begin Render pass commands recording
        vkCmdBeginRenderPass(m_DrawCommandBuffer.Get(), &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        for (const auto& drawObject : m_DrawQueue) {
            m_ActiveDefaultMaterial = std::dynamic_pointer_cast<VulkanStandardMaterial>(drawObject->MaterialData);

            m_ActiveDefaultMaterial->SetProjectionView(drawObject->TransformData.ProjectionView);
            m_ActiveDefaultMaterial->SetTransform(drawObject->TransformData.Transform);
            m_ActiveDefaultMaterial->SetTiltingColor(drawObject->Color.r, drawObject->Color.g, drawObject->Color.b, drawObject->Color.a);

            m_ActiveDefaultMaterial->UploadUniformBuffers();

            // Bind material Pipeline and Descriptors
            m_MaterialInfo[m_ActiveDefaultMaterial->GetName()].Pipeline->Bind(m_DrawCommandBuffer.Get());
            m_ActiveDefaultMaterial->BindDescriptorSet(m_DrawCommandBuffer.Get(), m_MaterialInfo[m_ActiveDefaultMaterial->GetName()].MaterialPipelineLayout);

            // Bind vertex and index buffers
            std::dynamic_pointer_cast<VulkanVertexBuffer>(drawObject->VertexBufferData)->Bind(m_DrawCommandBuffer.Get());
            std::dynamic_pointer_cast<VulkanIndexBuffer>(drawObject->IndexBufferData)->Bind(m_DrawCommandBuffer.Get());

            // Draw call command
            vkCmdDrawIndexed(m_DrawCommandBuffer.Get(), std::dynamic_pointer_cast<VulkanIndexBuffer>(drawObject->IndexBufferData)->GetCount(), 1, 0, 0, 0);
        }

        // End Render pass commands recording
        vkCmdEndRenderPass(m_DrawCommandBuffer.Get());

        // End command buffer recording
        m_DrawCommandBuffer.EndRecording();
    }

    auto VulkanRenderer::OnFramebufferResize(UInt32_T width, UInt32_T height) -> void {
        m_OffscreenExtent.width = width;
        m_OffscreenExtent.height = height;

        VulkanUtils::WaitOnDevice(VulkanContext::GetPrimaryLogicalDevice());

        CreateAttachments();
        CreateFrameBuffers();
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
        colorAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
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
        depthAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
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


        std::array<VkSubpassDependency, 2> dependencies{};

        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        dependencies[1].srcSubpass = 0;
        dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;


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
        if (m_OffscreenPrepareFinished)
            m_OffscreenFrameBuffer.OnRelease();

        std::array<VkImageView, 2> attachments{ m_OffscreenColorAttachment.GetView(), m_OffscreenDepthAttachment.GetView() };

        VkFramebufferCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfo.pNext = nullptr;
        createInfo.renderPass = m_OffscreenMainRenderPass;

        createInfo.width = m_OffscreenExtent.width;
        createInfo.height = m_OffscreenExtent.height;
        createInfo.layers = 1;

        createInfo.attachmentCount = static_cast<UInt32_T>(attachments.size());
        createInfo.pAttachments = attachments.data();

        m_OffscreenFrameBuffer.OnCreate(createInfo);
    }

    auto VulkanRenderer::UpdateViewport(UInt32_T x, UInt32_T y, UInt32_T width, UInt32_T height) -> void {
        m_OffscreenViewport = {
            .x = static_cast<float>(x),
            .y = static_cast<float>(y),
            .width = static_cast<float>(width),
            .height = static_cast<float>(height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
    }

    auto VulkanRenderer::UpdateScissor(Int32_T x, Int32_T y, VkExtent2D extent) -> void {
        m_OffscreenScissor = {
            .offset{ x, y },
            .extent{ extent },
        };
    }

    auto VulkanRenderer::CreateAttachments() -> void {
        if (m_OffscreenPrepareFinished) {
            m_OffscreenColorAttachment.OnRelease();
            m_OffscreenDepthAttachment.OnRelease();
        }

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

        VkImageViewCreateInfo colorAttachmentViewCreateInfo{};
        colorAttachmentViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        colorAttachmentViewCreateInfo.pNext = nullptr;
        colorAttachmentViewCreateInfo.flags = 0;
        colorAttachmentViewCreateInfo.image = m_OffscreenColorAttachment.Get();
        colorAttachmentViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        colorAttachmentViewCreateInfo.format = colorAttachmentCreateInfo.format; // match formats for simplicity

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
        depthAttachmentCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        depthAttachmentCreateInfo.extent.width = m_OffscreenExtent.width;
        depthAttachmentCreateInfo.extent.height = m_OffscreenExtent.height;
        depthAttachmentCreateInfo.extent.depth = 1;
        depthAttachmentCreateInfo.format = m_DepthAttachmentFormat;

        depthAttachmentCreateInfo.mipLevels = 1;
        depthAttachmentCreateInfo.arrayLayers = 1;
        depthAttachmentCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachmentCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;

        VkImageViewCreateInfo depthAttachmentViewCreateInfo{};
        depthAttachmentViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        depthAttachmentViewCreateInfo.pNext = nullptr;
        
        depthAttachmentViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        depthAttachmentViewCreateInfo.image = m_OffscreenDepthAttachment.Get();
        depthAttachmentViewCreateInfo.format = depthAttachmentCreateInfo.format;

        depthAttachmentViewCreateInfo.flags = 0;
        depthAttachmentViewCreateInfo.subresourceRange = {};
        depthAttachmentViewCreateInfo.subresourceRange.baseMipLevel = 0;
        depthAttachmentViewCreateInfo.subresourceRange.levelCount = 1;
        depthAttachmentViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        depthAttachmentViewCreateInfo.subresourceRange.layerCount = 1;
        depthAttachmentViewCreateInfo.subresourceRange.aspectMask =
                depthAttachmentCreateInfo.format < VK_FORMAT_D16_UNORM_S8_UINT ? VK_IMAGE_ASPECT_DEPTH_BIT : (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);

        m_OffscreenDepthAttachment.OnCreate({ depthAttachmentCreateInfo , depthAttachmentViewCreateInfo });
    }

    auto VulkanRenderer::PrepareOffscreen() -> void {
        m_OffscreenExtent.width = 1920;
        m_OffscreenExtent.height = 1032;

        m_ColorAttachmentFormat = VulkanContext::FindSupportedFormat(
                VulkanContext::GetPrimaryPhysicalDevice(),
                { VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_D32_SFLOAT, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SRGB },
                VK_IMAGE_TILING_OPTIMAL,
                VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT);

        m_DepthAttachmentFormat = VulkanContext::FindSupportedFormat(
                VulkanContext::GetPrimaryPhysicalDevice(),
                { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT },
                VK_IMAGE_TILING_OPTIMAL,
                VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

        CreateRenderPass();
        CreateAttachments();
        CreateFrameBuffers();

        UpdateViewport(0, 0, m_OffscreenExtent.width, m_OffscreenExtent.height);
        UpdateScissor(0, 0, { m_OffscreenExtent.width, m_OffscreenExtent.height });

        InitializeMaterialSpecificData();

        m_OffscreenPrepareFinished = true;
    }

    auto VulkanRenderer::SubmitToQueue() -> void {

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        std::array<VkCommandBuffer, 1> commandBuffers{ m_DrawCommandBuffer.Get() };

        // Command buffers to be submitted
        submitInfo.commandBufferCount = static_cast<UInt32_T>(commandBuffers.size());
        submitInfo.pCommandBuffers = commandBuffers.data();

        if (vkQueueSubmit(VulkanContext::GetPrimaryLogicalDeviceGraphicsQueue(), 1, &submitInfo, nullptr) != VK_SUCCESS)
            throw std::runtime_error("failed to submit draw command buffer!");

        VulkanUtils::WaitOnQueue(VulkanContext::GetPrimaryLogicalDeviceGraphicsQueue());
    }

    auto VulkanRenderer::InitializeMaterialSpecificData() -> void {
        // DEFAULT MATERIAL DATA INITIALIZATION
        const std::string standardMaterialName{ VulkanStandardMaterial::GetStandardMaterialName() };
        m_MaterialInfo.insert(std::make_pair(standardMaterialName, MaterialSharedSpecificData{}));
        MaterialSharedSpecificData& defaultMaterial{ m_MaterialInfo[standardMaterialName] };

        // Pipeline layout
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;

        // This descriptor set layout is global as in, global to all Standard Materials
        CreateGlobalDescriptorSetLayoutForStandardMaterial();
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_MaterialInfo[standardMaterialName].DescriptorSetLayout;

        if (vkCreatePipelineLayout(VulkanContext::GetPrimaryLogicalDevice(), &pipelineLayoutInfo, nullptr, &defaultMaterial.MaterialPipelineLayout) != VK_SUCCESS)
            throw std::runtime_error("Failed to create pipeline layout");

        auto defaultMatPipelineConfig{ VulkanPipeline::GetDefaultPipelineConfigInfo() };
        defaultMatPipelineConfig.RenderPass = m_OffscreenMainRenderPass;
        defaultMatPipelineConfig.PipelineLayout = defaultMaterial.MaterialPipelineLayout;

        defaultMaterial.Pipeline = std::make_shared<VulkanPipeline>("../assets/shaders/vulkan-spirv/basicVert.sprv", "../assets/shaders/vulkan-spirv/basicFrag.sprv", defaultMatPipelineConfig);


        // WIREFRAME MATERIAL DATA INITIALIZATION
        const std::string wireframeMaterialName{ "WireframeMaterial" };
        m_MaterialInfo.insert(std::make_pair(wireframeMaterialName, MaterialSharedSpecificData{}));
        MaterialSharedSpecificData& wireframeMaterial{ m_MaterialInfo[wireframeMaterialName] };

        // Pipeline layout
        VkPipelineLayoutCreateInfo wireframePipelineLayoutInfo{};
        wireframePipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        wireframePipelineLayoutInfo.pushConstantRangeCount = 0;
        wireframePipelineLayoutInfo.pPushConstantRanges = nullptr;

        // This descriptor set layout is global as in, global to all Standard Materials
        // For now the wireframe material takes the same descriptor set layout as the standard material
        wireframePipelineLayoutInfo.setLayoutCount = 1;
        wireframePipelineLayoutInfo.pSetLayouts = &m_MaterialInfo[standardMaterialName].DescriptorSetLayout;

        if (vkCreatePipelineLayout(VulkanContext::GetPrimaryLogicalDevice(), &wireframePipelineLayoutInfo, nullptr, &wireframeMaterial.MaterialPipelineLayout) != VK_SUCCESS)
            throw std::runtime_error("Failed to create pipeline layout");

        auto wireframePipelineConfig{ VulkanPipeline::GetDefaultPipelineConfigInfo() };

        constexpr float GPU_STANDARD_LINE_WIDTH{ 1.0f };
        wireframePipelineConfig.RasterizationInfo.polygonMode = VK_POLYGON_MODE_LINE;
        wireframePipelineConfig.RasterizationInfo.cullMode = VK_CULL_MODE_NONE;
        wireframePipelineConfig.RasterizationInfo.lineWidth = GPU_STANDARD_LINE_WIDTH;

        wireframePipelineConfig.RenderPass = m_OffscreenMainRenderPass;
        wireframePipelineConfig.PipelineLayout = wireframeMaterial.MaterialPipelineLayout;

        wireframeMaterial.Pipeline = std::make_shared<VulkanPipeline>("../assets/shaders/vulkan-spirv/basicVert.sprv", "../assets/shaders/vulkan-spirv/basicFrag.sprv", wireframePipelineConfig);
    }

    auto VulkanRenderer::CreateGlobalDescriptorSetLayoutForStandardMaterial() -> void {
        auto& standardMatInfo{ m_MaterialInfo[std::string(VulkanStandardMaterial::GetStandardMaterialName())] };

        VkDescriptorSetLayoutBinding transformLayoutBinding{};
        transformLayoutBinding.binding = 0;
        transformLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        transformLayoutBinding.descriptorCount = 1;
        transformLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        transformLayoutBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutBinding samplerLayoutBinding{};
        samplerLayoutBinding.binding = 1;
        samplerLayoutBinding.descriptorCount = 1;
        samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding.pImmutableSamplers = nullptr;
        samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array<VkDescriptorSetLayoutBinding, 2> bindings{ transformLayoutBinding, samplerLayoutBinding};

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<UInt32_T>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(VulkanContext::GetPrimaryLogicalDevice(), &layoutInfo, nullptr, &standardMatInfo.DescriptorSetLayout) != VK_SUCCESS)
            throw std::runtime_error("failed to create descriptor set layout!");

    }

    auto VulkanRenderer::Flush() -> void {
        Draw();

        // submit commands for execution
        SubmitToQueue();

        m_DrawQueue.clear();
    }

    auto VulkanRenderer::QueueForDrawing(std::shared_ptr<DrawData> data) -> void {
        m_DrawQueue.emplace_back(data);
    }
}