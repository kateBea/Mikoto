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

#include <ranges>

#include <EASTL/span.h>
#include <EASTL/vector.h>
#include <EASTL/algorithm.h>

#include <Core/Timer.hh>

#include <Renderer/Vulkan/VulkanDevice.hh>
#include <Renderer/Vulkan/VulkanPipeline.hh>
#include <Renderer/Vulkan/VulkanHelpers.hh>

namespace mikoto::renderer::vulkan {

    static auto CreatePipelineLayout(
        Device* device,
        PipelineReflection& mPipelineReflection,
        BindingSetLayoutsMap& bindingLayoutsMap,
        VkPipelineLayout& pipelineLayout ) -> void {

        // IMPORTANT:
        // In Vulkan, VkPipelineLayoutCreateInfo::pSetLayouts is an array where each element
        // corresponds to a descriptor set index in order:
        //    pSetLayouts[0] -> set = 0
        //    pSetLayouts[1] -> set = 1
        //    pSetLayouts[2] -> set = 2
        // Vulkan does NOT sort or remap them automatically. If the layouts in out.setLayouts
        // are not in the same order as the shader set indices, you will get validation errors.
        // For example, if the fragment shader uses set = 1 but out.setLayouts[1] corresponds
        // to set = 2, Vulkan will complain that the descriptor is missing.
        // Here we are just filling not used slots with empty descriptor set layouts.

        // TODO: VK_EXT_Pipeline library extension
        // [11:59:23] STDERR LOG [thread 67676] Validation Error: Validation Error: [ VUID-VkPipelineLayoutCreateInfo-graphicsPipelineLibrary-06753 ] |
        // MessageID = 0x57ab6143 | vkCreatePipelineLayout(): pCreateInfo->pSetLayouts[0] is VK_NULL_HANDLE, but VK_EXT_graphics_pipeline_library is not enabled.
        // The Vulkan spec states: If graphicsPipelineLibrary is not enabled, elements of pSetLayouts must be valid VkDescriptorSetLayout objects
        // (https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/vkspec.html#VUID-VkPipelineLayoutCreateInfo-graphicsPipelineLibrary-06753)

        // Find the highest set index
        u32 maxSet{ 0 };
        for ( const auto& setIndex: bindingLayoutsMap ) {
            maxSet = eastl::max( maxSet, setIndex.first );
        }

        // Initialize everything with "holes" and fill accordingly
        VkDescriptorSetLayout emptySetLayout{ device->GetLayoutForEmptySet() };
        eastl::vector<VkDescriptorSetLayout> setLayouts( maxSet + 1, emptySetLayout );

        // Place set layouts at correct set indices
        for ( const auto& [setIndex, layout]: bindingLayoutsMap ) {
            setLayouts[setIndex] = layout;
        }

        VkPipelineLayoutCreateInfo plInfo{ initializers::PipelineLayoutCreateInfo() };

        plInfo.setLayoutCount = as<u32>( setLayouts.size() );
        plInfo.pSetLayouts = setLayouts.data();

        plInfo.pushConstantRangeCount = as<u32>( mPipelineReflection.mPushConstantRanges.size() );
        plInfo.pPushConstantRanges = mPipelineReflection.mPushConstantRanges.data();

        MKT_VK_CHECK( vkCreatePipelineLayout( device->GetDevice(), &plInfo, nullptr, MKT_ADDRESSOF( pipelineLayout ) ) );
    }

