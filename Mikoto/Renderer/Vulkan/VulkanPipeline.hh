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

#include <vector>
#include <memory>

#include <volk.h>

#include <Common/Common.hh>
#include <Renderer/Core/Pipeline.hh>

namespace Mikoto {

    class VulkanPipeline {
    public:

        MKT_NODISCARD auto Get() const -> VkPipeline;
        MKT_NODISCARD auto GetLayout() const -> VkPipelineLayout;

        MKT_NODISCARD auto HasPushConstants() const -> bool;

        MKT_NODISCARD auto GetDescriptorLayoutCount() const -> Size;
        MKT_NODISCARD auto GetPushConstantRangeShaderFlags() const -> VkShaderStageFlags;
        MKT_NODISCARD auto GetDescriptorSetIndices() const -> std::vector<UInt32>;
        MKT_NODISCARD auto GetDescriptorSetLayout( UInt32 index ) const -> const VkDescriptorSetLayout&;

    protected:
        VkPipeline m_Pipeline{};
        VulkanHelpers::Reflection::ReflectedData m_ReflectionData{};
    };

    struct VulkanGraphicsPipelineDescription {
        GraphicsPipelineDescription Desc{};

        // Will be deprecated most likely in the future as
        // Mikoto targets Vulkan 1.3 onwards
#if !defined(MKT_USE_VULKAN_DYNAMIC_RENDERING)
        // Not needed if we do dynamic rendering
        UInt32 Subpass{};
        VkRenderPass RenderPass{};
#endif
    };

    struct VulkanGraphicsPipelineConfiguration {
        VkPipelineViewportStateCreateInfo ViewportInfo{};
        VkPipelineInputAssemblyStateCreateInfo InputAssemblyInfo{};
        VkPipelineRasterizationStateCreateInfo RasterizationInfo{};
        VkPipelineMultisampleStateCreateInfo MultisampleInfo{};
        VkPipelineColorBlendStateCreateInfo ColorBlendInfo{};
        VkPipelineDepthStencilStateCreateInfo DepthStencilInfo{};
        VkPipelineDynamicStateCreateInfo DynamicStateInfo{};

        std::vector<VkPipelineColorBlendAttachmentState> ColorBlendAttachment{};

        std::vector<VkDynamicState> DynamicStates{};
    };

    class VulkanGraphicsPipeline final : public VulkanPipeline, public GraphicsPipeline {
    public:
        explicit VulkanGraphicsPipeline(const VulkanGraphicsPipelineDescription& info);

        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) -> Object override;

        ~VulkanGraphicsPipeline() override;

    public:
        DISABLE_COPY_AND_MOVE_FOR(VulkanGraphicsPipeline);

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

        auto SetupDefaultConfiguration() -> void;

    private:
        VkFormat m_DepthAttachmentFormat{};
        std::vector<VkFormat> m_ColorAttachmentsFormats{};

        std::vector<VkDynamicState> m_DynamicStates{};
        VulkanGraphicsPipelineConfiguration m_PipelineConfig{};
    };

    class VulkanComputePipeline final : public VulkanPipeline, public ComputePipeline {
    public:
        explicit VulkanComputePipeline( const ComputePipelineDescription& info );

        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) -> Object override;

        ~VulkanComputePipeline() override;

    public:
        DISABLE_COPY_AND_MOVE_FOR(VulkanComputePipeline);

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;
    };

#define MKT_TO_VK_PIPELINE(PIPELINE_HANDLE) dynamic_cast<VulkanPipeline*>(PIPELINE_HANDLE.GetRaw())
#define MKT_TO_VK_GRAPHICS_PIPELINE(PIPELINE_HANDLE) dynamic_cast<VulkanGraphicsPipeline*>(PIPELINE_HANDLE.GetRaw())
#define MKT_TO_VK_COMPUTE_PIPELINE(PIPELINE_HANDLE) dynamic_cast<VulkanComputePipeline*>(PIPELINE_HANDLE.GetRaw())
}

#endif // MIKOTO_VULKAN_PIPELINE_HH
