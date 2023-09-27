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
        VkPipelineLayout PipelineLayout{};
        VkPipelineDynamicStateCreateInfo DynamicStateInfo{};
        VkRenderPass RenderPass{};
        UInt32_T Subpass{};

        std::vector<VkDynamicState> DynamicStateEnables{};
    };

    class VulkanPipeline {
    public:
        VulkanPipeline(const Path_T &vPath, const Path_T &fPath, const PipelineConfigInfo &config);
        auto Bind(VkCommandBuffer commandBuffer) const -> void;
        MKT_NODISCARD auto Get() const -> const VkPipeline& { return m_GraphicsPipeline; }
        MKT_NODISCARD static auto GetDefaultPipelineConfigInfo() -> PipelineConfigInfo &;

        auto operator==(const VulkanPipeline& other) const -> bool { return m_GraphicsPipeline == other.m_GraphicsPipeline; }

        auto OnRelease() const -> void;
        ~VulkanPipeline() = default;

    public:
        VulkanPipeline(const VulkanPipeline&)   = delete;
        auto operator=(const VulkanPipeline&)   = delete;
        VulkanPipeline(VulkanPipeline &&)       = delete;
        auto operator=(VulkanPipeline&&)        = delete;

    private:
        static auto CreateShaderModule(const std::string& srcCode, VkShaderModule* shaderModule) -> void;
        auto CreateGraphicsPipeline(const Path_T& vPath, const Path_T& fPath, const PipelineConfigInfo& config) -> void;

    private:
        VkPipeline m_GraphicsPipeline{};
        VkShaderModule m_VertShaderModule{};
        VkShaderModule  m_FragShaderModule{};
        PipelineConfigInfo  m_ConfigInfo{};
        std::vector<VulkanShader> m_ShaderStages{};
    };
}

#endif // MIKOTO_VULKAN_PIPELINE_HH
