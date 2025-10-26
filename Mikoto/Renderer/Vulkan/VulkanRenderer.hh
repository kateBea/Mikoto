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
#include <Renderer/Vulkan/VulkanDescriptorManager.hh>

namespace Mikoto {

    class VulkanRenderer final : public RendererBackend, public Singleton<VulkanRenderer> {
    public:
        explicit VulkanRenderer( GpuDevice* device, std::string_view name );

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto EndRender() -> void override;
        auto BeginRender( CommandListHandle cmd ) -> void override;

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
        auto InitGlobalShaderBuffers() -> void;
        auto CreateBindlessDescriptor() -> void;
        auto UpdateBindlessTextureDescriptor( Int32 index, VulkanTexture* texture ) -> void;

        auto InitCoreRenderPasses() -> void;

        auto RunComputeWorkflow() -> void;


    private:
        // Per frame data
        BufferHandle m_FrameUBOBuffer{};
        BufferHandle m_LightsBuffer{};

        VkDescriptorSet m_FrameSet{ VK_NULL_HANDLE };
        VkDescriptorSet m_TexturesSet{ VK_NULL_HANDLE };

        DescriptorSetLayoutHandle m_TextureLayout{};
        DescriptorSetLayoutHandle m_FrameLayout{};


#if defined( MKT_USE_VULKAN_BINDLESS )
        bool m_UpdateTextureDescriptor{ false };
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