/**
 * VulkanRenderer.hh
 * Created by kate on 7/3/23.
 * */

#ifndef MIKOTO_VULKAN_RENDERER_HH
#define MIKOTO_VULKAN_RENDERER_HH

// C++ Standard Library
#include <array>
#include <vector>
#include <filesystem>
#include <unordered_map>

// Third-Party Library
#include <glm/glm.hpp>
#include <volk.h>
#include <ankerl/unordered_dense.h>

// Project Headers
#include <Common/Common.hh>
#include <Renderer/RendererBackend.hh>
#include <Renderer/Vulkan/VulkanDevice.hh>

namespace Mikoto {

    class VulkanRenderer final : public RendererBackend {
    public:
        explicit VulkanRenderer(GpuDevice* device, std::string_view name);

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto EndRender() -> void override;
        auto BeginRender( const RenderInfo& info, CommandListHandle cmd ) -> void override;

        auto SetPipeline( PipelineHandle pipeline ) -> void override;

        auto DrawScene( Scene* scene ) -> void override;

        auto OnResize( UInt32 width, UInt32 height ) -> void override;

        auto SetCamera( const Camera* camera ) -> void override;
        auto SetViewport( float x, float y, float width, float height ) -> void override;

        ~VulkanRenderer() override = default;

    private:
        // Per frame data
        BufferHandle m_FrameUBOBuffer{};
        PipelineHandle m_Pipeline{};
        VkDescriptorSet m_FrameDescriptorSet{};

        // Attachments
        DepthAttachmentInfo m_DepthAttachment{};
        std::vector<AttachmentInfo> m_ColorAttachments{};

        VkViewport m_Viewport{};
        VkRect2D m_Scissor{};
        CommandListHandle m_GraphicsCommandList{};
    };
}

#endif // MIKOTO_VULKAN_RENDERER_HH