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
#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Logging/Assert.hh>

#include <Renderer/Rhi/Utility.hh>
#include <Renderer/Rhi/Vulkan/VulkanDevice.hh>
#include <Renderer/Rhi/Vulkan/VulkanShader.hh>
#include <Renderer/Rhi/Vulkan/VulkanHelpers.hh>
#include <Renderer/Rhi/Vulkan/VulkanPipeline.hh>

namespace mikoto::renderer::vulkan {

    using namespace mikoto::core;
    using namespace mikoto::renderer::rhi;

    MKT_NODISCARD static auto GetShaderStagesInfo( const eastl::span<const renderer::rhi::ShaderModuleHandle> shaders ) -> eastl::vector<VkPipelineShaderStageCreateInfo> {
        MKT_BEGIN_PROFILER();
        eastl::vector<VkPipelineShaderStageCreateInfo> result{};
        result.reserve( shaders.size() );

        eastl::for_each(
            shaders.begin(),
            shaders.end(),
            [&result]( const auto& h ) {
            if ( h.IsEmpty() ) {
                return;
            }

            const auto* shader{ core::checked_cast<const Shader*>( h.GetPtr() ) };
            result.emplace_back( shader->GetPipelineInfo() );
        } );

        if ( result.empty() ) {
            MKT_THROW_RUNTIME_ERROR( "Cannot create a Vulkan pipeline without any shader stages." );
        }

        return result;
    }

