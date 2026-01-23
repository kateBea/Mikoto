//    Copyright 2025 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#ifndef MIKOTO_SCENE_RENDERER_HH
#define MIKOTO_SCENE_RENDERER_HH

#include <Common/Common.hh>
#include <Renderer/Core/GpuDevice.hh>
#include <Library/Data/Registry.hh>
#include <Scene/Camera.hh>
#include <Scene/Scene.hh>
#include <Scene/SceneCamera.hh>

#include <Renderer/Core/Renderer.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/RenderUtility.hh>
#include <Renderer/Core/GraphicsContext.hh>

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

        auto SetClusterDebugVisualizer(bool enable) -> void;

        auto SetSkyBox(TextureHandle cubeMap) -> void;
        auto SetClearColor(const Vec4F& color) -> void;
        auto EnableSkybox(bool enable) -> void;

        MKT_NODISCARD auto GetRenderResolution() const -> RenderResolution;
        MKT_NODISCARD auto IsRenderResolution(RenderResolution resolution) const -> bool;
        auto SetRenderResolution( RenderResolution resolution ) -> void;

        auto SetEnvironmentGamma(float value) -> void;
        auto SetEnvironmentExposure(float value) -> void;

        MKT_NODISCARD auto GetTexture(std::string_view name) const -> TextureHandle;
        MKT_NODISCARD auto GetBuffer(std::string_view name) const -> BufferHandle;

    private:
        // [Internal usage]
        auto InitGraphicsContex() -> void;
        auto InitCoreFramePasses() -> void;

        auto PassPreSetup() -> void;

        auto CreateDebugPasses() -> void;
        auto CreateMainPasses() -> void;

        auto SetSceneParameters( Scene* scene ) -> void;

    private:

        GpuDevice* m_Device{ nullptr };
        SceneCamera* m_Camera{ nullptr };

        Unique<FrameGraph> m_FrameGraph{};
        Unique<GraphicsContext> m_GraphicsContext{};

        bool m_WantResize{ false };
        RenderResolution m_RenderResolution{ RenderResolution::FHD_1080 };
        std::pair<float, float> m_RenderTargetDimensions{ InferDimensions( m_RenderResolution ) };

        UInt32 m_ViewportWidth{ 0u };
        UInt32 m_ViewportHeight{ 0u };

        bool m_UseSkybox{ false };
        Vec4F m_ClearColor{ 0.1f, 0.2f, 0.5f, 1.0f };
        TextureHandle m_SkyBoxTexture{};
    };
}// namespace Mikoto

#endif// MIKOTO_SCENE_RENDERER_HH
