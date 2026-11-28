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

#include <chrono>
#include <cstring>

#include <Renderer/Core/CommandContext.hh>
#include <Renderer/ForwardRenderer/Passes/DlssModule.hh>
#include <Renderer/ForwardRenderer/Passes/RayTracingModule.hh>

#include <Scene/Camera.hh>

namespace mikoto::renderer {
    using namespace rhi;

    struct DlssModule::State final {
        FGDenoiserHandle mDenoiser{};
        DenoiserFrameParameters mEvaluation{};
        bool mInitialized{};
        DlssInputs mInputs{};
        FGTextureHandle mReconstructed{};
        FGTextureHandle mOutput{};
        core::uint2 mInputSize{};
        core::uint2 mOutputSize{};
        const scene::Scene *mScene{};
        const scene::Camera *mCamera{};
        core::float4x4 mProjection{ 1.0f };
        core::u64 mTopologyRevision{};
        core::u32 mResetRevision{};
        bool mClearGlass{};
        bool mHistoryValid{};
        bool mExecuted{};
        bool mReady{};
        eastl::string mError{};
        std::chrono::steady_clock::time_point mLastPreparation{};
    };

    DlssModule::DlssModule( RenderResolution inputResolution, RenderResolution viewportResolution )
        : DlssModule{ GetRenderDimensions( inputResolution ), GetRenderDimensions( viewportResolution ) } {
    }

    DlssModule::DlssModule( core::uint2 inputSize, core::uint2 outputSize )
        : mState{ eastl::make_unique<State>() }, mInputSize{ inputSize }, mOutputSize{ outputSize } {
    }

    DlssModule::~DlssModule() = default;

    auto DlssModule::SetSettings( const DenoiserSettings &settings ) -> void {
        mSettings = settings;
    }

    auto DlssModule::GetOutput() const -> FGTextureHandle {
        return mState->mOutput;
    }

    auto DlssModule::IsActive() const -> bool {
        return mSettings.mEnabled && mState->mExecuted;
    }

    auto DlssModule::GetError() const -> const eastl::string & {
        return mState->mError;
    }

    auto DlssModule::Prepare( const scene::Scene *scene, const scene::Camera *camera, const RayTracingModule &rays ) -> void {
        auto &state{ *mState };
        mRays = MKT_ADDRESSOF( rays );

        state.mReady = false;
        if ( !camera ) {
            state.mHistoryValid = false;
            return;
        }
        // View motion retains history. Cuts/reset requests, scene/topology changes
        // and projection changes invalidate it; graph replacement covers resizing.
        const auto projection{ camera->GetProjection() };
        if ( state.mScene != scene || state.mCamera != camera || state.mProjection != projection ||
             state.mTopologyRevision != rays.GetTopologyRevision() || state.mResetRevision != mSettings.mResetRevision || state.mClearGlass != mSettings.mDlssClearGlass ) {
            state.mHistoryValid = false;
        }

        state.mScene = scene;
        state.mCamera = camera;
        state.mProjection = projection;
        state.mTopologyRevision = rays.GetTopologyRevision();
        state.mResetRevision = mSettings.mResetRevision;
        state.mClearGlass = mSettings.mDlssClearGlass;

        const auto viewRowMajor{ glm::transpose( camera->GetViewMatrix() ) };
        const auto projectionRowMajor{ glm::transpose( projection ) };
        
        std::memcpy( state.mEvaluation.mWorldToView.data(), &viewRowMajor, sizeof( viewRowMajor ) );
        std::memcpy( state.mEvaluation.mViewToClip.data(), &projectionRowMajor, sizeof( projectionRowMajor ) );
        
        const auto now{ std::chrono::steady_clock::now() };
        constexpr float initialFrameTimeMilliseconds{ 1000.0f / 60.0f };
        
        state.mEvaluation.mFrameTimeMilliseconds = state.mLastPreparation == std::chrono::steady_clock::time_point{}
            ? initialFrameTimeMilliseconds : std::chrono::duration<float, std::milli>( now - state.mLastPreparation ).count();
        state.mLastPreparation = now;
        state.mReady = mSettings.mEnabled && rays.IsReady();
    }

