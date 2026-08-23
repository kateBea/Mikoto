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

#ifndef MIKOTO_POST_EFFECTS_PASSES_HH
#define MIKOTO_POST_EFFECTS_PASSES_HH

#include <EASTL/fixed_vector.h>

#include <Core/Core.hh>
#include <Core/String.hh>
#include <Core/Types.hh>

#include <Scene/Scene.hh>
#include <Scene/Camera.hh>
#include <Scene/Component.hh>

#include <Renderer/Text/Font.hh>

#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Passes/GeometryCullModule.hh>

namespace mikoto::renderer {

    inline constexpr core::u32 kSsaoKernelSize{ 64 };
    inline constexpr core::u32 kMaxBloomChainImages{ 4 };
    inline constexpr core::u32 kSsaoNoiseDimensions{ 8 };

    struct PostProcessModuleInfo {
        // SSAO
        FGSamplerHandle mSsaoSampler{};

        FGTextureHandle mSsaoColorTarget{};
        FGTextureHandle mSsaoNoiseTexture{};
        FGTextureHandle mSsaoBlurColorTarget{};

        FGBufferHandle mSsaoKernelBuffer{};

        FGPipelineHandle mSsaoBlurPipeline{};
        FGPipelineHandle mSsaoRenderPipeline{};

        // Bloom
        bool mEnabled{ true };
        core::f32 mThreshold{ 1.0f };
        core::f32 mIntensity{ 1.0f };
        core::f32 mScatter{ 0.7f };
        core::u32 mMipCount{ 4 };

        // Fixed order: mBloomChainImages[0] highest mip (original resolution)
        // Resolution decreases as we move towards end of vector
        eastl::fixed_vector<FGTextureHandle, kMaxBloomChainImages> mBloomChainImages{};

        // Post-process

        // Infinite grid
        FGPipelineHandle mInfiniteGridPipeline{};
    };

    class PostEffectsPass {
    public:
        explicit PostEffectsPass( rhi::RenderResolution resolution );

        auto SetScene( const scene::Scene* scene ) -> void;
        auto SetCamera( const scene::Camera* camera ) -> void;

        auto RegisterPasses( FrameGraph& graph ) -> void;

        auto SetEnableBloom( bool value ) -> void;

        auto SetGamma( core::f32 gamma ) -> void;
        auto SetExposure( core::f32 exposure ) -> void;

    private:
        auto RegisterSsao( FrameGraph& graph ) -> void;
        auto RegisterBloom( FrameGraph& graph ) -> void;

        auto RegisterPostProcess( FrameGraph& graph ) -> void;

        auto RegisterInfiniteGrid( FrameGraph& graph ) -> void;

        auto RegisterObjectOutline( FrameGraph& graph ) -> void;

        auto RegisterEyeAdaptationPass( FrameGraph& graph ) -> void;

        auto SetupPostProcessMaterials( CommandContext& ctx, Blackboard& b ) -> void;

    private:
        const scene::Scene* mScene{};
        const scene::Camera* mCamera{};

        rhi::RenderResolution mResolution{ rhi::RenderResolution::e1080P };

        // Bloom
        bool mEnableBloom{};

        // Tonemap
        core::f32 mGamma{ 1.0f };
        core::f32 mExposure{ 1.0f };

        // SSAO
        eastl::array<float4, kSsaoKernelSize> mSsaoKernelSamples{};
        eastl::fixed_vector<float4, kSsaoNoiseDimensions * kSsaoNoiseDimensions> mSsaoNoiseData{};
    };

}// namespace Mikoto

#endif // MIKOTO_POST_EFFECTS_PASSES_HH

