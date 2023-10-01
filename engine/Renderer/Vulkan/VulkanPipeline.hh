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
#include <volk.h>

// Project Headers
#include <Utility/Common.hh>
#include <Utility/VulkanUtils.hh>
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
        explicit VulkanPipeline(const PipelineConfigInfo &config);
        auto operator==(const VulkanPipeline& other) const -> bool { return m_GraphicsPipeline == other.m_GraphicsPipeline; }

        auto Bind(VkCommandBuffer commandBuffer) const -> void;

        MKT_NODISCARD auto Get() const -> const VkPipeline& { return m_GraphicsPipeline; }
        MKT_NODISCARD static auto GetDefaultPipelineConfigInfo() -> PipelineConfigInfo &;

        auto OnRelease() const -> void;
        ~VulkanPipeline() = default;

    public:
        VulkanPipeline(const VulkanPipeline&)   = delete;
        auto operator=(const VulkanPipeline&)   = delete;
        VulkanPipeline(VulkanPipeline&&)        = delete;
        auto operator=(VulkanPipeline&&)        = delete;

    private:
        auto CreateGraphicsPipeline(const PipelineConfigInfo& config) -> void;

    private:
        VkPipeline m_GraphicsPipeline{};
        PipelineConfigInfo  m_ConfigInfo{};

        VkShaderModule m_VertShaderModule{};
        VkShaderModule  m_FragShaderModule{};
    };
}

#endif // MIKOTO_VULKAN_PIPELINE_HH
