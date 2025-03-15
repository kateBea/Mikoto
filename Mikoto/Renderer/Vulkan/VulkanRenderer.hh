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

// Project Headers
#include <Common/Common.hh>
#include <Assets/Mesh.hh>
#include <Material/Core/Material.hh>
#include <Renderer/Core/RendererBackend.hh>
#include <Renderer/Vulkan/VulkanCommandPool.hh>
#include <Renderer/Vulkan/VulkanFrameBuffer.hh>
#include <Renderer/Vulkan/VulkanImage.hh>
#include <Renderer/Vulkan/VulkanPipeline.hh>
#include <Renderer/Vulkan/VulkanDevice.hh>
#include <Renderer/Vulkan/VulkanTextureCubeMap.hh>

namespace Mikoto {
    struct VulkanRendererCreateInfo {
        RendererCreateInfo Info{};
    };

    class VulkanRenderer final : public RendererBackend {
    public:
        explicit VulkanRenderer(const VulkanRendererCreateInfo& createInfo);

        auto Init() -> bool override;
        auto Shutdown() -> void override;

        auto BeginFrame() -> void override;
        auto EndFrame() -> void override;

        auto EnableWireframe( bool enable ) -> void override;

        auto RemoveFromDrawQueue( UInt64_T id ) -> bool override;
        auto AddToDrawQueue(  const EntityQueueInfo& queueInfo ) -> bool override;

        auto SetViewport( float x, float y, float width, float height ) -> void override;

        template<typename Func>
        auto RegisterResizeCallbacks(Func&& callback) -> void {
            m_ResizeCallbacks.emplace_back(std::forward<Func>(callback));
        }

        auto SetRenderMode( Size_T mode ) -> void override;

        MKT_NODISCARD auto HasUpdatedResolution() const -> bool override;

        auto SetOutlineRenderTargetEntity(UInt64_T id) -> void override;

        auto RemoveLight( UInt64_T id ) -> bool override;
        auto AddLight( UInt64_T id, const LightData& data, LightType activeType) -> bool override;

        auto SetRenderResolution(RenderResolution resolution) -> void override;

        auto SetupCubeMap(const TextureCubeMap* cubeMap) -> void override;

        MKT_NODISCARD auto GetFinalImage() const -> const VulkanImage& { return *m_OffscreenColorAttachment; }
        MKT_NODISCARD auto GetDepthImage() const -> const VulkanImage& { return *m_OffscreenDepthAttachment; }

        ~VulkanRenderer() override = default;

    private:
        struct MeshRenderInfo {
            const Mesh* Object{};

            glm::mat4 Transform{};

            // For now, we assume the
            // mesh has only one material
            Material* MaterialData{};

            bool IsRendered{ false };
        };

        struct LightRenderInfo {
            const LightData* Data{};
            LightType ActiveType{};
        };

    private:
        auto HandleRescaling() -> void;

        auto CreateCommandPools() -> void;
        auto CreateCommandBuffers() -> void;
        auto SetupObjectOutline(const MeshRenderInfo& meshRenderInfo) -> void;

        auto SetupPBRPass(const MeshRenderInfo& meshRenderInfo) -> void;
        auto SetupDefaultPass(const MeshRenderInfo& meshRenderInfo) -> void;

        auto RecordDrawCommands() -> void;
        auto RecordComputeCommands() -> void;
        auto RecordComputeCommandsDEBUG() -> void;
        auto PrepareOffscreenRender() -> void;

        auto CreateOffscreenRenderPass() -> void;
        auto CreateOffscreenAttachments() -> void;
        auto CreateOffscreenFramebuffers() -> void;

        auto InitializePBRWireFramePipeline() -> void;
        auto InitializeComputePipelines() -> void;

        auto InitializeDefaultPipeline() -> void;
        auto InitializePBRPipeline() -> void;

        auto InitializeOutlinePipeline() -> void;

        auto CreateRendererPipelines() -> void;

        auto UpdateViewport(float x, float y, float width, float height) -> void;

        auto UpdateScissor(Int32_T x, Int32_T y, VkExtent2D extent) -> void;

        auto SubmitCommands() const -> void;

        auto Flush() -> void;

    private:

        bool m_RequestRescale{ false };
        std::vector<std::function<void()>> m_ResizeCallbacks{};

        bool m_WireframeEnable{ false };

        VulkanDevice* m_Device{};

        UInt64_T m_TargetObjectOutlineID{};

        Size_T m_RenderMode{ DISPLAY_COLOR };

        const VulkanTextureCubeMap* m_CubeMap{};

        VkRenderPass m_OffscreenMainRenderPass{};
        Scope_T<VulkanImage> m_OffscreenColorAttachment{};
        Scope_T<VulkanImage> m_OffscreenDepthAttachment{};
        Scope_T<VulkanFrameBuffer> m_OffscreenFrameBuffer{};

        VkFormat m_ColorAttachmentFormat{};
        VkFormat m_DepthAttachmentFormat{};
        VkExtent2D m_OffscreenExtent{};
        VkViewport m_OffscreenViewport{};
        VkRect2D m_OffscreenScissor{};

        std::array<VkClearValue, 2> m_ClearValues{};

        VkCommandBuffer m_DrawCommandBuffer{};
        VkCommandBuffer m_ComputeCommandBuffer{};

        Scope_T<VulkanCommandPool> m_GraphicsCommandPool{};
        Scope_T<VulkanCommandPool> m_ComputeCommandPool{};

        std::unordered_map<UInt64_T, LightRenderInfo> m_Lights{};

        std::unordered_map<Size_T, VulkanPipeline> m_Pipelines{};
        std::unordered_map<UInt64_T, MeshRenderInfo> m_DrawQueue{};

        bool m_UseWireframe{};
    };
}

#endif // MIKOTO_VULKAN_RENDERER_HH