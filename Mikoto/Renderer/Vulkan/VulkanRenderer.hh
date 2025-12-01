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
#include <Renderer/Core/RenderPassBase.hh>
#include <Renderer/Core/RendererBackend.hh>
#include <Renderer/Vulkan/VulkanDescriptorManager.hh>
#include <Renderer/Vulkan/VulkanDevice.hh>

namespace Mikoto {

    class VulkanRenderer final : public RendererBackend, public Singleton<VulkanRenderer> {
    public:
        // A texture can be sampled from in different ways
        // when we add a new sampler to the bindless descriptor
        // we the actual texture and the sampler, textures generally
        // have a default sampler, but we can optionally specify a different sampler if we want
        // The binding index is automatically increased by the renderer
        struct TextureBinding {
            SamplerHandle Sampler{};
            TextureHandle Texture{};
        };

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

        auto SetClearColor( float r, float g, float b, float a ) -> void override;

        auto GetFinalComposition() const -> TextureHandle override;

        auto GetMaterialPreview() const -> TextureHandle override;
        auto SetMaterialPreviewMat( MaterialHandle material ) -> void override;
        auto SetMaterialPreviewViewport( float width, float height ) -> void override;

        auto RegisterTextureForRender( TextureHandle texture ) -> void;

        MKT_NODISCARD static auto GetMaxBindlessTextureCount() -> UInt32;

        MKT_NODISCARD  auto GetFontPassComposition() -> TextureHandle override;

        ~VulkanRenderer() override = default;

    private:
        // [Internal usage]
        auto CreateBindlessDescriptor() -> void;
        auto UpdateBindlessTextureDescriptor( Int32 index, VulkanTexture* texture ) -> void;

        auto InitCoreRenderPasses() -> void;

        auto RunComputeWorkflow() -> void;

    private:

        VkDescriptorSet m_TexturesSet{ VK_NULL_HANDLE };

#if defined( MKT_USE_VULKAN_BINDLESS )
        bool m_UpdateTextureDescriptor{ false };
        std::vector<TextureHandle> m_BindlessTextures{};
#endif

        Registry<IPass> m_Passes{};

        VkViewport m_Viewport{};
        VkRect2D m_Scissor{};
        CommandListHandle m_GraphicsCommandList{};
    };
}// namespace Mikoto

#endif// MIKOTO_VULKAN_RENDERER_HH