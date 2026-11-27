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

#ifndef MIKOTO_RENDERER_FORWARD_RENDERER_POST_PROCESS_MODULE_HH
#define MIKOTO_RENDERER_FORWARD_RENDERER_POST_PROCESS_MODULE_HH

#include <EASTL/array.h>
#include <EASTL/fixed_vector.h>
#include <EASTL/optional.h>
#include <EASTL/vector.h>

#include <glm/glm.hpp>

#include <Core/Core.hh>
#include <Core/Profiler.hh>
#include <Core/Types.hh>

#include <Math/Math.hh>

#include <Material/PostProcessMaterial.hh>

#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/InfiniteGrid.hh>
#include <Renderer/Postprocess/Bloom.hh>

#include <Scene/Camera.hh>
#include <Scene/Scene.hh>

namespace mikoto::renderer {

    inline constexpr core::u32 kSsaoKernelSize{ 64 };

    inline constexpr core::u32 kMaxBloomChainImages{ 7 };
    inline constexpr core::u32 kSsaoNoiseDimensions{ 8 };

    // Matches PostProcessMaterialData in PostProcessMaterial_Frag.slang.
    struct PostProcessMaterialData {
        core::float4 mTintColor{ 1.0f };
        core::f32 mContrast{ 1.0f };
        core::f32 mSaturation{ 1.0f };
        core::u32 mStage{};
        core::f32 mFilmGrainAmount{};
    };

    struct PostProcessMaterialStageInfo {
        FGBufferHandle mMaterials{};
        FGTextureHandle mColorImage{};
        FGSamplerHandle mSampler{};
        FGPipelineHandle mPipeline{};
        core::u32 mMaterialCount{};
        core::float2 mChromaticOffsetPixels{};
    };

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
        BloomSettings mBloomSettings{};
        FGSamplerHandle mBloomSampler{};
        FGTextureHandle mBloomResult{};

        FGPipelineHandle mBloomUpSamplePipeline{};
        FGPipelineHandle mBloomDownSamplePipeline{};

        // Starts at half resolution. Keep each downsample for reconstruction;
        // upsampling writes separate images, never sampling its own attachment.
        struct ImageDescription {
            core::u32 mWidth{};
            core::u32 mHeight{};
            FGTextureHandle mImage{};
            FGTextureHandle mReconstructed{};
        };
        eastl::fixed_vector<ImageDescription, kMaxBloomChainImages> mBloomChainImages{};

        // Post-process
        PostProcessMaterialStageInfo mBeforeToneMapping{};
        PostProcessMaterialStageInfo mAfterToneMapping{};

        // Infinite grid
        FGPipelineHandle mInfiniteGridPipeline{};
    };

    class PostEffectsPass {
    public:
        explicit PostEffectsPass( rhi::RenderResolution resolution );

        auto SetScene( const scene::Scene* scene ) -> void;
        auto SetCamera( const scene::Camera* camera ) -> void;
        auto SetPreviewMaterial( const material::PostProcessMaterial& material ) -> void;

        // Raster scenes can keep their emissive source unless a material supplies full-image bloom.
        auto RegisterBloom( FrameGraph& graph, FGTextureHandle source,
            FGTextureHandle materialSource = {} ) -> void;
        auto RegisterEyeAdaptationPass( FrameGraph& graph ) -> void;

        /** @brief Registers a material filter/upload pass and a full-screen draw pass.
         * @param graph Graph receiving the stage's material buffer and output texture.
         * @param stage Selects HDR input before tone mapping or display color after it.
         * @param source Optional input override in the stage's color space (HDR before tone mapping).
         * @details Register after the stage's source image has been created. Every active
         * component applies its material in stable entity-ID order, consuming the previous color.
         * The first enabled chromatic aberration in each stage sets the source image sampling.
         * Register the after stage after camera effects so grain retains its pixel detail.
         */
        auto RegisterPostProcess( FrameGraph& graph,
            material::PostProcessStage stage = material::PostProcessStage::eBeforeToneMapping,
            FGTextureHandle source = {} ) -> void;

        /** Maximum material applications per stage, matching the upload buffer. */
        static constexpr core::u32 kMaxPostProcessMaterials{ 4096 };

        // Editor overlay in display color space; register after tone mapping.
        auto RegisterInfiniteGrid( FrameGraph& graph ) -> void;
        auto SetInfiniteGridSettings( const InfiniteGridSettings &settings ) -> void;

        auto SetEnableBloom( bool value ) -> void;
        auto GetBloomSettings() -> BloomSettings&;

        auto SetGamma( core::f32 gamma ) -> void;
        auto SetExposure( core::f32 exposure ) -> void;

        auto RegisterSsao( FrameGraph& graph ) -> void;
    private:
        auto SetupPostProcessMaterials( CommandContext& ctx, core::Blackboard& b, material::PostProcessStage stage ) -> void;

    private:
        const scene::Scene* mScene{};
        const scene::Camera* mCamera{};

        rhi::RenderResolution mResolution{ rhi::RenderResolution::e1080P };
        eastl::vector<PostProcessMaterialData> mPostProcessMaterials{};
        eastl::optional<PostProcessMaterialData> mPreviewMaterial{};
        core::float2 mPreviewChromaticOffsetPixels{};

        // Editor grid appearance; changes update push constants without rebuilding pipelines.
        InfiniteGridSettings mInfiniteGrid{};

        // Bloom
        BloomSettings mBloomSettings{};

        // Tonemap
        core::f32 mGamma{ 1.0f };
        core::f32 mExposure{ 1.0f };

        // SSAO
        eastl::array<core::float4, kSsaoKernelSize> mSsaoKernelSamples{};
        eastl::fixed_vector<core::float4, kSsaoNoiseDimensions * kSsaoNoiseDimensions> mSsaoNoiseData{};
    };

}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_FORWARD_RENDERER_POST_PROCESS_MODULE_HH
