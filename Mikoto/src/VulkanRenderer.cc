/**
 * VulkanRenderer.cc
 * Created by kate on 7/3/23.
 * */

// C++ Standard Library
#include <array>

// Third-Party Libraries
#include <volk.h>

// Project Headers
#include <Common/Common.hh>
#include <Core/FileManager.hh>
#include <Renderer/Vulkan/DeletionQueue.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanIndexBuffer.hh>
#include <Renderer/Vulkan/VulkanPBRMaterial.hh>
#include <Renderer/Vulkan/VulkanRenderer.hh>
#include <Renderer/Vulkan/VulkanVertexBuffer.hh>

#include "STL/Filesystem/PathBuilder.hh"

namespace Mikoto {

    auto VulkanRenderer::Init() -> void {
        InitializeCommands();
        PrepareOffscreenRender();
        InitializePipelinesData();
    }

    auto VulkanRenderer::EnableWireframeMode() -> void {
        m_UseWireframe = true;
    }

    auto VulkanRenderer::DisableWireframeMode() -> void {
        m_UseWireframe = false;
    }

    auto VulkanRenderer::SetClearColor( const glm::vec4& color ) -> void {
        m_ClearValues[COLOR_BUFFER].color = { { color.r, color.g, color.b, color.a } };
    }

    auto VulkanRenderer::SetClearColor( float red, float green, float blue, float alpha ) -> void {
        m_ClearValues[COLOR_BUFFER].color = { { red, green, blue, alpha } };
    }

    auto VulkanRenderer::SetViewport( float x, float y, float width, float height ) -> void {
        UpdateViewport( x, y, width, height );
    }

    auto VulkanRenderer::Shutdown() -> void {
        VulkanUtils::WaitOnDevice( VulkanContext::GetPrimaryLogicalDevice() );
    }

    auto VulkanRenderer::InitCommandBuffers() -> void {
        constexpr UInt32_T COMMAND_BUFFERS_COUNT{ 1 };

        VkCommandBufferAllocateInfo allocInfo{ VulkanUtils::Initializers::CommandBufferAllocateInfo() };
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = m_CommandPool.Get();
        allocInfo.commandBufferCount = COMMAND_BUFFERS_COUNT;

        if ( vkAllocateCommandBuffers(
                VulkanContext::GetPrimaryLogicalDevice(),
                std::addressof(allocInfo),
                std::addressof(m_DrawCommandBuffer) ) != VK_SUCCESS )
        {
            MKT_THROW_RUNTIME_ERROR( "Failed to allocate command buffer" );
        }

        DeletionQueue::Push( [cmdPoolHandle = m_CommandPool.Get(), cmdHandle = m_DrawCommandBuffer]() -> void {
            vkFreeCommandBuffers( VulkanContext::GetPrimaryLogicalDevice(), cmdPoolHandle, 1, std::addressof( cmdHandle ) );
        } );
    }

