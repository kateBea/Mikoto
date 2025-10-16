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

    /**
     * @brief Class representing a Vulkan render pass.
     *
     * This class encapsulates the Vulkan render pass functionality, including
     * the creation and management of render passes, framebuffers, and attachments.
     */
    class VulkanRenderPass : public DeviceObject {
    public:


    private:

    };

    class VulkanGraphicsPipeline final : public GraphicsPipeline {
    public:
        // Pass what u need to create a vulkan graphics pipeine
        // no need to be the base description can be anything needed to create this vulkan pipoeline
        // the device which we have crteated at start of the engine knows how to consttuct api specific objects
        explicit VulkanGraphicsPipeline(/**/);

        auto Release() -> void override;
        auto Bind(VkCommandBuffer commandBuffer) const -> void;

        MKT_NODISCARD auto Get() const -> const VkPipeline& { return m_GraphicsPipeline; }
        MKT_NODISCARD auto GetLayout() const -> const VkPipelineLayout& { return m_PipelineLayout; }

        ~VulkanGraphicsPipeline() override;

    public:
        DISABLE_COPY_AND_MOVE_FOR(VulkanGraphicsPipeline);

    private:
        auto Allocate() -> void override;

    private:
        BufferLayout m_BufferLayout{};

        VkPipeline m_GraphicsPipeline{};
        VkPipelineLayout m_PipelineLayout{};
    };

    class VulkanComputePipeline final : public ComputePipeline {
    public:
        explicit VulkanComputePipeline(const ComputePipelineDescription& desc);

        auto Init() -> void;

        auto Release() -> void override;
        auto Bind(VkCommandBuffer commandBuffer) const -> void;

        MKT_NODISCARD auto Get() const -> const VkPipeline& { return m_GraphicsPipeline; }
        MKT_NODISCARD auto GetLayout() const -> const VkPipelineLayout& { return m_PipelineLayout; }

        ~VulkanComputePipeline() override;

    public:
        DISABLE_COPY_AND_MOVE_FOR(VulkanComputePipeline);

    private:
        auto Allocate() -> void override;

    private:
        VkPipeline m_GraphicsPipeline{};
        VkPipelineLayout m_PipelineLayout{};
    };
}

#endif // MIKOTO_VULKAN_PIPELINE_HH
