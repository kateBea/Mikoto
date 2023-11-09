/**
 * VulkanRenderer.cc
 * Created by kate on 7/3/23.
 * */

 // C++ Standard Library
#include <algorithm>
#include <memory>
#include <array>

// Third-Party Libraries
#include <volk.h>

// Project Headers
#include <Common/Types.hh>
#include <Common/Common.hh>
#include <Common/VulkanUtils.hh>

#include <Core/FileManager.hh>

#include <Renderer/Vulkan/DeletionQueue.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanRenderer.hh>
#include <Renderer/Vulkan/VulkanIndexBuffer.hh>
#include <Renderer/Vulkan/VulkanVertexBuffer.hh>
#include <Renderer/Vulkan/VulkanStandardMaterial.hh>

namespace Mikoto {
    auto VulkanRenderer::Init() -> void {
        // Create command pool to allocate draw command renderer command buffers from
        VkCommandPoolCreateInfo createInfo{ VulkanUtils::Initializers::CommandPoolCreateInfo() };
        createInfo.flags = 0;
        createInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        createInfo.queueFamilyIndex = VulkanContext::FindQueueFamilies(VulkanContext::GetPrimaryPhysicalDevice()).GraphicsFamilyIndex;

        m_CommandPool.OnCreate(createInfo);

        // Create command buffers
        CreateCommandBuffers();

        // Adjust initial clearing colors
        m_ClearValues[ClearValueIndex::COLOR_BUFFER].color = { { 0.2f, 0.2f, 0.2f, 1.0f } };
        m_ClearValues[ClearValueIndex::DEPTH_BUFFER].depthStencil = { 1.0f, 0 };

        // Prepare deferred
        PrepareDeferred();

        // Prepare offscreen rendering
        PrepareOffscreen();

        // Initialize material required structures
        InitializePipelinesData();
    }

    auto VulkanRenderer::EnableWireframeMode() -> void {
        m_UseWireframe = true;
    }

    auto VulkanRenderer::DisableWireframeMode() -> void {
        m_UseWireframe = false;
    }

    auto VulkanRenderer::SetClearColor(const glm::vec4 &color) -> void {
        m_ClearValues[COLOR_BUFFER].color = { { color.r, color.g, color.b, color.a } };
    }

    auto VulkanRenderer::SetClearColor(float red, float green, float blue, float alpha) -> void {
        m_ClearValues[COLOR_BUFFER].color = { { red, green, blue, alpha } };
    }

    auto VulkanRenderer::SetViewport(float x, float y, float width, float height) -> void {
        UpdateViewport(x, y, width, height);
    }

    auto VulkanRenderer::Shutdown() -> void {
        VulkanUtils::WaitOnDevice(VulkanContext::GetPrimaryLogicalDevice());
    }

    auto VulkanRenderer::CreateCommandBuffers() -> void {
        // Right now there's only one command buffer
        constexpr UInt32_T COMMAND_BUFFERS_COUNT{ 1 };

        VkCommandBufferAllocateInfo allocInfo{ VulkanUtils::Initializers::CommandBufferAllocateInfo() };
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = m_CommandPool.Get();
        allocInfo.commandBufferCount = COMMAND_BUFFERS_COUNT;

        if (vkAllocateCommandBuffers(VulkanContext::GetPrimaryLogicalDevice(), &allocInfo, &m_DrawCommandBuffer) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("Failed to allocate command buffer");
        }

        DeletionQueue::Push([cmdPoolHandle = m_CommandPool.Get(), cmdHandle = m_DrawCommandBuffer]() -> void {
            vkFreeCommandBuffers(VulkanContext::GetPrimaryLogicalDevice(), cmdPoolHandle, 1, std::addressof(cmdHandle));
        });
    }

