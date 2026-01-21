//
// Created by zanet on 4/5/2025.
//

#ifndef SCENERENDERER_HH
#define SCENERENDERER_HH

#include <Common/Common.hh>
#include <Renderer/Core/GpuDevice.hh>
#include <Library/Data/Registry.hh>
#include <Scene/Camera.hh>
#include <Scene/Scene.hh>
#include <Scene/SceneCamera.hh>

#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/RenderUtility.hh>
#include <Renderer/Core/FramePass.hh>
#include <Renderer/Core/GraphicsContext.hh>

#include "Renderer.hh"

namespace Mikoto {

    struct SceneRendererCreateInfo {
        std::string_view Name{};

        GpuDevice* Device{ nullptr };

        auto WithName(std::string_view name) -> SceneRendererCreateInfo&;
        auto WithDevice(GpuDevice* device) -> SceneRendererCreateInfo&;
    };

    class SceneRenderer final : public Renderer {
    public:
        explicit SceneRenderer( const SceneRendererCreateInfo& createInfo );

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto Render( Scene* scene ) -> void override;

        auto SetCamera( SceneCamera* camera ) -> void;
        auto SetViewport( UInt32 width, UInt32 height ) -> void;

        template<typename T>
        MKT_NODISCARD auto GetPass() -> T* {
            return m_PassRegistry.Get<T>();
        }

        auto GetGraph() -> FrameGraph&;

        // Public api to modify core passes
        auto SetClusterDebugVisualizer(bool enable) -> void;

        auto SetSkyBox(TextureHandle cubeMap) -> void;
        auto SetClearColor(const Vec4F& color) -> void;
        auto EnableSkybox(bool enable) -> void;

        MKT_NODISCARD auto GetRenderResolution() const -> RenderResolution;
        MKT_NODISCARD auto IsRenderResolution(RenderResolution resolution) const -> bool;
        auto SetRenderResolution( RenderResolution resolution ) -> void;

        auto SetEnvironmentGamma(float value) -> void;
        auto SetEnvironmentExposure(float value) -> void;

    private:
        // [Internal usage]
        auto InitGraphicsContex() -> void;
        auto InitCoreFramePasses() -> void;

        auto PassPreSetup() -> void;

        auto CreateDebugPasses(FrameGraphBuilder& builder) -> void;
        auto CreateMainPasses(FrameGraphBuilder& builder) -> void;

        auto SetSceneParameters( Scene* scene ) -> void;

    private:

        GpuDevice* m_Device{ nullptr };

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
