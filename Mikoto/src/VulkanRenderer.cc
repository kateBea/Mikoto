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
#include <Core/System/FileSystem.hh>
#include <Core/System/TimeSystem.hh>
#include <Library/Filesystem/PathBuilder.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanDeletionQueue.hh>
#include <Renderer/Vulkan/VulkanDescriptorManager.hh>
#include <Renderer/Vulkan/VulkanIndexBuffer.hh>
#include <Renderer/Vulkan/VulkanPBRMaterial.hh>
#include <Renderer/Vulkan/VulkanRenderer.hh>
#include <Renderer/Vulkan/VulkanShaderLibrary.hh>
#include <Renderer/Vulkan/VulkanStandardMaterial.hh>
#include <Renderer/Vulkan/VulkanVertexBuffer.hh>
#include <ranges>

namespace Mikoto {

    static auto GetDefaultGraphicsPipelineConfigInfo() -> VulkanPipelineCreateInfo {
        VulkanPipelineCreateInfo configInfo{};

        // [Input assembly]
        configInfo.InputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        configInfo.InputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;// Every three vertices are group together into a separate triangle
        configInfo.InputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

        // [Viewport and Scissor]
        configInfo.ViewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        configInfo.ViewportInfo.viewportCount = 1;// VK_DYNAMIC_VIEWPORT_WITH_COUNT has to be set for this to be 0
        configInfo.ViewportInfo.pViewports = nullptr;
        configInfo.ViewportInfo.scissorCount = 1;// VK_DYNAMIC_SCISSOR_WITH_COUNT has to be set for this to be 0
        configInfo.ViewportInfo.pScissors = nullptr;

        constexpr float GPU_STANDARD_LINE_WIDTH{ 1.0f };
        configInfo.RasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        configInfo.RasterizationInfo.depthClampEnable = VK_FALSE;
        configInfo.RasterizationInfo.rasterizerDiscardEnable = VK_FALSE;// requires extension if enabled
        configInfo.RasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
        // The maximum line width that is supported depends on the hardware, any line thicker than 1.0f requires you to enable the wideLines GPU feature.
        configInfo.RasterizationInfo.lineWidth = configInfo.RasterizationInfo.polygonMode == VK_POLYGON_MODE_LINE ? GPU_STANDARD_LINE_WIDTH : 0.0f;
        configInfo.RasterizationInfo.cullMode = VK_CULL_MODE_NONE;
        configInfo.RasterizationInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        configInfo.RasterizationInfo.depthBiasEnable = VK_FALSE;
        configInfo.RasterizationInfo.depthBiasConstantFactor = 0.0f;
        configInfo.RasterizationInfo.depthBiasClamp = 0.0f;
        configInfo.RasterizationInfo.depthBiasSlopeFactor = 0.0f;

        configInfo.MultisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        configInfo.MultisampleInfo.sampleShadingEnable = VK_FALSE;
        configInfo.MultisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        configInfo.MultisampleInfo.minSampleShading = 1.0f;         // Optional
        configInfo.MultisampleInfo.pSampleMask = nullptr;           // Optional
        configInfo.MultisampleInfo.alphaToCoverageEnable = VK_FALSE;// Optional
        configInfo.MultisampleInfo.alphaToOneEnable = VK_FALSE;     // Optional

        // Blending enabled by default
        configInfo.ColorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        configInfo.ColorBlendAttachment.blendEnable = VK_TRUE;
        configInfo.ColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        configInfo.ColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        configInfo.ColorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        configInfo.ColorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        configInfo.ColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        configInfo.ColorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        configInfo.ColorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        configInfo.ColorBlendInfo.logicOpEnable = VK_FALSE;
        configInfo.ColorBlendInfo.logicOp = VK_LOGIC_OP_COPY;
        configInfo.ColorBlendInfo.attachmentCount = 1;
        configInfo.ColorBlendInfo.pAttachments = &configInfo.ColorBlendAttachment;
        configInfo.ColorBlendInfo.blendConstants[0] = 0.0f;
        configInfo.ColorBlendInfo.blendConstants[1] = 0.0f;
        configInfo.ColorBlendInfo.blendConstants[2] = 0.0f;
        configInfo.ColorBlendInfo.blendConstants[3] = 0.0f;

        configInfo.DepthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        configInfo.DepthStencilInfo.depthTestEnable = VK_TRUE;
        configInfo.DepthStencilInfo.depthWriteEnable = VK_TRUE;
        configInfo.DepthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        configInfo.DepthStencilInfo.depthBoundsTestEnable = VK_FALSE;
        configInfo.DepthStencilInfo.stencilTestEnable = VK_TRUE;  // Enable stencil test
        configInfo.DepthStencilInfo.back.compareOp = VK_COMPARE_OP_ALWAYS;  // Always pass
        configInfo.DepthStencilInfo.back.failOp = VK_STENCIL_OP_REPLACE;
        configInfo.DepthStencilInfo.back.depthFailOp = VK_STENCIL_OP_REPLACE;
        configInfo.DepthStencilInfo.back.passOp = VK_STENCIL_OP_REPLACE;  // Write stencil value
        configInfo.DepthStencilInfo.back.reference = 1;  // Stencil value to write
        configInfo.DepthStencilInfo.back.compareMask = 0xFF;
        configInfo.DepthStencilInfo.back.writeMask = 0xFF;
        configInfo.DepthStencilInfo.front = configInfo.DepthStencilInfo.back;  // Use default settings for front faces

        // VK_DYNAMIC_STATE_VERTEX_INPUT_EXT can reduce the amount of pipelines the application needs to create
        // because it allows for vertex input binding and attribute descriptions to be dynamic. This is, of course, not a
        // core feature as of Vulkan 1.3 and requires to be enabled when creating the device on which this pipeline will be created
        // Make it static because pDynamicStates does not persist the value beyond this scope

        static constexpr std::array dynamicStates{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_LINE_WIDTH, /*VK_DYNAMIC_STATE_VERTEX_INPUT_EXT*/ };
        configInfo.DynamicStateEnables = dynamicStates;
        configInfo.DynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        configInfo.DynamicStateInfo.pDynamicStates = configInfo.DynamicStateEnables.data();
        configInfo.DynamicStateInfo.dynamicStateCount = configInfo.DynamicStateEnables.size();
        configInfo.DynamicStateInfo.flags = 0;

        return configInfo;
    }

