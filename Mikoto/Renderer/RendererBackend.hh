/**
 * @file RendererBackend.hh
 * @date 6/9/23
 * @author kate
 * */

#ifndef MIKOTO_RENDERER_API_HH
#define MIKOTO_RENDERER_API_HH

// C++ Standard Library
#include <any>
#include <memory>
#include <utility>

// Third-Party Libraries
#include <glm/glm.hpp>
#include <ankerl/unordered_dense.h>

// Project Headers
#include <Common/Common.hh>
#include <Material/TextureCube.hh>
#include <Renderer/GraphicsPipeline.hh>
#include <Scene/Camera.hh>
#include <Scene/Component.hh>
#include <Scene/Entity.hh>
#include <Scene/Scene.hh>

#include "RenderPass.hh"
#include "Renderer/RenderUtility.hh"
#include "Vulkan/VulkanDevice.hh"

#define MKT_SHADER_TRUE  1
#define MKT_SHADER_FALSE 0

namespace Mikoto {

    enum class RenderResolution {
        RENDER_RESOLUTION_HD,
        RENDER_RESOLUTION_FHD,
        RENDER_RESOLUTION_QHD,
        RENDER_RESOLUTION_UHD,
    };

    struct RendererDescription {
        std::string_view Name{};

        GpuDevice* GraphicsDevice{ nullptr };

        GraphicsAPI RendererAPI{ GraphicsAPI::VULKAN_API };
        RenderResolution Resolution{ RenderResolution::RENDER_RESOLUTION_FHD };
    };

    class CommandList final : ReferenceCounted {
    public:
        auto EndRecording() -> void;
        auto BeginRecording() -> void;

        auto BeginRenderPass(RenderPass* pass) -> void;
        auto UsePipeline(const GraphicsPipelineHandle & ref) -> void;
        auto SubmitMeshDraw(const MeshNode * node, Material * material, const glm::mat4& mat) -> void;
    };

    using CommandListHandle = Ref<CommandList>;

    class RendererBackend {
    public:
        virtual ~RendererBackend() = default;

        // Initializes and shuts down the renderer
        virtual auto Init() -> bool = 0;
        virtual auto Shutdown() -> void = 0;

        // Handles per-frame rendering
        virtual auto BeginFrame() -> void = 0;
        virtual auto EndFrame() -> void = 0;

        virtual auto RegisterRenderPass(RenderPass* pass) -> void = 0;
        virtual auto UnRegisterRenderPass(RenderPass* pass) -> void = 0;

        auto EnableWireframe(bool enable) -> void;

        // Queues a game object for rendering
        virtual auto RemoveFromDrawQueue( UInt64_T id ) -> bool = 0;
        virtual auto AddToDrawQueue( const Entity& entity ) -> bool = 0;

        virtual auto RemoveLight( UInt64_T id ) -> bool = 0;
        virtual auto AddLight( const Entity& entity ) -> bool = 0;

        MKT_NODISCARD auto GetRenderResolution() const -> RenderResolution;

        virtual auto EnableOutline(bool value) -> void;
        virtual auto SetOutlineRenderTargetEntity( UInt64_T id ) -> void = 0;

        // Camera management
        auto SetCamera( const Camera* camera ) -> void {
            if (camera != nullptr) {
                m_Camera = camera;
            }
        }

        // Viewport setup
        auto SetViewport( float x, float y, float width, float height ) -> void {
            m_ViewportConstraints.X = x;
            m_ViewportConstraints.Y = y;
            m_ViewportConstraints.Width = width;
            m_ViewportConstraints.Height = height;
        }

        // Clear color setup
        template<typename... Args>
        auto SetClearColor( Args&&... args ) -> void {
            m_ClearColor = glm::vec4{ std::forward<Args>(args)... };
        }

        auto SetupCubeTexture( TextureHandle handle ) -> void {
            m_CubeMap = handle;
        }

        auto SetScene( Scene* scene ) -> void {
            if (scene != nullptr) {
                m_Scene = scene;
            }
        }

        virtual auto OnResize( UInt32_T width, UInt32_T height ) -> void = 0;

        auto CreateCommandList() -> CommandListHandle;
        auto SubmitCommandList(CommandListHandle commandList) -> void;

    protected:
        struct ViewportConstraints {
            float Width{ 0 };///< The width of the viewport.
            float Height{ 0 };///< The height of the viewport.
            float X{ 0 };///< The x-coordinate of the viewport.
            float Y{ 0 };///< The y-coordinate of the viewport.
        };

    protected:
        // Factory method to create a renderer instance
        static auto Create( const RendererDescription& createInfo ) -> Scope_T<RendererBackend>;

        friend class RenderService;

        explicit RendererBackend( const RendererDescription& createInfo );

    protected:
        GpuDevice* m_GraphicsDevice{ nullptr };///< The graphics device used for rendering.

        std::string m_Name{};///< The name of the renderer.

        ViewportConstraints m_ViewportConstraints{};///< The constraints for the viewport.

        glm::vec4 m_ClearColor{};         ///< The clear color for the renderer.
        TextureHandle m_CubeMap{};///< The cubemap for environment lighting.

        bool m_OutlineEnable{};                                  ///< Whether outline rendering is enabled.
        float m_OutlineRenderWidth{ 1.5f };                      ///< The width of the outline rendering.
        glm::vec4 m_OutlineRenderColor{ 1.0f, 1.0f, 1.0f, 1.0f };///< The color of the outline render.

        RenderResolution m_RenderResolution{ RenderResolution::RENDER_RESOLUTION_FHD };///< The current render resolution.
        const Camera* m_Camera{ nullptr };                                             ///< The camera used for rendering.
        const Scene* m_Scene{ nullptr };                                             ///< The camera used for rendering.
    };
}// namespace Mikoto

#endif// MIKOTO_RENDERER_API_HH