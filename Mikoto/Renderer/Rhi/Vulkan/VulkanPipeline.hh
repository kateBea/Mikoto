//    Copyright 2026 ケイト
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

#ifndef MIKOTO_RENDERER_RHI_VULKAN_VULKAN_PIPELINE_HH
#define MIKOTO_RENDERER_RHI_VULKAN_VULKAN_PIPELINE_HH

#include <EASTL/utility.h>
#include <EASTL/array.h>
#include <EASTL/memory.h>
#include <EASTL/vector.h>
#include <EASTL/fixed_hash_map.h>

#include <ankerl/unordered_dense.h>

#include <volk.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Pipeline.hh>
#include <Renderer/Rhi/Vulkan/VulkanReflection.hh>
#include <Renderer/Rhi/Vulkan/VulkanMemoryAllocator.hh>

namespace mikoto::renderer::vulkan {

    using BindingSetLayoutsMap = eastl::fixed_hash_map<core::u32, VkDescriptorSetLayout,
            kMaxBindingsDescriptorSetLayouts>;

    class GraphicsPipeline final :  public rhi::GraphicsPipeline {
    public:
        explicit GraphicsPipeline( const rhi::GraphicsPipelineDescription& info, VkPipelineCache pipelineCache );

        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) -> rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object override;

        auto SetDebugName( eastl::string_view name ) -> void override;

        ~GraphicsPipeline() override;

    public:
        DISABLE_COPY_AND_MOVE_FOR( GraphicsPipeline );

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

    private:
        VkPipeline mPipeline{};
        VkPipelineCache mPipelineCache{};
        VkPipelineLayout mReflectedPipelineLayout{};

        VkFormat mDepthAttachmentFormat{};
        eastl::vector<VkFormat> mColorAttachmentsFormats{};
        eastl::vector<VkDynamicState> mDynamicStates{};

        VkPipelineViewportStateCreateInfo mViewportInfo{};
        VkPipelineInputAssemblyStateCreateInfo mInputAssemblyInfo{};
        VkPipelineRasterizationStateCreateInfo mRasterizationInfo{};
        VkPipelineMultisampleStateCreateInfo mMultisampleInfo{};
        VkPipelineColorBlendStateCreateInfo mColorBlendInfo{};
        VkPipelineDepthStencilStateCreateInfo mDepthStencilInfo{};
        VkPipelineDynamicStateCreateInfo mDynamicStateInfo{};

        // Config per color attachment this
        eastl::vector<VkPipelineColorBlendAttachmentState> mColorBlendAttachments{};

        eastl::fixed_hash_map<core::u32, VkDescriptorSetLayout, rhi::kMaxBindingLayouts> mDescriptorSetLayouts{};

        // Input layout
        eastl::fixed_vector<VkVertexInputAttributeDescription, rhi::kMaxVertexAttributes> mVertexInputDescriptions{};
        eastl::fixed_vector<VkVertexInputBindingDescription, rhi::kMaxVertexBindings> mVertexBindingDescriptions{};

        PipelineReflection mPipelineReflection{};
        BindingSetLayoutsMap mBindingLayoutsMap{};
    };

    class ComputePipeline final :  public rhi::ComputePipeline {
    public:
        explicit ComputePipeline( const rhi::ComputePipelineDescription& info, VkPipelineCache pipelineCache );

        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) -> rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object override;

        auto SetDebugName( eastl::string_view name ) -> void override;

        ~ComputePipeline() override;

    public:
        DISABLE_COPY_AND_MOVE_FOR( ComputePipeline );

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

    private:
        VkPipeline mPipeline{};
        VkPipelineCache mPipelineCache{};
        VkPipelineLayout mReflectedPipelineLayout{};
        PipelineReflection mPipelineReflection{};
        BindingSetLayoutsMap mBindingLayoutsMap{};
    };

    class RayTracingPipeline final : public rhi::RayTracingPipeline {
    public:
        explicit RayTracingPipeline( const rhi::RayTracingPipelineDescription& description, VkPipelineCache pipelineCache );

        ~RayTracingPipeline() override;

        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) -> rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object override;

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

    private:
        VkPipeline mPipeline{};
        VkPipelineCache mPipelineCache{};
        core::u64 mMaxDispatchInvocations{};
    };
}// namespace mikoto::renderer::vulkan

#endif// MIKOTO_RENDERER_RHI_VULKAN_VULKAN_PIPELINE_HH

