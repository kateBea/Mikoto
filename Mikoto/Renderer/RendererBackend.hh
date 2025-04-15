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
#include <Renderer/LightObject.hh>
#include <Scene/Camera.hh>
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

    class RendererBackend {
    public:
        virtual ~RendererBackend() = default;

        // Initializes and shuts down the renderer
        virtual auto Init() -> bool = 0;
        virtual auto Shutdown() -> void = 0;

        // Handles per-frame rendering
        virtual auto BeginFrame() -> void = 0;
        virtual auto EndFrame() -> void = 0;

        virtual auto RegisterRenderPass(RenderPassHandle pass) -> void = 0;
        virtual auto UnRegisterRenderPass(RenderPassHandle pass) -> void = 0;

        auto EnableWireframe(bool enable) -> void;

        // Queues a game object for rendering
        virtual auto RemoveFromDrawQueue( UInt64_T id ) -> bool = 0;
        virtual auto AddToDrawQueue( const Entity& entity ) -> bool = 0;

        virtual auto RemoveLight( UInt64_T id ) -> bool = 0;
        virtual auto AddLight( const Entity& entity ) -> bool = 0;

        auto SetOutlineRenderWidth( float width ) -> void;
        auto SetRenderResolution( RenderResolution resolution ) -> void;
        MKT_NODISCARD auto GetRenderResolution() const -> RenderResolution;

        // Outline rendering properties
        template<typename... Args>
        auto SetOutlineRenderColor( Args&&... args ) -> void {
            m_OutlineRenderColor = glm::vec4{ std::forward<Args>(args)... };
        }

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

        // Factory method to create a renderer instance
        static auto Create( const RendererDescription& createInfo ) -> Scope_T<RendererBackend>;

        /**
        * @brief Set the scene for rendering.
        *
        * Allows the renderer to associate a scene with the backend, enabling the renderer to fetch necessary resources.
        *
        * @param scene A reference to the scene to be rendered.
        */
        auto SetScene( Scene* scene ) -> void {
            if (scene != nullptr) {
                m_Scene = scene;
            }
        }

        /**
        * @brief Set the resolution for the renderer.
        *
        * Updates the internal resolution of the renderer and may trigger reconfiguration of rendering settings.
        *
        * @param width The new width of the render target.
        * @param height The new height of the render target.
        */
        virtual auto OnResize( UInt32_T width, UInt32_T height ) -> void = 0;
    protected:
        struct ViewportConstraints {
            float Width{ 0 };///< The width of the viewport.
            float Height{ 0 };///< The height of the viewport.
            float X{ 0 };///< The x-coordinate of the viewport.
            float Y{ 0 };///< The y-coordinate of the viewport.
        };

    protected:
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