    static auto CreateDescriptorSetLayouts(
        Device* device,
        PipelineReflection& pipelineReflection,
        BindingSetLayoutsMap& bindingLayoutsMap,
        BindingLayoutHandle bindingLayoutHandle = BindingLayoutHandle::CreateEmpty()  ) -> void {
        BindingLayout* bindingLayout{ bindingLayoutHandle.IsEmpty() ?
            nullptr : checked_cast<BindingLayout*>( bindingLayoutHandle.GetRaw() )
        };

        for ( const auto& [setIndex, setBindings]: pipelineReflection.mBindingSetsMap ) {
            if (bindingLayout && setIndex == bindingLayout->GetRegisterSpace()) {
                VkDescriptorSetLayout setLayout{ bindingLayout->GetNativeHandle( ObjectType::Vk_DescriptorSetLayout ) };
                bindingLayoutsMap[setIndex] = setLayout;
                continue;
            }

            // Get the max binding the other ones will be empty
            u32 maxBinding{0};
            for (const auto& item : setBindings) {
                maxBinding = eastl::max( maxBinding, item.second.mBinding );
            }
            std::vector<VkDescriptorSetLayoutBinding> layoutBindings{};
            layoutBindings.resize( maxBinding + 1 );

            // Initialize bindings
            for (u32 index{}; auto& item: layoutBindings ) {
                item = VkDescriptorSetLayoutBinding{
                    .binding = index++,
                    .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER, // Dummy
                    .descriptorCount = 1, // Dummy
                    .stageFlags = VK_SHADER_STAGE_ALL , // Dummy
                };
            }

            // Fill accordingly
            for ( const auto& [bindingIndex, bindingInfo]: setBindings ) {
                layoutBindings[bindingIndex] = VkDescriptorSetLayoutBinding{
                    .binding = bindingIndex,
                    .descriptorType = bindingInfo.mType,
                    .descriptorCount = bindingInfo.mCount,
                    .stageFlags = bindingInfo.mStageFlags,
                };
            }

            VkDescriptorSetLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
            layoutInfo.bindingCount = as<u32>( layoutBindings.size() );
            layoutInfo.pBindings = layoutBindings.data();

            VkDescriptorBindingFlags bindingFlags{ VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT };

            VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{};
            flagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;

            flagsInfo.bindingCount = 1;
            flagsInfo.pBindingFlags = MKT_ADDRESSOF( bindingFlags );

            MKT_VK_CHECK( vkCreateDescriptorSetLayout(
                device->GetDevice(),
                MKT_ADDRESSOF( layoutInfo ),
                nullptr,
                MKT_ADDRESSOF( bindingLayoutsMap[setIndex] ) ) );
        }
    }

    MKT_NODISCARD static auto GetShaderStagesInfo(const eastl::span<const ShaderModuleHandle> shaders) -> eastl::vector<VkPipelineShaderStageCreateInfo> {
        eastl::vector<VkPipelineShaderStageCreateInfo> result{};
        result.reserve(shaders.size());

        eastl::for_each(
            shaders.begin(),
            shaders.end(),
            [&result](const auto& h) {
                if (h.IsEmpty())
                    return;

                const auto* shader{ checked_cast<const Shader*>(h.GetRaw()) };
                result.emplace_back(shader->GetPipelineInfo());
            }
        );

        return result;
    }