    auto VulkanRenderer::Draw() -> void {
        VkCommandBufferBeginInfo beginInfo{ VulkanUtils::Initializers::CommandBufferBeginInfo() };

        if ( vkBeginCommandBuffer( m_DrawCommandBuffer, &beginInfo ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "Failed to begin recording to command buffer" );
        }

        // Pre setup
        VkRenderPassBeginInfo renderPassInfo{ VulkanUtils::Initializers::RenderPassBeginInfo() };
        renderPassInfo.renderPass = m_OffscreenMainRenderPass;
        renderPassInfo.framebuffer = m_OffscreenFrameBuffer.Get();
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = m_OffscreenExtent;
        renderPassInfo.clearValueCount = static_cast<UInt32_T>( m_ClearValues.size() );
        renderPassInfo.pClearValues = m_ClearValues.data();

        if ( m_UseWireframe ) { SetClearColor( 1.0f, 1.0f, 1.0f, 1.0f ); }

        UpdateViewport( 0, static_cast<float>( m_OffscreenExtent.height ), static_cast<float>( m_OffscreenExtent.width ), -static_cast<float>( m_OffscreenExtent.height ) );
        UpdateScissor( 0, 0, { m_OffscreenExtent.width, m_OffscreenExtent.height } );

        // Set Viewport and Scissor
        vkCmdSetViewport( m_DrawCommandBuffer, 0, 1, std::addressof(m_OffscreenViewport) );
        vkCmdSetScissor( m_DrawCommandBuffer, 0, 1, std::addressof(m_OffscreenScissor) );

        // Begin Render pass commands recording
        vkCmdBeginRenderPass( m_DrawCommandBuffer, std::addressof(renderPassInfo), VK_SUBPASS_CONTENTS_INLINE );

        const VulkanPipeline* pipeline{ nullptr };
        const VkPipelineLayout* pipelineLayout{ nullptr };

        for ( const auto& [objectId, meshRenderInfo]: m_DrawQueue ) {
            if (!meshRenderInfo.Data) {
                MKT_CORE_LOGGER_WARN("Object data for {} is null", objectId);
            }
            else {
                const auto& meshInfo{ *meshRenderInfo.Data };
                auto& materialRef{ *meshRenderInfo.MaterialData };

                switch ( materialRef.GetType() ) {
                    case Type::MATERIAL_TYPE_STANDARD:
                        m_ActiveDefaultMaterial = dynamic_cast<VulkanStandardMaterial*>( std::addressof( materialRef ) );

                        {
                            // Ideally would want to take the transform from this mesh and not from the model
                            m_ActiveDefaultMaterial->SetProjection(meshRenderInfo.Data->Transform.Projection );
                            m_ActiveDefaultMaterial->SetView(meshRenderInfo.Data->Transform.View );
                            m_ActiveDefaultMaterial->SetTransform(meshRenderInfo.Data->Transform.Transform );
                            m_ActiveDefaultMaterial->UpdateLightsInfo();

                            m_ActiveDefaultMaterial->UploadUniformBuffers();

                            pipeline = std::addressof( m_MaterialInfo[StandardMaterial::GetName()].Pipeline );
                            pipelineLayout = std::addressof( m_MaterialInfo[StandardMaterial::GetName()].MaterialPipelineLayout );

                            m_ActiveDefaultMaterial->BindDescriptorSet( m_DrawCommandBuffer, *pipelineLayout );
                        }

                        break;
                    case Type::MATERIAL_TYPE_PBR:
                        MKT_THROW_RUNTIME_ERROR( "Not yet supported" );
                        break;
                }

                // Bind material pipeline
                pipeline->Bind( m_DrawCommandBuffer );

                // Bind vertex and index buffers
                std::dynamic_pointer_cast<VulkanVertexBuffer>(meshRenderInfo.Data->MeshData.Data->GetVertexBuffer() )->Bind(m_DrawCommandBuffer );
                std::dynamic_pointer_cast<VulkanIndexBuffer>(meshRenderInfo.Data->MeshData.Data->GetIndexBuffer() )->Bind(m_DrawCommandBuffer );

                // Draw call command
                vkCmdDrawIndexed(m_DrawCommandBuffer, std::dynamic_pointer_cast<VulkanIndexBuffer>(meshRenderInfo.Data->MeshData.Data->GetIndexBuffer() )->GetCount(), 1, 0, 0, 0 );
            }
        }

        // End Render pass commands recording
        vkCmdEndRenderPass( m_DrawCommandBuffer );

        // End command buffer recording
        if ( vkEndCommandBuffer( m_DrawCommandBuffer ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "Failed to record command buffer" );
        }
    }

    auto VulkanRenderer::CreateOffscreenRenderPass() -> void {
        // Color Attachment
        VkAttachmentDescription colorAttachmentDesc{};
        colorAttachmentDesc.format = m_ColorAttachmentFormat;
        colorAttachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;// VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

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
        // This dependency tells Vulkan that the depth attachment in a renderpass cannot
        // be used before previous render-passes have finished using it.
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
        info.attachmentCount = static_cast<UInt32_T>( attachmentDescriptions.size() );
        info.pAttachments = attachmentDescriptions.data();

        info.dependencyCount = static_cast<UInt32_T>( attachmentDependencies.size() );
        info.pDependencies = attachmentDependencies.data();

        info.subpassCount = 1;
        info.pSubpasses = &subpass;

        if ( vkCreateRenderPass( VulkanContext::GetPrimaryLogicalDevice(), &info, nullptr, &m_OffscreenMainRenderPass ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "Failed to create render pass for the Vulkan Renderer!" );
        }

        DeletionQueue::Push( [renderPass = m_OffscreenMainRenderPass]() -> void {
            vkDestroyRenderPass( VulkanContext::GetPrimaryLogicalDevice(), renderPass, nullptr );
        } );
    }

