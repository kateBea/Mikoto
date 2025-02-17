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
#include <Common/Common.hh>
#include <Core/Logging/Logger.hh>
#include <Library/Utility/Types.hh>
#include <Renderer/Vulkan/VulkanHelpers.hh>
#include <Renderer/Vulkan/VulkanDeletionQueue.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanPipeline.hh>
#include <Renderer/Vulkan/VulkanVertexBuffer.hh>

namespace Mikoto {

    VulkanPipeline::VulkanPipeline( const VulkanPipelineCreateInfo& config )
        : m_ConfigInfo{ config }
    {

    }

    auto VulkanPipeline::Init() -> void {
        MKT_ASSERT( m_ConfigInfo.PipelineLayout != VK_NULL_HANDLE, "VulkanPipeline::Init - Cannot create graphics pipeline. No Pipeline Layout in PipelineConfigInfo" );
        MKT_ASSERT( m_ConfigInfo.RenderPass != VK_NULL_HANDLE, "VulkanPipeline::Init - Cannot create graphics pipeline. No Render Pass Layout in PipelineConfigInfo" );

        VkGraphicsPipelineCreateInfo pipelineInfo{ VulkanHelpers::Initializers::GraphicsPipelineCreateInfo() };

        // Setup Shaders
        pipelineInfo.stageCount = m_ConfigInfo.ShaderStages.size();
        pipelineInfo.pStages = m_ConfigInfo.ShaderStages.data();

        // Setup Vertex input
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{ VulkanHelpers::Initializers::PipelineVertexInputStateCreateInfo() };

        // Binding descriptions (define data layout)
        const auto& bindingDesc{ VulkanVertexBuffer::GetDefaultBindingDescriptions() };
        const auto& attributeDesc{ VulkanVertexBuffer::GetDefaultAttributeDescriptions() };
        vertexInputInfo.vertexBindingDescriptionCount = bindingDesc.size();
        vertexInputInfo.vertexAttributeDescriptionCount = attributeDesc.size();
        vertexInputInfo.pVertexAttributeDescriptions = attributeDesc.data();
        vertexInputInfo.pVertexBindingDescriptions = bindingDesc.data();

        // Pipeline create info submit
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &m_ConfigInfo.InputAssemblyInfo;
        pipelineInfo.pViewportState = &m_ConfigInfo.ViewportInfo;
        pipelineInfo.pRasterizationState = &m_ConfigInfo.RasterizationInfo;
        pipelineInfo.pMultisampleState = &m_ConfigInfo.MultisampleInfo;
        pipelineInfo.pColorBlendState = &m_ConfigInfo.ColorBlendInfo;
        pipelineInfo.pDepthStencilState = &m_ConfigInfo.DepthStencilInfo;
        pipelineInfo.layout = m_ConfigInfo.PipelineLayout;
        pipelineInfo.renderPass = m_ConfigInfo.RenderPass;
        pipelineInfo.subpass = m_ConfigInfo.Subpass;
        pipelineInfo.basePipelineIndex = -1;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.pDynamicState = &m_ConfigInfo.DynamicStateInfo;

        VulkanDevice& device{ VulkanContext::Get().GetDevice() };

        if ( vkCreateGraphicsPipelines( device.GetLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_GraphicsPipeline ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanPipeline::Init - Failed to create Graphics pipeline" );
        }

        VulkanDeletionQueue::Push( [device = device.GetLogicalDevice(), pipeline = m_GraphicsPipeline]() -> void {
            vkDestroyPipeline( device, pipeline, nullptr );
        } );
    }

    auto VulkanPipeline::Release() -> void {
        VulkanDevice& device{ VulkanContext::Get().GetDevice() };

        vkDestroyPipeline( device.GetLogicalDevice(), m_GraphicsPipeline, nullptr );
    }

    VulkanPipeline::~VulkanPipeline() {
        if (!m_IsReleased) {
            Release();
            Invalidate();
        }
    }

    auto VulkanPipeline::Bind( const VkCommandBuffer commandBuffer) const -> void {
        MKT_ASSERT(m_GraphicsPipeline != VK_NULL_HANDLE, "VulkanPipeline::Bind - Graphics pipeline is null.");
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline);
    }
}