    GraphicsPipeline::GraphicsPipeline( const renderer::rhi::GraphicsPipelineDescription& info, VkPipelineCache pipelineCache )
        : rhi::GraphicsPipeline{ info }, mPipelineCache{ pipelineCache } {
        MKT_BEGIN_PROFILER();
        // [Input assembly]
        mInputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        mInputAssemblyInfo.topology = GetTopology( mDesc.mPrimitiveTopology );
        ;
        mInputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

        // [Viewport and Scissor]
        mViewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        mViewportInfo.viewportCount = 1;
        mViewportInfo.pViewports = nullptr;
        mViewportInfo.scissorCount = 1;
        mViewportInfo.pScissors = nullptr;

        mDynamicStates.emplace_back( VK_DYNAMIC_STATE_VIEWPORT );
        mDynamicStates.emplace_back( VK_DYNAMIC_STATE_SCISSOR );

        // [Raster]
        mRasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        mRasterizationInfo.depthClampEnable = VK_FALSE;
        mRasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
        mRasterizationInfo.polygonMode = vulkan::GetPolygonMode( mDesc.mPolygonMode );
        mRasterizationInfo.cullMode = vulkan::GetCullMode( mDesc.mCullMode );

        constexpr float GPU_STANDARD_LINE_WIDTH{ 1.0f };
        mRasterizationInfo.lineWidth = GPU_STANDARD_LINE_WIDTH;

        if ( mDesc.mPolygonMode == renderer::rhi::PolygonMode::eLines ) {
            // This dynamic state is not added because not all hardware support width != 1
            // so we just keep it at 1.0f for everyone by default
            // vkCmdSetLineWidth can be used to change the width
            mDynamicStates.emplace_back( VK_DYNAMIC_STATE_LINE_WIDTH );
        }

        mRasterizationInfo.cullMode = vulkan::GetCullMode( mDesc.mCullMode );
        ;
        mRasterizationInfo.frontFace = vulkan::GetWindingOrder( mDesc.mWindingOrder );

        mRasterizationInfo.depthBiasEnable = VK_FALSE;
        mRasterizationInfo.depthBiasConstantFactor = 0.0f;
        mRasterizationInfo.depthBiasClamp = 0.0f;
        mRasterizationInfo.depthBiasSlopeFactor = 0.0f;

        // [Multisampling]
        mMultisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        mMultisampleInfo.sampleShadingEnable = mDesc.mEnableSampleRateShading ? VK_TRUE : VK_FALSE;
        ;
        mMultisampleInfo.rasterizationSamples = GetSampleCount( mDesc.mMultisampling );// TODO: fallback to device max supported
        mMultisampleInfo.minSampleShading = 1.0f;                                      // Optional
        mMultisampleInfo.pSampleMask = nullptr;                                        // Optional
        mMultisampleInfo.alphaToCoverageEnable = VK_FALSE;                             // Optional
        mMultisampleInfo.alphaToOneEnable = VK_FALSE;                                  // Optional

        // [Depth]
        mDepthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        mDepthStencilInfo.depthTestEnable = mDesc.mEnableDepthTest ? VK_TRUE : VK_FALSE;
        ;
        mDepthStencilInfo.depthWriteEnable = mDesc.mEnableDepthWrite ? VK_TRUE : VK_FALSE;
        ;
        mDepthStencilInfo.depthCompareOp = GetCompareOp( mDesc.mDepthCompareOp );
        mDepthStencilInfo.depthBoundsTestEnable = VK_FALSE;
        mDepthStencilInfo.stencilTestEnable = VK_TRUE;          // Enable stencil test
        mDepthStencilInfo.back.compareOp = VK_COMPARE_OP_ALWAYS;// Always pass
        mDepthStencilInfo.back.failOp = VK_STENCIL_OP_REPLACE;
        mDepthStencilInfo.back.depthFailOp = VK_STENCIL_OP_REPLACE;
        mDepthStencilInfo.back.passOp = VK_STENCIL_OP_REPLACE;// Write stencil value
        mDepthStencilInfo.back.reference = 1;                 // Stencil value to write
        mDepthStencilInfo.back.compareMask = 0xFF;
        mDepthStencilInfo.back.writeMask = 0xFF;
        mDepthStencilInfo.front = mDepthStencilInfo.back;//Use default settings for front faces

        // [Color blend]
        for ( const auto& _: mDesc.mColorFormats ) {
            auto& blendAttachmentInfo{ mColorBlendAttachments.emplace_back() };
            const auto& blend{ mDesc.mAlphaBlendState };
            blendAttachmentInfo.colorWriteMask = vulkan::GetColorWriteMask( blend.mWriteMask );
            blendAttachmentInfo.blendEnable = blend.mEnabled ? VK_TRUE : VK_FALSE;
            blendAttachmentInfo.srcColorBlendFactor = vulkan::GetBlendFactor( blend.mSrcColorFactor );
            blendAttachmentInfo.dstColorBlendFactor = vulkan::GetBlendFactor( blend.mDstColorFactor );
            blendAttachmentInfo.colorBlendOp = vulkan::GetBlendOp( blend.mColorOperation );
            blendAttachmentInfo.srcAlphaBlendFactor = vulkan::GetBlendFactor( blend.mSrcAlphaFactor );
            blendAttachmentInfo.dstAlphaBlendFactor = vulkan::GetBlendFactor( blend.mDstAlphaFactor );
            blendAttachmentInfo.alphaBlendOp = vulkan::GetBlendOp( blend.mAlphaOperation );
        }

        mColorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        mColorBlendInfo.logicOpEnable = VK_FALSE;
        mColorBlendInfo.logicOp = VK_LOGIC_OP_COPY;
        mColorBlendInfo.attachmentCount = core::as<core::u32>( mColorBlendAttachments.size() );
        mColorBlendInfo.pAttachments = mColorBlendAttachments.data();
        mColorBlendInfo.blendConstants[0] = 0.0f;
        mColorBlendInfo.blendConstants[1] = 0.0f;
        mColorBlendInfo.blendConstants[2] = 0.0f;
        mColorBlendInfo.blendConstants[3] = 0.0f;

        mDynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        mDynamicStateInfo.pDynamicStates = mDynamicStates.data();
        mDynamicStateInfo.dynamicStateCount = core::as<core::u32>( mDynamicStates.size() );
        mDynamicStateInfo.flags = 0;

        // [Color attachment formats]
        mDepthAttachmentFormat = GetFormat( mDesc.mDepthFormat );

        for ( const auto& format: mDesc.mColorFormats ) {
            mColorAttachmentsFormats.emplace_back( GetFormat( format ) );
        }
    }