    GraphicsPipeline::GraphicsPipeline( const GraphicsPipelineDescription &info, VkPipelineCache pipelineCache )
        : IGraphicsPipeline{ info }, mPipelineCache{ pipelineCache } {
        // [Input assembly]
        mInputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        mInputAssemblyInfo.topology = GetTopology(mDesc.mPrimitiveTopology);;
        mInputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

        // [Viewport and Scissor]
        mViewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        mViewportInfo.viewportCount = 1;
        mViewportInfo.pViewports = nullptr;
        mViewportInfo.scissorCount = 1;
        mViewportInfo.pScissors = nullptr;

        // [Raster]
        mRasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        mRasterizationInfo.depthClampEnable = VK_FALSE;
        mRasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
        mRasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
        mRasterizationInfo.cullMode = GetCullMode(mDesc.mCullMode);

        constexpr float GPU_STANDARD_LINE_WIDTH{ 1.0f };
        mRasterizationInfo.lineWidth = GPU_STANDARD_LINE_WIDTH;

        if (mDesc.mPolygonMode == PolygonMode::eLines) {
            mRasterizationInfo.polygonMode = VK_POLYGON_MODE_LINE;
            //mDynamicStates.emplace_back( VK_DYNAMIC_STATE_LINE_WIDTH ); // Not all hardware support width != 1
        }

        // The maximum line width that is supported depends on the hardware,
        // any line thicker than 1.0f requires you to enable the wideLines GPU feature.
        mRasterizationInfo.lineWidth = 0.0f;
        mRasterizationInfo.cullMode = VK_CULL_MODE_NONE;
        mRasterizationInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        mRasterizationInfo.depthBiasEnable = VK_FALSE;
        mRasterizationInfo.depthBiasConstantFactor = 0.0f;
        mRasterizationInfo.depthBiasClamp = 0.0f;
        mRasterizationInfo.depthBiasSlopeFactor = 0.0f;

        // [Multisampling]
        mMultisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        mMultisampleInfo.sampleShadingEnable = mDesc.mEnableSampleRateShading ? VK_TRUE : VK_FALSE;;
        mMultisampleInfo.rasterizationSamples = GetSampleCount( mDesc.mMultisampling );
        mMultisampleInfo.minSampleShading = 1.0f;         // Optional
        mMultisampleInfo.pSampleMask = nullptr;           // Optional
        mMultisampleInfo.alphaToCoverageEnable = VK_FALSE;// Optional
        mMultisampleInfo.alphaToOneEnable = VK_FALSE;     // Optional

        // [Depth]
        mDepthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        mDepthStencilInfo.depthTestEnable = mDesc.mEnableDepthTest ? VK_TRUE : VK_FALSE;;
        mDepthStencilInfo.depthWriteEnable = mDesc.mEnableDepthWrite ? VK_TRUE : VK_FALSE;;
        mDepthStencilInfo.depthCompareOp = GetCompareOp(mDesc.mDepthCompareOp);
        mDepthStencilInfo.depthBoundsTestEnable = VK_FALSE;
        mDepthStencilInfo.stencilTestEnable = VK_TRUE;              // Enable stencil test
        mDepthStencilInfo.back.compareOp = VK_COMPARE_OP_ALWAYS;    // Always pass
        mDepthStencilInfo.back.failOp = VK_STENCIL_OP_REPLACE;
        mDepthStencilInfo.back.depthFailOp = VK_STENCIL_OP_REPLACE;
        mDepthStencilInfo.back.passOp = VK_STENCIL_OP_REPLACE;      // Write stencil value
        mDepthStencilInfo.back.reference = 1;                       // Stencil value to write
        mDepthStencilInfo.back.compareMask = 0xFF;
        mDepthStencilInfo.back.writeMask = 0xFF;
        mDepthStencilInfo.front = mDepthStencilInfo.back;           //Use default settings for front faces

        // [Color blend]
        for (const auto& _ : mDesc.mColorFormats) {
            auto& blendAttachmentInfo{ mColorBlendAttachments.emplace_back() };
            blendAttachmentInfo.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            blendAttachmentInfo.blendEnable = mDesc.mEnableAlphaBlending ? VK_TRUE : VK_FALSE;
            blendAttachmentInfo.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            blendAttachmentInfo.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            blendAttachmentInfo.colorBlendOp = VK_BLEND_OP_ADD;
            blendAttachmentInfo.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            blendAttachmentInfo.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            blendAttachmentInfo.alphaBlendOp = VK_BLEND_OP_ADD;
        }

        mColorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        mColorBlendInfo.logicOpEnable = VK_FALSE;
        mColorBlendInfo.logicOp = VK_LOGIC_OP_COPY;
        mColorBlendInfo.attachmentCount = as<u32>(mColorBlendAttachments.size());
        mColorBlendInfo.pAttachments = mColorBlendAttachments.data();
        mColorBlendInfo.blendConstants[0] = 0.0f;
        mColorBlendInfo.blendConstants[1] = 0.0f;
        mColorBlendInfo.blendConstants[2] = 0.0f;
        mColorBlendInfo.blendConstants[3] = 0.0f;

        // [Dynamic states]
        mDynamicStates.emplace_back( VK_DYNAMIC_STATE_VIEWPORT );
        mDynamicStates.emplace_back( VK_DYNAMIC_STATE_SCISSOR );

        mDynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        mDynamicStateInfo.pDynamicStates = mDynamicStates.data();
        mDynamicStateInfo.dynamicStateCount = as<u32>( mDynamicStates.size() );
        mDynamicStateInfo.flags = 0;

        // [Color attachment formats]
        mDepthAttachmentFormat = GetFormat( mDesc.mDepthFormat );
        for (const auto& format : mDesc.mColorFormats) {
            mColorAttachmentsFormats.emplace_back( GetFormat( format ) );
        }
    }

