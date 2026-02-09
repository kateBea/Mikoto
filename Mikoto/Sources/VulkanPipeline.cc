//    Copyright 2025 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <fstream>
#include <array>
#include <ranges>
#include <vector>

#include <volk.h>

#include <Common/Common.hh>

#include <Logging/Logger.hh>

#include <Library/Utility/Types.hh>

#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanDevice.hh>
#include <Renderer/Vulkan/VulkanHelpers.hh>
#include <Renderer/Vulkan/VulkanPipeline.hh>
#include <Renderer/Vulkan/VulkanShader.hh>

namespace Mikoto {

    static auto InferVulkanTopology(Topology topology) -> VkPrimitiveTopology {
        switch (topology) {
            case Topology::POINT_LIST: return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
            case Topology::LINE_LIST: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
            case Topology::LINE_STRIP: return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
            case Topology::TRIANGLE_LIST: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            case Topology::TRIANGLE_STRIP: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
            case Topology::TRIANGLE_FAN: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
        }

        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    }

    static auto InferCullMode(CullMode cullMode) -> VkCullModeFlagBits {
        switch (cullMode) {
            case CullMode::NONE: return VK_CULL_MODE_NONE;
            case CullMode::CULL_BACK: return VK_CULL_MODE_BACK_BIT;
            case CullMode::CULL_FRONT: return VK_CULL_MODE_FRONT_BIT;
        }

        return VK_CULL_MODE_NONE;
    }

    static auto GetDefaultGraphicsPipelineConfigInfo() -> VulkanGraphicsPipelineConfiguration {
        VulkanGraphicsPipelineConfiguration configInfo{};

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

        configInfo.RasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        configInfo.RasterizationInfo.depthClampEnable = VK_FALSE;
        configInfo.RasterizationInfo.rasterizerDiscardEnable = VK_FALSE;// requires extension if enabled
        configInfo.RasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;

        // The maximum line width that is supported depends on the hardware, any line thicker than 1.0f requires you to enable the wideLines GPU feature.
        configInfo.RasterizationInfo.lineWidth = 0.0f;
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

        return configInfo;
    }

    static auto GetDefaultAttributeDescriptions( std::vector<AttributesSpec>& attributeSpec ) -> std::vector<VkVertexInputAttributeDescription> {
        Size attributeCount{ };
        for (Size attributeBinding{}; attributeBinding < attributeSpec.size(); ++attributeBinding ) {
            attributeCount += attributeSpec[attributeBinding].DefaultVertexLayout.GetCount();
        }

        std::vector attributeDescriptions( attributeCount, VkVertexInputAttributeDescription{} );

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
        Size attributeIndex{};
        for (Size attributeBinding{}; attributeBinding < attributeSpec.size(); ++attributeBinding ) {
            for ( Size currentBufferLayoutCount{}; currentBufferLayoutCount < attributeSpec[attributeBinding].DefaultVertexLayout.GetCount(); ++currentBufferLayoutCount ) {
                const BufferLayout& layout{ attributeSpec[attributeBinding].DefaultVertexLayout };

                attributeDescriptions[attributeIndex] = {};
                attributeDescriptions[attributeIndex].binding = attributeBinding;
                attributeDescriptions[attributeIndex].location = attributeIndex;
                attributeDescriptions[attributeIndex].format = VulkanHelpers::ToVkShaderDataType( layout[currentBufferLayoutCount].GetType() );
                attributeDescriptions[attributeIndex].offset = layout[currentBufferLayoutCount].GetOffset();

                ++attributeIndex;
            }
        }


        return attributeDescriptions;
    }

    // Helper function to convert Mikoto InputRate -> Vulkan VkVertexInputRate
    MKT_NODISCARD static auto ToVulkanInputRate(InputRate rate) -> VkVertexInputRate {
        switch (rate) {
            case InputRate::PER_VERTEX:   return VK_VERTEX_INPUT_RATE_VERTEX;
            case InputRate::PER_INSTANCE: return VK_VERTEX_INPUT_RATE_INSTANCE;
            default:                      return VK_VERTEX_INPUT_RATE_VERTEX; // fallback
        }
    }

