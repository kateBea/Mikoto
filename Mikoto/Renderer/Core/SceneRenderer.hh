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

#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/unique_ptr.h>

#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Core/Renderer.hh>
#include <Renderer/Passes/CameraModule.hh>
#include <Renderer/Passes/DebugModule.hh>
#include <Renderer/Passes/HelperModule.hh>
#include <Renderer/Passes/TonemapModule.hh>
#include <Renderer/Passes/MaterialModule.hh>
#include <Renderer/Passes/MousePickingModule.hh>
#include <Renderer/Passes/PathTracingModule.hh>
#include <Renderer/Passes/PostProcessModule.hh>
#include <Renderer/Passes/PrepassModule.hh>
#include <Renderer/Passes/IndirectLightingModule.hh>
#include <Renderer/Passes/PresentationModule.hh>
#include <Renderer/Passes/RayTracingModule.hh>
#include <Renderer/Passes/ShadowMappingModule.hh>
#include <Renderer/Passes/TextRenderModule.hh>
#include <Renderer/Passes/GeometryCullModule.hh>
#include <Renderer/Passes/WaterSimulationModule.hh>
#include <Renderer/Passes/DisplayEffectsModule.hh>
#include <Renderer/Passes/GeometryShadingModule.hh>
#include <Renderer/Passes/ParticleSimulationModule.hh>
#include <Renderer/Passes/AtmosphericScatteringModule.hh>

namespace mikoto::renderer {

    struct SceneRendererCreateInfo {
        IGpuDevice* mDevice{};
        eastl::string_view mName{};
        eastl::string_view mShaderBasePath{};

        // Scene renderer does a full quad render on these
        // Just edit the final pass to specify what gets rendered onto these
        rhi::TextureHandle mPresentTexture{};

        rhi::RenderResolution mResolution{ rhi::RenderResolution::e1080P };

        auto SetName( eastl::string_view name ) -> SceneRendererCreateInfo&;
        auto SetDevice( rhi::IGpuDevice* device ) -> SceneRendererCreateInfo&;
        auto SetPresentImage( rhi::TextureHandle texture ) -> SceneRendererCreateInfo&;
        auto SetRenderResolution( rhi::RenderResolution resolution ) -> SceneRendererCreateInfo&;
    };

    class SceneRenderer final : public IRenderer {
    public:
        explicit SceneRenderer( const SceneRendererCreateInfo& createInfo );

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto Render( const scene::Scene* scene ) -> void override;

        auto SetPresentType( PresentTarget type ) -> void;

        auto SetMainCamera( const scene::SceneCamera* camera ) -> void;
        auto SetClearColor( const rhi::Color& color ) -> void;

        auto SetTonemapType( ToneMappingType type ) -> void;

        auto SetGamma( core::f32 gamma ) -> void;
        auto SetExposure( core::f32 exposure ) -> void;
        auto SetAmbientScale( core::f32 ambient ) -> void;

        auto SetSkyboxMaterial(material::MaterialHandle material) -> void;

        auto SetRenderBackground( SceneBackgroundType bg ) -> void;

        auto SetEnableInfiniteGrid( bool value ) -> void;

        auto DisablePass( eastl::string_view passName ) -> void;
        auto EnablePass( eastl::string_view passName ) -> void;

        MKT_NODISCARD auto ReadPixel( core::u32 x, core::u32 y) const -> core::u32;
        MKT_NODISCARD auto ReadPixel( const ReadPixelViewportInfo& ínfo ) const -> core::u32;

        MKT_NODISCARD auto GetNodeControl() const -> const FGNodeControl&;

        MKT_NODISCARD auto GetTexture( FGTextureHandle handle ) const -> rhi::TextureHandle;
        MKT_NODISCARD auto GetBuffer( FGBufferHandle handle ) const -> rhi::BufferHandle;

        MKT_NODISCARD auto GetPassList() const -> const ankerl::unordered_dense::map<eastl::string, FGNode>&;
        MKT_NODISCARD auto GetPassStatistics() const -> const ankerl::unordered_dense::map<eastl::string, FGNodeStatistics>&;

        MKT_NODISCARD static auto Create( const SceneRendererCreateInfo& spec) -> eastl::unique_ptr<SceneRenderer>;

    private:
        rhi::IGpuDevice* mDevice{};
        scene::SceneCamera* mCamera{};

        rhi::TextureHandle mPresentTexture{};

        rhi::RenderResolution mTargetResolution{ rhi::RenderResolution::e1080P };

        // Passes
        CameraModule mCameraPass{ mTargetResolution };
        DebugModule mDebugPasses{ mTargetResolution };
        PrepassModule mRenderPrepass{ mTargetResolution };

        GeometryCullModule mGeometryManagement{};
        GeometryShadingModule mGeometryShading{ mTargetResolution };
        PresentationModule mPresentationModule{ mTargetResolution };

        IndirectLightingModule mIndirectLightingModule{ mTargetResolution };

        TonemapModule mTonemapModule{ mTargetResolution };
        DisplayEffectsModule mDisplayEffectsModule{ mTargetResolution };

        MousePickingModule mMousePickingModule{ mTargetResolution };

        MaterialModule mMaterialModule{ mTargetResolution };
        ShadowMappingModule mShadowMapping{ mTargetResolution };

        PostEffectsPass mPostEffectsPasses{ mTargetResolution };

        ParticleSimulationModule mParticleRendering{ mTargetResolution };

        AtmosphericScatteringModule mAtmosModule{};
        WaterSimulationModule mWaterSimulation{};

        TextRenderModule mTextRendering{ mTargetResolution };

        HelperModule mHelperModule{};

        // Raytracing
        PathTracingModule mPathTracing{};
        RayTracingModule mRayTracingPass{};

        eastl::unique_ptr<FrameGraph> mFrameGraph{};
        eastl::unique_ptr<material::ShaderLibrary> mShaderLibrary{};
    };
}// namespace Mikoto

#endif// MIKOTO_SCENE_RENDERER_HH