    auto GraphicsPipeline::GetNativeHandle( ObjectType type ) -> Object {
        switch (type) {
            case ObjectType::Vk_Pipeline: return Object( mPipeline );
            case ObjectType::Vk_PipelineLayout:
                return mReflectedPipelineLayout == VK_NULL_HANDLE ?
                mDesc.mPipelineLayout->GetNativeHandle( ObjectType::Vk_PipelineLayout ) : Object( mReflectedPipelineLayout );

            default:;
        }

        return Object(nullptr);
    }

     auto GraphicsPipeline::GetNativeHandle( ObjectType type ) const -> Object {
        switch (type) {
            case ObjectType::Vk_Pipeline: return Object( mPipeline );
            case ObjectType::Vk_PipelineLayout:
                return mReflectedPipelineLayout == VK_NULL_HANDLE ?
                mDesc.mPipelineLayout->GetNativeHandle( ObjectType::Vk_PipelineLayout ) : Object( mReflectedPipelineLayout );

            default:;
        }

        return Object(nullptr);
    }

    auto GraphicsPipeline::SetDebugName( eastl::string_view name ) -> void {
        mDebugName = name;

        auto* device{ checked_cast<Device*>( mDevice ) };
        device->SetDebugName( VK_OBJECT_TYPE_PIPELINE, rc_cast<u64>( mPipeline ), mDebugName );
    }

    GraphicsPipeline::~GraphicsPipeline() {
        if (mIsAllocated) {
            Release();
        }
    }