    auto VulkanRenderer::Draw() -> void {
        VkCommandBufferBeginInfo beginInfo{ VulkanUtils::Initializers::CommandBufferBeginInfo() };

        if (vkBeginCommandBuffer(m_DrawCommandBuffer, &beginInfo) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("Failed to begin recording to command buffer");
        }

        // Pre setup
        VkRenderPassBeginInfo renderPassInfo{ VulkanUtils::Initializers::RenderPassBeginInfo() };
        renderPassInfo.renderPass = m_OffscreenMainRenderPass;
        renderPassInfo.framebuffer = m_OffscreenFrameBuffer.Get();
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = m_OffscreenExtent;
        renderPassInfo.clearValueCount = static_cast<UInt32_T>(m_ClearValues.size());
        renderPassInfo.pClearValues = m_ClearValues.data();

        if (m_UseWireframe) { SetClearColor(1.0f, 1.0f, 1.0f, 1.0f); }

        UpdateViewport(0, static_cast<float>(m_OffscreenExtent.height), static_cast<float>(m_OffscreenExtent.width), -static_cast<float>(m_OffscreenExtent.height));
        UpdateScissor(0, 0, { m_OffscreenExtent.width, m_OffscreenExtent.height });

        // Set Viewport and Scissor
        vkCmdSetViewport(m_DrawCommandBuffer, 0, 1, &m_OffscreenViewport);
        vkCmdSetScissor(m_DrawCommandBuffer, 0, 1, &m_OffscreenScissor);

        // Begin Render pass commands recording
        vkCmdBeginRenderPass(m_DrawCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        const VulkanPipeline* pipeline{ nullptr };
        const VkPipelineLayout* pipelineLayout{ nullptr };

        for (const auto& drawObject : m_DrawQueue) {
            // Process each mesh of the current renderable
            for (const auto& meshMeta : *drawObject.MeshMeta) {
                auto& materialRef{ *meshMeta.MeshMaterial };
                switch (materialRef.GetType()) {
                    case Material::Type::MATERIAL_TYPE_STANDARD:
                        m_ActiveDefaultMaterial = dynamic_cast<VulkanStandardMaterial*>(std::addressof(materialRef));

                        {
                            m_ActiveDefaultMaterial->SetProjection(drawObject.TransformData.Projection);
                            m_ActiveDefaultMaterial->SetView(drawObject.TransformData.View);
                            m_ActiveDefaultMaterial->SetTransform(drawObject.TransformData.Transform);
                            m_ActiveDefaultMaterial->SetLights(Renderer::GetLights().data(), Renderer::GetActiveLightsCount());
                            m_ActiveDefaultMaterial->SetViewPosition(Renderer::GetLightsView());
                            m_ActiveDefaultMaterial->UploadUniformBuffers();

                            pipeline = std::addressof(m_MaterialInfo[m_ActiveDefaultMaterial->GetName()].Pipeline);
                            pipelineLayout = std::addressof(m_MaterialInfo[m_ActiveDefaultMaterial->GetName()].MaterialPipelineLayout);
                            m_ActiveDefaultMaterial->SetTiltingColor(drawObject.Color.r, drawObject.Color.g, drawObject.Color.b, drawObject.Color.a);

                            // Bind material descriptors sets
                            m_ActiveDefaultMaterial->BindDescriptorSet(m_DrawCommandBuffer, *pipelineLayout);
                        }

                        break;
                    case Material::Type::MATERIAL_TYPE_UNKNOWN:
                    case Material::Type::COUNT:
                        MKT_THROW_RUNTIME_ERROR("Not a valid type of material");
                }

                // Bind material pipeline
                pipeline->Bind(m_DrawCommandBuffer);

                // Bind vertex and index buffers
                std::dynamic_pointer_cast<VulkanVertexBuffer>(meshMeta.ModelMesh->GetVertexBuffer())->Bind(m_DrawCommandBuffer);
                std::dynamic_pointer_cast<VulkanIndexBuffer>(meshMeta.ModelMesh->GetIndexBuffer())->Bind(m_DrawCommandBuffer);

                // Draw call command
                vkCmdDrawIndexed(m_DrawCommandBuffer, std::dynamic_pointer_cast<VulkanIndexBuffer>(meshMeta.ModelMesh->GetIndexBuffer())->GetCount(), 1, 0, 0, 0);
            }
        }

        // End Render pass commands recording
        vkCmdEndRenderPass(m_DrawCommandBuffer);

        // End command buffer recording
        if (vkEndCommandBuffer(m_DrawCommandBuffer) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("Failed to record command buffer");
        }
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
        // This dependency tells Vulkan that the depth attachment in a renderpass cannot be used before previous render-passes have finished using it.
        VkSubpassDependency deptAttachmentDependency{};
        deptAttachmentDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        deptAttachmentDependency.dstSubpass = 0;
        deptAttachmentDependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        deptAttachmentDependency.srcAccessMask = 0;
        deptAttachmentDependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        deptAttachmentDependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;


        std::array<VkSubpassDependency, 2> attachmentDependencies{ colorAttachmentDependency, deptAttachmentDependency };
        std::array<VkAttachmentDescription, 2> attachmentDescriptions{ colorAttachmentDesc, depthAttachmentDesc };

        VkRenderPassCreateInfo info{ VulkanUtils::Initializers::RenderPassCreateInfo() };
        info.attachmentCount = static_cast<UInt32_T>(attachmentDescriptions.size());
        info.pAttachments = attachmentDescriptions.data();

        info.dependencyCount = static_cast<UInt32_T>(attachmentDependencies.size());
        info.pDependencies = attachmentDependencies.data();

        info.subpassCount = 1;
        info.pSubpasses = &subpass;

        if (vkCreateRenderPass(VulkanContext::GetPrimaryLogicalDevice(), &info, nullptr, &m_OffscreenMainRenderPass) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("Failed to create render pass for the Vulkan Renderer!");
        }

        DeletionQueue::Push([renderPass = m_OffscreenMainRenderPass]() -> void {
            vkDestroyRenderPass(VulkanContext::GetPrimaryLogicalDevice(), renderPass, nullptr);
        });
    }

    auto VulkanRenderer::CreateFrameBuffers() -> void {
        std::array<VkImageView, 2> attachments{ m_OffscreenColorAttachment.GetView(), m_OffscreenDepthAttachment.GetView() };

        VkFramebufferCreateInfo createInfo{ VulkanUtils::Initializers::FramebufferCreateInfo() };
        createInfo.pNext = nullptr;
        createInfo.renderPass = m_OffscreenMainRenderPass;

        createInfo.width = m_OffscreenExtent.width;
        createInfo.height = m_OffscreenExtent.height;
        createInfo.layers = 1;

        createInfo.attachmentCount = static_cast<UInt32_T>(attachments.size());
        createInfo.pAttachments = attachments.data();

        m_OffscreenFrameBuffer.OnCreate(createInfo);
    }

    auto VulkanRenderer::UpdateViewport(float x, float y, float width, float height) -> void {
        m_OffscreenViewport = {
            .x = x,
            .y = y,
            .width = width,
            .height = height,
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
        // Color Buffer attachment
        VkImageCreateInfo colorAttachmentCreateInfo{ VulkanUtils::Initializers::ImageCreateInfo() };
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

        VkImageViewCreateInfo colorAttachmentViewCreateInfo{ VulkanUtils::Initializers::ImageViewCreateInfo() };
        colorAttachmentViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        colorAttachmentViewCreateInfo.pNext = nullptr;
        colorAttachmentViewCreateInfo.flags = 0;
        colorAttachmentViewCreateInfo.image = VK_NULL_HANDLE; // Set by OnCreate()
        colorAttachmentViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        colorAttachmentViewCreateInfo.format = colorAttachmentCreateInfo.format; // match formats for simplicity

        colorAttachmentViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        colorAttachmentViewCreateInfo.subresourceRange.baseMipLevel = 0;
        colorAttachmentViewCreateInfo.subresourceRange.levelCount = 1;
        colorAttachmentViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        colorAttachmentViewCreateInfo.subresourceRange.layerCount = 1;

        m_OffscreenColorAttachment.OnCreate({ colorAttachmentCreateInfo , colorAttachmentViewCreateInfo });

        // Depth attachment
        VkImageCreateInfo depthAttachmentCreateInfo{ VulkanUtils::Initializers::ImageCreateInfo() };
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

        VkImageViewCreateInfo depthAttachmentViewCreateInfo{ VulkanUtils::Initializers::ImageViewCreateInfo() };

        depthAttachmentViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        depthAttachmentViewCreateInfo.image = VK_NULL_HANDLE; // Set by OnCreate()
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

        // https://www.saschawillems.de/blog/2019/03/29/flipping-the-vulkan-viewport/
        UpdateViewport(0, static_cast<float>(m_OffscreenExtent.height), static_cast<float>(m_OffscreenExtent.width), -static_cast<float>(m_OffscreenExtent.height));
        UpdateScissor(0, 0, { m_OffscreenExtent.width, m_OffscreenExtent.height });

        m_OffscreenPrepareFinished = true;
    }

    auto VulkanRenderer::SubmitToQueue() -> void {
        VulkanContext::BatchCommandBuffer(m_DrawCommandBuffer);
    }

    auto VulkanRenderer::InitializeDefaultPipeline() -> void {
        // Shaders for this pipeline
        // Vertex shader
        ShaderCreateInfo vertexStage{};
        vertexStage.Directory = FileManager::GetAssetsRootPath() / "shaders/vulkan-spirv/StandardVertexShader.sprv";
        vertexStage.Stage = ShaderStage::SHADER_VERTEX_STAGE;

        VulkanShader vertexShader{ vertexStage };

        // Vertex shader
        ShaderCreateInfo fragmentStage{};
        fragmentStage.Directory = FileManager::GetAssetsRootPath() / "shaders/vulkan-spirv/StandardFragmentShader.sprv";
        fragmentStage.Stage = ShaderStage::SHADER_FRAGMENT_STAGE;

        VulkanShader fragmentShader{ fragmentStage };

        // VkPipelineShaderStageCreateInfo pre-setup for the colored pipeline
        std::vector<VkPipelineShaderStageCreateInfo> pipelineShaderStageCreateInfos{};
        pipelineShaderStageCreateInfos.emplace_back(vertexShader.GetPipelineStageCreateInfo());
        pipelineShaderStageCreateInfos.emplace_back(fragmentShader.GetPipelineStageCreateInfo());

        //  DATA INITIALIZATION
        const std::string standardMaterialName{ VulkanStandardMaterial::GetStandardMaterialName() };

        PipelineInfo materialSharedSpecificData{};
        m_MaterialInfo.try_emplace(standardMaterialName, std::move(materialSharedSpecificData));
        PipelineInfo& defaultMaterial{ m_MaterialInfo[standardMaterialName] };

        // Create the pipeline layout
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{ VulkanUtils::Initializers::PipelineLayoutCreateInfo() };
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;

        // First binding for the uniform buffer. Set 0, binding 0.
        VkDescriptorSetLayoutCreateInfo transformBindLayoutCreateInfo{ VulkanUtils::Initializers::DescriptorSetLayoutCreateInfo() };
        VkDescriptorSetLayoutBinding transformBind{ VulkanUtils::CreateDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0) };
        transformBindLayoutCreateInfo.bindingCount = 1;
        transformBindLayoutCreateInfo.pBindings = &transformBind;

        // Second binding for the texture sampler. Set 0, binding 1.
        VkDescriptorSetLayoutCreateInfo textureBindLayoutCreateInfo{ VulkanUtils::Initializers::DescriptorSetLayoutCreateInfo() };
        VkDescriptorSetLayoutBinding textureBind{ VulkanUtils::CreateDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1) };
        textureBindLayoutCreateInfo.bindingCount = 1;
        textureBindLayoutCreateInfo.pBindings = &textureBind;

        // Second binding for the texture sampler. Set 0, binding 2.
        VkDescriptorSetLayoutCreateInfo lightUniformCreateInfo{ VulkanUtils::Initializers::DescriptorSetLayoutCreateInfo() };
        VkDescriptorSetLayoutBinding lightingBind{ VulkanUtils::CreateDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 2) };
        lightUniformCreateInfo.bindingCount = 1;
        lightUniformCreateInfo.pBindings = &lightingBind;

