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
#include <Library/Utility/Types.hh>
#include <Logging/Logger.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanDevice.hh>
#include <Renderer/Vulkan/VulkanHelpers.hh>
#include <Renderer/Vulkan/VulkanPipeline.hh>
#include <Renderer/Vulkan/VulkanShader.hh>

namespace Mikoto {

    static auto GetDefaultAttributeDescriptions(const BufferLayout& layout ) -> std::vector<VkVertexInputAttributeDescription> {
        auto attributeDescriptions{ std::vector<VkVertexInputAttributeDescription>( layout.GetCount() ) };

        /**
         * The binding parameter tells Vulkan from which binding the per-vertex (if do per vertex and not instanced) data comes.
         * The location parameter references the location directive of the input in the vertex shader.
         * The input in the vertex shader with location 0 is the position, which has two 32-bit float
         * components. The format parameter describes the type of data for the attribute
         *
         * See: https://vulkan-tutorial.com/Vertex_buffers/Vertex_input_description
         * */

        // The index refers to how the vertex attributes are laid out according to s_DefaultBufferLayout
        // so index 0 -> s_DefaultBufferLayout first attribute,
        // index 1 -> s_DefaultBufferLayout second attribute and so on
        for ( Size index{}; index < attributeDescriptions.size(); ++index ) {
            attributeDescriptions[index] = {};
            attributeDescriptions[index].binding = 0;
            attributeDescriptions[index].location = index;
            attributeDescriptions[index].format = VulkanHelpers::GetVulkanAttributeDataType( layout[index].GetType() );
            attributeDescriptions[index].offset = layout[index].GetOffset();
        }

        return attributeDescriptions;
    }

    static auto GetDefaultBindingDescriptions(const BufferLayout& layout ) -> std::vector<VkVertexInputBindingDescription> {
        // All of our per-vertex data is packed together in one array, so we're only going to have one binding.
        // See: https://vulkan-tutorial.com/Vertex_buffers/Vertex_input_description

        auto bindingDescriptions{ std::vector<VkVertexInputBindingDescription>(1) };

        bindingDescriptions[0] = {};
        bindingDescriptions[0].binding = 0;
        bindingDescriptions[0].stride = layout.GetStride();
        bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX; // not using instanced rendering, so we'll stick to per-vertex data. (Optional) to specify as parameter to this function

        return bindingDescriptions;
    }

    static auto GetShaderStagesInfo(std::span<ShaderModuleHandle> shaders) -> std::vector<VkPipelineShaderStageCreateInfo> {
        std::vector<VkPipelineShaderStageCreateInfo> shaderStagesInfos{};

        for (auto& shader : shaders) {
            if (!shader.IsEmpty()) {
                const auto vkModule{ dynamic_cast<VulkanShader*>(shader.GetRaw() ) };
                shaderStagesInfos.emplace_back(vkModule->GetPipelineStageCreateInfo());
            }
        }

        return shaderStagesInfos;
    }

    static auto CreatePipelineLayout(std::span<ShaderModuleHandle> shaders) -> VkPipelineLayout {
        VkPipelineLayout result{ VK_NULL_HANDLE };


        return result;
    }

    VulkanGraphicsPipeline::VulkanGraphicsPipeline( const VulkanGraphicsPipelineCreateInfo& info)
        : GraphicsPipeline{ info.ShaderModules }, m_ConfigInfo{ info }, m_BufferLayout{ info.Layout } {

        m_DepthAttachmentFormat = dynamic_cast<const VulkanTexture*>(info.Depth.GetRaw() )->GetViewCreateInfo().format;

        for (auto& attachment : info.ColorAttachments) {
            m_ColorAttachmentsFormats.emplace_back( dynamic_cast<const VulkanTexture*>(attachment.GetRaw() )->GetViewCreateInfo().format );
        }
    }

    auto VulkanGraphicsPipeline::Release() -> void {
        DestroyReflectedPipeline( VK_DEVICE(m_Device), m_ReflectionData );

        vkDestroyPipeline( VK_DEVICE(m_Device), m_GraphicsPipeline, nullptr );
        m_IsAllocated = false;
    }