    static auto GetDefaultComputePipelineConfigInfo() -> VulkanPipelineCreateInfo {
        const VulkanPipelineCreateInfo configInfo{};

        VkComputePipelineCreateInfo pipelineInfo{ VulkanHelpers::Initializers::ComputePipelineCreateInfo() };

        return configInfo;
    }


    VulkanRenderer::VulkanRenderer( const VulkanRendererCreateInfo& createInfo )
        : m_OffscreenExtent{
              .width{ createInfo.Info.ViewportWidth },
              .height{ createInfo.Info.ViewportHeight }
          },
          m_Device{ std::addressof( VulkanContext::Get().GetDevice() ) } {}

    auto VulkanRenderer::Init() -> bool {
        MKT_CORE_LOGGER_INFO( "VulkanRenderer::Init - Initializing Vulkan Renderer." );

        bool success{ true };

        try {
            CreateCommandPools();
            CreateCommandBuffers();

            PrepareOffscreenRender();

            CreateRendererPipelines();
        } catch ( std::exception& exception ) {
            MKT_CORE_LOGGER_ERROR( "VulkanRenderer::Init - Exception {}", exception.what() );
            success = false;
        }

        MKT_CORE_LOGGER_INFO( "VulkanRenderer::Init - Exiting Vulkan Renderer initialization." );

        return success;
    }

    auto VulkanRenderer::SetViewport( const float x, const float y, const float width, const float height ) -> void {
        // We need to update the viewport and scissor for the offscreen render
        // If the values are out of range for the current render images we might need to recreate the offscreen render images and framebuffers
        UpdateViewport( x, y, width, height );
    }
    void VulkanRenderer::SetRenderMode( Size_T mode ) {
        m_RenderMode = mode;
    }

    auto VulkanRenderer::Shutdown() -> void {
        m_Device->WaitIdle();

        m_OffscreenColorAttachment = nullptr;
        m_OffscreenDepthAttachment = nullptr;
    }

    auto VulkanRenderer::BeginFrame() -> void {
        // Checks
        if ( m_Camera == nullptr ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanRenderer::BeginFrame - Camera for rendering is null. Forgot to call SetCamera() ?" );
        }
    }

    auto VulkanRenderer::EndFrame() -> void {
        Flush();
    }

    auto VulkanRenderer::EnableWireframe( bool enable ) -> void {
        m_WireframeEnable = enable;
    }

    auto VulkanRenderer::RemoveLight( const UInt64_T id ) -> bool {
        const UInt64_T count{ m_Lights.erase( id ) };

        return count != 0;
    }

    auto VulkanRenderer::AddLight( const UInt64_T id, const LightData& data, LightType activeType ) -> bool {
        const auto itFind{ m_Lights.find( id ) };

        if ( itFind != m_Lights.end() ) {
            itFind->second.Data = std::addressof( data );
            itFind->second.ActiveType = activeType;

            return true;
        }

        auto [it, success]{
            m_Lights.try_emplace( id, LightRenderInfo{
                                              .Data{ std::addressof( data ) },
                                              .ActiveType{ activeType } } )
        };

        return success;
    }

    auto VulkanRenderer::SetupCubeMap( const TextureCubeMap* cubeMap ) -> void {
        // Preferably dynamic cast, but base class is not polymorphic (not single virtual method) for now
        m_CubeMap = static_cast<const VulkanTextureCubeMap *>( cubeMap );
    }

    auto VulkanRenderer::CreateCommandBuffers() -> void {
        // Create as many as images we have to render
        constexpr UInt32_T COMMAND_BUFFERS_COUNT{ 1 };

        VkCommandBufferAllocateInfo allocInfo{ VulkanHelpers::Initializers::CommandBufferAllocateInfo() };
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = m_CommandPool->Get();
        allocInfo.commandBufferCount = COMMAND_BUFFERS_COUNT;

        m_DrawCommandBuffer = *m_CommandPool->AllocateCommandBuffer( allocInfo );

        // Compute command buffer
        m_ComputeCommandBuffer = *m_CommandPool->AllocateCommandBuffer( allocInfo );
    }

