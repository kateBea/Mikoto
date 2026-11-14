//    Copyright 2026 ケイト
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

#ifndef MIKOTO_RENDERER_FORWARD_RENDERER_DISPLAY_EFFECTS_MODULE_HH
#define MIKOTO_RENDERER_FORWARD_RENDERER_DISPLAY_EFFECTS_MODULE_HH

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/ForwardRenderer/Passes/GeometryCullModule.hh>

#include <Scene/Camera.hh>

namespace mikoto::renderer {

    enum class DepthOfFieldQuality {
        eOff,
        eLow,
        eMedium,
        eHigh,
        eCinematic,
    };

    struct ChromaticAberrationSettings {
        bool mEnabled{};
        core::f32 mIntensity{ 3.0f }; // Pixel step between adjacent color channels.
        core::f32 mDirection{ 45.0f }; // Degrees; diagonal matches the original shader.
    };

    struct DisplayEffectsModuleInfo {
        // Chromatic aberration
        FGTextureHandle mChromaAbRenderTarget{};
        FGPipelineHandle mChromaAbPipeline{};
        bool mChromaticAberrationEnabled{};

        // DoF
        FGTextureHandle mDepthOfFieldColor{};
        bool mVisualizeDepthOfField{};

        // Camera vignette: shape mask followed by the color composite.
        FGTextureHandle mVignetteShapeTarget{};
        FGTextureHandle mVignetteCompositeTarget{};
        FGPipelineHandle mVignetteShapePipeline{};
        FGPipelineHandle mVignetteCompositePipeline{};
        bool mVignetteEnabled{};
    };

    class DisplayEffectsModule final {
    public:
        explicit DisplayEffectsModule( rhi::RenderResolution resolution );

        auto SetScene( const scene::Scene *scene ) -> void;
        auto SetCamera( const scene::Camera *camera ) -> void;

        auto SetMultisampling( rhi::Multisampling multisampling ) -> void;

        auto SetGeometryManager( GeometryCullModule& geom ) -> void;

        auto RegisterLensDirtPass( FrameGraph& graph ) -> void;
        auto RegisterLensFlarePass( FrameGraph& graph ) -> void;
        auto RegisterMotionBlurrPass( FrameGraph& graph ) -> void;
        auto RegisterVignetteShapeRenderPass( FrameGraph& graph ) -> void;
        auto RegisterVignetteShapeCompositePass( FrameGraph& graph ) -> void;
        auto RegisterLensDistortionPass( FrameGraph& graph ) -> void;
        auto RegisterChromaticAberration( FrameGraph& graph ) -> void;
        auto RegisterDepthOfField( FrameGraph& graph ) -> void;

        auto SetDepthOfFieldQuality( DepthOfFieldQuality quality, bool visualize ) -> void;
        auto SetChromaticAberration( const ChromaticAberrationSettings& settings ) -> void;

    private:
        const scene::Scene* mScene{};
        ChromaticAberrationSettings mChromaticAberration{};
        DepthOfFieldQuality mDepthOfFieldQuality{ DepthOfFieldQuality::eHigh };
        bool mVisualizeDepthOfField{};
        const scene::Camera* mCamera{};

        GeometryCullModule* mGeometryCullModule{};

        rhi::RenderResolution mResolution { rhi::RenderResolution::e1080P };
    };
}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_FORWARD_RENDERER_DISPLAY_EFFECTS_MODULE_HH
