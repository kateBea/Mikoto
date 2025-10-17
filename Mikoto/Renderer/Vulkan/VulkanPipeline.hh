/**
 * VulkanPipeline.hh
 * Created by kate on 6/2/23.
 * */

#ifndef MIKOTO_VULKAN_PIPELINE_HH
#define MIKOTO_VULKAN_PIPELINE_HH

// C++ Standard Library
#include <filesystem>
#include <vector>
#include <memory>

// Third-Party Library
#include "volk.h"

// Project Headers
#include <Common/Common.hh>
#include <Renderer/Pipeline.hh>

namespace Mikoto {

    struct VulkanGraphicsPipelineCreateInfo {
        UInt32 Subpass{};

#if !defined(MKT_USE_VULKAN_DYNAMIC_RENDERING)
        // Not needed if we do dynamic rendering
        VkRenderPass RenderPass{};
#endif

        //TODO: Review, this causes this struct to pass inconsistent data (diff values when you put this field vs when u don't)
        //BufferLayout Layout{};

        VkPipelineViewportStateCreateInfo ViewportInfo{};
        VkPipelineInputAssemblyStateCreateInfo InputAssemblyInfo{};
        VkPipelineRasterizationStateCreateInfo RasterizationInfo{};
        VkPipelineMultisampleStateCreateInfo MultisampleInfo{};
        VkPipelineColorBlendAttachmentState ColorBlendAttachment{};
        VkPipelineColorBlendStateCreateInfo ColorBlendInfo{};
        VkPipelineDepthStencilStateCreateInfo DepthStencilInfo{};
        VkPipelineDynamicStateCreateInfo DynamicStateInfo{};

        std::span<const VkDynamicState> DynamicStateEnables{};
    };

    class VulkanGraphicsPipeline final : public GraphicsPipeline {
    public:

        explicit VulkanGraphicsPipeline(const VulkanGraphicsPipelineCreateInfo& info);

        auto Release() -> void override;
        auto Bind(VkCommandBuffer commandBuffer) const -> void;

        MKT_NODISCARD auto Get() const -> const VkPipeline& { return m_GraphicsPipeline; }
        MKT_NODISCARD auto GetLayout() const -> const VkPipelineLayout& { return m_PipelineLayout; }

        ~VulkanGraphicsPipeline() override;

    public:
        DISABLE_COPY_AND_MOVE_FOR(VulkanGraphicsPipeline);

    private:
        auto Initialize() -> void override;

    private:
        BufferLayout m_BufferLayout{};

        VkPipeline m_GraphicsPipeline{};
        VkPipelineLayout m_PipelineLayout{};

        VulkanGraphicsPipelineCreateInfo m_ConfigInfo{};
    };

    class VulkanComputePipeline final : public ComputePipeline {
    public:
        explicit VulkanComputePipeline();

        auto Release() -> void override;
        auto Bind(VkCommandBuffer commandBuffer) const -> void;

        MKT_NODISCARD auto Get() const -> const VkPipeline& { return m_GraphicsPipeline; }
        MKT_NODISCARD auto GetLayout() const -> const VkPipelineLayout& { return m_PipelineLayout; }

        ~VulkanComputePipeline() override;

    public:
        DISABLE_COPY_AND_MOVE_FOR(VulkanComputePipeline);

    private:
        auto Initialize() -> void override;

    private:
        VkPipeline m_GraphicsPipeline{};
        VkPipelineLayout m_PipelineLayout{};
        std::span<VkPipelineShaderStageCreateInfo> m_ShaderStages{};
    };
}

#endif // MIKOTO_VULKAN_PIPELINE_HH