    static auto GetDefaultBindingDescriptions(std::vector<AttributesSpec>& attributeSpec ) -> std::vector<VkVertexInputBindingDescription> {
        // All of our per-vertex data is packed together in one array, so we're only going to have one binding.
        // See: https://vulkan-tutorial.com/Vertex_buffers/Vertex_input_description

        auto bindingDescriptions{ std::vector<VkVertexInputBindingDescription>(attributeSpec.size()) };

        for (Size attributeBinding{}; attributeBinding < attributeSpec.size(); ++attributeBinding ) {
            bindingDescriptions[attributeBinding] = {};
            bindingDescriptions[attributeBinding].binding = attributeBinding;
            bindingDescriptions[attributeBinding].stride = attributeSpec[attributeBinding].DefaultVertexLayout.GetStride();
            bindingDescriptions[attributeBinding].inputRate = ToVulkanInputRate(attributeSpec[attributeBinding].InputRateSpec.AttributeRate);
        }

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

    auto VulkanPipeline::Get() const -> VkPipeline {
        return m_Pipeline;
    }

    auto VulkanPipeline::GetLayout() const -> VkPipelineLayout {
        return m_ReflectionData.pipelineLayout;
    }

    auto VulkanPipeline::HasPushConstants() const -> bool {
        return !m_ReflectionData.pushConstantRanges.empty();
    }

    auto VulkanPipeline::GetDescriptorLayoutCount() const -> Size {
        return m_ReflectionData.setLayouts.size();
    }

    auto VulkanPipeline::GetPushConstantRangeShaderFlags() const -> VkShaderStageFlags {
        return m_ReflectionData.pushConstantRanges.empty() ? VK_FLAGS_NONE : m_ReflectionData.pushConstantRanges.front().stageFlags;
    }

    auto VulkanPipeline::GetDescriptorSetIndices() const -> std::vector<UInt32> {
        std::vector<UInt32> keys{};
        keys.reserve(m_ReflectionData.setLayouts.size());

        for ( const auto& key: m_ReflectionData.setLayouts | std::views::keys )
            keys.push_back(key);

        return keys;
    }

    auto VulkanPipeline::GetDescriptorSetLayout( UInt32 index ) const -> const VkDescriptorSetLayout& {
        return m_ReflectionData.setLayouts.at(index);
    }

    VulkanGraphicsPipeline::VulkanGraphicsPipeline( const VulkanGraphicsPipelineDescription& info)
        : GraphicsPipeline{ info.Desc.ShaderStages } {

        m_Topology = info.Desc.PrimitiveTopology;
        m_CullMode = info.Desc.PipelineCullMode;
        m_Wireframe = info.Desc.Wireframe;
        m_VertexAttributesSpec = info.Desc.VertexAttributesSpec;

        m_Multisampling = info.Desc.Multisampling;
        m_EnableSampleRateShading = info.Desc.EnableSampleRateShading;

        // Depth Render target format
        m_DepthAttachmentFormat = VulkanHelpers::ToVkFormat( info.Desc.DepthAttachmentFormat );

        // Color render target formats
        for (auto& attachment : info.Desc.ColorAttachmentFormats) {
            // TODO: configure blend for each attachment in this order
            m_ColorAttachmentsFormats.emplace_back(VulkanHelpers::ToVkFormat( attachment ));
        }
    }

    auto VulkanGraphicsPipeline::Release() -> void {
        DestroyReflectedPipeline( VK_DEVICE(m_Device), m_ReflectionData );

        vkDestroyPipeline( VK_DEVICE(m_Device), m_Pipeline, nullptr );
        m_IsAllocated = false;
    }

    auto VulkanGraphicsPipeline::SetupDefaultConfiguration() -> void {
        m_PipelineConfig = GetDefaultGraphicsPipelineConfigInfo();

        for (const auto& _ : m_ColorAttachmentsFormats) {
            auto& blendAttachmentInfo{ m_PipelineConfig.ColorBlendAttachment.emplace_back() };
            // Blending enabled by default
            blendAttachmentInfo.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            blendAttachmentInfo.blendEnable = VK_TRUE;
            blendAttachmentInfo.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            blendAttachmentInfo.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            blendAttachmentInfo.colorBlendOp = VK_BLEND_OP_ADD;
            blendAttachmentInfo.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            blendAttachmentInfo.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            blendAttachmentInfo.alphaBlendOp = VK_BLEND_OP_ADD;
        }

        m_PipelineConfig.ColorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        m_PipelineConfig.ColorBlendInfo.logicOpEnable = VK_FALSE;
        m_PipelineConfig.ColorBlendInfo.logicOp = VK_LOGIC_OP_COPY;
        m_PipelineConfig.ColorBlendInfo.attachmentCount = static_cast<UInt32>(m_PipelineConfig.ColorBlendAttachment.size());
        m_PipelineConfig.ColorBlendInfo.pAttachments = m_PipelineConfig.ColorBlendAttachment.data();
        m_PipelineConfig.ColorBlendInfo.blendConstants[0] = 0.0f;
        m_PipelineConfig.ColorBlendInfo.blendConstants[1] = 0.0f;
        m_PipelineConfig.ColorBlendInfo.blendConstants[2] = 0.0f;
        m_PipelineConfig.ColorBlendInfo.blendConstants[3] = 0.0f;

        m_PipelineConfig.InputAssemblyInfo.topology = InferVulkanTopology(m_Topology);

        // Multisampling config
        m_PipelineConfig.MultisampleInfo.sampleShadingEnable = m_EnableSampleRateShading ? VK_TRUE : VK_FALSE;
        m_PipelineConfig.MultisampleInfo.rasterizationSamples = VulkanHelpers::ToVkRasterSamples( m_Multisampling );

        m_PipelineConfig.DepthStencilInfo.depthWriteEnable = m_DepthWrite ? VK_TRUE : VK_FALSE;
        m_PipelineConfig.DepthStencilInfo.depthTestEnable = m_DepthTest ? VK_TRUE : VK_FALSE;

        m_PipelineConfig.RasterizationInfo.cullMode = InferCullMode(m_CullMode);

        constexpr float GPU_STANDARD_LINE_WIDTH{ 1.0f };
        m_PipelineConfig.RasterizationInfo.lineWidth = GPU_STANDARD_LINE_WIDTH;

        if (m_Wireframe) {
            m_PipelineConfig.RasterizationInfo.polygonMode = VK_POLYGON_MODE_LINE;
            m_DynamicStates.emplace_back( VK_DYNAMIC_STATE_LINE_WIDTH );
        }

        // VK_DYNAMIC_STATE_VERTEX_INPUT_EXT can reduce the amount of pipelines the application needs to create
        // because it allows for vertex input binding and attribute descriptions to be dynamic. This is, of course, not a
        // core feature as of Vulkan 1.3 and requires to be enabled when creating the device on which this pipeline will be created
        // Make it static because pDynamicStates does not persist the value beyond this scope
        m_DynamicStates.emplace_back( VK_DYNAMIC_STATE_VIEWPORT );
        m_DynamicStates.emplace_back( VK_DYNAMIC_STATE_SCISSOR );

        std::ranges::copy( m_DynamicStates, std::back_inserter( m_PipelineConfig.DynamicStates ) );

        m_PipelineConfig.DynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        m_PipelineConfig.DynamicStateInfo.pDynamicStates = m_PipelineConfig.DynamicStates.data();
        m_PipelineConfig.DynamicStateInfo.dynamicStateCount = m_PipelineConfig.DynamicStates.size();
        m_PipelineConfig.DynamicStateInfo.flags = 0;

    }

    auto VulkanGraphicsPipeline::GetNativeHandle( ObjectType type ) -> Object {
        switch (type) {

            case ObjectType::Vk_PipelineLayout:
                return Object( m_ReflectionData.pipelineLayout );

            case ObjectType::Vk_Pipeline:
                return Object( m_Pipeline );

            default:;
        }

        return Object(nullptr);
    }

    VulkanGraphicsPipeline::~VulkanGraphicsPipeline() {
        if (m_IsAllocated) {
            Release();
        }
    }

    auto VulkanGraphicsPipeline::Initialize() -> void {
        SetupDefaultConfiguration();

        // Setup Shaders
        const auto& shaderStageInfos{ GetShaderStagesInfo(m_ShaderModules) };
        if (shaderStageInfos.empty()) {
            MKT_THROW_RUNTIME_ERROR( "VulkanGraphicsPipeline::Initialize - stage infos is empty" );
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{ VulkanHelpers::Initializers::GraphicsPipelineCreateInfo() };
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
        // Attribute layout is what we specify because we are the ones that know how is buffer data laid out in CPU side
        const auto& attributeDesc{ GetDefaultAttributeDescriptions( m_VertexAttributesSpec ) };
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<UInt32>( attributeDesc.size() );
        vertexInputInfo.pVertexAttributeDescriptions = attributeDesc.data();

        const auto& bindingDescriptions{ GetDefaultBindingDescriptions( m_VertexAttributesSpec ) };
        vertexInputInfo.vertexBindingDescriptionCount = static_cast<UInt32>( bindingDescriptions.size() );
        vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.data();

        if (m_ReflectionData.vertexBindings.empty() || m_VertexAttributesSpec.empty()) {
            vertexInputInfo.vertexBindingDescriptionCount = 0;
            vertexInputInfo.pVertexBindingDescriptions = nullptr;

            vertexInputInfo.vertexAttributeDescriptionCount = 0;
            vertexInputInfo.pVertexAttributeDescriptions = nullptr;
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
        pipelineInfo.pInputAssemblyState = std::addressof( m_PipelineConfig.InputAssemblyInfo );
        pipelineInfo.pViewportState = std::addressof( m_PipelineConfig.ViewportInfo );
        pipelineInfo.pRasterizationState = std::addressof( m_PipelineConfig.RasterizationInfo );
        pipelineInfo.pMultisampleState = std::addressof( m_PipelineConfig.MultisampleInfo );
        pipelineInfo.pColorBlendState = std::addressof( m_PipelineConfig.ColorBlendInfo );
        pipelineInfo.pDepthStencilState = std::addressof( m_PipelineConfig.DepthStencilInfo );
        pipelineInfo.layout = m_ReflectionData.pipelineLayout;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineIndex = -1;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.pDynamicState = std::addressof( m_PipelineConfig.DynamicStateInfo );

        if ( vkCreateGraphicsPipelines( VK_DEVICE( m_Device ), VK_NULL_HANDLE, 1, std::addressof( pipelineInfo ), nullptr, std::addressof( m_Pipeline ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanGraphicsPipeline::Initialize - Failed to create Graphics pipeline" );
        }

        if (m_DebugName == GetDefaultDebugName()) {
            m_DebugName = fmt::format( "MikotoPipeline {}, Pool ID: {}", reinterpret_cast<UInt64>( m_Pipeline ), GetHandle() );
        }

        VulkanHelpers::SetObjectDebugName(VK_DEVICE( m_Device ),VK_OBJECT_TYPE_PIPELINE, reinterpret_cast<UInt64>( m_Pipeline ),m_DebugName.c_str() );

        m_IsAllocated = true;
    }

    VulkanComputePipeline::VulkanComputePipeline(const ComputePipelineDescription& info)
        : ComputePipeline( info )
    {}

    auto VulkanComputePipeline::Release() -> void {
        DestroyReflectedPipeline( VK_DEVICE(m_Device), m_ReflectionData );

        vkDestroyPipeline( VK_DEVICE( m_Device ), m_Pipeline, nullptr );
        m_IsAllocated = false;
    }

    VulkanComputePipeline::~VulkanComputePipeline() {
        if (m_IsAllocated) {
            Release();
        }
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

        if (shaderStageInfos.empty() || m_ReflectionData.pipelineLayout == VK_NULL_HANDLE || res != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR( "VulkanComputePipeline::Initialize - Failed to reflect compute pipeline stages." );
        }

        // I use front() because compute pipeline only have one stage, the compute shader
        computePipelineCreateInfo.stage = shaderStageInfos.front();
        computePipelineCreateInfo.layout = m_ReflectionData.pipelineLayout;

        if ( vkCreateComputePipelines( VK_DEVICE(m_Device), VK_NULL_HANDLE, 1, std::addressof( computePipelineCreateInfo ), nullptr, std::addressof( m_Pipeline ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanComputePipeline::Initialize - Failed to create compute pipeline" );
        }

        if (m_DebugName == GetDefaultDebugName()) {
            m_DebugName = fmt::format( "MikotoPipeline {}, Pool ID: {}", reinterpret_cast<UInt64>( m_Pipeline ), GetHandle() );
        }
        VulkanHelpers::SetObjectDebugName(VK_DEVICE( m_Device ),VK_OBJECT_TYPE_PIPELINE, reinterpret_cast<UInt64>( m_Pipeline ),m_DebugName.c_str() );

        m_IsAllocated = true;
    }

    auto VulkanComputePipeline::GetNativeHandle( ObjectType type ) -> Object {
        switch (type) {
            case ObjectType::Vk_PipelineLayout:
                return Object(m_ReflectionData.pipelineLayout);

            case ObjectType::Vk_Pipeline:
                return Object(m_Pipeline );

            default:;
        }

        return Object(nullptr);
    }
}