    auto VulkanGraphicsPipeline::Bind( const VkCommandBuffer commandBuffer ) const -> void {
        MKT_ASSERT( m_GraphicsPipeline != VK_NULL_HANDLE, "VulkanGraphicsPipeline::Bind - Graphics pipeline is null." );
        vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline );
    }

    VulkanGraphicsPipeline::~VulkanGraphicsPipeline() {
        if (m_IsAllocated) {
            Release();
        }
    }

    auto VulkanGraphicsPipeline::Initialize() -> void {
        VkGraphicsPipelineCreateInfo pipelineInfo{ VulkanHelpers::Initializers::GraphicsPipelineCreateInfo() };

#if !defined(MKT_USE_VULKAN_DYNAMIC_RENDERING)
        MKT_ASSERT( m_ConfigInfo.RenderPass != VK_NULL_HANDLE, "VulkanGraphicsPipeline::Initialize - Cannot create graphics pipeline. No Render Pass" );
        pipelineInfo.renderPass = m_ConfigInfo.RenderPass;
#endif

        // Setup Shaders
        const auto& shaderStageInfos{ GetShaderStagesInfo(m_ShaderModules) };
        if (shaderStageInfos.empty()) {
            MKT_THROW_RUNTIME_ERROR( "VulkanComputePipeline::Initialize - stage infos is empty" );
        }

        pipelineInfo.stageCount = shaderStageInfos.size();
        pipelineInfo.pStages = shaderStageInfos.data();

        // Pipeline layout
        std::vector<std::vector<UInt32>> shaderBlocks{};
        for (const auto& shader : m_ShaderModules) {
            shaderBlocks.emplace_back( (UInt32*)shader->GetContents(), (UInt32*)(shader->GetContents() + shader->GetContentSize()) );
        }

        VkResult res{ ReflectSPIRV( VK_DEVICE(m_Device), shaderBlocks, m_ReflectionData) };
        const auto pipelineLayout { m_ReflectionData.pipelineLayout };
        if (pipelineLayout == VK_NULL_HANDLE) {
            MKT_THROW_RUNTIME_ERROR( "VulkanComputePipeline::Initialize - Layout is null handle" );
        }
        m_PipelineLayout = pipelineLayout;

        // Setup Vertex input
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{ VulkanHelpers::Initializers::PipelineVertexInputStateCreateInfo() };

        // Binding descriptions (define data layout)
        if (m_BufferLayout.HasElements()) {
            const auto& bindingDesc{ GetDefaultBindingDescriptions( m_BufferLayout ) };
            const auto& attributeDesc{ GetDefaultAttributeDescriptions( m_BufferLayout ) };
            vertexInputInfo.vertexBindingDescriptionCount = bindingDesc.size();
            vertexInputInfo.vertexAttributeDescriptionCount = attributeDesc.size();
            vertexInputInfo.pVertexAttributeDescriptions = attributeDesc.data();
            vertexInputInfo.pVertexBindingDescriptions = bindingDesc.data();
        } else {
            vertexInputInfo.vertexBindingDescriptionCount = 0;
            vertexInputInfo.vertexAttributeDescriptionCount = 0;
            vertexInputInfo.pVertexAttributeDescriptions = nullptr;
            vertexInputInfo.pVertexBindingDescriptions = nullptr;
        }

        // Create pipeline rendering info for dynamic rendering
        VkPipelineRenderingCreateInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        renderingInfo.colorAttachmentCount = static_cast<UInt32>(m_ColorAttachmentsFormats.size());
        renderingInfo.pColorAttachmentFormats = m_ColorAttachmentsFormats.data();
        renderingInfo.depthAttachmentFormat = m_DepthAttachmentFormat;
        renderingInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

        // Pipeline create info submit
        pipelineInfo.pNext = std::addressof( renderingInfo );
        pipelineInfo.pVertexInputState = std::addressof( vertexInputInfo );
        pipelineInfo.pInputAssemblyState = std::addressof( m_ConfigInfo.InputAssemblyInfo );
        pipelineInfo.pViewportState = std::addressof( m_ConfigInfo.ViewportInfo );
        pipelineInfo.pRasterizationState = std::addressof( m_ConfigInfo.RasterizationInfo );
        pipelineInfo.pMultisampleState = std::addressof( m_ConfigInfo.MultisampleInfo );
        pipelineInfo.pColorBlendState = std::addressof( m_ConfigInfo.ColorBlendInfo );
        pipelineInfo.pDepthStencilState = std::addressof( m_ConfigInfo.DepthStencilInfo );
        pipelineInfo.layout = m_PipelineLayout;
        pipelineInfo.subpass = m_ConfigInfo.Subpass;
        pipelineInfo.basePipelineIndex = -1;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.pDynamicState = std::addressof( m_ConfigInfo.DynamicStateInfo );

        if ( vkCreateGraphicsPipelines( VK_DEVICE( m_Device ), VK_NULL_HANDLE, 1, std::addressof( pipelineInfo ), nullptr, std::addressof( m_GraphicsPipeline ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanGraphicsPipeline::Initialize - Failed to create Graphics pipeline" );
        }

        m_IsAllocated = true;
    }

    VulkanComputePipeline::VulkanComputePipeline() {
    }

    auto VulkanComputePipeline::Release() -> void {
        vkDestroyPipeline( VK_DEVICE( m_Device ), m_ComputePipeline, nullptr );
        m_IsAllocated = false;
    }

    auto VulkanComputePipeline::Bind( VkCommandBuffer commandBuffer ) const -> void {
        MKT_ASSERT( m_ComputePipeline != VK_NULL_HANDLE, "VulkanComputePipeline::Bind - Compute pipeline is null." );
        vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ComputePipeline );
    }

    VulkanComputePipeline::~VulkanComputePipeline() {
        if (m_IsAllocated) {
            Release();
        }
    }

    auto VulkanComputePipeline::Initialize() -> void {
        VkComputePipelineCreateInfo computePipelineCreateInfo{ VulkanHelpers::Initializers::ComputePipelineCreateInfo() };

        const auto& shaderStageInfos{ GetShaderStagesInfo(m_ShaderModules) };
        const auto pipelineLayout { CreatePipelineLayout(m_ShaderModules) };

        if (shaderStageInfos.empty() || pipelineLayout == VK_NULL_HANDLE) {
            MKT_THROW_RUNTIME_ERROR( "VulkanComputePipeline::Initialize - stage infos is empty or pipeline layout is null handle." );
        }

        // Save if needed later
        m_PipelineLayout = pipelineLayout;

        computePipelineCreateInfo.stage = shaderStageInfos.front(); // I use front because compute pipeline only have one stage, the compute shader
        computePipelineCreateInfo.layout = pipelineLayout;

        if ( vkCreateComputePipelines( VK_DEVICE(m_Device), VK_NULL_HANDLE, 1, std::addressof( computePipelineCreateInfo ), nullptr, std::addressof( m_ComputePipeline ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanComputePipeline::Initialize - Failed to create compute pipeline" );
        }
    }
}// namespace Mikoto