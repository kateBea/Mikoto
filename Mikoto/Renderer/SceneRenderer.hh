//
// Created by zanet on 4/5/2025.
//

#ifndef SCENERENDERER_HH
#define SCENERENDERER_HH

#include <Common/Common.hh>
#include <Common/ScopedService.hh>
#include <Library/Data/Registry.hh>
#include <Library/Utility/Types.hh>
#include <Renderer/GpuDevice.hh>
#include <Renderer/RenderPass.hh>
#include <Renderer/RendererBackend.hh>
#include <Scene/Camera.hh>
#include <Scene/Scene.hh>
#include <nlohmann/json_fwd.hpp>

namespace Mikoto {


    /**
     * @brief Structure for creating a `SceneRenderer` instance.
     *
     * This structure holds the configuration parameters required to initialize a `SceneRenderer`.
     * The `SceneRendererCreateInfo` is used to specify essential information like the name and viewport dimensions.
     */
    struct SceneRendererCreateInfo {
        std::string_view Name{};  ///< The name of the renderer for debugging purposes.
        UInt32_T ViewportWidth{}; ///< The width of the viewport.
        UInt32_T ViewportHeight{};///< The height of the viewport.

        Path_T RenderGraphPath{}; ///< The path to the render graph configuration file.

        GpuDevice* Device{ nullptr }; ///< The GPU device associated with the renderer.

        /**
         * @brief Set the resolution for the renderer.
         *
         * Sets the viewport width and height for the scene renderer.
         *
         * @param width The width of the viewport.
         * @param height The height of the viewport.
         * @return This object to allow chaining.
         */
        auto WithResolution( UInt32_T width, UInt32_T height ) -> SceneRendererCreateInfo&;

        /**
        * @brief Set the name for the renderer.
        *
        * This function sets the name of the renderer for debugging purposes.
        *
        * @param name The name of the renderer.
        * @return This object to allow chaining.
        */
        auto WithName(std::string_view name) -> SceneRendererCreateInfo&;
    };

    /**
     * @brief Enum representing the current state of the scene renderer.
     *
     * This enum is used to track whether the renderer is idle or actively simulating the scene.
     */
    enum class SceneState {
        IDLE,
        SIMULATING
    };

    /**
     * @brief SceneRenderer is responsible for rendering a scene in the engine.
     *
     * The `SceneRenderer` handles the initialization, rendering, and management of scene rendering tasks.
     * It is designed to be scoped, meaning multiple instances can be created depending on the needs of the application.
     * The renderer provides functionality to manage scene state, viewport resizing, and rendering tasks.
     */
    class SceneRenderer final : public IScopedService<SceneRenderer> {
    public:
        /**
         * @brief Constructs a `SceneRenderer` instance using the provided configuration.
         *
         * @param createInfo A structure containing the configuration parameters for the renderer.
         */
        explicit SceneRenderer( const SceneRendererCreateInfo& createInfo );

        /**
         * @brief Initializes the scene renderer.
         *
         * This function sets up the renderer, initializes the necessary backend, and prepares it to render the scene.
         */
        auto Init() -> void override;

        /**
         * @brief Shuts down the scene renderer.
         *
         * Cleans up resources used by the renderer, releasing any allocated memory or resources.
         */
        auto Shutdown() -> void override;

        /**
         * @brief Sets the state of the scene renderer.
         *
         * This function changes the state of the renderer, such as transitioning from idle to simulating.
         *
         * @param state The new state to set for the renderer.
         */
        auto SetState( SceneState state ) -> void;

        /**
         * @brief Sets the scene that will be rendered by the renderer.
         *
         * This function assigns the scene to be rendered. The scene may contain entities, lights, cameras, etc.
         *
         * @param scene A reference to the scene that will be rendered.
         */
        auto SetScene( Scene* scene ) -> void;

        /**
         * @brief Renders the current scene.
         *
         * This function performs the rendering of the active scene using the renderer backend and frame graph.
         */
        auto Render( double timeStep ) const -> void;

        /**
         * @brief Handles viewport resizing.
         *
         * This function is called when the window is resized and updates the internal viewport size.
         *
         * @param width The new width of the viewport.
         * @param height The new height of the viewport.
         */
        auto OnResize( UInt32_T width, UInt32_T height ) -> void;

        auto SetCamera( Camera* camera ) -> void;
        auto SetRenderBackend( RendererBackend* backend ) -> void;
        auto SetRenderResolution(RenderResolution resolution) -> void;

        /**
         * @brief Creates a new `SceneRenderer` instance.
         *
         * This method provides a convenient way to create a scoped instance of the `SceneRenderer`.
         * It uses the provided configuration to initialize the renderer.
         *
         * @param createInfo The configuration parameters for creating the renderer.
         * @return A `Scope_T` wrapping the newly created `SceneRenderer` instance.
         */
        MKT_NODISCARD static auto Create(const SceneRendererCreateInfo& createInfo) -> Scope_T<SceneRenderer>;

    protected:
        auto AddCoreRenderPasses() -> void;

    private:
        UInt32_T m_ViewportWidth{}; ///< The current width of the viewport.
        UInt32_T m_ViewportHeight{};///< The current height of the viewport.

        SceneState m_SceneState{ SceneState::IDLE };///< The current state of the scene renderer.

        Path_T m_FrameGraphPath{};

        GpuDevice* m_Device{ nullptr };

        RendererBackend* m_RendererBackend{ nullptr };///< The backend renderer responsible for rendering.

        RenderResolution m_RenderResolution{ RenderResolution::RENDER_RESOLUTION_FHD };///< The current resolution for rendering.

        // Core passes
        Ref<GBufferPass> m_GBufferPass{};
    };
}// namespace Mikoto


#endif//SCENERENDERER_HH