    auto GraphicsPipeline::Initialize() -> void {
        eastl::fixed_vector<ShaderModuleHandle, kMaxShaders> shaders{};
        for (const auto& [type, shader] : mDesc.mShaders) {
            shaders.emplace_back(shader);
        }

        // [Pipeline layout and Descriptor sets layout]
        mPipelineReflection = PipelineReflection::Reflect( shaders );

        // TODO: This path will be done when we want reflection
        // When reflection is enabled the whole pipeline state is reflected
        // as in all necessary properties are inferred from provided shaders
        // This logic will be moved the GpuDevice will expose via the descriptions
        // some parameters to specify if certain objects should be constructed via reflection
        // like the binding layout, the pipeline layout simply consists of a group of binding layouts and push constant ranges
        // so we only really need to reflect the descriptor set layouts (called Binding layouts in the RHI)
        if (mDesc.mUseReflection) {
            CreateDescriptorSetLayouts( checked_cast<Device*>( mDevice ), mPipelineReflection, mBindingLayoutsMap );
            CreatePipelineLayout( checked_cast<Device*>( mDevice ), mPipelineReflection, mBindingLayoutsMap, mReflectedPipelineLayout );
        }

        // Create pipeline rendering info for dynamic rendering
        VkPipelineRenderingCreateInfo renderingInfo{ initializers::PipelineRenderingCreateInfo() };
        renderingInfo.colorAttachmentCount = as<u32>(mColorAttachmentsFormats.size());
        renderingInfo.pColorAttachmentFormats = mColorAttachmentsFormats.data();
        renderingInfo.depthAttachmentFormat = mDepthAttachmentFormat;
        renderingInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

        VkGraphicsPipelineCreateInfo pipelineInfo{ initializers::GraphicsPipelineCreateInfo() };

        // [Shaders]
        const auto& shaderStageInfos{ GetShaderStagesInfo(shaders) };
        MKT_ASSERT( !shaderStageInfos.empty(), "No shader stage infos available" );
        pipelineInfo.stageCount = mDesc.mShaders.size();
        pipelineInfo.pStages = shaderStageInfos.data();

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{ initializers::PipelineVertexInputStateCreateInfo() };

        // [Vertex attributes]
        // If the client provided an input layout
        if (!mDesc.mInputLayout.IsEmpty()) {
            InputLayout* inputLayout{ checked_cast<InputLayout*>( mDesc.mInputLayout.GetRaw() ) };
            mVertexBindingDescriptions = inputLayout->GetVertexBindingDesc();
            mVertexInputDescriptions = inputLayout->GetVertexAttributesDesc();
        }

        // [VALIDATION] If the vertex shader itself did not declare these attributes
        if (mPipelineReflection.mVertexBindings.empty() || mPipelineReflection.mVertexAttributes.empty()) {
            mVertexInputDescriptions.clear();
            mVertexBindingDescriptions.clear();
        }

        vertexInputInfo.vertexAttributeDescriptionCount = as<u32>( mVertexInputDescriptions.size() );
        vertexInputInfo.pVertexAttributeDescriptions = mVertexInputDescriptions.data();
        vertexInputInfo.vertexBindingDescriptionCount = as<u32>( mVertexBindingDescriptions.size() );
        vertexInputInfo.pVertexBindingDescriptions = mVertexBindingDescriptions.data();

        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineIndex = -1;
        pipelineInfo.layout = mDesc.mPipelineLayout->GetNativeHandle( ObjectType::Vk_PipelineLayout );
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.pDynamicState = MKT_ADDRESSOF( mDynamicStateInfo );

        pipelineInfo.pNext = MKT_ADDRESSOF( renderingInfo );
        pipelineInfo.pVertexInputState = MKT_ADDRESSOF( vertexInputInfo );
        pipelineInfo.pInputAssemblyState = MKT_ADDRESSOF( mInputAssemblyInfo );
        pipelineInfo.pViewportState = MKT_ADDRESSOF( mViewportInfo );
        pipelineInfo.pRasterizationState = MKT_ADDRESSOF( mRasterizationInfo );
        pipelineInfo.pMultisampleState = MKT_ADDRESSOF( mMultisampleInfo );
        pipelineInfo.pColorBlendState = MKT_ADDRESSOF( mColorBlendInfo );
        pipelineInfo.pDepthStencilState = MKT_ADDRESSOF( mDepthStencilInfo );

        MKT_VK_CHECK( vkCreateGraphicsPipelines(
            checked_cast<Device*>( mDevice )->GetDevice(),
            mPipelineCache,
            1,
            MKT_ADDRESSOF( pipelineInfo ),
            nullptr,
            MKT_ADDRESSOF( mPipeline ) ) );

        mIsAllocated = true;
    }

    auto GraphicsPipeline::Release() -> void {
        auto* device{ checked_cast<Device*>( mDevice ) };

        for (const auto& pipelineInfo : mBindingLayoutsMap) {
            if (pipelineInfo.second != VK_NULL_HANDLE) {
                vkDestroyDescriptorSetLayout( device->GetDevice(), pipelineInfo.second, nullptr );
            }
        }

        if (mReflectedPipelineLayout != VK_NULL_HANDLE ) {
            vkDestroyPipelineLayout( device->GetDevice(), mReflectedPipelineLayout, nullptr );
        }

        vkDestroyPipeline( device->GetDevice(), mPipeline, nullptr );

        mIsAllocated = false;
    }

