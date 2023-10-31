/**
 * VulkanPipeline.cc
 * Created by kate on 6/2/23.
 * */

// C++ Standard Library
#include <fstream>
#include <array>

// Third-Party Libraries
#include <volk.h>

// Project Headers
#include <Common/Types.hh>
#include <Common/Common.hh>
#include <Common/VulkanUtils.hh>

#include <Core/Logger.hh>

#include <Renderer/Vulkan/DeletionQueue.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanPipeline.hh>
#include <Renderer/Vulkan/VulkanVertexBuffer.hh>

namespace Mikoto {
    auto VulkanPipeline::CreateGraphicsPipeline(const PipelineConfigInfo& config) -> void {
        MKT_ASSERT(config.PipelineLayout != VK_NULL_HANDLE, "Cannot create graphics pipeline. No Pipeline Layout in PipelineConfigInfo");
        MKT_ASSERT(config.RenderPass != VK_NULL_HANDLE, "Cannot create graphics pipeline. No Render Pass Layout in PipelineConfigInfo");

        VkGraphicsPipelineCreateInfo pipelineInfo{ VulkanUtils::Initializers::GraphicsPipelineCreateInfo() };

        // Setup shaders
        pipelineInfo.stageCount = config.ShaderStages->size();
        pipelineInfo.pStages =  config.ShaderStages->data();

        // Setup Vertex input
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{ VulkanUtils::Initializers::PipelineVertexInputStateCreateInfo() };

        const auto& bindingDesc{ VulkanVertexBuffer::GetDefaultBindingDescriptions() };
        const auto& attributeDesc{ VulkanVertexBuffer::GetDefaultAttributeDescriptions() };
        vertexInputInfo.vertexBindingDescriptionCount = bindingDesc.size();
        vertexInputInfo.vertexAttributeDescriptionCount = attributeDesc.size();
        vertexInputInfo.pVertexAttributeDescriptions = attributeDesc.data();
        vertexInputInfo.pVertexBindingDescriptions = bindingDesc.data();

        // Pipeline create info submit
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &config.InputAssemblyInfo;
        pipelineInfo.pViewportState = &config.ViewportInfo;
        pipelineInfo.pRasterizationState = &config.RasterizationInfo;
        pipelineInfo.pMultisampleState = &config.MultisampleInfo;
        pipelineInfo.pColorBlendState = &config.ColorBlendInfo;
        pipelineInfo.pDepthStencilState = &config.DepthStencilInfo;
        pipelineInfo.layout = config.PipelineLayout;
        pipelineInfo.renderPass = config.RenderPass;
        pipelineInfo.subpass = config.Subpass;
        pipelineInfo.basePipelineIndex = -1;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.pDynamicState = &config.DynamicStateInfo;

        if (vkCreateGraphicsPipelines(VulkanContext::GetPrimaryLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_GraphicsPipeline) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("Failed to create Graphics pipeline");
        }

        DeletionQueue::Push([pipeline = m_GraphicsPipeline]() -> void {
            vkDestroyPipeline(VulkanContext::GetPrimaryLogicalDevice(), pipeline, nullptr);
        });
    }

    auto VulkanPipeline::GetDefaultPipelineConfigInfo() -> PipelineConfigInfo& {
        static PipelineConfigInfo configInfo{};

        configInfo.InputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        configInfo.InputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;    // Every three vertices are group together into a separate triangle
        configInfo.InputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

        // Viewport and Scissor set dynamically
        configInfo.ViewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        configInfo.ViewportInfo.viewportCount = 1; // VK_DYNAMIC_VIEWPORT_WITH_COUNT has to be set for this to be 0
        configInfo.ViewportInfo.pViewports = nullptr;
        configInfo.ViewportInfo.scissorCount = 1; // VK_DYNAMIC_SCISSOR_WITH_COUNT has to be set for this to be 0
        configInfo.ViewportInfo.pScissors = nullptr;

        constexpr float GPU_STANDARD_LINE_WIDTH{ 1.0f };
        configInfo.RasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        configInfo.RasterizationInfo.depthClampEnable = VK_FALSE;
        configInfo.RasterizationInfo.rasterizerDiscardEnable = VK_FALSE; // requires extension if enabled
        configInfo.RasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
        configInfo.RasterizationInfo.lineWidth = configInfo.RasterizationInfo.polygonMode == VK_POLYGON_MODE_LINE ? GPU_STANDARD_LINE_WIDTH : 0.0f; // The maximum line width that is supported depends on the hardware, any line thicker than 1.0f requires you to enable the wideLines GPU feature.
        configInfo.RasterizationInfo.cullMode = VK_CULL_MODE_NONE;
        configInfo.RasterizationInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        configInfo.RasterizationInfo.depthBiasEnable = VK_FALSE;
        configInfo.RasterizationInfo.depthBiasConstantFactor = 0.0f;
        configInfo.RasterizationInfo.depthBiasClamp = 0.0f;
        configInfo.RasterizationInfo.depthBiasSlopeFactor = 0.0f;

        configInfo.MultisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        configInfo.MultisampleInfo.sampleShadingEnable = VK_FALSE;
        configInfo.MultisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        configInfo.MultisampleInfo.minSampleShading = 1.0f;             // Optional
        configInfo.MultisampleInfo.pSampleMask = nullptr;               // Optional
        configInfo.MultisampleInfo.alphaToCoverageEnable = VK_FALSE;    // Optional
        configInfo.MultisampleInfo.alphaToOneEnable = VK_FALSE;         // Optional

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
        configInfo.DepthStencilInfo.minDepthBounds = 0.0f;
        configInfo.DepthStencilInfo.maxDepthBounds = 1.0f;
        configInfo.DepthStencilInfo.stencilTestEnable = VK_FALSE;
        configInfo.DepthStencilInfo.front = {};
        configInfo.DepthStencilInfo.back = {};
        configInfo.DepthStencilInfo.back.compareOp = VK_COMPARE_OP_ALWAYS;

        configInfo.DynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_LINE_WIDTH, /*VK_DYNAMIC_STATE_VERTEX_INPUT_EXT*/ };
        configInfo.DynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        configInfo.DynamicStateInfo.pDynamicStates = configInfo.DynamicStateEnables.data();
        configInfo.DynamicStateInfo.dynamicStateCount = configInfo.DynamicStateEnables.size();
        configInfo.DynamicStateInfo.flags = 0;

        return configInfo;
    }

    auto VulkanPipeline::Bind(VkCommandBuffer commandBuffer) const -> void {
        MKT_ASSERT(m_GraphicsPipeline, "Graphics pipeline is NULL");
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline);
    }
}