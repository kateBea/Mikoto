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
#include <Common/VulkanUtils.hh>
#include <Renderer/Vulkan/VulkanShader.hh>

namespace Mikoto {
    struct PipelineConfigInfo {
        VkPipelineViewportStateCreateInfo ViewportInfo{};
        VkPipelineInputAssemblyStateCreateInfo InputAssemblyInfo{};
        VkPipelineRasterizationStateCreateInfo RasterizationInfo{};
        VkPipelineMultisampleStateCreateInfo MultisampleInfo{};
        VkPipelineColorBlendAttachmentState ColorBlendAttachment{};
        VkPipelineColorBlendStateCreateInfo ColorBlendInfo{};
        VkPipelineDepthStencilStateCreateInfo DepthStencilInfo{};
        VkPipelineDynamicStateCreateInfo DynamicStateInfo{};
        VkPipelineLayout PipelineLayout{};
        VkRenderPass RenderPass{};
        UInt32_T Subpass{};

        std::vector<VkDynamicState> DynamicStateEnables{};
        std::vector<VkPipelineShaderStageCreateInfo>* ShaderStages{ nullptr };
    };

    class VulkanPipeline {
    public:
        explicit VulkanPipeline() = default;

        VulkanPipeline(VulkanPipeline&&) = default;
        auto operator=(VulkanPipeline&&) noexcept -> VulkanPipeline& = default;

        auto operator==(const VulkanPipeline& other) const -> bool { return m_GraphicsPipeline == other.m_GraphicsPipeline; }

        auto CreateGraphicsPipeline(const PipelineConfigInfo& config) -> void;

        auto Bind(VkCommandBuffer commandBuffer) const -> void;

        MKT_NODISCARD auto Get() const -> const VkPipeline& { return m_GraphicsPipeline; }
        MKT_NODISCARD static auto GetDefaultPipelineConfigInfo() -> PipelineConfigInfo &;

        ~VulkanPipeline() = default;

    public:
        DELETE_COPY_FOR(VulkanPipeline);

    private:

    private:
        VkPipeline m_GraphicsPipeline{};
        PipelineConfigInfo m_ConfigInfo{};
    };
}

#endif // MIKOTO_VULKAN_PIPELINE_HH