        // Second binding for the texture sampler. Set 0, binding 3.
        VkDescriptorSetLayoutCreateInfo specularSamplerCreateInfo{ VulkanUtils::Initializers::DescriptorSetLayoutCreateInfo() };
        VkDescriptorSetLayoutBinding specularSamplerBind{ VulkanUtils::CreateDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3) };
        specularSamplerCreateInfo.bindingCount = 1;
        specularSamplerCreateInfo.pBindings = &specularSamplerBind;

        std::array<VkDescriptorSetLayoutBinding, 4> bindings{ transformBind, textureBind, lightingBind, specularSamplerBind };

        VkDescriptorSetLayoutCreateInfo layoutInfo{ VulkanUtils::Initializers::DescriptorSetLayoutCreateInfo() };
        layoutInfo.bindingCount = static_cast<UInt32_T>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(VulkanContext::GetPrimaryLogicalDevice(), &layoutInfo, nullptr, std::addressof(defaultMaterial.DescriptorSetLayout)) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("Failed to create descriptor set layout!");
        }

        std::array<VkDescriptorSetLayout, 1> descLayouts{ defaultMaterial.DescriptorSetLayout };

        pipelineLayoutInfo.setLayoutCount = static_cast<UInt32_T>(descLayouts.size());
        pipelineLayoutInfo.pSetLayouts = descLayouts.data();

        if (vkCreatePipelineLayout(VulkanContext::GetPrimaryLogicalDevice(), std::addressof(pipelineLayoutInfo), nullptr, std::addressof(defaultMaterial.MaterialPipelineLayout)) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("Failed to create pipeline layout");
        }

        DeletionQueue::Push([descSetLayout = defaultMaterial.DescriptorSetLayout,
                              pipelineLayout = defaultMaterial.MaterialPipelineLayout]() -> void {
            vkDestroyDescriptorSetLayout(VulkanContext::GetPrimaryLogicalDevice(), descSetLayout, nullptr);
            vkDestroyPipelineLayout(VulkanContext::GetPrimaryLogicalDevice(), pipelineLayout, nullptr);
        });

        // Create pipeline
        auto defaultMatPipelineConfig{ VulkanPipeline::GetDefaultPipelineConfigInfo() };
        defaultMatPipelineConfig.RenderPass = m_OffscreenMainRenderPass;
        defaultMatPipelineConfig.PipelineLayout = defaultMaterial.MaterialPipelineLayout;
        defaultMatPipelineConfig.ShaderStages = std::addressof(pipelineShaderStageCreateInfos);

        defaultMaterial.Pipeline.CreateGraphicsPipeline(defaultMatPipelineConfig);
    }

    auto VulkanRenderer::InitializeWireFramePipeline() -> void {
        // Shaders for this pipeline
        // Vertex shader
        ShaderCreateInfo vertexStage{};
        vertexStage.Directory = FileManager::GetAssetsRootPath() / "shaders/vulkan-spirv/StandardVertexShader.sprv";
        vertexStage.Stage = ShaderStage::SHADER_VERTEX_STAGE;

        VulkanShader vertexShader{ vertexStage };

        // Vertex shader
        ShaderCreateInfo fragmentStage{};
        fragmentStage.Directory = FileManager::GetAssetsRootPath() / "shaders/vulkan-spirv/ColoredFragmentShader.sprv";
        fragmentStage.Stage = ShaderStage::SHADER_FRAGMENT_STAGE;

        VulkanShader fragmentShader{ fragmentStage };

        // VkPipelineShaderStageCreateInfo pre-setup for the colored pipeline
        std::vector<VkPipelineShaderStageCreateInfo> pipelineShaderStageCreateInfos{};
        pipelineShaderStageCreateInfos.emplace_back(vertexShader.GetPipelineStageCreateInfo());
        pipelineShaderStageCreateInfos.emplace_back(fragmentShader.GetPipelineStageCreateInfo());

        //  DATA INITIALIZATION
        const std::string wireframeMaterialName{ "WireframeMaterial" };
        m_MaterialInfo.insert(std::make_pair(wireframeMaterialName, PipelineInfo{}));
        PipelineInfo& wireframeMaterial{ m_MaterialInfo[wireframeMaterialName] };

        // Create the pipeline layout
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{ VulkanUtils::Initializers::PipelineLayoutCreateInfo() };
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;

        // First binding for the uniform buffer. Set 0, binding 0. This pipeline just expects a single descriptor set with uniform buffer data
        VkDescriptorSetLayoutCreateInfo transformBindLayoutCreateInfo{ VulkanUtils::Initializers::DescriptorSetLayoutCreateInfo() };
        VkDescriptorSetLayoutBinding transformBind{ VulkanUtils::CreateDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0) };
        transformBindLayoutCreateInfo.bindingCount = 1;
        transformBindLayoutCreateInfo.pBindings = std::addressof(transformBind);

        std::array<VkDescriptorSetLayoutBinding, 1> bindings{ transformBind };

        VkDescriptorSetLayoutCreateInfo layoutInfo{ VulkanUtils::Initializers::DescriptorSetLayoutCreateInfo() };
        layoutInfo.bindingCount = static_cast<UInt32_T>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(VulkanContext::GetPrimaryLogicalDevice(), std::addressof(layoutInfo), nullptr, std::addressof(wireframeMaterial.DescriptorSetLayout)) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("Failed to create descriptor set layout!");
        }

        std::array<VkDescriptorSetLayout, 1> descLayouts{ wireframeMaterial.DescriptorSetLayout };

        pipelineLayoutInfo.setLayoutCount = static_cast<UInt32_T>(descLayouts.size());
        pipelineLayoutInfo.pSetLayouts = descLayouts.data();

        if (vkCreatePipelineLayout(VulkanContext::GetPrimaryLogicalDevice(), std::addressof(pipelineLayoutInfo), nullptr, std::addressof(wireframeMaterial.MaterialPipelineLayout)) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("Failed to create colored pipeline layout");
        }

        DeletionQueue::Push([descSetLayout = wireframeMaterial.DescriptorSetLayout,
                              pipelineLayout = wireframeMaterial.MaterialPipelineLayout]() -> void {
            vkDestroyDescriptorSetLayout(VulkanContext::GetPrimaryLogicalDevice(), descSetLayout, nullptr);
            vkDestroyPipelineLayout(VulkanContext::GetPrimaryLogicalDevice(), pipelineLayout, nullptr);
        });

        // Create the pipeline
        auto wireframePipelineConfig{ VulkanPipeline::GetDefaultPipelineConfigInfo() };

        constexpr float GPU_STANDARD_LINE_WIDTH{ 1.0f };
        wireframePipelineConfig.RasterizationInfo.polygonMode = VK_POLYGON_MODE_LINE;
        wireframePipelineConfig.RasterizationInfo.cullMode = VK_CULL_MODE_NONE;
        wireframePipelineConfig.RasterizationInfo.lineWidth = GPU_STANDARD_LINE_WIDTH;

        wireframePipelineConfig.RenderPass = m_OffscreenMainRenderPass;
        wireframePipelineConfig.PipelineLayout = wireframeMaterial.MaterialPipelineLayout;
        wireframePipelineConfig.ShaderStages = std::addressof(pipelineShaderStageCreateInfos);

        wireframeMaterial.Pipeline.CreateGraphicsPipeline(wireframePipelineConfig);
    }

    auto VulkanRenderer::InitializePipelinesData() -> void {
        // Create the colored pipeline
        // InitializeColorPipeline();

        // Create the textured pipeline
        InitializeDefaultPipeline();

        // Create the wireframe pipeline
        // InitializeWireFramePipeline();

        // Create the deferred pipeline
        // InitializeDeferredPipeline();
    }

    auto VulkanRenderer::Flush() -> void {
        // Record drawing commands
        Draw();

        // Submit recorded commands
        SubmitToQueue();

        // Clear draw queue
        m_DrawQueue.clear();
    }

    auto VulkanRenderer::QueueForDrawing(std::shared_ptr<DrawData>&& data) -> void {
        if (!data) {
            // Should not really happen
            MKT_CORE_LOGGER_WARN("Null pointer passed to VulkanRenderer::QueueForDrawing(...)");
            return;
        }

        m_DrawQueue.emplace_back(data->MeshMeta, data->TransformData, data->Color);
    }


    auto VulkanRenderer::CreateFrameBufferAttachment(FrameBufferAttachment& frameBufferAttachment, const FramebufferAttachmentCreateInfo& framebufferAttachmentCreateInfo) -> void {
        // Initialize FrameBufferAttachment structure
        frameBufferAttachment.ImageCreateInfo = VulkanUtils::Initializers::ImageCreateInfo();
        frameBufferAttachment.ImageViewCreateInfo = VulkanUtils::Initializers::ImageViewCreateInfo();

        // Setup Image create info
        frameBufferAttachment.ImageCreateInfo.pNext = nullptr;
        frameBufferAttachment.ImageCreateInfo.flags = 0;
        frameBufferAttachment.ImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        frameBufferAttachment.ImageCreateInfo.format = framebufferAttachmentCreateInfo.Format;
        frameBufferAttachment.ImageCreateInfo.extent.width = framebufferAttachmentCreateInfo.Width;
        frameBufferAttachment.ImageCreateInfo.extent.height = framebufferAttachmentCreateInfo.Height;
        frameBufferAttachment.ImageCreateInfo.extent.depth = 1;
        frameBufferAttachment.ImageCreateInfo.mipLevels = 1;
        frameBufferAttachment.ImageCreateInfo.arrayLayers = 1;
        frameBufferAttachment.ImageCreateInfo.samples = framebufferAttachmentCreateInfo.ImageSampleCount;
        frameBufferAttachment.ImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        frameBufferAttachment.ImageCreateInfo.usage = framebufferAttachmentCreateInfo.Usage;

        // Determine aspect mask based on usage
        VkImageAspectFlags aspectMask{};

        // Color attachment
        if (framebufferAttachmentCreateInfo.Usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
            aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }

        // Depth (and/or stencil) attachment
        if (framebufferAttachmentCreateInfo.Usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            aspectMask = framebufferAttachmentCreateInfo.Format < VK_FORMAT_D16_UNORM_S8_UINT ? VK_IMAGE_ASPECT_DEPTH_BIT : (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
        }

        // Setup Image view create info
        frameBufferAttachment.ImageViewCreateInfo.pNext = nullptr;
        frameBufferAttachment.ImageViewCreateInfo.flags = 0;
        frameBufferAttachment.ImageViewCreateInfo.image = VK_NULL_HANDLE; // To be set by VulkanImage::OnCreate()
        frameBufferAttachment.ImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        frameBufferAttachment.ImageViewCreateInfo.format = framebufferAttachmentCreateInfo.Format;
        frameBufferAttachment.ImageViewCreateInfo.subresourceRange.aspectMask = aspectMask;
        frameBufferAttachment.ImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        frameBufferAttachment.ImageViewCreateInfo.subresourceRange.levelCount = 1;
        frameBufferAttachment.ImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        frameBufferAttachment.ImageViewCreateInfo.subresourceRange.layerCount = framebufferAttachmentCreateInfo.LayerCount;

        frameBufferAttachment.Image.OnCreate({ frameBufferAttachment.ImageCreateInfo , frameBufferAttachment.ImageViewCreateInfo });

        // Fill attachment descriptions
        frameBufferAttachment.AttachmentDescription = {};
        frameBufferAttachment.AttachmentDescription.samples = framebufferAttachmentCreateInfo.ImageSampleCount;
        frameBufferAttachment.AttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        frameBufferAttachment.AttachmentDescription.storeOp =
                (framebufferAttachmentCreateInfo.Usage & VK_IMAGE_USAGE_SAMPLED_BIT) ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
        frameBufferAttachment.AttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        frameBufferAttachment.AttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        frameBufferAttachment.AttachmentDescription.format = framebufferAttachmentCreateInfo.Format;
        frameBufferAttachment.AttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        // Fill the final layout according to the type of attachment
        if (framebufferAttachmentCreateInfo.Usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            frameBufferAttachment.AttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        }
        else {
            frameBufferAttachment.AttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }
    }


    auto VulkanRenderer::PrepareDeferredAttachments() -> void {
        FramebufferAttachmentCreateInfo framebufferAttachmentCreateInfo{};

        framebufferAttachmentCreateInfo.Width = 2048;
        framebufferAttachmentCreateInfo.Height = 2048;
        framebufferAttachmentCreateInfo.LayerCount = 1;
        framebufferAttachmentCreateInfo.Format = VK_FORMAT_R16G16B16A16_SFLOAT;
        framebufferAttachmentCreateInfo.Usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

        // Positions attachment -------------------------
        CreateFrameBufferAttachment(m_PositionsDeferredAttachment, framebufferAttachmentCreateInfo);


        // Normals attachment ---------------------------
        CreateFrameBufferAttachment(m_NormalsDeferredAttachment, framebufferAttachmentCreateInfo);


        // Albedo attachment ----------------------------
        framebufferAttachmentCreateInfo.Format = VK_FORMAT_R8G8B8A8_UNORM;
        CreateFrameBufferAttachment(m_AlbedoDeferredAttachment, framebufferAttachmentCreateInfo);


        // Depth attachment -----------------------------
        framebufferAttachmentCreateInfo.Format = VulkanContext::FindSupportedFormat(
                VulkanContext::GetPrimaryPhysicalDevice(),
                { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT },
                VK_IMAGE_TILING_OPTIMAL, // this tiling is the same as the one used in CreateFrameBufferAttachment()
                VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

        framebufferAttachmentCreateInfo.Usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        CreateFrameBufferAttachment(m_DepthDeferredAttachment, framebufferAttachmentCreateInfo);
    }

    auto VulkanRenderer::CreateDeferredSampler() -> void {
        VkSamplerCreateInfo samplerInfo{ VulkanUtils::Initializers::SamplerCreateInfo() };
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;

        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

        auto properties{ VulkanContext::GetPrimaryLogicalDeviceProperties() };
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;

        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

        samplerInfo.unnormalizedCoordinates = VK_FALSE;

        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 1.0f;

        if (vkCreateSampler(VulkanContext::GetPrimaryLogicalDevice(), &samplerInfo, nullptr, &m_DeferredSampler) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("Failed to create deferred texture sampler!");
        }

        DeletionQueue::Push([sampler = m_DeferredSampler]() -> void {
            vkDestroySampler(VulkanContext::GetPrimaryLogicalDevice(), sampler, nullptr);
        });
    }

    auto VulkanRenderer::CreateDeferredRenderPass() -> void {
        // Collect attachment references
        std::array<VkAttachmentReference, 3> colorAttachments{};
        VkAttachmentReference depthAttachmentsReference{};

        // Albedo attachment reference
        colorAttachments[0].attachment = 0;
        colorAttachments[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // Positions attachment reference
        colorAttachments[1].attachment = 1;
        colorAttachments[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // Normals attachment reference
        colorAttachments[2].attachment = 2;
        colorAttachments[2].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // Depth attachment reference
        depthAttachmentsReference.attachment = 3;
        depthAttachmentsReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // Sub-passes ----------------------

        // This render pass uses only one subpass
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = static_cast<UInt32_T>(colorAttachments.size());
        subpass.pColorAttachments = colorAttachments.data();
        subpass.pDepthStencilAttachment = std::addressof(depthAttachmentsReference);

        // Use subpass dependencies for attachment layout transitions
        VkSubpassDependency colorAttachmentsDependency{};
        colorAttachmentsDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        colorAttachmentsDependency.dstSubpass = 0;

        colorAttachmentsDependency.srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        colorAttachmentsDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        colorAttachmentsDependency.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        colorAttachmentsDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        colorAttachmentsDependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;


        VkSubpassDependency deptAttachmentDependency{};
        deptAttachmentDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        deptAttachmentDependency.dstSubpass = 0;

        deptAttachmentDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        deptAttachmentDependency.dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

        deptAttachmentDependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        deptAttachmentDependency.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

        deptAttachmentDependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;


        std::array<VkSubpassDependency, 2> attachmentDependencies{ colorAttachmentsDependency, deptAttachmentDependency };
        std::array<VkAttachmentDescription, 4> attachmentDescriptions{ m_AlbedoDeferredAttachment.AttachmentDescription,
                                                                       m_PositionsDeferredAttachment.AttachmentDescription,
                                                                       m_NormalsDeferredAttachment.AttachmentDescription,
                                                                       m_DepthDeferredAttachment.AttachmentDescription };

        VkRenderPassCreateInfo renderPassCreateInfo{ VulkanUtils::Initializers::RenderPassCreateInfo() };
        renderPassCreateInfo.attachmentCount = static_cast<UInt32_T>(attachmentDescriptions.size());
        renderPassCreateInfo.pAttachments = attachmentDescriptions.data();

        renderPassCreateInfo.dependencyCount = static_cast<UInt32_T>(attachmentDependencies.size());
        renderPassCreateInfo.pDependencies = attachmentDependencies.data();

        renderPassCreateInfo.subpassCount = 1;
        renderPassCreateInfo.pSubpasses = &subpass;

        if (vkCreateRenderPass(VulkanContext::GetPrimaryLogicalDevice(), std::addressof(renderPassCreateInfo), nullptr, std::addressof(m_DeferredRenderPass)) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("Failed to create render pass for the Vulkan Renderer!");
        }

        DeletionQueue::Push([renderPass = m_DeferredRenderPass]() -> void {
            vkDestroyRenderPass(VulkanContext::GetPrimaryLogicalDevice(), renderPass, nullptr);
        });
    }

    auto VulkanRenderer::CreateDeferredFrameBuffer() -> void {
        // Find mx number of layers across attachments
        std::array<VkImageViewCreateInfo , 4> attachmentsImageViews{ m_AlbedoDeferredAttachment.ImageViewCreateInfo,
                                                m_PositionsDeferredAttachment.ImageViewCreateInfo,
                                                m_NormalsDeferredAttachment.ImageViewCreateInfo,
                                                m_DepthDeferredAttachment.ImageViewCreateInfo };

        UInt32_T maxLayers{};
        for (auto attachment : attachmentsImageViews) {
            if (attachment.subresourceRange.layerCount > maxLayers) {
                maxLayers = attachment.subresourceRange.layerCount;
            }
        }

        std::array<VkImageView, 4> attachments{ m_AlbedoDeferredAttachment.Image.GetView(),
                                                m_PositionsDeferredAttachment.Image.GetView(),
                                                m_NormalsDeferredAttachment.Image.GetView(),
                                                m_DepthDeferredAttachment.Image.GetView() };

        // All attachments use the same width
        const UInt32_T width{ m_AlbedoDeferredAttachment.ImageCreateInfo.extent.width };
        const UInt32_T height{ m_AlbedoDeferredAttachment.ImageCreateInfo.extent.height };

        VkFramebufferCreateInfo createInfo{ VulkanUtils::Initializers::FramebufferCreateInfo() };
        createInfo.pNext = nullptr;
        createInfo.renderPass = m_DeferredRenderPass;

        createInfo.width = width;
        createInfo.height = height;
        createInfo.layers = maxLayers;

        createInfo.attachmentCount = static_cast<UInt32_T>(attachments.size());
        createInfo.pAttachments = attachments.data();

        m_OffscreenFrameBuffer.OnCreate(createInfo);
    }

    auto VulkanRenderer::PrepareDeferred() -> void {
        // Create attachments, four total: color, normals and positions and one for the depth
        PrepareDeferredAttachments();

        // Create the sampler to sample from the attachments
        CreateDeferredSampler();

        // Create the render pass
        CreateDeferredRenderPass();

        // Create the frame buffer
        CreateDeferredFrameBuffer();
    }

    auto VulkanRenderer::InitializeDeferredPipeline() -> void {
        // Init deferred pipeline -----------------------------------------------------------------------------
        {
            // 1. [Load shaders]

            // Vertex shader
            ShaderCreateInfo vertexStage{};
            vertexStage.Directory = FileManager::GetAssetsRootPath() / "shaders/vulkan-spirv/DeferredVertexShader.sprv";
            vertexStage.Stage = ShaderStage::SHADER_VERTEX_STAGE;
            VulkanShader vertexShader{ vertexStage };

            // Fragment shader
            ShaderCreateInfo fragmentStage{};
            fragmentStage.Directory = FileManager::GetAssetsRootPath() / "shaders/vulkan-spirv/DeferredFragmentShader.sprv";
            fragmentStage.Stage = ShaderStage::SHADER_FRAGMENT_STAGE;
            VulkanShader fragmentShader{ fragmentStage };

            std::vector<VkPipelineShaderStageCreateInfo> pipelineShaderStageCreateInfos{};
            pipelineShaderStageCreateInfos.emplace_back(vertexShader.GetPipelineStageCreateInfo());
            pipelineShaderStageCreateInfos.emplace_back(fragmentShader.GetPipelineStageCreateInfo());

            //  2. [Init structure]
            const std::string deferredPipelineDataName{ "DeferredPipelineName" };
            PipelineInfo materialSharedSpecificData{};
            m_MaterialInfo.try_emplace( deferredPipelineDataName, std::move(materialSharedSpecificData) );
            PipelineInfo& defaultMaterial{ m_MaterialInfo[deferredPipelineDataName] };

            // 3. [Create Descriptor set layout]: five bindings in total. Vertex shader Uniform buffer, Albedo, Normals, Positions, Fragment shader uniform
            VkPipelineLayoutCreateInfo pipelineLayoutInfo{ VulkanUtils::Initializers::PipelineLayoutCreateInfo() };
            // Push constants
            pipelineLayoutInfo.pushConstantRangeCount = 0;
            pipelineLayoutInfo.pPushConstantRanges = nullptr;

            // Binding for the uniform buffer. Set 0, binding 0.
            VkDescriptorSetLayoutCreateInfo transformBindLayoutCreateInfo{ VulkanUtils::Initializers::DescriptorSetLayoutCreateInfo() };
            VkDescriptorSetLayoutBinding transformBind{ VulkanUtils::CreateDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 0) };
            transformBindLayoutCreateInfo.bindingCount = 1;
            transformBindLayoutCreateInfo.pBindings = std::addressof(transformBind);

            // Binding for the sampler color map. Set 0, binding 1.
            VkDescriptorSetLayoutCreateInfo albedoBindLayoutCreateInfo{ VulkanUtils::Initializers::DescriptorSetLayoutCreateInfo() };
            VkDescriptorSetLayoutBinding albedoBind{ VulkanUtils::CreateDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1) };
            albedoBindLayoutCreateInfo.bindingCount = 1;
            albedoBindLayoutCreateInfo.pBindings = std::addressof(albedoBind);

            // Binding for the sampler normal map. Set 0, binding 2.
            VkDescriptorSetLayoutCreateInfo normalsBindLayoutCreateInfo{ VulkanUtils::Initializers::DescriptorSetLayoutCreateInfo() };
            VkDescriptorSetLayoutBinding normalsBind{ VulkanUtils::CreateDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2) };
            normalsBindLayoutCreateInfo.bindingCount = 1;
            normalsBindLayoutCreateInfo.pBindings = std::addressof(normalsBind);

            // Binding for the sampler positions map. Set 0, binding 3.
            VkDescriptorSetLayoutCreateInfo positionBindLayoutCreateInfo{ VulkanUtils::Initializers::DescriptorSetLayoutCreateInfo() };
            VkDescriptorSetLayoutBinding positionBind{ VulkanUtils::CreateDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3) };
            positionBindLayoutCreateInfo.bindingCount = 1;
            positionBindLayoutCreateInfo.pBindings = std::addressof(positionBind);

            std::array<VkDescriptorSetLayoutBinding, 4> bindings{ transformBind, albedoBind, normalsBind, positionBind };

            VkDescriptorSetLayoutCreateInfo layoutInfo{ VulkanUtils::Initializers::DescriptorSetLayoutCreateInfo() };
            layoutInfo.bindingCount = static_cast<UInt32_T>(bindings.size());
            layoutInfo.pBindings = bindings.data();

            if (vkCreateDescriptorSetLayout(VulkanContext::GetPrimaryLogicalDevice(), &layoutInfo, nullptr, std::addressof(defaultMaterial.DescriptorSetLayout)) != VK_SUCCESS) {
                MKT_THROW_RUNTIME_ERROR("Failed to create descriptor set layout!");
            }

            // 4. [Create Pipeline layout]
            std::array<VkDescriptorSetLayout, 1> descLayouts{ defaultMaterial.DescriptorSetLayout };
            pipelineLayoutInfo.setLayoutCount = static_cast<UInt32_T>(descLayouts.size());
            pipelineLayoutInfo.pSetLayouts = descLayouts.data();

            if (vkCreatePipelineLayout(VulkanContext::GetPrimaryLogicalDevice(), std::addressof(pipelineLayoutInfo), nullptr, std::addressof(defaultMaterial.MaterialPipelineLayout)) != VK_SUCCESS) {
                MKT_THROW_RUNTIME_ERROR("Failed to create pipeline layout");
            }

            DeletionQueue::Push([descSetLayout = defaultMaterial.DescriptorSetLayout,
                                  pipelineLayout = defaultMaterial.MaterialPipelineLayout]() -> void {
                vkDestroyDescriptorSetLayout(VulkanContext::GetPrimaryLogicalDevice(), descSetLayout, nullptr);
                vkDestroyPipelineLayout(VulkanContext::GetPrimaryLogicalDevice(), pipelineLayout, nullptr);
            });

            // Create pipeline
            auto defaultMatPipelineConfig{ VulkanPipeline::GetDefaultPipelineConfigInfo() };

            defaultMatPipelineConfig.RenderPass = m_OffscreenMainRenderPass;
            defaultMatPipelineConfig.PipelineLayout = defaultMaterial.MaterialPipelineLayout;
            defaultMatPipelineConfig.ShaderStages = std::addressof(pipelineShaderStageCreateInfos);

            defaultMaterial.Pipeline.CreateGraphicsPipeline(defaultMatPipelineConfig);
        }

        // Init MRT pipeline ----------------------------------------------------------------------------------
        {
            // 1. [Load shaders]

            // Vertex shader
            ShaderCreateInfo vertexStage{};
            vertexStage.Directory = FileManager::GetAssetsRootPath() / "shaders/vulkan-spirv/MultipleRenderTargetsVert.sprv";
            vertexStage.Stage = ShaderStage::SHADER_VERTEX_STAGE;
            VulkanShader vertexShader{ vertexStage };

            // Fragment shader
            ShaderCreateInfo fragmentStage{};
            fragmentStage.Directory = FileManager::GetAssetsRootPath() / "shaders/vulkan-spirv/MultipleRenderTargetsFrag.sprv";
            fragmentStage.Stage = ShaderStage::SHADER_FRAGMENT_STAGE;
            VulkanShader fragmentShader{ fragmentStage };

            std::vector<VkPipelineShaderStageCreateInfo> pipelineShaderStageCreateInfos{};
            pipelineShaderStageCreateInfos.emplace_back(vertexShader.GetPipelineStageCreateInfo());
            pipelineShaderStageCreateInfos.emplace_back(fragmentShader.GetPipelineStageCreateInfo());

            //  2. [Init structure]
            const std::string mrtPipelineDataName{ "MRTPipeline" };
            m_MaterialInfo.try_emplace( mrtPipelineDataName, PipelineInfo{} );
            PipelineInfo& mrtPipelineInfo{ m_MaterialInfo[mrtPipelineDataName] };

            // 3. [Create Descriptor set layout]: three bindings in total for this pipeline. Vertex shader Uniform buffer, normal sampler, albedo sampler
            VkPipelineLayoutCreateInfo pipelineLayoutInfo{ VulkanUtils::Initializers::PipelineLayoutCreateInfo() };

            // Push constants
            pipelineLayoutInfo.pushConstantRangeCount = 0;
            pipelineLayoutInfo.pPushConstantRanges = nullptr;

            // First binding for the uniform buffer. Set 0, binding 0.
            VkDescriptorSetLayoutCreateInfo transformBindLayoutCreateInfo{ VulkanUtils::Initializers::DescriptorSetLayoutCreateInfo() };
            VkDescriptorSetLayoutBinding transformBind{ VulkanUtils::CreateDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0) };
            transformBindLayoutCreateInfo.bindingCount = 1;
            transformBindLayoutCreateInfo.pBindings = std::addressof(transformBind);

            // Second binding for the sampler color map. Set 0, binding 1.
            VkDescriptorSetLayoutCreateInfo albedoBindLayoutCreateInfo{ VulkanUtils::Initializers::DescriptorSetLayoutCreateInfo() };
            VkDescriptorSetLayoutBinding albedoBind{ VulkanUtils::CreateDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1) };
            albedoBindLayoutCreateInfo.bindingCount = 1;
            albedoBindLayoutCreateInfo.pBindings = std::addressof(albedoBind);

            // Second binding for the sampler normal map. Set 0, binding 2.
            VkDescriptorSetLayoutCreateInfo normalsBindLayoutCreateInfo{ VulkanUtils::Initializers::DescriptorSetLayoutCreateInfo() };
            VkDescriptorSetLayoutBinding normalsBind{ VulkanUtils::CreateDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2) };
            normalsBindLayoutCreateInfo.bindingCount = 1;
            normalsBindLayoutCreateInfo.pBindings = std::addressof(normalsBind);

            std::array<VkDescriptorSetLayoutBinding, 3> bindings{ transformBind, albedoBind, normalsBind };

            VkDescriptorSetLayoutCreateInfo layoutInfo{ VulkanUtils::Initializers::DescriptorSetLayoutCreateInfo() };
            layoutInfo.bindingCount = static_cast<UInt32_T>(bindings.size());
            layoutInfo.pBindings = bindings.data();

            if (vkCreateDescriptorSetLayout(VulkanContext::GetPrimaryLogicalDevice(), std::addressof(layoutInfo), nullptr, std::addressof( mrtPipelineInfo.DescriptorSetLayout ) ) != VK_SUCCESS) {
                MKT_THROW_RUNTIME_ERROR("Failed to create descriptor set layout!");
            }

            // 4. [Create Pipeline layout]
            std::array<VkDescriptorSetLayout, 1> descLayouts{ mrtPipelineInfo.DescriptorSetLayout };
            pipelineLayoutInfo.setLayoutCount = static_cast<UInt32_T>(descLayouts.size());
            pipelineLayoutInfo.pSetLayouts = descLayouts.data();

            if (vkCreatePipelineLayout(VulkanContext::GetPrimaryLogicalDevice(), std::addressof(pipelineLayoutInfo), nullptr, std::addressof( mrtPipelineInfo.MaterialPipelineLayout)) != VK_SUCCESS) {
                MKT_THROW_RUNTIME_ERROR("Failed to create pipeline layout");
            }

            DeletionQueue::Push([descSetLayout = mrtPipelineInfo.DescriptorSetLayout,
                                  pipelineLayout = mrtPipelineInfo.MaterialPipelineLayout]() -> void {
                vkDestroyDescriptorSetLayout(VulkanContext::GetPrimaryLogicalDevice(), descSetLayout, nullptr);
                vkDestroyPipelineLayout(VulkanContext::GetPrimaryLogicalDevice(), pipelineLayout, nullptr);
            });

            // Create pipeline
            auto defaultMatPipelineConfig{ VulkanPipeline::GetDefaultPipelineConfigInfo() };

            // Blend attachment states required for all color attachments.
            // This is important, as color write mask will otherwise be 0x0, and you
            // won't see anything rendered to the attachment.
            std::array<VkPipelineColorBlendAttachmentState, 3> blendAttachmentStates{};

            for (auto& blendAttachmentState : blendAttachmentStates) {
                blendAttachmentState.colorWriteMask = 0xF;
                blendAttachmentState.blendEnable = VK_FALSE;
            }

            defaultMatPipelineConfig.ColorBlendInfo.attachmentCount = static_cast<uint32_t>(blendAttachmentStates.size());
            defaultMatPipelineConfig.ColorBlendInfo.pAttachments = blendAttachmentStates.data();

            defaultMatPipelineConfig.RenderPass = m_DeferredRenderPass;
            defaultMatPipelineConfig.PipelineLayout = mrtPipelineInfo.MaterialPipelineLayout;
            defaultMatPipelineConfig.ShaderStages = std::addressof(pipelineShaderStageCreateInfos);

            mrtPipelineInfo.Pipeline.CreateGraphicsPipeline(defaultMatPipelineConfig);
        }

    }
}