    auto VulkanRenderer::CreateOffscreenFramebuffers() -> void {
        std::array<VkImageView, 2> attachments{ m_OffscreenColorAttachment.GetView(), m_OffscreenDepthAttachment.GetView() };

        VkFramebufferCreateInfo createInfo{ VulkanUtils::Initializers::FramebufferCreateInfo() };
        createInfo.pNext = nullptr;
        createInfo.renderPass = m_OffscreenMainRenderPass;

        createInfo.width = m_OffscreenExtent.width;
        createInfo.height = m_OffscreenExtent.height;
        createInfo.layers = 1;

        createInfo.attachmentCount = static_cast<UInt32_T>( attachments.size() );
        createInfo.pAttachments = attachments.data();

        m_OffscreenFrameBuffer.OnCreate( createInfo );
    }

    auto VulkanRenderer::UpdateViewport( float x, float y, float width, float height ) -> void {
        m_OffscreenViewport = {
            .x = x,
            .y = y,
            .width = width,
            .height = height,
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
    }

    auto VulkanRenderer::UpdateScissor( Int32_T x, Int32_T y, VkExtent2D extent ) -> void {
        m_OffscreenScissor = {
            .offset{ x, y },
            .extent{ extent },
        };
    }

    auto VulkanRenderer::CreateOffscreenAttachments() -> void {
        // Color Buffer attachment
        VkImageCreateInfo colorAttachmentCreateInfo{ VulkanUtils::Initializers::ImageCreateInfo() };
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
        colorAttachmentViewCreateInfo.image = VK_NULL_HANDLE;// Set by OnCreate()
        colorAttachmentViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        colorAttachmentViewCreateInfo.format = colorAttachmentCreateInfo.format;// match formats for simplicity

        colorAttachmentViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        colorAttachmentViewCreateInfo.subresourceRange.baseMipLevel = 0;
        colorAttachmentViewCreateInfo.subresourceRange.levelCount = 1;
        colorAttachmentViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        colorAttachmentViewCreateInfo.subresourceRange.layerCount = 1;

        m_OffscreenColorAttachment.OnCreate( { colorAttachmentCreateInfo, colorAttachmentViewCreateInfo } );

        // Depth attachment
        VkImageCreateInfo depthAttachmentCreateInfo{ VulkanUtils::Initializers::ImageCreateInfo() };
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
        depthAttachmentViewCreateInfo.image = VK_NULL_HANDLE;// Set by OnCreate()
        depthAttachmentViewCreateInfo.format = depthAttachmentCreateInfo.format;
        depthAttachmentViewCreateInfo.subresourceRange.baseMipLevel = 0;
        depthAttachmentViewCreateInfo.subresourceRange.levelCount = 1;
        depthAttachmentViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        depthAttachmentViewCreateInfo.subresourceRange.layerCount = 1;
        depthAttachmentViewCreateInfo.subresourceRange.aspectMask =
                depthAttachmentCreateInfo.format < VK_FORMAT_D16_UNORM_S8_UINT ? VK_IMAGE_ASPECT_DEPTH_BIT : ( VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT );

        m_OffscreenDepthAttachment.OnCreate( { depthAttachmentCreateInfo, depthAttachmentViewCreateInfo } );
    }

    auto VulkanRenderer::PrepareOffscreenRender() -> void {
        m_OffscreenExtent.width = 1920;
        m_OffscreenExtent.height = 1032;
        m_ClearValues[ClearValueIndex::COLOR_BUFFER].color = { { 0.2f, 0.2f, 0.2f, 1.0f } };
        m_ClearValues[ClearValueIndex::DEPTH_BUFFER].depthStencil = { 1.0f, 0 };

        m_ColorAttachmentFormat = VulkanContext::FindSupportedFormat(
                VulkanContext::GetPrimaryPhysicalDevice(),
                { VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_D32_SFLOAT, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SRGB },
                VK_IMAGE_TILING_OPTIMAL,
                VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT );

        m_DepthAttachmentFormat = VulkanContext::FindSupportedFormat(
                VulkanContext::GetPrimaryPhysicalDevice(),
                { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT },
                VK_IMAGE_TILING_OPTIMAL,
                VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT );

        CreateOffscreenRenderPass();
        CreateOffscreenAttachments();
        CreateOffscreenFramebuffers();

        // https://www.saschawillems.de/blog/2019/03/29/flipping-the-vulkan-viewport/
        UpdateViewport( 0, static_cast<float>( m_OffscreenExtent.height ), static_cast<float>( m_OffscreenExtent.width ), -static_cast<float>( m_OffscreenExtent.height ) );
        UpdateScissor( 0, 0, { m_OffscreenExtent.width, m_OffscreenExtent.height } );
    }

    auto VulkanRenderer::SubmitToQueue() const -> void {
        VulkanContext::BatchCommandBuffer( m_DrawCommandBuffer );
    }

    auto VulkanRenderer::InitializeDefaultPipeline() -> void {
        // Vertex shader
        ShaderCreateInfo vertexStage{
            .Stage = SHADER_VERTEX_STAGE,
            .Directory = PathBuilder()
                                 .WithPath( FileManager::Assets::GetRootPath().string() )
                                 .WithPath( "Shaders/vulkan-spirv/StandardVertexShader.sprv" )
                                 .Build(),
        };

        // Vertex shader
        ShaderCreateInfo fragmentStage{
            .Stage = SHADER_FRAGMENT_STAGE,
            .Directory = PathBuilder()
                                 .WithPath( FileManager::Assets::GetRootPath().string() )
                                 .WithPath( "Shaders/vulkan-spirv/StandardFragmentShader.sprv" )
                                 .Build(),
        };

        VulkanShader vertexShader{ vertexStage };
        VulkanShader fragmentShader{ fragmentStage };

        // VkPipelineShaderStageCreateInfo pre-setup for the colored pipeline
        std::vector<VkPipelineShaderStageCreateInfo> pipelineShaderStageCreateInfos{};
        pipelineShaderStageCreateInfos.emplace_back( vertexShader.GetPipelineStageCreateInfo() );
        pipelineShaderStageCreateInfos.emplace_back( fragmentShader.GetPipelineStageCreateInfo() );

        //  DATA INITIALIZATION
        const std::string standardMaterialName{ StandardMaterial::GetName() };

        PipelineInfo materialSharedSpecificData{};
        m_MaterialInfo.try_emplace( standardMaterialName, std::move( materialSharedSpecificData ) );
        PipelineInfo& defaultMaterial{ m_MaterialInfo[standardMaterialName] };

        // Create the pipeline layout
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{ VulkanUtils::Initializers::PipelineLayoutCreateInfo() };
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;

        // First binding for the uniform buffer. Set 0, binding 0.
        VkDescriptorSetLayoutCreateInfo transformBindLayoutCreateInfo{ VulkanUtils::Initializers::DescriptorSetLayoutCreateInfo() };
        VkDescriptorSetLayoutBinding transformBind{ VulkanUtils::CreateDescriptorSetLayoutBinding( VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0 ) };
        transformBindLayoutCreateInfo.bindingCount = 1;
        transformBindLayoutCreateInfo.pBindings = &transformBind;

        // Second binding for the texture sampler. Set 0, binding 1.
        VkDescriptorSetLayoutCreateInfo textureBindLayoutCreateInfo{ VulkanUtils::Initializers::DescriptorSetLayoutCreateInfo() };
        VkDescriptorSetLayoutBinding textureBind{ VulkanUtils::CreateDescriptorSetLayoutBinding( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1 ) };
        textureBindLayoutCreateInfo.bindingCount = 1;
        textureBindLayoutCreateInfo.pBindings = &textureBind;

        // Second binding for the texture sampler. Set 0, binding 2.
        VkDescriptorSetLayoutCreateInfo lightUniformCreateInfo{ VulkanUtils::Initializers::DescriptorSetLayoutCreateInfo() };
        VkDescriptorSetLayoutBinding lightingBind{ VulkanUtils::CreateDescriptorSetLayoutBinding( VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 2 ) };
        lightUniformCreateInfo.bindingCount = 1;
        lightUniformCreateInfo.pBindings = &lightingBind;

        // Second binding for the texture sampler. Set 0, binding 3.
        VkDescriptorSetLayoutCreateInfo specularSamplerCreateInfo{ VulkanUtils::Initializers::DescriptorSetLayoutCreateInfo() };
        VkDescriptorSetLayoutBinding specularSamplerBind{ VulkanUtils::CreateDescriptorSetLayoutBinding( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3 ) };
        specularSamplerCreateInfo.bindingCount = 1;
        specularSamplerCreateInfo.pBindings = &specularSamplerBind;

        std::array<VkDescriptorSetLayoutBinding, 4> bindings{ transformBind, textureBind, lightingBind, specularSamplerBind };

        VkDescriptorSetLayoutCreateInfo layoutInfo{ VulkanUtils::Initializers::DescriptorSetLayoutCreateInfo() };
        layoutInfo.bindingCount = static_cast<UInt32_T>( bindings.size() );
        layoutInfo.pBindings = bindings.data();

        if ( vkCreateDescriptorSetLayout( VulkanContext::GetPrimaryLogicalDevice(), &layoutInfo, nullptr, std::addressof( defaultMaterial.DescriptorSetLayout ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "Failed to create descriptor set layout!" );
        }

        std::array<VkDescriptorSetLayout, 1> descLayouts{ defaultMaterial.DescriptorSetLayout };

        pipelineLayoutInfo.setLayoutCount = static_cast<UInt32_T>( descLayouts.size() );
        pipelineLayoutInfo.pSetLayouts = descLayouts.data();

        if ( vkCreatePipelineLayout( VulkanContext::GetPrimaryLogicalDevice(), std::addressof( pipelineLayoutInfo ), nullptr, std::addressof( defaultMaterial.MaterialPipelineLayout ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "Failed to create pipeline layout" );
        }

        DeletionQueue::Push( [descSetLayout = defaultMaterial.DescriptorSetLayout,
                              pipelineLayout = defaultMaterial.MaterialPipelineLayout]() -> void {
            vkDestroyDescriptorSetLayout( VulkanContext::GetPrimaryLogicalDevice(), descSetLayout, nullptr );
            vkDestroyPipelineLayout( VulkanContext::GetPrimaryLogicalDevice(), pipelineLayout, nullptr );
        } );

        // Create pipeline
        auto defaultMatPipelineConfig{ VulkanPipeline::GetDefaultPipelineConfigInfo() };
        defaultMatPipelineConfig.RenderPass = m_OffscreenMainRenderPass;
        defaultMatPipelineConfig.PipelineLayout = defaultMaterial.MaterialPipelineLayout;
        defaultMatPipelineConfig.ShaderStages = std::addressof( pipelineShaderStageCreateInfos );

        defaultMaterial.Pipeline.CreateGraphicsPipeline( defaultMatPipelineConfig );
    }

    auto VulkanRenderer::InitializeWireFramePipeline() -> void {
        // Shaders for this pipeline
        // Vertex shader
        ShaderCreateInfo vertexStage{};
        vertexStage.Directory = FileManager::Assets::GetRootPath() / "Shaders\\vulkan-spirv\\StandardVertexShader.sprv";
        vertexStage.Stage = ShaderStage::SHADER_VERTEX_STAGE;

        VulkanShader vertexShader{ vertexStage };

        // Vertex shader
        ShaderCreateInfo fragmentStage{};
        fragmentStage.Directory = FileManager::Assets::GetRootPath() / "Shaders\\vulkan-spirv\\ColoredFragmentShader.sprv";
        fragmentStage.Stage = ShaderStage::SHADER_FRAGMENT_STAGE;

        VulkanShader fragmentShader{ fragmentStage };

        // VkPipelineShaderStageCreateInfo pre-setup for the colored pipeline
        std::vector<VkPipelineShaderStageCreateInfo> pipelineShaderStageCreateInfos{};
        pipelineShaderStageCreateInfos.emplace_back( vertexShader.GetPipelineStageCreateInfo() );
        pipelineShaderStageCreateInfos.emplace_back( fragmentShader.GetPipelineStageCreateInfo() );

        //  DATA INITIALIZATION
        const std::string wireframeMaterialName{ "WireframeMaterial" };
        m_MaterialInfo.insert( std::make_pair( wireframeMaterialName, PipelineInfo{} ) );
        PipelineInfo& wireframeMaterial{ m_MaterialInfo[wireframeMaterialName] };

        // Create the pipeline layout
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{ VulkanUtils::Initializers::PipelineLayoutCreateInfo() };
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;

        // First binding for the uniform buffer. Set 0, binding 0. This pipeline just expects a single descriptor set with uniform buffer data
        VkDescriptorSetLayoutCreateInfo transformBindLayoutCreateInfo{ VulkanUtils::Initializers::DescriptorSetLayoutCreateInfo() };
        VkDescriptorSetLayoutBinding transformBind{ VulkanUtils::CreateDescriptorSetLayoutBinding( VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0 ) };
        transformBindLayoutCreateInfo.bindingCount = 1;
        transformBindLayoutCreateInfo.pBindings = std::addressof( transformBind );

        std::array<VkDescriptorSetLayoutBinding, 1> bindings{ transformBind };

        VkDescriptorSetLayoutCreateInfo layoutInfo{ VulkanUtils::Initializers::DescriptorSetLayoutCreateInfo() };
        layoutInfo.bindingCount = static_cast<UInt32_T>( bindings.size() );
        layoutInfo.pBindings = bindings.data();

        if ( vkCreateDescriptorSetLayout( VulkanContext::GetPrimaryLogicalDevice(), std::addressof( layoutInfo ), nullptr, std::addressof( wireframeMaterial.DescriptorSetLayout ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "Failed to create descriptor set layout!" );
        }

        std::array<VkDescriptorSetLayout, 1> descLayouts{ wireframeMaterial.DescriptorSetLayout };

        pipelineLayoutInfo.setLayoutCount = static_cast<UInt32_T>( descLayouts.size() );
        pipelineLayoutInfo.pSetLayouts = descLayouts.data();

        if ( vkCreatePipelineLayout( VulkanContext::GetPrimaryLogicalDevice(), std::addressof( pipelineLayoutInfo ), nullptr, std::addressof( wireframeMaterial.MaterialPipelineLayout ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "Failed to create colored pipeline layout" );
        }

        DeletionQueue::Push( [descSetLayout = wireframeMaterial.DescriptorSetLayout,
                              pipelineLayout = wireframeMaterial.MaterialPipelineLayout]() -> void {
            vkDestroyDescriptorSetLayout( VulkanContext::GetPrimaryLogicalDevice(), descSetLayout, nullptr );
            vkDestroyPipelineLayout( VulkanContext::GetPrimaryLogicalDevice(), pipelineLayout, nullptr );
        } );

        // Create the pipeline
        auto wireframePipelineConfig{ VulkanPipeline::GetDefaultPipelineConfigInfo() };

        constexpr float GPU_STANDARD_LINE_WIDTH{ 1.0f };
        wireframePipelineConfig.RasterizationInfo.polygonMode = VK_POLYGON_MODE_LINE;
        wireframePipelineConfig.RasterizationInfo.cullMode = VK_CULL_MODE_NONE;
        wireframePipelineConfig.RasterizationInfo.lineWidth = GPU_STANDARD_LINE_WIDTH;

        wireframePipelineConfig.RenderPass = m_OffscreenMainRenderPass;
        wireframePipelineConfig.PipelineLayout = wireframeMaterial.MaterialPipelineLayout;
        wireframePipelineConfig.ShaderStages = std::addressof( pipelineShaderStageCreateInfos );

        wireframeMaterial.Pipeline.CreateGraphicsPipeline( wireframePipelineConfig );
    }

    auto VulkanRenderer::InitializePipelinesData() -> void {
        InitializeDefaultPipeline();

        // Create the wireframe pipeline
        // InitializeWireFramePipeline();
    }

    auto VulkanRenderer::Flush() -> void {
        // Record drawing commands
        Draw();

        // Submit recorded commands
        SubmitToQueue();
    }

    auto VulkanRenderer::QueueForDrawing( const std::string& id, std::shared_ptr<GameObject>&& data, std::shared_ptr<Material>&& material ) -> void {
        auto it{ m_DrawQueue.find( id ) };
        MeshRenderInfo info{
            .Data = data,
            .MaterialData = std::dynamic_pointer_cast<VulkanStandardMaterial>( material ),
        };

        if ( it != m_DrawQueue.end() ) {
            it->second = info;

        } else {
            m_DrawQueue.emplace( id, info );
        }
    }
    auto VulkanRenderer::RemoveFromRenderQueue( const std::string& id ) -> bool {
        auto result{ false };

        try {
            const auto eraseCount{ m_DrawQueue.erase( id ) };
            result = eraseCount != 0;

        } catch ( std::exception& exception ) {
            MKT_THROW_RUNTIME_ERROR( fmt::format( "VulkanRenderer - {}", exception.what() ) );
        }

        return result;
    }

    auto VulkanRenderer::InitializeCommands() -> void {
        VkCommandPoolCreateInfo createInfo{ VulkanUtils::Initializers::CommandPoolCreateInfo() };
        createInfo.flags = 0;
        createInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        createInfo.queueFamilyIndex = VulkanUtils::FindQueueFamilies( VulkanContext::GetPrimaryPhysicalDevice() , VulkanContext::GetSurface() ).GraphicsFamilyIndex;

        m_CommandPool.Create( createInfo );

        InitCommandBuffers();
    }
}