//
// Created by zanet on 4/5/2025.
//

#ifndef SCENERENDERER_HH
#define SCENERENDERER_HH

#include <Common/Common.hh>
#include <Renderer/Core/GpuDevice.hh>
#include <Renderer/Core/RenderPassBase.hh>
#include <Renderer/Core/RendererBackend.hh>
#include <Library/Data/Registry.hh>
#include <Scene/Camera.hh>
#include <Scene/Scene.hh>
#include <Scene/SceneCamera.hh>

#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/FramePass.hh>
#include <Renderer/Core/GraphicsContext.hh>

namespace Mikoto {

    enum RenderResolution {
        RESOLUTION_HD,
        RESOLUTION_FHD,
        RESOLUTION_QHD,
        RESOLUTION_UHD
    };


    /**
     * @brief Structure for creating a `SceneRenderer` instance.
     *
     * This structure holds the configuration parameters required to initialize a `SceneRenderer`.
     * The `SceneRendererCreateInfo` is used to specify essential information like the name and viewport dimensions.
     */
    struct SceneRendererCreateInfo {
        std::string_view Name{};

        GpuDevice* Device{ nullptr };

        /**
        * @brief Set the name for the renderer.
        *
        * This function sets the name of the renderer for debugging purposes.
        *
        * @param name The name of the renderer.
        * @return This object to allow chaining.
        */
        auto WithName(std::string_view name) -> SceneRendererCreateInfo&;

        auto WithDevice(GpuDevice* device) -> SceneRendererCreateInfo&;
    };

    /**
     * @brief SceneRenderer is responsible for rendering a scene in the engine.
     * The `SceneRenderer` handles the initialization, rendering, and management of scene rendering tasks.
     * It is designed to be scoped, meaning multiple instances can be created depending on the needs of the application.
     * The renderer provides functionality to manage scene state, viewport resizing, and rendering tasks.
     */
    class SceneRenderer final {
    public:
        /**
         * @brief Constructs a `SceneRenderer` instance using the provided configuration.
         * @param createInfo A structure containing the configuration parameters for the renderer.
         */
        explicit SceneRenderer( const SceneRendererCreateInfo& createInfo );

        /**
         * @brief Initializes the scene renderer.
         * This function sets up the renderer, initializes the necessary backend, and prepares it to render the scene.
         */
        auto Init() -> void;

        /**
         * @brief Shuts down the scene renderer.
         * Cleans up resources used by the renderer, releasing any allocated memory or resources.
         */
        auto Shutdown() -> void;


        /**
         * @brief Sets the scene that will be rendered by the renderer.
         * This function assigns the scene to be rendered. The scene may contain entities, lights, cameras, etc.
         * @param scene A reference to the scene that will be rendered.
         */
        auto SetScene( Scene* scene ) -> void;

        /**
         * @brief Renders the current scene.
         * This function performs the rendering of the active scene using the renderer backend and frame graph.
         */
        auto Render( double timeStep ) const -> void;

        /**
         * @brief Handles viewport resizing. This function is called when the window is resized and updates the internal viewport size.
         * @param width The new width of the viewport.
         * @param height The new height of the viewport.
         */
        auto SetViewport( UInt32 width, UInt32 height ) -> void;

        /**
         * @brief Sets the camera to be used for rendering the scene.
         * This function assigns the camera that will be used to render the scene.
         * @param camera A pointer to the `SceneCamera` to be used for rendering.
         */
        auto SetCamera( SceneCamera* camera ) -> void;

        
        /**
         * @brief Gets the final composition texture produced by the renderer.
         * This function retrieves the final rendered texture that can be used for display or further processing.
         * @return A `TextureHandle` representing the final composition texture.
         */
        MKT_NODISCARD auto GetFinalComposition() const -> TextureHandle;

        /**
         * @brief Sets the render resolution for the renderer.
         * This function updates the internal render resolution setting.
         * @param resolution The desired render resolution.
         */
        auto SetRenderResolution( RenderResolution resolution ) -> void;

        /**
         * @brief Gets the current render resolution of the renderer.
         * This function retrieves the internal render resolution setting.
         * @return The current render resolution.
         */
        MKT_NODISCARD auto GetRenderResolution() const -> RenderResolution;

        /**
         * @brief Sets the clear color for the renderer.
         * This function updates the clear color used when clearing the render target.
         * @param r The red component of the clear color.
         * @param g The green component of the clear color.
         * @param b The blue component of the clear color.
         * @param a The alpha component of the clear color.
         */
        auto SetClearColor( float r, float g, float b, float a ) -> void;

        /**
         * @brief Creates a new `SceneRenderer` instance.
         *
         * This method provides a convenient way to create a scoped instance of the `SceneRenderer`.
         * It uses the provided configuration to initialize the renderer.
         *
         * @param createInfo The configuration parameters for creating the renderer.
         * @return A `Scope_T` wrapping the newly created `SceneRenderer` instance.
         */
        MKT_NODISCARD static auto Create(const SceneRendererCreateInfo& createInfo) -> Unique<SceneRenderer>;

    private:
        // [Internal usage]
        auto InitCoreFramePasses() -> void;

    private:

        RenderResolution m_RenderResolution{ RenderResolution::RESOLUTION_FHD };

        GpuDevice* m_Device{ nullptr };

        Scene* m_Scene{ nullptr };
        SceneCamera* m_Camera{ nullptr };

        RendererBackend* m_RendererBackend{ nullptr };

        FrameGraph m_FrameGraph{};
        GraphicsContext* m_RenderContex{ nullptr };

        UInt32 m_ViewportWidth{ 0u };
        UInt32 m_ViewportHeight{ 0u };
    };
}// namespace Mikoto


#endif//SCENERENDERER_HH
