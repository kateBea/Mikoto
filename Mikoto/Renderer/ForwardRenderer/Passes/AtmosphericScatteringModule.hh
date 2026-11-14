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

#ifndef MIKOTO_RENDERER_FORWARD_RENDERER_ATMOSPHERIC_SCATTERING_MODULE_HH
#define MIKOTO_RENDERER_FORWARD_RENDERER_ATMOSPHERIC_SCATTERING_MODULE_HH

#include <EASTL/optional.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Renderer/Core/FrameGraph.hh>

namespace mikoto::scene {
    class Scene;
    class Camera;
}

namespace mikoto::renderer {

    // Earth-like participating media. Distances inside the solver are kilometers;
    // mWorldUnitsPerKilometer explicitly defines the scene-to-atmosphere scale.
    struct AtmosphereSettings {
        core::f32 mPlanetRadius{ 6360.0f };
        core::f32 mAtmosphereHeight{ 100.0f };
        core::f32 mWorldUnitsPerKilometer{ 1000.0f };
        core::f32 mGroundHeight{};
        core::f32 mRayleighScale{ 1.0f };
        core::f32 mMieScale{ 1.0f };
        core::f32 mOzoneScale{ 1.0f };
        core::f32 mMieAnisotropy{ 0.8f };
        core::f32 mGroundAlbedo{ 0.1f };
        core::f32 mSunElevation{ 35.0f };
        core::f32 mSunAzimuth{ 135.0f };
        core::f32 mSunIlluminance{ 10.0f };
        core::f32 mSunAngularRadius{ 0.266f };
        core::f32 mAerialDistance{ 128.0f };

        bool mUseDirectionalLight{ true };
        bool mAerialPerspective{ true };

        auto Sanitize() -> void;
        auto operator==( const AtmosphereSettings & ) const -> bool = default;
    };

    struct AtmosphereModuleInfo {
        FGBufferHandle mParameters{};
        FGSamplerHandle mSampler{};
        FGTextureHandle mTransmittance{};
        FGTextureHandle mMultipleScattering{};
        FGTextureHandle mSkyView{};
        FGTextureHandle mEnvironmentCube{};
        FGTextureHandle mAerialRadiance{};
        FGTextureHandle mAerialTransmittance{};
        FGTextureHandle mSceneCopy{};
        FGPipelineHandle mLutPipeline{};
        FGPipelineHandle mAerialPipeline{};
        FGPipelineHandle mCompositePipeline{};
        FGPipelineHandle mSunPipeline{};

        eastl::optional<core::u32> mSunLight{};
    };

    // Matches AtmosphereData in Atmosphere_Common.slang. Vector-sized fields
    // keep the buffer ABI explicit across C++ and Slang.
    struct AtmosphereData {
        core::float4 mRadii{};
        core::float4 mRayleigh{};
        core::float4 mMie{};
        core::float4 mOzone{};
        core::float4 mGround{};
        core::float4 mSunDirection{};
        core::float4 mSunIlluminance{};
        core::float4 mView{};
    };

    class AtmosphericScatteringModule {
    public:
        explicit AtmosphericScatteringModule( rhi::RenderResolution resolution );

        auto RegisterTransmittance( FrameGraph& graph ) -> void;
        auto RegisterParameterUpload( FrameGraph& graph ) -> void;
        
        auto RegisterSkyView( FrameGraph& graph ) -> void;
        auto RegisterMultipleScattering( FrameGraph& graph ) -> void;
        
        auto RegisterEnvironment( FrameGraph& graph ) -> void;
        auto RegisterAerialPerspective( FrameGraph& graph ) -> void;

        auto RegisterSun( FrameGraph& graph ) -> void;
        auto RegisterSceneCopy( FrameGraph& graph ) -> void;
        auto RegisterComposite( FrameGraph& graph ) -> void;

        auto SetSkyVisible( bool visible ) -> void;
        auto SetCamera( const scene::Camera* camera ) -> void;

        // Returns true when the environment cube map changes and IBL must be
        // regenerated. Camera motion only updates the view-dependent LUTs.
        MKT_NODISCARD auto Prepare( const scene::Scene* scene, FrameGraph& graph ) -> bool;

        MKT_NODISCARD auto IsEnabled() const -> bool;
        MKT_NODISCARD auto GetData() const -> const AtmosphereData&;

    private:
        auto RegisterLookupTablePass( FrameGraph& graph, core::cstr name, FGTextureHandle output, core::u32 stage ) -> void;
        auto RegisterCompositePass( FrameGraph& graph, core::u32 stage ) -> void;

        auto Draw( CommandContext& ctx, core::Blackboard& blackboard, core::u32 stage, core::u32 face = 0 ) -> void;

    private:
        AtmosphereData mData{};
        AtmosphereSettings mSettings{};

        const scene::Camera* mCamera{};

        rhi::RenderResolution mResolution{};

        bool mInitialized{ false };
        bool mConfigured{ false };
        bool mEnabled{ false };
        bool mSkyVisible{ true };
    };
}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_FORWARD_RENDERER_ATMOSPHERIC_SCATTERING_MODULE_HH

