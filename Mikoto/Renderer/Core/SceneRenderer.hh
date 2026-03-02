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

#include <atomic>

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

#include "Renderer/Passes/ClusteredShading.hh"
#include "Renderer/Passes/DebugPasses.hh"
#include "Renderer/Passes/IBLPasses.hh"
#include "Renderer/Passes/PostEffectsPasses.hh"
#include <Renderer/Passes/MeshCulling.hh>
#include <Renderer/Passes/MaterialDebug.hh>
#include <Renderer/Passes/CameraPass.hh>

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
        auto SetClearColor(const Vec4F& color) -> void;

        // IBL
        auto EnableSkybox(bool enable) -> void;
        auto SetEnvironmentGamma(float value) -> void;
        auto SetMaxReflectionLOD( float value ) -> void;
        auto SetEnvironmentExposure(float value) -> void;

        auto IsUsingPrecomputedLDRCubeMap() -> bool;

        auto UpdateEquirectangularMapAsync(std::string_view path) -> void;
        auto SetUseConvolutedCube( bool enable )-> void;
        auto UseLDRCubeMap( bool enable ) -> void;

        MKT_NODISCARD auto IsUsingConvolutedCube() const -> bool;
        MKT_NODISCARD auto GetEquirectangularMap() -> TextureHandle;

        // SSAO
        auto SetEnableSSAO(bool enable) -> void;
        auto SetEnableSSAOBlurred(bool enable) -> void;
        auto SetSSAOIntensity(float value) -> void;

        // Wireframe
        auto SetWireframeEnable(bool enable) -> void;
        auto SetWireframeLineLineWidth(float value) -> void;
        auto SetWireframeLineColor(const Vec4F& color) -> void;
        auto SetWireframeClearColor(const Vec4F& color) -> void;

        MKT_NODISCARD auto GetPassList() const -> const PassList&;

        // Resolution
        auto SetRenderResolution( RenderResolution resolution ) -> void;
        MKT_NODISCARD auto GetRenderResolution() const -> RenderResolution;
        MKT_NODISCARD auto IsRenderResolution(RenderResolution resolution) const -> bool;

        MKT_NODISCARD auto GetTexture(std::string_view name) const -> TextureHandle;
        MKT_NODISCARD auto GetBuffer(std::string_view name) const -> BufferHandle;

        MKT_NODISCARD static auto Create( const SceneRendererCreateInfo& spec) -> Unique<SceneRenderer>;

    private:
        // [Internal usage]
        auto InitGraphicsContex() -> void;
        auto InitCoreFramePasses() -> void;

        auto OnPreRender() -> void;
        auto OnPostRender() -> void;

        auto SetEquirectangularMap() -> void;

    private:
        // Scene graph
        GpuDevice* m_Device{ nullptr };
        Unique<FrameGraph> m_FrameGraph{};
        Unique<GraphicsContext> m_GraphicsContext{};

        // ViewPort
        bool m_WantResize{ false };
        SceneCamera* m_Camera{ nullptr };
        RenderResolution m_RenderResolution{ RenderResolution::QHD_1440P };
        std::pair<float, float> m_RenderTargetDimensions{ InferDimensions( m_RenderResolution ) };

        // Passes
        MeshCulling m_MeshCulling{};
        IBLPasses m_IBLPasses{ m_RenderResolution };
        PostEffectsPass m_PostEffectsPasses{ m_RenderResolution };
        DebugPasses m_DebugPasses{ m_RenderResolution };
        ClusteredShading m_ClusteredShadingPasses{ m_RenderResolution };
        MaterialDebug m_MaterialDebug{ m_RenderResolution };
        CameraPass m_CameraPass{ m_RenderResolution };

        // Async load HDR
        std::atomic_bool m_LoadedHDR{ false };
        TextureHandle m_Equirectangular{};
        TextureHandle m_CubeMap{};
    };
}// namespace Mikoto

#endif// MIKOTO_SCENE_RENDERER_HH