    void VulkanRenderer::SetupObjectOutline(const MeshRenderInfo& meshRenderInfo) {
        const VulkanPipeline* pipeline{ nullptr };

        VulkanPBRMaterial* pbrMaterial{ dynamic_cast<VulkanPBRMaterial*>( meshRenderInfo.MaterialData ) };

        // Setup render mode
        pbrMaterial->SetRenderMode( m_RenderMode );
        pbrMaterial->EnableWireframe( m_WireframeEnable ? MKT_SHADER_TRUE : MKT_SHADER_FALSE );

        // The material will store its passes so we dont have to do the switch stamentnt below
        auto findIt{ m_Pipelines.find( MATERIAL_PASS_OUTLINE ) };

        pipeline = findIt != m_Pipelines.end() ? std::addressof( findIt->second ) : nullptr;
        if ( pipeline == nullptr ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanRenderer::RecordCommands - Pipeline objects are null." );
        }

        pbrMaterial->SetProjection( m_Camera->GetProjection() );
        pbrMaterial->SetView( m_Camera->GetViewMatrix() );
        pbrMaterial->SetTransform( meshRenderInfo.MeshRenderInfo::Transform );
        pbrMaterial->SetViewPosition( m_Camera->GetPosition() );

        pbrMaterial->ResetLights();

        for ( const auto& lightInfo: m_Lights | std::views::values ) {
            pbrMaterial->UpdateLightsInfo( *lightInfo.Data, lightInfo.ActiveType );
        }

        pbrMaterial->UploadUniformBuffers();
        pbrMaterial->BindDescriptorSet( m_DrawCommandBuffer, pipeline->GetLayout() );

        pipeline->Bind( m_DrawCommandBuffer );

        const VulkanVertexBuffer* vulkanVertexBuffer{ dynamic_cast<const VulkanVertexBuffer*>( meshRenderInfo.Object->GetVertexBuffer() ) };
        const VulkanIndexBuffer* vulkanIndexBuffer{ dynamic_cast<const VulkanIndexBuffer*>( meshRenderInfo.Object->GetIndexBuffer() ) };

        vulkanVertexBuffer->Bind( m_DrawCommandBuffer );
        vulkanIndexBuffer->Bind( m_DrawCommandBuffer );

        vkCmdDrawIndexed( m_DrawCommandBuffer, vulkanIndexBuffer->GetCount(), 1, 0, 0, 0 );
    }
    auto VulkanRenderer::RecordCommands() -> void {
        VkCommandBufferBeginInfo beginInfo{ VulkanHelpers::Initializers::CommandBufferBeginInfo() };

        if ( vkBeginCommandBuffer( m_DrawCommandBuffer, std::addressof( beginInfo ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanRenderer - Failed to begin recording to command buffer." );
        }

        // Clear values
        m_ClearValues[1].depthStencil = { 1.0f, 0 };
        m_ClearValues[0].color = { { m_ClearColor.r, m_ClearColor.g, m_ClearColor.b, m_ClearColor.a } };

        VkRenderPassBeginInfo renderPassInfo{ VulkanHelpers::Initializers::RenderPassBeginInfo() };
        renderPassInfo.renderPass = m_OffscreenMainRenderPass;
        renderPassInfo.framebuffer = m_OffscreenFrameBuffer->Get();
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = m_OffscreenExtent;
        renderPassInfo.clearValueCount = static_cast<UInt32_T>( m_ClearValues.size() );
        renderPassInfo.pClearValues = m_ClearValues.data();

        UpdateViewport( 0, 0, static_cast<float>( m_OffscreenExtent.width ), static_cast<float>( m_OffscreenExtent.height ) );
        UpdateScissor( 0, 0, { m_OffscreenExtent.width, m_OffscreenExtent.height } );

        vkCmdSetViewport( m_DrawCommandBuffer, 0, 1, std::addressof( m_OffscreenViewport ) );
        vkCmdSetScissor( m_DrawCommandBuffer, 0, 1, std::addressof( m_OffscreenScissor ) );

        vkCmdBeginRenderPass( m_DrawCommandBuffer, std::addressof( renderPassInfo ), VK_SUBPASS_CONTENTS_INLINE );

        for ( const auto& [objectId, meshRenderInfo]: m_DrawQueue ) {

            if ( !meshRenderInfo.Object ) {
                MKT_CORE_LOGGER_WARN( "VulkanRenderer::RecordCommands - Object data for {} is null.", objectId );

            } else {
                if ( meshRenderInfo.MaterialData->GetType() == MaterialType::STANDARD ) {
                    const VulkanPipeline* pipeline{ nullptr };

                    VulkanStandardMaterial* standardMaterial{ dynamic_cast<VulkanStandardMaterial*>( meshRenderInfo.MaterialData ) };

                    // The material will store its passes so we dont have to do the switch stamentnt below
                    auto findIt{ m_Pipelines.find( standardMaterial->GetPass() ) };

                    pipeline = findIt != m_Pipelines.end() ? std::addressof( findIt->second ) : nullptr;
                    if ( pipeline == nullptr ) {
                        MKT_THROW_RUNTIME_ERROR( "VulkanRenderer::RecordCommands - Pipeline objects are null." );
                    }

                    standardMaterial->SetProjection( m_Camera->GetProjection() );
                    standardMaterial->SetView( m_Camera->GetViewMatrix() );
                    standardMaterial->SetTransform( meshRenderInfo.Transform );

                    standardMaterial->SetViewPosition( m_Camera->GetPosition() );

                    standardMaterial->ResetLights();

                    for ( const auto& lightInfo: m_Lights | std::views::values ) {
                        standardMaterial->UpdateLightsInfo( *lightInfo.Data, lightInfo.ActiveType );
                    }

                    standardMaterial->UploadUniformBuffers();
                    standardMaterial->BindDescriptorSet( m_DrawCommandBuffer, pipeline->GetLayout() );

                    pipeline->Bind( m_DrawCommandBuffer );

                    const VulkanVertexBuffer* vulkanVertexBuffer{ dynamic_cast<const VulkanVertexBuffer*>( meshRenderInfo.Object->GetVertexBuffer() ) };
                    const VulkanIndexBuffer* vulkanIndexBuffer{ dynamic_cast<const VulkanIndexBuffer*>( meshRenderInfo.Object->GetIndexBuffer() ) };

                    vulkanVertexBuffer->Bind( m_DrawCommandBuffer );
                    vulkanIndexBuffer->Bind( m_DrawCommandBuffer );

                    vkCmdDrawIndexed( m_DrawCommandBuffer, vulkanIndexBuffer->GetCount(), 1, 0, 0, 0 );
                } else if ( meshRenderInfo.MaterialData->GetType() == MaterialType::PBR ) {
                    const VulkanPipeline* pipeline{ nullptr };

                    VulkanPBRMaterial* pbrMaterial{ dynamic_cast<VulkanPBRMaterial*>( meshRenderInfo.MaterialData ) };

                    // Setup render mode
                    pbrMaterial->SetRenderMode( m_RenderMode );
                    pbrMaterial->EnableWireframe( m_WireframeEnable ? MKT_SHADER_TRUE : MKT_SHADER_FALSE );

                    // The material will store its passes so we dont have to do the switch stamentnt below
                    auto findIt{ m_Pipelines.find( m_WireframeEnable ? MATERIAL_PASS_WIREFRAME : pbrMaterial->GetPass() ) };

                    pipeline = findIt != m_Pipelines.end() ? std::addressof( findIt->second ) : nullptr;
                    if ( pipeline == nullptr ) {
                        MKT_THROW_RUNTIME_ERROR( "VulkanRenderer::RecordCommands - Pipeline objects are null." );
                    }

                    pbrMaterial->SetProjection( m_Camera->GetProjection() );
                    pbrMaterial->SetView( m_Camera->GetViewMatrix() );
                    pbrMaterial->SetTransform( meshRenderInfo.Transform );
                    pbrMaterial->SetViewPosition( m_Camera->GetPosition() );

                    pbrMaterial->ResetLights();

                    for ( const auto& lightInfo: m_Lights | std::views::values ) {
                        pbrMaterial->UpdateLightsInfo( *lightInfo.Data, lightInfo.ActiveType );
                    }

                    pbrMaterial->UploadUniformBuffers();
                    pbrMaterial->BindDescriptorSet( m_DrawCommandBuffer, pipeline->GetLayout() );

                    pipeline->Bind( m_DrawCommandBuffer );

                    const VulkanVertexBuffer* vulkanVertexBuffer{ dynamic_cast<const VulkanVertexBuffer*>( meshRenderInfo.Object->GetVertexBuffer() ) };
                    const VulkanIndexBuffer* vulkanIndexBuffer{ dynamic_cast<const VulkanIndexBuffer*>( meshRenderInfo.Object->GetIndexBuffer() ) };

                    vulkanVertexBuffer->Bind( m_DrawCommandBuffer );
                    vulkanIndexBuffer->Bind( m_DrawCommandBuffer );

                    vkCmdDrawIndexed( m_DrawCommandBuffer, vulkanIndexBuffer->GetCount(), 1, 0, 0, 0 );
                }

                //SetupObjectOutline(meshRenderInfo);
            }
        }

        vkCmdEndRenderPass( m_DrawCommandBuffer );

        if ( vkEndCommandBuffer( m_DrawCommandBuffer ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanRenderer::RecordCommands - Failed to end recording command buffer" );
        }
    }

    auto VulkanRenderer::RecordComputeCommands() -> void {
        //RecordComputeCommandsDEBUG();
    }

    auto VulkanRenderer::RecordComputeCommandsDEBUG() -> void {
        // This serves as an example on how to record compute shader commands at its simple level
        // This everything needed for this function is in this scope declared with static storage to
        // the objects being destroyed. We are simply reading from the buffer as can be seen below
        // with no synchronization whether the operations are completed just for the example sake

        // VMA will probaly complain as the buffer here has storage duration and is last to get destroyed

        // Compute pipelines consist of one stage, the compute shader, therefore they take one shader stage on construction
        // Compute shaders operate independently, with no user-defined inputs or outputs,
        // meaning there’s no direct way for consecutive compute shaders to communicate within
        // a single pipeline. Each dispatch runs a single compute shader stage, and that’s it.
        // If you need multiple compute shaders in sequence, you must create separate pipelines
        // for each shader and use proper synchronization to manage data transfer between them

        // DEBUG
        static std::vector<float> values(10);

        VkBufferCreateInfo stagingBufferInfo{ VulkanHelpers::Initializers::BufferCreateInfo() };
        stagingBufferInfo.pNext = nullptr;

        stagingBufferInfo.size = static_cast<UInt32_T>( values.size() * sizeof(float));
        stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

        //let the VMA library know that this data should be on CPU RAM
        VmaAllocationCreateInfo vmaStagingAllocationCreateInfo{};
        vmaStagingAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
        vmaStagingAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        const VulkanBufferCreateInfo stagingBufferBufferCreateInfo{
            .BufferCreateInfo{ stagingBufferInfo },
            .AllocationCreateInfo{ vmaStagingAllocationCreateInfo },
            .WantMapping{ true }
        };

        static Scope_T<VulkanBuffer> stagingBuffer{ VulkanBuffer::Create( stagingBufferBufferCreateInfo ) };
        static bool first{ true };
        static VkDescriptorSet s_DescriptorSet{};
        static TimeSystem& timeSystem{ Engine::GetSystem<TimeSystem>() };

        if (first) {
            const VkDescriptorSetLayout& descriptorSetLayout{ VulkanContext::Get().GetDescriptorSetLayouts( DESCRIPTOR_SET_LAYOUT_COMPUTE_PIPELINE ) };

            const VulkanDevice& device{ VulkanContext::Get().GetDevice() };
            VulkanDescriptorAllocator& descriptorAllocator{ VulkanContext::Get().GetDescriptorAllocator() };

            s_DescriptorSet = *descriptorAllocator.Allocate( device.GetLogicalDevice(), descriptorSetLayout );

            VulkanDescriptorWriter descWriters{};

            descWriters
                .WriteBuffer( 0, stagingBuffer->Get(), stagingBuffer->GetSize(), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
                .UpdateSet( device.GetLogicalDevice(), s_DescriptorSet );

            first = false;
        } else {
            std::memcpy(values.data(), stagingBuffer->GetMappedPtr(), stagingBuffer->GetSize());
        }

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(m_ComputeCommandBuffer, &beginInfo) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("VulkanRenderer::RecordComputeCommands - Failed to begin recording command buffer!");
        }

        auto findIt{ m_Pipelines.find( MATERIAL_PASS_COMPUTE ) };

        const VulkanPipeline* computePipeline{ findIt == m_Pipelines.end() ? nullptr : std::addressof( findIt->second ) };

        vkCmdBindPipeline(m_ComputeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline->Get());
        vkCmdBindDescriptorSets(m_ComputeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline->GetLayout(), 0, 1, std::addressof( s_DescriptorSet ), 0, 0);

        vkCmdDispatch(m_ComputeCommandBuffer, 10, 0, 0);

        if (vkEndCommandBuffer(m_ComputeCommandBuffer) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("VulkanRenderer::RecordComputeCommands - Failed to record compute commands");
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

        std::array attachmentDependencies{ colorAttachmentDependency, deptAttachmentDependency };
        std::array attachmentDescriptions{ colorAttachmentDesc, depthAttachmentDesc };

        VkRenderPassCreateInfo info{ VulkanHelpers::Initializers::RenderPassCreateInfo() };
        info.attachmentCount = static_cast<UInt32_T>( attachmentDescriptions.size() );
        info.pAttachments = attachmentDescriptions.data();

        info.dependencyCount = static_cast<UInt32_T>( attachmentDependencies.size() );
        info.pDependencies = attachmentDependencies.data();

        info.subpassCount = 1;
        info.pSubpasses = &subpass;

        if ( vkCreateRenderPass( m_Device->GetLogicalDevice(), &info, nullptr, &m_OffscreenMainRenderPass ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "Failed to create render pass for the Vulkan Renderer!" );
        }

        VulkanDeletionQueue::Push( [device = m_Device->GetLogicalDevice(), renderPass = m_OffscreenMainRenderPass]() -> void {
            vkDestroyRenderPass( device, renderPass, nullptr );
        } );
    }

    auto VulkanRenderer::CreateOffscreenFramebuffers() -> void {
        const std::array attachments{ m_OffscreenColorAttachment->GetView(), m_OffscreenDepthAttachment->GetView() };

        VkFramebufferCreateInfo createInfo{ VulkanHelpers::Initializers::FramebufferCreateInfo() };
        createInfo.pNext = nullptr;
        createInfo.renderPass = m_OffscreenMainRenderPass;

        createInfo.width = m_OffscreenExtent.width;
        createInfo.height = m_OffscreenExtent.height;
        createInfo.layers = 1;

        createInfo.attachmentCount = static_cast<UInt32_T>( attachments.size() );
        createInfo.pAttachments = attachments.data();

        VulkanFrameBufferCreateInfo frameBufferCreateInfo{
            .CreateInfo{ createInfo },
        };

        m_OffscreenFrameBuffer = CreateScope<VulkanFrameBuffer>( frameBufferCreateInfo );
    }

    auto VulkanRenderer::UpdateViewport( const float x, const float y, const float width, const float height ) -> void {
        // https://www.saschawillems.de/blog/2019/03/29/flipping-the-vulkan-viewport/
        // As the engine uses Vulkan 1.3, VK_KHR_MAINTENANCE1  is not required since its part of the Core Api since 1.1

        m_OffscreenViewport = {
            .x = x,
            .y = y,
            .width = width,
            .height = height,
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
    }

    auto VulkanRenderer::UpdateScissor( const Int32_T x, const Int32_T y, VkExtent2D extent ) -> void {
        m_OffscreenScissor = {
            .offset{ x, y },
            .extent{ extent },
        };
    }

    auto VulkanRenderer::CreateOffscreenAttachments() -> void {
        // Color Buffer attachment
        VkImageCreateInfo colorAttachmentCreateInfo{ VulkanHelpers::Initializers::ImageCreateInfo() };
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

        VkImageViewCreateInfo colorAttachmentViewCreateInfo{ VulkanHelpers::Initializers::ImageViewCreateInfo() };
        colorAttachmentViewCreateInfo.image = VK_NULL_HANDLE;
        colorAttachmentViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        colorAttachmentViewCreateInfo.format = colorAttachmentCreateInfo.format;// match formats for simplicity

        colorAttachmentViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        colorAttachmentViewCreateInfo.subresourceRange.baseMipLevel = 0;
        colorAttachmentViewCreateInfo.subresourceRange.levelCount = 1;
        colorAttachmentViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        colorAttachmentViewCreateInfo.subresourceRange.layerCount = 1;

        VulkanImageCreateInfo colorImageCreateInfo{
            .Image{ VK_NULL_HANDLE },
            .ImageCreateInfo{ colorAttachmentCreateInfo },
            .ImageViewCreateInfo{ colorAttachmentViewCreateInfo },
        };

        m_OffscreenColorAttachment = CreateScope<VulkanImage>( colorImageCreateInfo );

        // Depth attachment
        VkImageCreateInfo depthAttachmentCreateInfo{ VulkanHelpers::Initializers::ImageCreateInfo() };
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

        VkImageViewCreateInfo depthAttachmentViewCreateInfo{ VulkanHelpers::Initializers::ImageViewCreateInfo() };

        depthAttachmentViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        depthAttachmentViewCreateInfo.image = VK_NULL_HANDLE;// Set by OnCreate()
        depthAttachmentViewCreateInfo.format = depthAttachmentCreateInfo.format;
        depthAttachmentViewCreateInfo.subresourceRange.baseMipLevel = 0;
        depthAttachmentViewCreateInfo.subresourceRange.levelCount = 1;
        depthAttachmentViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        depthAttachmentViewCreateInfo.subresourceRange.layerCount = 1;
        depthAttachmentViewCreateInfo.subresourceRange.aspectMask =
                depthAttachmentCreateInfo.format < VK_FORMAT_D16_UNORM_S8_UINT ? VK_IMAGE_ASPECT_DEPTH_BIT : ( VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT );

        VulkanImageCreateInfo depthImageCreateInfo{
            .Image{ VK_NULL_HANDLE },
            .ImageCreateInfo{ depthAttachmentCreateInfo },
            .ImageViewCreateInfo{ depthAttachmentViewCreateInfo },
        };

        m_OffscreenDepthAttachment = CreateScope<VulkanImage>( depthImageCreateInfo );
    }

    auto VulkanRenderer::PrepareOffscreenRender() -> void {
        m_ClearValues[0].color = { { 0.2f, 0.2f, 0.2f, 1.0f } };
        m_ClearValues[1].depthStencil = { 1.0f, 0 };

        m_ColorAttachmentFormat = m_Device->FindSupportedFormat(
                { VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_D32_SFLOAT, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SRGB },
                VK_IMAGE_TILING_OPTIMAL,
                VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT );

        m_DepthAttachmentFormat = m_Device->FindSupportedFormat(
                { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT },
                VK_IMAGE_TILING_OPTIMAL,
                VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT );

        CreateOffscreenRenderPass();
        CreateOffscreenAttachments();
        CreateOffscreenFramebuffers();

        UpdateViewport( 0, 0, static_cast<float>( m_OffscreenExtent.width ), static_cast<float>( m_OffscreenExtent.height ) );
        UpdateScissor( 0, 0, { m_OffscreenExtent.width, m_OffscreenExtent.height } );
    }

    auto VulkanRenderer::SubmitCommands() const -> void {
        //m_Device->RegisterComputeCommand( m_ComputeCommandBuffer );

        m_Device->RegisterGraphicsCommand( m_DrawCommandBuffer );
    }

    auto VulkanRenderer::InitializeDefaultPipeline() -> void {
        const auto& fileSystem{ Engine::GetSystem<FileSystem>() };

        const VulkanShaderCreateInfo vertexStage{
            .FilePath{ PathBuilder()
                               .WithPath( fileSystem.GetShadersRootPath().string() )
                               .WithPath( "vulkan-spirv" )
                               .WithPath( "StandardVertexShader.sprv" )
                               .Build() },
            .Stage{ VERTEX_STAGE },
        };

        const VulkanShaderCreateInfo fragmentStage{
            .FilePath{ PathBuilder()
                               .WithPath( fileSystem.GetShadersRootPath().string() )
                               .WithPath( "vulkan-spirv" )
                               .WithPath( "StandardFragmentShader.sprv" )
                               .Build() },
            .Stage{ FRAGMENT_STAGE },
        };

        const VulkanShader* fragmentShader = VulkanShaderLibrary::LoadShader( fragmentStage );
        const VulkanShader* vertexShader = VulkanShaderLibrary::LoadShader( vertexStage );

        const std::array pipelineShaderStageCreateInfos{
            vertexShader->GetPipelineStageCreateInfo(),
            fragmentShader->GetPipelineStageCreateInfo()
        };

        const std::array descLayouts{ VulkanContext::Get().GetDescriptorSetLayouts( DESCRIPTOR_SET_LAYOUT_BASE_SHADER ) };

        // Create the pipeline layout
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{ VulkanHelpers::Initializers::PipelineLayoutCreateInfo() };
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;
        pipelineLayoutInfo.setLayoutCount = static_cast<UInt32_T>( descLayouts.size() );
        pipelineLayoutInfo.pSetLayouts = descLayouts.data();

        VkPipelineLayout layout{};
        if ( vkCreatePipelineLayout( m_Device->GetLogicalDevice(), std::addressof( pipelineLayoutInfo ), nullptr, std::addressof( layout ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "Failed to create pipeline layout" );
        }

        VulkanDeletionQueue::Push( [device = m_Device->GetLogicalDevice(),
                                    pipelineLayout = layout]() -> void {
            vkDestroyPipelineLayout( device, pipelineLayout, nullptr );
        } );

        // Create the pipeline
        auto defaultMatPipelineConfig{ GetDefaultGraphicsPipelineConfigInfo() };

        defaultMatPipelineConfig.PipelineLayout = layout;
        defaultMatPipelineConfig.RenderPass = m_OffscreenMainRenderPass;
        defaultMatPipelineConfig.ShaderStages = pipelineShaderStageCreateInfos;

        auto [it, success]{ m_Pipelines.try_emplace( MATERIAL_PASS_COLOR, defaultMatPipelineConfig ) };
        if ( !success ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanRenderer::InitializeDefaultPipeline - Failed to create standard pipeline." );
        } else {
            it->second.Init();
        }
    }

    auto VulkanRenderer::InitializePBRPipeline() -> void {
        const auto& fileSystem{ Engine::GetSystem<FileSystem>() };

        const VulkanShaderCreateInfo vertexStage{
            .FilePath{ PathBuilder()
                               .WithPath( fileSystem.GetShadersRootPath().string() )
                               .WithPath( "vulkan-spirv" )
                               .WithPath( "PBRVertexShader.sprv" )
                               .Build() },
            .Stage{ VERTEX_STAGE },
        };

        const VulkanShaderCreateInfo fragmentStage{
            .FilePath{ PathBuilder()
                               .WithPath( fileSystem.GetShadersRootPath().string() )
                               .WithPath( "vulkan-spirv" )
                               .WithPath( "PBRFragmentShader.sprv" )
                               .Build() },
            .Stage{ FRAGMENT_STAGE },
        };

        const VulkanShader* fragmentShader = VulkanShaderLibrary::LoadShader( fragmentStage );
        const VulkanShader* vertexShader = VulkanShaderLibrary::LoadShader( vertexStage );

        const std::array pipelineShaderStageCreateInfos{
            vertexShader->GetPipelineStageCreateInfo(),
            fragmentShader->GetPipelineStageCreateInfo()
        };

        // Set layout
        const std::array descLayouts{ VulkanContext::Get().GetDescriptorSetLayouts( DESCRIPTOR_SET_LAYOUT_PBR_SHADER ) };

        // Push constants. See push constants in frag shader
        // First data is 12 bytes is which size sizeof(glm::vec3) using floats for the vec3
        std::array pushConstantRanges{
            VulkanHelpers::Initializers::PushConstantRange( VK_SHADER_STAGE_FRAGMENT_BIT, 32, sizeof( glm::vec4 ) ),
        };

        // Create the pipeline layout
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{ VulkanHelpers::Initializers::PipelineLayoutCreateInfo() };
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;
        pipelineLayoutInfo.setLayoutCount = static_cast<UInt32_T>( descLayouts.size() );
        pipelineLayoutInfo.pSetLayouts = descLayouts.data();

        VkPipelineLayout layout{};
        if ( vkCreatePipelineLayout( m_Device->GetLogicalDevice(), std::addressof( pipelineLayoutInfo ), nullptr, std::addressof( layout ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "Failed to create pipeline layout" );
        }

        VulkanDeletionQueue::Push( [device = m_Device->GetLogicalDevice(),
                                    pipelineLayout = layout]() -> void {
            vkDestroyPipelineLayout( device, pipelineLayout, nullptr );
        } );

        // Create the pipeline
        auto defaultMatPipelineConfig{ GetDefaultGraphicsPipelineConfigInfo() };

        defaultMatPipelineConfig.PipelineLayout = layout;
        defaultMatPipelineConfig.RenderPass = m_OffscreenMainRenderPass;
        defaultMatPipelineConfig.ShaderStages = pipelineShaderStageCreateInfos;

        auto [it, success]{ m_Pipelines.try_emplace( MATERIAL_PASS_PBR, defaultMatPipelineConfig ) };
        if ( !success ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanRenderer::InitializeDefaultPipeline - Failed to create standard pipeline." );
        } else {
            it->second.Init();
        }
    }

    auto VulkanRenderer::InitializeOutlinePipeline() -> void {
const auto& fileSystem{ Engine::GetSystem<FileSystem>() };

        const VulkanShaderCreateInfo vertexStage{
            .FilePath{ PathBuilder()
                               .WithPath( fileSystem.GetShadersRootPath().string() )
                               .WithPath( "vulkan-spirv" )
                               .WithPath( "Outline_Vert.sprv" )
                               .Build() },
            .Stage{ VERTEX_STAGE },
        };

        const VulkanShaderCreateInfo fragmentStage{
            .FilePath{ PathBuilder()
                               .WithPath( fileSystem.GetShadersRootPath().string() )
                               .WithPath( "vulkan-spirv" )
                               .WithPath( "Outline_Frag.sprv" )
                               .Build() },
            .Stage{ FRAGMENT_STAGE },
        };

        const VulkanShader* fragmentShader = VulkanShaderLibrary::LoadShader( fragmentStage );
        const VulkanShader* vertexShader = VulkanShaderLibrary::LoadShader( vertexStage );

        const std::array pipelineShaderStageCreateInfos{
            vertexShader->GetPipelineStageCreateInfo(),
            fragmentShader->GetPipelineStageCreateInfo()
        };

        // Set layout
        const std::array descLayouts{ VulkanContext::Get().GetDescriptorSetLayouts( DESCRIPTOR_SET_LAYOUT_PBR_SHADER ) };

        // Push constants. See push constants in frag shader
        // First data is 12 bytes is which size sizeof(glm::vec3) using floats for the vec3
        std::array pushConstantRanges{
            VulkanHelpers::Initializers::PushConstantRange( VK_SHADER_STAGE_FRAGMENT_BIT, 32, sizeof( glm::vec4 ) ),
        };

        // Create the pipeline layout
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{ VulkanHelpers::Initializers::PipelineLayoutCreateInfo() };
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;
        pipelineLayoutInfo.setLayoutCount = static_cast<UInt32_T>( descLayouts.size() );
        pipelineLayoutInfo.pSetLayouts = descLayouts.data();

        VkPipelineLayout layout{};
        if ( vkCreatePipelineLayout( m_Device->GetLogicalDevice(), std::addressof( pipelineLayoutInfo ), nullptr, std::addressof( layout ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "Failed to create pipeline layout" );
        }

        VulkanDeletionQueue::Push( [device = m_Device->GetLogicalDevice(),
                                    pipelineLayout = layout]() -> void {
            vkDestroyPipelineLayout( device, pipelineLayout, nullptr );
        } );

        // Create the pipeline
        auto defaultMatPipelineConfig{ GetDefaultGraphicsPipelineConfigInfo() };

        defaultMatPipelineConfig.DepthStencilInfo.back.compareOp = VK_COMPARE_OP_NOT_EQUAL;  // Draw where stencil != 1
        defaultMatPipelineConfig.DepthStencilInfo.back.failOp = VK_STENCIL_OP_KEEP;
        defaultMatPipelineConfig.DepthStencilInfo.back.depthFailOp = VK_STENCIL_OP_KEEP;
        defaultMatPipelineConfig.DepthStencilInfo.back.passOp = VK_STENCIL_OP_REPLACE;
        defaultMatPipelineConfig.DepthStencilInfo.front = defaultMatPipelineConfig.DepthStencilInfo.back;
        defaultMatPipelineConfig.DepthStencilInfo.depthTestEnable = VK_FALSE;

        defaultMatPipelineConfig.PipelineLayout = layout;
        defaultMatPipelineConfig.RenderPass = m_OffscreenMainRenderPass;
        defaultMatPipelineConfig.ShaderStages = pipelineShaderStageCreateInfos;

        auto [it, success]{ m_Pipelines.try_emplace( MATERIAL_PASS_OUTLINE, defaultMatPipelineConfig ) };
        if ( !success ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanRenderer::InitializeDefaultPipeline - Failed to create standard pipeline." );
        } else {
            it->second.Init();
        }
    }

    auto VulkanRenderer::InitializePBRWireFramePipeline() -> void {
        const auto& fileSystem{ Engine::GetSystem<FileSystem>() };

        const VulkanShaderCreateInfo vertexStage{
            .FilePath{ PathBuilder()
                               .WithPath( fileSystem.GetShadersRootPath().string() )
                               .WithPath( "vulkan-spirv" )
                               .WithPath( "PBRVertexShader.sprv" )
                               .Build() },
            .Stage{ VERTEX_STAGE },
        };

        const VulkanShaderCreateInfo fragmentStage{
            .FilePath{ PathBuilder()
                               .WithPath( fileSystem.GetShadersRootPath().string() )
                               .WithPath( "vulkan-spirv" )
                               .WithPath( "PBRFragmentShader.sprv" )
                               .Build() },
            .Stage{ FRAGMENT_STAGE },
        };

        const VulkanShader* fragmentShader = VulkanShaderLibrary::LoadShader( fragmentStage );
        const VulkanShader* vertexShader = VulkanShaderLibrary::LoadShader( vertexStage );

        const std::array pipelineShaderStageCreateInfos{
            vertexShader->GetPipelineStageCreateInfo(),
            fragmentShader->GetPipelineStageCreateInfo()
        };

        const std::array descLayouts{ VulkanContext::Get().GetDescriptorSetLayouts( DESCRIPTOR_SET_LAYOUT_PBR_SHADER ) };

        // Create the pipeline layout
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{ VulkanHelpers::Initializers::PipelineLayoutCreateInfo() };
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;
        pipelineLayoutInfo.setLayoutCount = static_cast<UInt32_T>( descLayouts.size() );
        pipelineLayoutInfo.pSetLayouts = descLayouts.data();

        VkPipelineLayout layout{};
        if ( vkCreatePipelineLayout( m_Device->GetLogicalDevice(), std::addressof( pipelineLayoutInfo ), nullptr, std::addressof( layout ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "Failed to create pipeline layout" );
        }

        VulkanDeletionQueue::Push( [device = m_Device->GetLogicalDevice(),
                                    pipelineLayout = layout]() -> void {
            vkDestroyPipelineLayout( device, pipelineLayout, nullptr );
        } );


        // Create the pipeline
        auto wireframePipelineConfig{ GetDefaultGraphicsPipelineConfigInfo() };

        constexpr float GPU_STANDARD_LINE_WIDTH{ 1.0f };
        wireframePipelineConfig.RasterizationInfo.polygonMode = VK_POLYGON_MODE_LINE;
        wireframePipelineConfig.RasterizationInfo.cullMode = VK_CULL_MODE_NONE;
        wireframePipelineConfig.RasterizationInfo.lineWidth = GPU_STANDARD_LINE_WIDTH;

        wireframePipelineConfig.RenderPass = m_OffscreenMainRenderPass;
        wireframePipelineConfig.PipelineLayout = layout;
        wireframePipelineConfig.ShaderStages = pipelineShaderStageCreateInfos;

        auto [it, success]{ m_Pipelines.try_emplace( MATERIAL_PASS_WIREFRAME, wireframePipelineConfig ) };
        if ( !success ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanRenderer::InitializeDefaultPipeline - Failed to create standard pipeline." );
        } else {
            it->second.Init();
        }
    }

    auto VulkanRenderer::InitializeComputePipelines() -> void {
        const auto& fileSystem{ Engine::GetSystem<FileSystem>() };

        // Compute pipeline only takes one shader stage
        const VulkanShaderCreateInfo lightCullingComputeShaderCreateInfo{
            .FilePath{ PathBuilder()
                               .WithPath( fileSystem.GetShadersRootPath().string() )
                               .WithPath( "vulkan-spirv" )
                               .WithPath( "Compute_Shader.sprv" )
                               .Build() },
            .Stage{ COMPUTE_STAGE },
        };


        const VulkanShader* lightCullingComputeShader{ VulkanShaderLibrary::LoadShader( lightCullingComputeShaderCreateInfo ) };

        const std::array pipelineShaderStageCreateInfos{
            lightCullingComputeShader->GetPipelineStageCreateInfo(),
        };

        const std::array descLayouts{ VulkanContext::Get().GetDescriptorSetLayouts( DESCRIPTOR_SET_LAYOUT_COMPUTE_PIPELINE ) };

        // Create the pipeline layout
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{ VulkanHelpers::Initializers::PipelineLayoutCreateInfo() };
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;
        pipelineLayoutInfo.setLayoutCount = static_cast<UInt32_T>( descLayouts.size() );
        pipelineLayoutInfo.pSetLayouts = descLayouts.data();

        VkPipelineLayout layout{};
        if ( vkCreatePipelineLayout( m_Device->GetLogicalDevice(), std::addressof( pipelineLayoutInfo ), nullptr, std::addressof( layout ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanRenderer::InitializeComputePipelines - Failed to create pipeline layout" );
        }

        VulkanDeletionQueue::Push( [device = m_Device->GetLogicalDevice(), pipelineLayout = layout]() -> void {
            vkDestroyPipelineLayout( device, pipelineLayout, nullptr );
        } );

        // Create the pipeline
        auto computePipelineCreateInfo{ GetDefaultComputePipelineConfigInfo() };
        computePipelineCreateInfo.Type = PipelineType::VULKAN_COMPUTE_PIPELINE;

        computePipelineCreateInfo.PipelineLayout = layout;
        computePipelineCreateInfo.ShaderStages = pipelineShaderStageCreateInfos;

        auto [it, success]{ m_Pipelines.try_emplace( MATERIAL_PASS_COMPUTE, computePipelineCreateInfo ) };
        if ( !success ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanRenderer::InitializeDefaultPipeline - Failed to create standard pipeline." );
        } else {
            it->second.Init();
        }
    }

    auto VulkanRenderer::CreateRendererPipelines() -> void {
        InitializeDefaultPipeline();

        InitializePBRWireFramePipeline();

        InitializePBRPipeline();

        InitializeComputePipelines();

        InitializeOutlinePipeline();
    }

    auto VulkanRenderer::Flush() -> void {
        RecordComputeCommands();

        RecordCommands();

        // NOTE: Compute, Graphics and Present queues might be the same
        // Watch-out to properly sync operations between command buffers and commands
        // If Graphics queue and Compute queue are the same queue they will share index
        // so might consider for this specific case to use pipeline barriers between these two commands buffers
        SubmitCommands();

        // For light culling we want to:
        // submit depth pre-pass command buffer
        // submit light culling command buffer
        // Submitting final composition command buffer
    }

    auto VulkanRenderer::AddToDrawQueue( const EntityQueueInfo& queueInfo ) -> bool {
        const auto it{ m_DrawQueue.find( queueInfo.Tag.GetGUID() ) };

        MeshRenderInfo info{
            .Object = queueInfo.Render.GetMesh(),
            .Transform{ queueInfo.Transform.GetTransform() },
            .MaterialData{ std::addressof( queueInfo.Material.GetMaterial() ) },
        };

        if ( it != m_DrawQueue.end() ) {
            it->second = info;
            return true;
        }

        auto [insertIt, success]{
            m_DrawQueue.try_emplace( queueInfo.Tag.GetGUID(), info )
        };

        return success;
    }

    auto VulkanRenderer::RemoveFromDrawQueue( const UInt64_T id ) -> bool {
        auto result{ false };

        try {
            const auto eraseCount{ m_DrawQueue.erase( id ) };
            result = eraseCount != 0;

        } catch ( std::exception& exception ) {
            MKT_THROW_RUNTIME_ERROR( fmt::format( "VulkanRenderer - {}", exception.what() ) );
        }

        return result;
    }

    auto VulkanRenderer::CreateCommandPools() -> void {
        const auto& [Present, Graphics, Compute]{
            m_Device->GetLogicalDeviceQueues()
        };

        VkCommandPoolCreateInfo createInfo{ VulkanHelpers::Initializers::CommandPoolCreateInfo() };
        createInfo.flags = 0;
        createInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        createInfo.queueFamilyIndex = Graphics->FamilyIndex;

        VulkanCommandPoolCreateInfo vulkanCommandPoolCreateInfo{
            .CreateInfo{ createInfo },
        };

        m_CommandPool = VulkanCommandPool::Create( vulkanCommandPoolCreateInfo );

        // TODO create command pool to alllocate compute command buffers
    }
}// namespace Mikoto