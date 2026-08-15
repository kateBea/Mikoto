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

#include <Renderer/Rhi/Vulkan/VulkanDevice.hh>
#include <Renderer/Rhi/Vulkan/VulkanShader.hh>
#include <Renderer/Rhi/Vulkan/VulkanPipeline.hh>
#include <Renderer/Rhi/Vulkan/VulkanHelpers.hh>

namespace mikoto::renderer::vulkan {

    using namespace mikoto::core;
    using namespace mikoto::renderer::rhi;

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
        mRasterizationInfo.polygonMode = vulkan::GetPolygonMode(mDesc.mPolygonMode);
        mRasterizationInfo.cullMode = GetCullMode(mDesc.mCullMode);

        constexpr float GPU_STANDARD_LINE_WIDTH{ 1.0f };
        mRasterizationInfo.lineWidth = GPU_STANDARD_LINE_WIDTH;

        if (mDesc.mPolygonMode == PolygonMode::eLines) {
            //mDynamicStates.emplace_back( VK_DYNAMIC_STATE_LINE_WIDTH ); // Not all hardware support width != 1
        }

        // The maximum line width that is supported depends on the hardware,
        // any line thicker than 1.0f requires you to enable the wideLines GPU feature.
        mRasterizationInfo.lineWidth = 0.0f;
        mRasterizationInfo.cullMode = VK_CULL_MODE_NONE;
        mRasterizationInfo.frontFace = vulkan::GetWindingOrder(mDesc.mWindingOrder);

        // This produces black screen on my PC
        // mRasterizationInfo.cullMode = vulkan::GetCullMode( mDesc.mCullMode );

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

        // TODO: This path will be done when we want reflection
        // When reflection is enabled the whole pipeline state is reflected
        // as in all necessary properties are inferred from provided shaders
        // This logic will be moved the GpuDevice will expose via the descriptions
        // some parameters to specify if certain objects should be constructed via reflection
        // like the binding layout, the pipeline layout simply consists of a group of binding layouts and push constant ranges
        // so we only really need to reflect the descriptor set layouts (called Binding layouts in the RHI)
        // if (mDesc.mUseReflection) {
        //     CreateDescriptorSetLayouts( checked_cast<Device*>( mDevice ), mPipelineReflection, mBindingLayoutsMap );
        //     CreatePipelineLayout( checked_cast<Device*>( mDevice ), mPipelineReflection, mBindingLayoutsMap, mReflectedPipelineLayout );
        // }

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
        // if (mPipelineReflection.mVertexBindings.empty() || mPipelineReflection.mVertexAttributes.empty()) {
        //     mVertexInputDescriptions.clear();
        //     mVertexBindingDescriptions.clear();
        // }

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

        // TODO: This path will be done when we want reflection
        // When reflection is enabled the whole pipeline state is reflected
        // as in all necessary properties are inferred from provided shaders
        // if (mDesc.mUseReflection) {
        //     CreateDescriptorSetLayouts( checked_cast<Device*>( mDevice ), mPipelineReflection, mBindingLayoutsMap);
        //     CreatePipelineLayout( checked_cast<Device*>( mDevice ), mPipelineReflection, mBindingLayoutsMap, mReflectedPipelineLayout );
        // }

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