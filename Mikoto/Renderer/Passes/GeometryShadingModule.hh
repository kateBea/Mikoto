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

#ifndef MIKOTO_IBL_PASSES_HH
#define MIKOTO_IBL_PASSES_HH

#include <glm/glm.hpp>

#include <EASTL/fixed_vector.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Scene/Scene.hh>
#include <Scene/Camera.hh>

#include <Renderer/Core/Rhi.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/CommandContext.hh>

#include <Renderer/Passes/GeometryCullModule.hh>

namespace mikoto::renderer {

    // TODO: study how to account for bump mapping etc
    // I need it to properly render the ancient rune stones mesh from sketch-fab
    // https://youtu.be/cM7RjEtZGHw

    enum class SceneBackgroundType {
        eSkybox,
        ePrefilterMap, // Blurred map (In shader mip 3 is used)
        eClearColor,
    };

    struct WireframeData {
        FGTextureHandle mColorImage{};
        FGPipelineHandle mPipeline{};
    };

    struct GeomShadingModuleInfo {
        FGTextureHandle mBrdfColorTarget{};

        FGTextureHandle mShadingColorImage{};

        FGTextureHandle mSkyboxCubeRT{};
        FGTextureHandle mPrefilterCubeRT{};
        FGTextureHandle mIrradianceCubeRT{};

        FGPipelineHandle mShadingPipeline{};
        FGPipelineHandle mSkyboxRenderPipeline{};
        FGPipelineHandle mSkyboxProjectionPipeline{};

        FGPipelineHandle mBrdfPipeline{};
        FGPipelineHandle mIrradiancePipeline{};
        FGPipelineHandle mPrefilterPipeline{};

        FGSamplerHandle mIBLCubeSampler{};
        FGSamplerHandle mBasicSampler{};

        f32 mExposure{};
        f32 mGamma{};

        SceneBackgroundType mBackground{ SceneBackgroundType::eClearColor };
    };

    class GeometryShadingModule {
    public:
        explicit GeometryShadingModule( RenderResolution resolution);

        auto SetClearColor( const Color& color ) -> void;
        auto SetResolution( RenderResolution resolution) -> void;
        auto RegisterPasses( FrameGraph& graph ) -> void;

        auto SetScene( const scene::Scene* scene ) -> void;
        auto SetCamera( const scene::Camera *camera ) -> void;
        auto SetGeometryManager( GeometryCullModule& geom) -> void;

        // SSAO
        auto SetEnableSsao( bool enable ) -> void;
        auto SetSsaoIntensity( float value ) -> void;

        // HDR
        auto SetGamma( float value ) -> void;
        auto SetExposure( float value ) -> void;
        auto SetEquirectangular(FGTextureHandle texture) -> void;
        auto SetRenderBackground(SceneBackgroundType bg) -> void;

    private:
        auto RegisterShading( FrameGraph& graph ) -> void;

        auto RegisterWireframe( FrameGraph& graph ) -> void;

        auto RegisterSkyboxRender( FrameGraph& graph ) -> void;
        auto RegisterSkyboxProjection( FrameGraph& graph ) -> void;

        auto RegisterBrdfLut( FrameGraph& graph ) -> void;
        auto RegisterPrefilter( FrameGraph& graph ) -> void;
        auto RegisterIrradiance( FrameGraph& graph ) -> void;


    private:
        inline static const eastl::fixed_vector<float4x4, kMaxCubeFaces> kMatrices{
            glm::lookAt( float3( 0 ), float3( 1, 0, 0 ), float3( 0, -1, 0 ) ), // +X
            glm::lookAt( float3( 0 ), float3( -1, 0, 0 ), float3( 0, -1, 0 ) ),// -X
            glm::lookAt( float3( 0 ), float3( 0, -1, 0 ), float3( 0, 0, -1 ) ),  // +Y // Is this correct? Vulkan needs center pointing downwards
            glm::lookAt( float3( 0 ), float3( 0, 1, 0 ), float3( 0, 0, 1 ) ),// -Y
            glm::lookAt( float3( 0 ), float3( 0, 0, 1 ), float3( 0, -1, 0 ) ), // +Z
            glm::lookAt( float3( 0 ), float3( 0, 0, -1 ), float3( 0, -1, 0 ) ),// -Z
        };

    private:
        const scene::Scene* mScene{};
        const scene::Camera* mCamera{};

        GeometryCullModule* mGeometryManagement{};

        Color mClearColor{ kColorBlue };

        static constexpr u32 kIrradianceDimensions{ 64 };
        static constexpr u32 kIrradianceMipLevels{ 1 };

        static constexpr u32 kPrefilterDimensions{ 1024 };
        u32 mPrefilterMipLevels{ as<u32>( math::Floor( math::Log2( kPrefilterDimensions ) ) ) + 1 };

        RenderResolution mResolution{ RenderResolution::e1080P };
        SceneBackgroundType mBackgroundType{ SceneBackgroundType::eClearColor };

        // SSAO
        bool mEnableSsao{ true };
        f32 mSsaoIntensity{ 1.0f };

        // IBL
        f32 mGamma{ 1.0f };
        f32 mExposure{ 1.0f };
        FGTextureHandle mEquirectangularTexture{};
    };
}

#endif//MIKOTO_IBL_PASSES_HH
