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

#ifndef MIKOTO_VULKAN_PIPELINE_HH
#define MIKOTO_VULKAN_PIPELINE_HH

#include <EASTL/memory.h>
#include <EASTL/utility.h>
#include <EASTL/vector.h>
#include <ankerl/unordered_dense.h>
#include <volk.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Renderer/Core/Rhi.hh>
#include <Renderer/Vulkan/VulkanReflection.hh>

namespace mikoto::renderer::vulkan {

    using BindingSetLayoutsMap = eastl::fixed_hash_map<u32, VkDescriptorSetLayout,
            kMaxBindingsDescriptorSetLayouts>;

    class GraphicsPipeline final :  public IGraphicsPipeline {
    public:
        explicit GraphicsPipeline( const GraphicsPipelineDescription& info, VkPipelineCache pipelineCache );

        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) -> Object override;
        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) const -> Object override;

        auto SetDebugName( eastl::string_view name ) -> void override;

        ~GraphicsPipeline() override;

    public:
        DISABLE_COPY_AND_MOVE_FOR( GraphicsPipeline );

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

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

        eastl::fixed_hash_map<u32, VkDescriptorSetLayout, kMaxBindingLayouts> mDescriptorSetLayouts{};

        // Input layout
        eastl::fixed_vector<VkVertexInputAttributeDescription, kMaxVertexAttributes> mVertexInputDescriptions{};
        eastl::fixed_vector<VkVertexInputBindingDescription, kMaxVertexBindings> mVertexBindingDescriptions{};

        PipelineReflection mPipelineReflection{};

        BindingSetLayoutsMap mBindingLayoutsMap{};
    };

    class ComputePipeline final :  public IComputePipeline {
    public:
        explicit ComputePipeline( const ComputePipelineDescription& info, VkPipelineCache pipelineCache );

        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) -> Object override;
        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) const -> Object override;

        auto SetDebugName( eastl::string_view name ) -> void override;

        ~ComputePipeline() override;

    public:
        DISABLE_COPY_AND_MOVE_FOR( ComputePipeline );

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

    private:
        VkPipeline mPipeline{};

        VkPipelineCache mPipelineCache{};

        VkPipelineLayout mReflectedPipelineLayout{};

        PipelineReflection mPipelineReflection{};

        BindingSetLayoutsMap mBindingLayoutsMap{};
    };
}// namespace mikoto::renderer::vulkan

#endif// MIKOTO_VULKAN_PIPELINE_HH
