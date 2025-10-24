/**
 * VulkanRenderer.hh
 * Created by kate on 7/3/23.
 * */

#ifndef MIKOTO_VULKAN_RENDERER_HH
#define MIKOTO_VULKAN_RENDERER_HH

// C++ Standard Library
#include <array>
#include <filesystem>
#include <unordered_map>
#include <vector>

// Third-Party Library
#include <ankerl/unordered_dense.h>
#include <volk.h>

#include <glm/glm.hpp>

// Project Headers
#include <Common/Common.hh>
#include <Library/Data/Registry.hh>
#include <Material/PBRMaterial.hh>
#include <Renderer/RenderPassBase.hh>
#include <Renderer/RendererBackend.hh>
#include <Renderer/Vulkan/VulkanDevice.hh>

namespace Mikoto {

    class VulkanRenderer final : public RendererBackend {
    public:
        explicit VulkanRenderer( GpuDevice* device, std::string_view name );

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto EndRender() -> void override;
        auto BeginRender( CommandListHandle cmd ) -> void override;

        auto SetPipeline( PipelineHandle pipeline ) -> void override;

        auto DrawScene( Scene* scene ) -> void override;

        auto OnResize( UInt32 width, UInt32 height ) -> void override;

        auto SetCamera( const Camera* camera ) -> void override;
        auto SetViewport( float x, float y, float width, float height ) -> void override;

        auto CreateMaterial( /* params */ ) -> MaterialHandle override;

        auto GetFinalComposition() const -> TextureHandle override;

        auto RegisterTextureForRender( TextureHandle texture ) -> void;

        MKT_NODISCARD static auto GetMaxBindlessTextureCount() -> UInt32;

        ~VulkanRenderer() override = default;

    private:
        // [Internal usage]
        auto CreateBindlessDescriptor() -> void;
        auto UpdateBindlessTextureDescriptor( Int32 index, VulkanTexture* texture ) -> void;

        auto InitCoreRenderPasses() -> void;


    private:
        // Per frame data
        BufferHandle m_FrameUBOBuffer{};
        PipelineHandle m_Pipeline{};
        VkDescriptorSet m_FrameDescriptorSet{};

#if defined( MKT_USE_VULKAN_BINDLESS )
        VkDescriptorPool m_BindlessPool{ VK_NULL_HANDLE };
        VkDescriptorSet m_BindlessDescriptorSet{ VK_NULL_HANDLE };
        VkDescriptorSetLayout m_BindlessDescriptorSetLayout{ VK_NULL_HANDLE };
        std::vector<TextureHandle> m_BindlessTextures{};
#endif

        ResourcePoolTyped<PBRMaterial> m_Materials{};

        Registry<IPass> m_Passes{};

        VkViewport m_Viewport{};
        VkRect2D m_Scissor{};
        CommandListHandle m_GraphicsCommandList{};
    };
}// namespace Mikoto

#endif// MIKOTO_VULKAN_RENDERER_HH