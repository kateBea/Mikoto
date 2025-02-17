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
#include <Renderer/Vulkan/VulkanObject.hh>
#include <Renderer/Vulkan/VulkanShader.hh>

namespace Mikoto {
    struct VulkanPipelineCreateInfo {
        UInt32_T Subpass{};
        VkRenderPass RenderPass{};
        VkPipelineLayout PipelineLayout{};

        VkPipelineViewportStateCreateInfo ViewportInfo{};
        VkPipelineInputAssemblyStateCreateInfo InputAssemblyInfo{};
        VkPipelineRasterizationStateCreateInfo RasterizationInfo{};
        VkPipelineMultisampleStateCreateInfo MultisampleInfo{};
        VkPipelineColorBlendAttachmentState ColorBlendAttachment{};
        VkPipelineColorBlendStateCreateInfo ColorBlendInfo{};
        VkPipelineDepthStencilStateCreateInfo DepthStencilInfo{};
        VkPipelineDynamicStateCreateInfo DynamicStateInfo{};

        std::span<const VkDynamicState> DynamicStateEnables{};
        std::span<const VkPipelineShaderStageCreateInfo> ShaderStages{};
    };

    class VulkanPipeline final : public VulkanObject {
    public:
        explicit VulkanPipeline(const VulkanPipelineCreateInfo& config);

        auto Init() -> void;

        auto Release() -> void override;
        auto Bind(VkCommandBuffer commandBuffer) const -> void;

        MKT_NODISCARD auto Get() const -> const VkPipeline& { return m_GraphicsPipeline; }
        MKT_NODISCARD auto GetLayout() const -> const VkPipelineLayout& { return m_PipelineLayout; }

        ~VulkanPipeline() override;

    public:
        DELETE_COPY_FOR(VulkanPipeline);

    private:
        VkPipeline m_GraphicsPipeline{};
        VkPipelineLayout m_PipelineLayout{};
        VulkanPipelineCreateInfo m_ConfigInfo{};
    };
}

#endif // MIKOTO_VULKAN_PIPELINE_HH
