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
#include <volk.h>
#include <ankerl/unordered_dense.h>

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

        auto CreateMaterial( /* params */ ) -> MaterialHandle override;
            
        auto SetClearColor( float r, float g, float b, float a ) -> void override;

        auto GetFinalComposition() const -> TextureHandle override;

        auto RegisterTextureForRender( TextureHandle texture ) -> void;

        MKT_NODISCARD static auto GetMaxBindlessTextureCount() -> UInt32;

        ~VulkanRenderer() override = default;

    private:
        // [Internal usage]
        auto InitGlobalShaderBuffers() -> void;
        auto CreateBindlessDescriptor() -> void;
        auto UpdateBindlessTextureDescriptor( Int32 index, VulkanTexture* texture ) const -> void;

        auto InitCoreRenderPasses() -> void;

        auto RunComputeWorkflow() -> void;

    private:
        // TODO: temporary matches pbr_instance shaders layout
#define MAX_LIGHTS 50

        struct FrameUBO {
            glm::mat4 View{};
            glm::mat4 Projection{};
            Vec4F CameraPosition{};
        };

        struct SpotLightShader {
            Vec4F Position{};
            Vec4F Direction{};
            Vec4F Ambient{};
            Vec4F Diffuse{};
            // x=cutOff, y=outerCutOff, z=intensity, w=radius
            Vec4F CutOffValues{};
        };

        struct PointLightShader {
            Vec4F Position{};
            Vec4F Ambient{};
            Vec4F Diffuse{};
            Vec4F AttenuationParams{};
        };

        struct DirectionalLightShader {
            Vec4F Position{};
            Vec4F Ambient{};
            Vec4F Diffuse{};
        };

        struct LightInfo {
            enum class DisplayModes {
                DISPLAY_NORMAL = 1,
                DISPLAY_COLOR = 2,
                DISPLAY_METAL = 3,
                DISPLAY_AO = 4,
                DISPLAY_ROUGH = 5,
            };

            std::array<SpotLightShader, MAX_LIGHTS> SpotLights{};
            std::array<PointLightShader, MAX_LIGHTS> PointLights{};
            std::array<DirectionalLightShader, MAX_LIGHTS> DirectionalLights{};

            Int32 DirectionalLightCount{};
            Int32 PointLightCount{};
            Int32 SpotLightCount{};
            Int32 DisplayMode{};
        };

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

        Unique<LightInfo> m_LightsInfo{};

        VkViewport m_Viewport{};
        VkRect2D m_Scissor{};
        CommandListHandle m_GraphicsCommandList{};
    };
}// namespace Mikoto

#endif// MIKOTO_VULKAN_RENDERER_HH