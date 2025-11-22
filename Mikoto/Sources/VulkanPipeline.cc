/**
 * VulkanPipeline.cc
 * Created by kate on 6/2/23.
 * */

// C++ Standard Library
#include <fstream>
#include <array>
#include <ranges>
#include <vector>

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

    struct GraphicsPipelineConfiguration {
        VkPipelineViewportStateCreateInfo ViewportInfo{};
        VkPipelineInputAssemblyStateCreateInfo InputAssemblyInfo{};
        VkPipelineRasterizationStateCreateInfo RasterizationInfo{};
        VkPipelineMultisampleStateCreateInfo MultisampleInfo{};
        VkPipelineColorBlendAttachmentState ColorBlendAttachment{};
        VkPipelineColorBlendStateCreateInfo ColorBlendInfo{};
        VkPipelineDepthStencilStateCreateInfo DepthStencilInfo{};
        VkPipelineDynamicStateCreateInfo DynamicStateInfo{};

        std::vector<VkDynamicState> DynamicStates{};
    };

    static auto GetDefaultGraphicsPipelineConfigInfo() -> GraphicsPipelineConfiguration {
        GraphicsPipelineConfiguration configInfo{};

        // [Input assembly]
        configInfo.InputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        configInfo.InputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        //configInfo.InputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;// Every three vertices are group together into a separate triangle
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
        configInfo.DepthStencilInfo.stencilTestEnable = VK_TRUE;          // Enable stencil test
        configInfo.DepthStencilInfo.back.compareOp = VK_COMPARE_OP_ALWAYS;// Always pass
        configInfo.DepthStencilInfo.back.failOp = VK_STENCIL_OP_REPLACE;
        configInfo.DepthStencilInfo.back.depthFailOp = VK_STENCIL_OP_REPLACE;
        configInfo.DepthStencilInfo.back.passOp = VK_STENCIL_OP_REPLACE;// Write stencil value
        configInfo.DepthStencilInfo.back.reference = 1;                 // Stencil value to write
        configInfo.DepthStencilInfo.back.compareMask = 0xFF;
        configInfo.DepthStencilInfo.back.writeMask = 0xFF;
        configInfo.DepthStencilInfo.front = configInfo.DepthStencilInfo.back;// Use default settings for front faces

        // VK_DYNAMIC_STATE_VERTEX_INPUT_EXT can reduce the amount of pipelines the application needs to create
        // because it allows for vertex input binding and attribute descriptions to be dynamic. This is, of course, not a
        // core feature as of Vulkan 1.3 and requires to be enabled when creating the device on which this pipeline will be created
        // Make it static because pDynamicStates does not persist the value beyond this scope

        constexpr std::array dynamicStates{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_LINE_WIDTH, /*VK_DYNAMIC_STATE_VERTEX_INPUT_EXT*/ };
        std::ranges::copy( dynamicStates, std::back_inserter( configInfo.DynamicStates ) );
        
        configInfo.DynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        configInfo.DynamicStateInfo.pDynamicStates = configInfo.DynamicStates.data();
        configInfo.DynamicStateInfo.dynamicStateCount = configInfo.DynamicStates.size();
        configInfo.DynamicStateInfo.flags = 0;

        return configInfo;
    }

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
        // index 1 -> s_DefaultBufferLayout second attribute, and so on
        for ( Size index{}; index < attributeDescriptions.size(); ++index ) {
            attributeDescriptions[index] = {};
            attributeDescriptions[index].binding = 0;
            attributeDescriptions[index].location = index;
            attributeDescriptions[index].format = VulkanHelpers::ToVkShaderDataType( layout[index].GetType() );
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
        //bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX; // not using instanced rendering, so we'll stick to per-vertex data. (Optional) to specify as parameter to this function
        bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescriptions;
    }

    static auto GetShaderStagesInfo(const std::vector<ShaderModuleHandle>& shaders) -> std::vector<VkPipelineShaderStageCreateInfo> {
        std::vector<VkPipelineShaderStageCreateInfo> shaderStagesInfos{};

        for (auto& shader : shaders) {
            if (!shader.IsEmpty()) {
                const auto vkModule{ dynamic_cast<const VulkanShader*>(shader.GetRaw() ) };
                shaderStagesInfos.emplace_back(vkModule->GetPipelineStageCreateInfo());
            }
        }

        return shaderStagesInfos;
    }

    VulkanGraphicsPipeline::VulkanGraphicsPipeline( const VulkanGraphicsPipelineDescription& info)
        : GraphicsPipeline{ info.ShaderModules } {

        m_DepthAttachmentFormat = dynamic_cast<const VulkanTexture*>(info.Depth.GetRaw() )->GetViewCreateInfo().format;

        for (auto& attachment : info.ColorAttachments) {
            m_ColorAttachmentsFormats.emplace_back( dynamic_cast<const VulkanTexture*>(attachment.GetRaw() )->GetViewCreateInfo().format );
        }
    }

    auto VulkanGraphicsPipeline::Release() -> void {
        DestroyReflectedPipeline( VK_DEVICE(m_Device), m_ReflectionData );

        vkDestroyPipeline( VK_DEVICE(m_Device), m_Pipeline, nullptr );
        m_IsAllocated = false;
    }

    auto VulkanGraphicsPipeline::Bind( const VkCommandBuffer commandBuffer ) const -> void {
        MKT_ASSERT( m_Pipeline != VK_NULL_HANDLE, "VulkanGraphicsPipeline::Bind - Graphics pipeline is null." );
        vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline );
    }

    auto VulkanGraphicsPipeline::GetNativeHandle( ObjectType type ) -> Object {
        switch (type) {

            case ObjectType::Vk_PipelineLayout:
                return Object(m_ReflectionData.pipelineLayout);

            case ObjectType::Vk_Pipeline:
                return Object(m_Pipeline );

            case ObjectType::Vk_DescriptorSetLayout:
                return Object( m_ReflectionData.setLayouts.data() );

            default:;
        }

        return Object(nullptr);
    }

    auto VulkanGraphicsPipeline::GetDescriptorSetLayout( UInt32 index ) const -> const VkDescriptorSetLayout& {
        MKT_ASSERT( index < m_ReflectionData.setLayouts.size(), "VulkanGraphicsPipeline::GetDescriptorSetLayout - Index out of bounds." );
        return m_ReflectionData.setLayouts.at(index);
    }

    VulkanGraphicsPipeline::~VulkanGraphicsPipeline() {
        if (m_IsAllocated) {
            Release();
        }
    }

    auto VulkanGraphicsPipeline::Initialize() -> void {
        // TODO: Memory corrupted after setting up the shaders Debug for more details and check ColorBlendInfo
        auto defaultInfo{ GetDefaultGraphicsPipelineConfigInfo() };

        // FIXME: See GetDefaultGraphicsPipelineConfigInfo
        defaultInfo.ColorBlendInfo.pAttachments = &defaultInfo.ColorBlendAttachment;

        VkGraphicsPipelineCreateInfo pipelineInfo{ VulkanHelpers::Initializers::GraphicsPipelineCreateInfo() };

        // Setup Shaders
        const auto& shaderStageInfos{ GetShaderStagesInfo(m_ShaderModules) };
        if (shaderStageInfos.empty()) {
            MKT_THROW_RUNTIME_ERROR( "VulkanGraphicsPipeline::Initialize - stage infos is empty" );
        }

        pipelineInfo.stageCount = shaderStageInfos.size();
        pipelineInfo.pStages = shaderStageInfos.data();

        // Pipeline layout
        std::vector<std::vector<UInt32>> shaderBlocks{};
        for (const auto& shader : m_ShaderModules) {
            const void* data{ shader->GetContents() };
            const Size size{ shader->GetContentSize() };

            const auto* begin{ reinterpret_cast<const UInt32*>(static_cast<const std::byte*>(data)) };
            const auto* end{ reinterpret_cast<const UInt32*>(static_cast<const std::byte*>(data) + size) };

            shaderBlocks.emplace_back(begin, end);
        }

        const VkResult res{ ReflectSPIRV( VK_DEVICE(m_Device), shaderBlocks, m_ReflectionData) };
        if (res != VK_SUCCESS || m_ReflectionData.pipelineLayout == VK_NULL_HANDLE) {
            MKT_THROW_RUNTIME_ERROR( "VulkanGraphicsPipeline::Initialize - Layout is null handle" );
        }

        // Setup Vertex input
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{ VulkanHelpers::Initializers::PipelineVertexInputStateCreateInfo() };

        // Binding descriptions (define data layout)
        const auto& bindingDesc{ GetDefaultBindingDescriptions( DEFAULT_VERTEX_BUFFER_LAYOUT ) };
        const auto& attributeDesc{ GetDefaultAttributeDescriptions( DEFAULT_VERTEX_BUFFER_LAYOUT ) };

        vertexInputInfo.vertexBindingDescriptionCount = bindingDesc.size();
        vertexInputInfo.vertexAttributeDescriptionCount = attributeDesc.size();
        vertexInputInfo.pVertexAttributeDescriptions = attributeDesc.data();
        vertexInputInfo.pVertexBindingDescriptions = bindingDesc.data();

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
        pipelineInfo.pInputAssemblyState = std::addressof( defaultInfo.InputAssemblyInfo );
        pipelineInfo.pViewportState = std::addressof( defaultInfo.ViewportInfo );
        pipelineInfo.pRasterizationState = std::addressof( defaultInfo.RasterizationInfo );
        pipelineInfo.pMultisampleState = std::addressof( defaultInfo.MultisampleInfo );
        pipelineInfo.pColorBlendState = std::addressof( defaultInfo.ColorBlendInfo );
        pipelineInfo.pDepthStencilState = std::addressof( defaultInfo.DepthStencilInfo );
        pipelineInfo.layout = m_ReflectionData.pipelineLayout;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineIndex = -1;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.pDynamicState = std::addressof( defaultInfo.DynamicStateInfo );

        if ( vkCreateGraphicsPipelines( VK_DEVICE( m_Device ), VK_NULL_HANDLE, 1, std::addressof( pipelineInfo ), nullptr, std::addressof( m_Pipeline ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanGraphicsPipeline::Initialize - Failed to create Graphics pipeline" );
        }

        m_IsAllocated = true;
    }

    VulkanComputePipeline::VulkanComputePipeline(const ComputePipelineDescription& info)
        : ComputePipeline( info )
    {

    }

    auto VulkanComputePipeline::Release() -> void {
        DestroyReflectedPipeline( VK_DEVICE(m_Device), m_ReflectionData );

        vkDestroyPipeline( VK_DEVICE( m_Device ), m_Pipeline, nullptr );
        m_IsAllocated = false;
    }

    auto VulkanComputePipeline::Bind( const VkCommandBuffer commandBuffer ) const -> void {
        MKT_ASSERT( m_Pipeline != VK_NULL_HANDLE, "VulkanComputePipeline::Bind - Compute pipeline is null." );
        vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline );
    }

    VulkanComputePipeline::~VulkanComputePipeline() {
        if (m_IsAllocated) {
            Release();
        }
    }

    auto VulkanComputePipeline::GetDescriptorSetLayout( UInt32 index ) const -> const VkDescriptorSetLayout& {
        MKT_ASSERT( index < m_ReflectionData.setLayouts.size(), "VulkanGraphicsPipeline::GetDescriptorSetLayout - Index out of bounds." );
        return m_ReflectionData.setLayouts.at( index );
    }

    auto VulkanComputePipeline::Initialize() -> void {
        VkComputePipelineCreateInfo computePipelineCreateInfo{ VulkanHelpers::Initializers::ComputePipelineCreateInfo() };

        const auto& shaderStageInfos{ GetShaderStagesInfo(m_ShaderModules) };

        std::vector<std::vector<UInt32>> shaderBlocks{};
        for (const auto& shader : m_ShaderModules) {
            const void* data{ shader->GetContents() };
            const Size size{ shader->GetContentSize() };

            const auto* begin{ reinterpret_cast<const UInt32*>(static_cast<const std::byte*>(data)) };
            const auto* end{ reinterpret_cast<const UInt32*>(static_cast<const std::byte*>(data) + size) };

            shaderBlocks.emplace_back(begin, end);
        }

        VkResult res{ ReflectSPIRV( VK_DEVICE(m_Device), shaderBlocks, m_ReflectionData ) };

        if (shaderStageInfos.empty() || m_ReflectionData.pipelineLayout == VK_NULL_HANDLE) {
            MKT_THROW_RUNTIME_ERROR( "VulkanComputePipeline::Initialize - stage infos is empty or pipeline layout is null handle." );
        }

        // I use front() because compute pipeline only have one stage, the compute shader
        computePipelineCreateInfo.stage = shaderStageInfos.front();
        computePipelineCreateInfo.layout = m_ReflectionData.pipelineLayout;

        if ( vkCreateComputePipelines( VK_DEVICE(m_Device), VK_NULL_HANDLE, 1, std::addressof( computePipelineCreateInfo ), nullptr, std::addressof( m_Pipeline ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanComputePipeline::Initialize - Failed to create compute pipeline" );
        }

        m_IsAllocated = true;
    }

    auto VulkanComputePipeline::GetNativeHandle( ObjectType type ) -> Object {
        switch (type) {
            case ObjectType::Vk_PipelineLayout:
                return Object(m_ReflectionData.pipelineLayout);

            case ObjectType::Vk_Pipeline:
                return Object(m_Pipeline );

            case ObjectType::Vk_DescriptorSetLayout:
                return Object( m_ReflectionData.setLayouts.data() );

            default:;
        }

        return Object(nullptr);
    }
}// namespace Mikoto