    auto GraphicsPipeline::GetNativeHandle( renderer::rhi::ObjectType type ) -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();

        switch ( type ) {
            case renderer::rhi::ObjectType::Vk_Pipeline:
                return renderer::rhi::Object( mPipeline );
            case renderer::rhi::ObjectType::Vk_PipelineLayout:
                return mReflectedPipelineLayout == VK_NULL_HANDLE ? mDesc.mPipelineLayout->GetNativeHandle( renderer::rhi::ObjectType::Vk_PipelineLayout ) : renderer::rhi::Object( mReflectedPipelineLayout );

            default:;
        }

        return renderer::rhi::Object( nullptr );
    }

    auto GraphicsPipeline::GetNativeHandle( renderer::rhi::ObjectType type ) const -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();

        switch ( type ) {
            case renderer::rhi::ObjectType::Vk_Pipeline:
                return renderer::rhi::Object( mPipeline );
            case renderer::rhi::ObjectType::Vk_PipelineLayout:
                return mReflectedPipelineLayout == VK_NULL_HANDLE ? mDesc.mPipelineLayout->GetNativeHandle( renderer::rhi::ObjectType::Vk_PipelineLayout ) : renderer::rhi::Object( mReflectedPipelineLayout );

            default:;
        }

        return renderer::rhi::Object( nullptr );
    }

    auto GraphicsPipeline::SetDebugName( eastl::string_view name ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !mIsAllocated || name.empty() ) {
            return;
        }

        mDebugName = name;

        auto* device{ core::checked_cast<Device*>( mDevice ) };
        device->SetDebugName( VK_OBJECT_TYPE_PIPELINE, core::rc_cast<core::u64>( mPipeline ), mDebugName );
    }

    GraphicsPipeline::~GraphicsPipeline() {
        MKT_BEGIN_PROFILER();

        if ( mIsAllocated ) {
            Destroy();
        }
    }

    auto GraphicsPipeline::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        eastl::fixed_vector<renderer::rhi::ShaderModuleHandle, renderer::rhi::kMaxShaders> shaders{};

        for ( const auto& [type, shader]: mDesc.mShaders ) {
            shaders.emplace_back( shader );
        }

        Device* device{ core::checked_cast<Device*>( mDevice ) };

        // TODO: This path will be done when we want reflection
        // When reflection is enabled the whole pipeline state must be reflected
        // as in all necessary properties are inferred from provided shaders
        // This logic will be moved the GpuDevice will expose via the descriptions
        // some parameters to specify if certain objects should be constructed via reflection
        // like the binding layout, the pipeline layout simply consists of a group of binding layouts and push constant ranges
        // so we only really need to reflect the descriptor set layouts (called Binding layouts in the RHI)
        // if (mDesc.mUseReflection) {
        //     CreateDescriptorSetLayouts( core::checked_cast<Device*>( mDevice ), mPipelineReflection, mBindingLayoutsMap );
        //     CreatePipelineLayout( core::checked_cast<Device*>( mDevice ), mPipelineReflection, mBindingLayoutsMap, mReflectedPipelineLayout );
        // }

        // Create pipeline rendering info for dynamic rendering
        VkPipelineRenderingCreateInfo renderingInfo{ initializers::PipelineRenderingCreateInfo() };
        renderingInfo.colorAttachmentCount = core::as<core::u32>( mColorAttachmentsFormats.size() );
        renderingInfo.pColorAttachmentFormats = mColorAttachmentsFormats.data();
        renderingInfo.depthAttachmentFormat = mDepthAttachmentFormat;
        renderingInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

        VkGraphicsPipelineCreateInfo pipelineInfo{ initializers::GraphicsPipelineCreateInfo() };

        // [Shaders]
        const auto& shaderStageInfos{ GetShaderStagesInfo( shaders ) };
        MKT_ASSERT( !shaderStageInfos.empty(), "No shader stage infos available" );
        pipelineInfo.stageCount = core::as<core::u32>( shaderStageInfos.size() );
        pipelineInfo.pStages = shaderStageInfos.data();

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{ initializers::PipelineVertexInputStateCreateInfo() };

        // [Vertex attributes]
        // If the client provided an input layout
        if ( !mDesc.mInputLayout.IsEmpty() ) {
            InputLayout* inputLayout{ core::checked_cast<InputLayout*>( mDesc.mInputLayout.GetPtr() ) };
            mVertexBindingDescriptions = inputLayout->GetVertexBindingDesc();
            mVertexInputDescriptions = inputLayout->GetVertexAttributesDesc();
        }

        // [VALIDATION] If the vertex shader itself did not declare these attributes
        // if (mPipelineReflection.mVertexBindings.empty() || mPipelineReflection.mVertexAttributes.empty()) {
        //     mVertexInputDescriptions.clear();
        //     mVertexBindingDescriptions.clear();
        // }

        vertexInputInfo.vertexAttributeDescriptionCount = core::as<core::u32>( mVertexInputDescriptions.size() );
        vertexInputInfo.pVertexAttributeDescriptions = mVertexInputDescriptions.data();
        vertexInputInfo.vertexBindingDescriptionCount = core::as<core::u32>( mVertexBindingDescriptions.size() );
        vertexInputInfo.pVertexBindingDescriptions = mVertexBindingDescriptions.data();

        if ( !mDesc.mPipelineLayout.IsEmpty() ) {
            pipelineInfo.layout = mDesc.mPipelineLayout->GetNativeHandle( renderer::rhi::ObjectType::Vk_PipelineLayout );
        }
        else {
            pipelineInfo.layout = *device->GetDummyPipelineLayout();
        }

        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineIndex = -1;
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
            device->GetDevice(),
            mPipelineCache,
            1,
            MKT_ADDRESSOF( pipelineInfo ),
            nullptr,
            MKT_ADDRESSOF( mPipeline ) ) );

        mIsAllocated = true;
    }

    auto GraphicsPipeline::Destroy() -> void {
        MKT_BEGIN_PROFILER();
        auto* device{ core::checked_cast<Device*>( mDevice ) };

        for ( const auto& pipelineInfo: mBindingLayoutsMap ) {
            if ( pipelineInfo.second != VK_NULL_HANDLE ) {
                vkDestroyDescriptorSetLayout( device->GetDevice(), pipelineInfo.second, nullptr );
            }
        }

        if ( mReflectedPipelineLayout != VK_NULL_HANDLE ) {
            vkDestroyPipelineLayout( device->GetDevice(), mReflectedPipelineLayout, nullptr );
        }

        vkDestroyPipeline( device->GetDevice(), mPipeline, nullptr );

        mIsAllocated = false;
    }

    ComputePipeline::ComputePipeline( const renderer::rhi::ComputePipelineDescription& info, VkPipelineCache pipelineCache )
        : rhi::ComputePipeline{ info }, mPipelineCache{ pipelineCache } {
    }

    auto ComputePipeline::GetNativeHandle( renderer::rhi::ObjectType type ) -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();

        switch ( type ) {
            case renderer::rhi::ObjectType::Vk_Pipeline:
                return renderer::rhi::Object( mPipeline );
            case renderer::rhi::ObjectType::Vk_PipelineLayout:
                return mReflectedPipelineLayout == VK_NULL_HANDLE ? mDesc.mPipelineLayout->GetNativeHandle( renderer::rhi::ObjectType::Vk_PipelineLayout ) : renderer::rhi::Object( mReflectedPipelineLayout );

            default:;
        }

        return renderer::rhi::Object( nullptr );
    }

    auto ComputePipeline::GetNativeHandle( renderer::rhi::ObjectType type ) const -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();

        switch ( type ) {
            case renderer::rhi::ObjectType::Vk_Pipeline:
                return renderer::rhi::Object( mPipeline );
            case renderer::rhi::ObjectType::Vk_PipelineLayout:
                return mReflectedPipelineLayout == VK_NULL_HANDLE ? mDesc.mPipelineLayout->GetNativeHandle( renderer::rhi::ObjectType::Vk_PipelineLayout ) : renderer::rhi::Object( mReflectedPipelineLayout );

            default:;
        }

        return renderer::rhi::Object( nullptr );
    }

    auto ComputePipeline::SetDebugName( eastl::string_view name ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !mIsAllocated || name.empty() ) {
            return;
        }

        mDebugName = name;

        auto* device{ core::checked_cast<Device*>( mDevice ) };
        device->SetDebugName( VK_OBJECT_TYPE_PIPELINE, core::rc_cast<core::u64>( mPipeline ), mDebugName );
    }

    ComputePipeline::~ComputePipeline() {
        MKT_BEGIN_PROFILER();

        if ( mIsAllocated ) {
            Destroy();
        }
    }

    auto ComputePipeline::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        eastl::array shaders{ mDesc.mStage };

        Device* device{ core::checked_cast<Device*>( mDevice ) };

        // TODO: This path will be done when we want reflection
        // When reflection is enabled the whole pipeline state is reflected
        // as in all necessary properties are inferred from provided shaders
        // if (mDesc.mUseReflection) {
        //     CreateDescriptorSetLayouts( core::checked_cast<Device*>( mDevice ), mPipelineReflection, mBindingLayoutsMap);
        //     CreatePipelineLayout( core::checked_cast<Device*>( mDevice ), mPipelineReflection, mBindingLayoutsMap, mReflectedPipelineLayout );
        // }

        VkComputePipelineCreateInfo pipelineInfo{ initializers::ComputePipelineCreateInfo() };

        // [Shaders]
        const auto& shaderStageInfos{ GetShaderStagesInfo( shaders ) };
        pipelineInfo.stage = shaderStageInfos.front();

        if ( !mDesc.mPipelineLayout.IsEmpty() ) {
            pipelineInfo.layout = mDesc.mPipelineLayout->GetNativeHandle( renderer::rhi::ObjectType::Vk_PipelineLayout );
        }
        else {
            pipelineInfo.layout = *device->GetDummyPipelineLayout();
        }

        MKT_VK_CHECK( vkCreateComputePipelines(
            core::checked_cast<Device*>( mDevice )->GetDevice(),
            mPipelineCache,
            1,
            MKT_ADDRESSOF( pipelineInfo ),
            nullptr,
            MKT_ADDRESSOF( mPipeline ) ) );

        mIsAllocated = true;
    }

    auto ComputePipeline::Destroy() -> void {
        MKT_BEGIN_PROFILER();
        auto* device{ core::checked_cast<Device*>( mDevice ) };

        for ( const auto& pipelineInfo: mBindingLayoutsMap ) {
            if ( pipelineInfo.second != VK_NULL_HANDLE ) {
                vkDestroyDescriptorSetLayout( device->GetDevice(), pipelineInfo.second, nullptr );
            }
        }

        if ( mReflectedPipelineLayout != VK_NULL_HANDLE ) {
            vkDestroyPipelineLayout( device->GetDevice(), mReflectedPipelineLayout, nullptr );
        }

        vkDestroyPipeline( device->GetDevice(), mPipeline, nullptr );

        mIsAllocated = false;
    }

    RayTracingPipeline::RayTracingPipeline( const renderer::rhi::RayTracingPipelineDescription& description, VkPipelineCache pipelineCache )
        : rhi::RayTracingPipeline{ description }, mPipelineCache{ pipelineCache } {
    }

    RayTracingPipeline::~RayTracingPipeline() {
        if ( mIsAllocated ) {
            Destroy();
        }
    }

    auto RayTracingPipeline::GetNativeHandle( rhi::ObjectType type ) -> rhi::Object {
        return static_cast<const RayTracingPipeline*>( this )->GetNativeHandle( type );
    }

    auto RayTracingPipeline::GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object {
        return type == rhi::ObjectType::Vk_Pipeline ? rhi::Object{ mPipeline } : rhi::Object{ nullptr };
    }

    auto RayTracingPipeline::Initialize() -> void {
        Device* device{ checked_cast<Device*>( mDevice ) };

        VkPhysicalDeviceRayTracingPipelinePropertiesKHR properties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };
        VkPhysicalDeviceProperties2 physical{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &properties };

        vkGetPhysicalDeviceProperties2( device->GetPhysicalDevice()->mPhysicalDevice, &physical );
        mMaxDispatchInvocations = properties.maxRayDispatchInvocationCount;

        MKT_ASSERT( mDescription.mRecursionDepth <= properties.maxRayRecursionDepth, "Unsupported ray recursion depth." );

        // Vulkan 1.3 consumes the shader modules already compiled by the RHI.
        enum StageIndices : core::u32 {
            eRaygen,
            eMiss,
            eClosestHit,
            eStageCount
        };

        eastl::array<VkPipelineShaderStageCreateInfo, eStageCount> stages{};
        stages[eMiss] = core::checked_cast<Shader*>( mDescription.mMiss.GetPtr() )->GetPipelineInfo();
        stages[eRaygen] = core::checked_cast<Shader*>( mDescription.mRayGeneration.GetPtr() )->GetPipelineInfo();
        stages[eClosestHit] = core::checked_cast<Shader*>( mDescription.mClosestHit.GetPtr() )->GetPipelineInfo();

        // Group indices are stable: ray generation, miss, then the triangle hit group.
        VkRayTracingShaderGroupCreateInfoKHR group{ VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR };
        group.generalShader = VK_SHADER_UNUSED_KHR;
        group.closestHitShader = VK_SHADER_UNUSED_KHR;
        group.anyHitShader = VK_SHADER_UNUSED_KHR;
        group.intersectionShader = VK_SHADER_UNUSED_KHR;

        eastl::array<VkRayTracingShaderGroupCreateInfoKHR, 3> groups{};
        group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        group.generalShader = eRaygen;
        groups[0] = group;

        group.generalShader = eMiss;
        groups[1] = group;

        group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
        group.generalShader = VK_SHADER_UNUSED_KHR;
        group.closestHitShader = eClosestHit;
        groups[2] = group;

        // Descriptor sets and push constants belong to the caller's pipeline layout.
        // Initialize has validated the requested recursion depth against device limits.
        VkRayTracingPipelineCreateInfoKHR pipelineInfo{ VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR };
        pipelineInfo.stageCount = core::as<core::u32>( stages.size() );
        pipelineInfo.pStages = stages.data();
        pipelineInfo.groupCount = core::as<core::u32>( groups.size() );
        pipelineInfo.pGroups = groups.data();
        pipelineInfo.maxPipelineRayRecursionDepth = mDescription.mRecursionDepth;
        pipelineInfo.layout = mDescription.mPipelineLayout->GetNativeHandle( rhi::ObjectType::Vk_PipelineLayout );

        MKT_VK_CHECK( vkCreateRayTracingPipelinesKHR( device->GetDevice(), VK_NULL_HANDLE, mPipelineCache, 1, &pipelineInfo, nullptr, &mPipeline ) );

        mIsAllocated = true;
    }

    auto RayTracingPipeline::Destroy() -> void {
        if ( !mDevice ) {
            return;
        }

        Device* device{ checked_cast<Device*>( mDevice ) };

        if ( mPipeline ) {
            vkDestroyPipeline( device->GetDevice(), mPipeline, nullptr );
            mPipeline = VK_NULL_HANDLE;
        }

        mIsAllocated = false;
    }

}// namespace mikoto::renderer::vulkan
