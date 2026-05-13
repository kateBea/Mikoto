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

    static constexpr u32 kSsaoKernelSize{ 64 };
    static constexpr u32 kMaxBloomChainImages{ 4 };

    // Index order matches shader
    // See base/Tonemap_Helpers.slang
    enum class ToneMappingType {
        Linear,
        Reinhard,
        Uncharted2,
        Aces,
        Khronos_Neutral,
        Max_Count,
    };

    struct PostProcessModuleInfo {
        // SSAO
        FGSamplerHandle mSsaoSampler{};

        // Bloom
        bool mEnabled{ true };
        f32 mThreshold{ 1.0f };
        f32 mIntensity{ 1.0f };
        f32 mScatter{ 0.7f };
        u32 mMipCount{ 4 };

        // Fixed order: mBloomChainImages[0] highest mip (original resolution)
        // Resolution decreases as we move towards end of vector
        eastl::fixed_vector<FGTextureHandle, kMaxBloomChainImages> mBloomChainImages{};

        // Tonemap
        f32 mExposure{};
        f32 mGamma{};
        i32 mToneMapType{};
        FGTextureHandle mTonemapColor{};
        FGPipelineHandle mTonemapPipeline{};

        // Post-process
    };

    class PostEffectsPass {
    public:
        explicit PostEffectsPass( RenderResolution resolution );

        auto SetScene( const scene::Scene& scene ) -> void;
        auto SetCamera( const scene::Camera& camera ) -> void;

        auto RegisterPasses( FrameGraph& graph ) -> void;

        auto SetToneMapping( ToneMappingType type ) -> void;

        auto SetEnableBloom( bool value ) -> void;

    private:
        auto RegisterSsao( FrameGraph& graph ) -> void;
        auto RegisterBloom( FrameGraph& graph ) -> void;
        auto RegisterTonemap( FrameGraph& graph ) -> void;

        auto RegisterPostProcess( FrameGraph& graph ) -> void;

        auto RegisterObjectOutline( FrameGraph& graph ) -> void;
        auto RegisterDepthOfField( FrameGraph& graph ) -> void;

        auto SetupPostProcessMaterials( CommandContext& ctx, Blackboard& b  ) -> void;

    private:
        const scene::Scene* mScene{};
        const scene::Camera* mCamera{};

        RenderResolution mResolution{ RenderResolution::e1080P };
        ToneMappingType mToneMapType{ ToneMappingType::Aces };

        // Bloom
        bool mEnableBloom{};
    };

}// namespace Mikoto

#endif // MIKOTO_POST_EFFECTS_PASSES_HH