    auto DlssModule::RegisterPasses( FrameGraph &graph ) -> void {
        if ( !mSettings.mEnabled ) {
            return;
        }

        auto &state{ *mState };
        state.mInputs = graph.GetData<RayTracingInfo>().mReconstruction;
        
        if ( !state.mInputs.mNoisyColor.mHandle ) {
            return;
        }

        const auto inputWidth{ mInputSize.x };
        const auto inputHeight{ mInputSize.y };
        const auto outputWidth{ mOutputSize.x };
        const auto outputHeight{ mOutputSize.y };
        state.mInputSize = { inputWidth, inputHeight };
        state.mOutputSize = { outputWidth, outputHeight };

        state.mReconstructed = graph.Create( FGTextureDescription{}
            .SetName( "DLSS_ReconstructedHDR" )
            .SetWidth( outputWidth )
            .SetHeight( outputHeight )
            .SetFormat( Format::eRGBA32_FLOAT )
            .SetUsage( TextureUsageFlagsBits::ShaderResource | TextureUsageFlagsBits::UnorderedAccess ) );
        state.mOutput = graph.Create( FGTextureDescription{}
            .SetName( "DLSS_ViewportHDR" )
            .SetWidth( outputWidth )
            .SetHeight( outputHeight )
            .SetFormat( Format::eRGBA32_FLOAT )
            .SetUsage( TextureUsageFlagsBits::ShaderResource | TextureUsageFlagsBits::UnorderedAccess ) );

        state.mDenoiser = graph.Create( FGDenoiserDescription{}
            .SetName( "DLSS_ViewState" )
            .SetType( ImageDenoiserType::eDlssRayReconstruction )
            .SetPreset( mSettings.mDlssPreset )
            .SetInputSize( state.mInputSize )
            .SetOutputSize( state.mOutputSize ) );

        // Initialization writes the opaque weights/feature resource. Evaluation
        // writes its temporal history, so the graph orders and synchronizes both.
        graph.RegisterPass(
            "DLSS_Initialize",
            FGPassType::eCompute,
            [this]( FGNodeBuilder &builder, core::Blackboard & ) {
                builder.UseResource( mState->mDenoiser, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                builder.UseResource( mState->mDenoiser, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
            },
            [this]( CommandContext &context, core::Blackboard & ) {
                mState->mInitialized = context.InitializeDenoiser( mState->mDenoiser );
                mState->mHistoryValid = false;
                mState->mError = context.GetDenoiser( mState->mDenoiser )->GetError();
            } );

        graph.SetExecutionPolicy( "DLSS_Initialize", FGExecutionPolicy::eOnce );

        graph.RegisterPass(
            "DLSS_Reconstruct",
            FGPassType::eCompute,
            [this]( FGNodeBuilder &builder, core::Blackboard & ) {
                builder.UseResource( mState->mDenoiser, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
                for ( const auto texture: mState->mInputs.GetTextures() ) {
                    builder.UseResource( texture, FGPipelineStage::eComputeShader, FGResourceAccess::eReadStorage );
                }
                builder.UseResource( mState->mReconstructed, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
            },
            [this]( CommandContext &context, core::Blackboard &blackboard ) {
                auto &state{ *mState };
                state.mExecuted = false;

                if ( !state.mInitialized || !state.mReady || !blackboard.Get<RayTracingInfo>().mReconstructionReady ) {
                    state.mHistoryValid = false;
                    return;
                }

                const auto &guides{ mRays->GetReconstructionData() };
                state.mEvaluation.mJitter = { guides.mJitter.x, guides.mJitter.y };
                state.mEvaluation.mReset = !state.mHistoryValid || guides.mJitter.z == 0.0f;
                const FGDenoiserDispatch dispatch{
                    .mColor = state.mInputs.mNoisyColor,
                    .mDiffuseAlbedo = state.mInputs.mDiffuseAlbedo,
                    .mSpecularAlbedo = state.mInputs.mSpecularAlbedo,
                    .mNormalRoughness = state.mInputs.mNormalRoughness,
                    .mDepth = state.mInputs.mDepth,
                    .mMotion = state.mInputs.mMotion,
                    .mOutput = state.mReconstructed,
                    .mFrame = state.mEvaluation,
                };

                const auto result{ context.DispatchDenoiser( state.mDenoiser, dispatch ) };

                state.mExecuted = result == DenoiserResult::eSuccess;
                state.mHistoryValid = state.mExecuted;
                if ( result == DenoiserResult::eFailed ) {
                    state.mError = context.GetDenoiser( state.mDenoiser )->GetError();
                    state.mReady = false;
                }
            } );

        const auto sampler{ graph.Create( FGSamplerDescription{}
            .SetName( "DLSS_ResolveSampler" )
            .SetFilter( SamplerFilter::eLinear )
            .SetWrap( SamplerWrapMode::eClampToEdge ) ) };

        const auto pipeline{ graph.Create( FGPipelineDescription{}
            .SetName( "DLSS_ResolvePipeline" )
            .SetPipelineType( PipelineType::eCompute )
            .PushShader( "Dlss_Resolve.slang", FGStageType::eCompute ) ) };

        // A busy/unsupported SDK still produces a defined viewport image. This
        // pass also restores Mikoto's pipeline/table bindings after the RHI denoiser dispatch.
        graph.RegisterPass(
            "DLSS_Resolve",
            FGPassType::eCompute,
            [this]( FGNodeBuilder &builder, core::Blackboard & ) {
                builder.UseResource( mState->mInputs.mNoisyColor, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( mState->mReconstructed, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( mState->mOutput, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
            },
            [this, sampler, pipeline]( CommandContext &context, core::Blackboard & ) {
                struct Parameters {
                    core::u32 mSource{};
                    core::u32 mOutput{};
                    core::u32 mSampler{};
                    core::u32 mWidth{};
                    core::u32 mHeight{};
                } parameters{
                    .mSource = context.PushSRV( mState->mExecuted ? mState->mReconstructed : mState->mInputs.mNoisyColor ),
                    .mOutput = context.PushUAV( mState->mOutput ),
                    .mSampler = context.PushSampler( sampler ),
                    .mWidth = mState->mOutputSize.x,
                    .mHeight = mState->mOutputSize.y,
                };
                context.BindPipeline( pipeline );
                context.PushConstants( parameters );
                constexpr core::u32 groupSide{ 8 };
                context.Dispatch( ( parameters.mWidth + groupSide - 1 ) / groupSide,
                    ( parameters.mHeight + groupSide - 1 ) / groupSide, 1 );
            } );
    }
}