    ComputePipeline::ComputePipeline( const ComputePipelineDescription &info, VkPipelineCache pipelineCache )
        : IComputePipeline{ info }, mPipelineCache{ pipelineCache }
    {}

    auto ComputePipeline::GetNativeHandle( ObjectType type ) -> Object {
        switch (type) {
            case ObjectType::Vk_Pipeline: return Object( mPipeline );
            case ObjectType::Vk_PipelineLayout:
                return mReflectedPipelineLayout == VK_NULL_HANDLE ?
                mDesc.mPipelineLayout->GetNativeHandle( ObjectType::Vk_PipelineLayout ) : Object( mReflectedPipelineLayout );

            default:;
        }

        return Object(nullptr);
    }

    auto ComputePipeline::GetNativeHandle( ObjectType type ) const -> Object {
        switch (type) {
            case ObjectType::Vk_Pipeline: return Object( mPipeline );
            case ObjectType::Vk_PipelineLayout:
                return mReflectedPipelineLayout == VK_NULL_HANDLE ?
                mDesc.mPipelineLayout->GetNativeHandle( ObjectType::Vk_PipelineLayout ) : Object( mReflectedPipelineLayout );

            default:;
        }

        return Object(nullptr);
    }

    auto ComputePipeline::SetDebugName( eastl::string_view name ) -> void {
        mDebugName = name;

        auto* device{ checked_cast<Device*>( mDevice ) };
        device->SetDebugName( VK_OBJECT_TYPE_PIPELINE, rc_cast<u64>( mPipeline ), mDebugName );
    }

    ComputePipeline::~ComputePipeline() {
        if (mIsAllocated) {
            Release();
        }
    }

    auto ComputePipeline::Initialize() -> void {
        eastl::array shaders{ mDesc.mStage };

        // [Pipeline layout and Descriptor sets layout]
        mPipelineReflection = PipelineReflection::Reflect( shaders );

        // TODO: This path will be done when we want reflection
        // When reflection is enabled the whole pipeline state is reflected
        // as in all necessary properties are inferred from provided shaders
        if (mDesc.mUseReflection) {
            CreateDescriptorSetLayouts( checked_cast<Device*>( mDevice ), mPipelineReflection, mBindingLayoutsMap);
            CreatePipelineLayout( checked_cast<Device*>( mDevice ), mPipelineReflection, mBindingLayoutsMap, mReflectedPipelineLayout );
        }

        VkComputePipelineCreateInfo pipelineInfo{ initializers::ComputePipelineCreateInfo() };

        // [Shaders]
        const auto& shaderStageInfos{ GetShaderStagesInfo( shaders ) };
        pipelineInfo.stage = shaderStageInfos.front();
        pipelineInfo.layout = mDesc.mPipelineLayout->GetNativeHandle( ObjectType::Vk_PipelineLayout );;

        MKT_VK_CHECK( vkCreateComputePipelines(
            checked_cast<Device*>( mDevice )->GetDevice(),
            mPipelineCache,
            1,
            MKT_ADDRESSOF( pipelineInfo ),
            nullptr,
            MKT_ADDRESSOF( mPipeline ) ) );

        mIsAllocated = true;
    }

    auto ComputePipeline::Release() -> void {
        auto* device{ checked_cast<Device*>( mDevice ) };

        for (const auto& pipelineInfo : mBindingLayoutsMap) {
            if (pipelineInfo.second != VK_NULL_HANDLE) {
                vkDestroyDescriptorSetLayout( device->GetDevice(), pipelineInfo.second, nullptr );
            }
        }

        if (mReflectedPipelineLayout != VK_NULL_HANDLE ) {
            vkDestroyPipelineLayout( device->GetDevice(), mReflectedPipelineLayout, nullptr );
        }

        vkDestroyPipeline( device->GetDevice(), mPipeline, nullptr );

        mIsAllocated = false;
    }
}// namespace mikoto::renderer::vulkan