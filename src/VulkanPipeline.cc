//
// Created by kate on 6/2/23.
//

#include <fstream>
#include <array>

#include <Utility/Common.hh>

#include <Core/Logger.hh>

#include <Renderer/Vulkan/VulkanVertexBuffer.hh>
#include <Renderer/Vulkan/VulkanPipeline.hh>
#include <Renderer/Vulkan/VulkanContext.hh>

namespace Mikoto {

    VulkanPipeline::VulkanPipeline(const Path_T &vPath, const Path_T &fPath, const PipelineConfigInfo& config) {
        CreateGraphicsPipeline(vPath, fPath, config);
    }

    auto VulkanPipeline::CreateGraphicsPipeline(const Path_T& vPath, const Path_T& fPath, const PipelineConfigInfo& config) -> void {
        KT_ASSERT(config.PipelineLayout != VK_NULL_HANDLE, "Cannot create graphics pipeline. No Pipeline Layout in PipelineConfigInfo");
        KT_ASSERT(config.RenderPass != VK_NULL_HANDLE, "Cannot create graphics pipeline. No Render Pass Layout in PipelineConfigInfo");

        auto vData{ GetFileData(vPath) };
        auto fData{ GetFileData(fPath) };

        CreateShaderModule(vData, &m_VertShaderModule);
        CreateShaderModule(fData, &m_FragShaderModule);

        std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{};

        shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        shaderStages[0].module = m_VertShaderModule;
        shaderStages[0].pName = "main";
        shaderStages[0].flags = 0;
        shaderStages[0].pNext = nullptr;
        shaderStages[0].pSpecializationInfo = nullptr;

        shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shaderStages[1].module = m_FragShaderModule;
        shaderStages[1].pName = "main";
        shaderStages[1].flags = 0;
        shaderStages[1].pNext = nullptr;
        shaderStages[1].pSpecializationInfo = nullptr;

        // Setup Vertex Input State Info
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        auto bindingDesc{ VulkanVertexBuffer::GetDefaultBindingDescriptions() };
        auto attributeDesc{ VulkanVertexBuffer::GetDefaultAttributeDescriptions() };

        vertexInputInfo.vertexBindingDescriptionCount = bindingDesc.size();
        vertexInputInfo.vertexAttributeDescriptionCount = attributeDesc.size();

        vertexInputInfo.pVertexAttributeDescriptions = attributeDesc.data();
        vertexInputInfo.pVertexBindingDescriptions = bindingDesc.data();

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

        pipelineInfo.stageCount = shaderStages.size();
        pipelineInfo.pStages =  shaderStages.data();

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

        if (vkCreateGraphicsPipelines(VulkanContext::GetPrimaryLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_GraphicsPipeline) != VK_SUCCESS)
            throw std::runtime_error("Failed to create Graphics pipeline");
    }

    auto VulkanPipeline::CreateShaderModule(const CharArray& srcCode, VkShaderModule* shaderModule) -> void {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = srcCode.size();
        // It seems this casts is valid since the default std::vector allocator
        // ensures the data satisfies the worst case alignment
        createInfo.pCode = reinterpret_cast<const UInt32_T*>(srcCode.data());

        if (vkCreateShaderModule(VulkanContext::GetPrimaryLogicalDevice(), &createInfo, nullptr, shaderModule) != VK_SUCCESS)
            throw std::runtime_error("Failed to create shader module");
    }


    auto VulkanPipeline::GetDefaultPipelineConfigInfo() -> PipelineConfigInfo {
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
        configInfo.RasterizationInfo.cullMode = VK_CULL_MODE_BACK_BIT;
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
        configInfo.ColorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        configInfo.ColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
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
        configInfo.DepthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS;
        configInfo.DepthStencilInfo.depthBoundsTestEnable = VK_FALSE;
        configInfo.DepthStencilInfo.minDepthBounds = 0.0f;
        configInfo.DepthStencilInfo.maxDepthBounds = 1.0f;
        configInfo.DepthStencilInfo.stencilTestEnable = VK_FALSE;
        configInfo.DepthStencilInfo.front = {};
        configInfo.DepthStencilInfo.back = {};

        configInfo.DynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_LINE_WIDTH, /*VK_DYNAMIC_STATE_VERTEX_INPUT_EXT*/ };
        configInfo.DynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        configInfo.DynamicStateInfo.pDynamicStates = configInfo.DynamicStateEnables.data();
        configInfo.DynamicStateInfo.dynamicStateCount = configInfo.DynamicStateEnables.size();
        configInfo.DynamicStateInfo.flags = 0;


        return configInfo;
    }

    auto VulkanPipeline::Bind(VkCommandBuffer commandBuffer) const -> void {
        KT_ASSERT(m_GraphicsPipeline, "Graphics pipeline is NULL");
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline);
    }

    auto VulkanPipeline::OnRelease() const -> void {
        vkDeviceWaitIdle(VulkanContext::GetPrimaryLogicalDevice());

        vkDestroyShaderModule(VulkanContext::GetPrimaryLogicalDevice(), m_VertShaderModule, nullptr);
        vkDestroyShaderModule(VulkanContext::GetPrimaryLogicalDevice(), m_FragShaderModule, nullptr);
        vkDestroyPipeline(VulkanContext::GetPrimaryLogicalDevice(), m_GraphicsPipeline, nullptr);
    }


}