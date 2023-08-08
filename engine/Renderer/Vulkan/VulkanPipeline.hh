//
// Created by kate on 6/2/23.
//

#ifndef KATE_ENGINE_VULKAN_PIPELINE_HH
#define KATE_ENGINE_VULKAN_PIPELINE_HH

#include <filesystem>
#include <vector>
#include <memory>

#include <volk.h>
#include <GLFW/glfw3.h>

#include <Utility/Common.hh>
#include <Renderer/Vulkan/VulkanPresetPipelineConfigurations.hh>

namespace kaTe {

    struct PipelineConfigInfo {
        VkPipelineViewportStateCreateInfo       ViewportInfo{};
        VkPipelineInputAssemblyStateCreateInfo  inputAssemblyInfo{};
        VkPipelineRasterizationStateCreateInfo  rasterizationInfo{};
        VkPipelineMultisampleStateCreateInfo    multisampleInfo{};
        VkPipelineColorBlendAttachmentState     colorBlendAttachment{};
        VkPipelineColorBlendStateCreateInfo     colorBlendInfo{};
        VkPipelineDepthStencilStateCreateInfo   depthStencilInfo{};
        VkPipelineLayout                        pipelineLayout{};
        std::vector<VkDynamicState>             DynamicStateEnables{};
        VkPipelineDynamicStateCreateInfo        DynamicStateInfo{};
        VkRenderPass                            renderPass{};
        UInt32_T                                subpass{};

        explicit PipelineConfigInfo() = default;

        PipelineConfigInfo(PipelineConfigInfo&& other) = default;
        auto operator=(PipelineConfigInfo&& other) -> PipelineConfigInfo& = default;

        PipelineConfigInfo(const PipelineConfigInfo&) = delete;
        auto operator=(const PipelineConfigInfo&) -> PipelineConfigInfo& = delete;
    };

    class VulkanPipeline {
    public:
        using Path_T = std::filesystem::path;
        using Cont_T = std::vector<char>;

        KT_NODISCARD static auto GetDefaultPipelineConfigInfo() -> PipelineConfigInfo;

        VulkanPipeline(const Path_T &vPath, const Path_T &fPath, const PipelineConfigInfo &config);
        auto Bind(VkCommandBuffer commandBuffer) const -> void;

        auto OnRelease() const -> void;
        ~VulkanPipeline() = default;
    public:
        // Forbidden operations
        VulkanPipeline(const VulkanPipeline&) = delete;
        auto operator=(const VulkanPipeline&) -> VulkanPipeline& = delete;

        VulkanPipeline(VulkanPipeline &&) = delete;
        VulkanPipeline &operator=(VulkanPipeline&&) = delete;
    private:
        KT_NODISCARD static auto GetFileData(const Path_T& path) -> Cont_T;
        static auto CreateShaderModule(const Cont_T& srcCode, VkShaderModule* shaderModule) -> void;
        auto CreateGraphicsPipeline(const Path_T &vPath, const Path_T &fPath, const PipelineConfigInfo& config) -> void;
    private:
        VkPipeline                      m_GraphicsPipeline{};
        VkShaderModule                  m_VertShaderModule{};
        VkShaderModule                  m_FragShaderModule{};
    };

}

#endif //KATE_ENGINE_VULKAN_PIPELINE_HH
