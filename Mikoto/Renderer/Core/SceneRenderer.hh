//
// Created by zanet on 4/5/2025.
//

#ifndef SCENERENDERER_HH
#define SCENERENDERER_HH

#include <Common/Common.hh>
#include <Renderer/Core/GpuDevice.hh>
#include <Renderer/Core/RenderPassBase.hh>
#include <Library/Data/Registry.hh>
#include <Scene/Camera.hh>
#include <Scene/Scene.hh>
#include <Scene/SceneCamera.hh>

#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/RenderUtility.hh>
#include <Renderer/Core/FramePass.hh>
#include <Renderer/Core/GraphicsContext.hh>

namespace Mikoto {

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

    class MaterialViewer {
    public:
        explicit MaterialViewer( RendererBackend* backend );

        auto SetMaterial( MaterialHandle material ) -> void;

        auto SetViewPort( float width, float height ) -> void;

    private:
        RendererBackend* m_RendererBackend{};
        float m_ViewportWidth{ 0u };
        float m_ViewportHeight{ 0u };

        MaterialHandle m_Material{};
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
        auto Render( double timeStep ) -> void;

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

        template<typename T>
        MKT_NODISCARD auto GetPass() -> T* {
            return m_PassRegistry.Get<T>();
        }

        auto GetGraph() -> FrameGraph&;

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

        // Public api to modify core passes
        auto SetClusterDebugVisualizer(bool enable) -> void;

        auto SetSkyBox(TextureHandle cubeMap) -> void;
        auto SetClearColor(const Vec4F& color) -> void;
        auto EnableSkybox(bool enable) -> void;

        MKT_NODISCARD auto GetRenderResolution() const -> RenderResolution;
        MKT_NODISCARD auto IsRenderResolution(RenderResolution resolution) const -> bool;
        auto SetRenderResolution( RenderResolution resolution ) -> void;

    private:
        // [Internal usage]
        auto InitGraphicsContex() -> void;
        auto InitCoreFramePasses() -> void;

        auto PassPreSetup() -> void;

    private:

        Unique<MaterialViewer> m_MaterialViewer{};

        GpuDevice* m_Device{ nullptr };

        Scene* m_Scene{ nullptr };
        SceneCamera* m_Camera{ nullptr };

        Registry<FramePass> m_PassRegistry{};

        Unique<FrameGraph> m_FrameGraph{};
        Unique<GraphicsContext> m_GraphicsContext{};

        bool m_WantResize{ false };
        RenderResolution m_RenderResolution{ RenderResolution::RES_FHD_1080 };
        std::pair<float, float> m_RenderTargetDimensions{ InferDimensions( m_RenderResolution ) };

        UInt32 m_ViewportWidth{ 0u };
        UInt32 m_ViewportHeight{ 0u };

        bool m_UseSkybox{ false };
        Vec4F m_ClearColor{ 0.1f, 0.2f, 0.5f, 1.0f };
        TextureHandle m_SkyBoxTexture{};
    };
}// namespace Mikoto


#endif//SCENERENDERER_HH
