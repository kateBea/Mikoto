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
#include <Renderer/Core/Pipeline.hh>
#include <Common/Common.hh>

namespace Mikoto {

    struct VulkanGraphicsPipelineDescription {

#if !defined(MKT_USE_VULKAN_DYNAMIC_RENDERING)
        // Not needed if we do dynamic rendering
        UInt32 Subpass{};
        VkRenderPass RenderPass{};
#endif

        BufferLayout VertexBufferLayout{ DEFAULT_VERTEX_BUFFER_LAYOUT };

        TextureHandle Depth{};
        std::vector<TextureHandle> ColorAttachments{};
        std::vector<ShaderModuleHandle> ShaderModules{};
    };

    class VulkanGraphicsPipeline final : public GraphicsPipeline {
    public:


        explicit VulkanGraphicsPipeline(const VulkanGraphicsPipelineDescription& info);

        auto Bind(VkCommandBuffer commandBuffer) const -> void;

        MKT_NODISCARD auto GetImplHandle() -> VulkanGraphicsPipeline* { return this; }

        MKT_NODISCARD auto Get() const -> const VkPipeline& { return m_Pipeline; }

        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) -> Object override;

        MKT_NODISCARD auto GetDescriptorSetLayout( UInt32 index ) const -> const VkDescriptorSetLayout&;

        ~VulkanGraphicsPipeline() override;

    public:
        DISABLE_COPY_AND_MOVE_FOR(VulkanGraphicsPipeline);

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

    private:
        VkPipeline m_Pipeline{};

        // Needed when using dynamic rendering
        // format is not dynamic
        VkFormat m_DepthAttachmentFormat{};
        std::vector<VkFormat> m_ColorAttachmentsFormats{};

        VulkanHelpers::Reflection::ReflectedData m_ReflectionData{};
    };

    class VulkanComputePipeline final : public ComputePipeline {
    public:
        explicit VulkanComputePipeline( const ComputePipelineDescription& info );

        auto Bind(VkCommandBuffer commandBuffer) const -> void;

        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) -> Object override;

        MKT_NODISCARD auto GetDescriptorSetLayout( UInt32 index ) const -> const VkDescriptorSetLayout&;

        ~VulkanComputePipeline() override;

    public:
        DISABLE_COPY_AND_MOVE_FOR(VulkanComputePipeline);

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

    private:

        VkPipeline m_Pipeline{};
        VulkanHelpers::Reflection::ReflectedData m_ReflectionData{};
    };
}

#endif // MIKOTO_VULKAN_PIPELINE_HH
