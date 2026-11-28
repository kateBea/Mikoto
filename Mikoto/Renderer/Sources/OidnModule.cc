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

#include <Renderer/Core/CommandContext.hh>
#include <Renderer/ForwardRenderer/Passes/OidnModule.hh>
#include <Renderer/ForwardRenderer/Passes/RayTracingModule.hh>

namespace mikoto::renderer {

    // https://github.com/nvpro-samples/vk_oidn/blob/main/docs/architecture.md
    using namespace mikoto::core;
    using namespace mikoto::renderer::rhi;

    struct OidnModule::State final {
        eastl::string mError{};
        bool mInitialized{};
        bool mReady{};
        bool mExecuted{};
        bool mHasResult{};
        FGDenoiserHandle mDenoiser{};
        FGTextureHandle mInput{};
        FGTextureHandle mOutput{};
        core::u32 mWidth{};
        core::u32 mHeight{};
    };

    OidnModule::OidnModule( RenderResolution resolution )
        : OidnModule{ rhi::GetRenderDimensions( resolution ) } {
    }

    OidnModule::OidnModule( core::uint2 dimensions )
        : mState{ eastl::make_unique<State>() }, mDimensions{ dimensions } {
    }

    OidnModule::~OidnModule() = default;

    auto OidnModule::SetSettings( DenoiserSettings settings ) -> void {
        mSettings = settings;
    }

    auto OidnModule::IsActive() const -> bool {
        return mSettings.mEnabled && mState->mReady && mState->mHasResult;
    }

    auto OidnModule::GetError() const -> const eastl::string & {
        return mState->mError;
    }

    auto OidnModule::Prepare( const RayTracingModule &rays ) -> void {
        mState->mReady = mSettings.mEnabled && mState->mError.empty() && rays.IsReady();
    }

    auto OidnModule::RegisterPasses( FrameGraph &graph, bool pathTracing ) -> void {
        if ( !mSettings.mEnabled ) {
            return;
        }

        auto &state{ *mState };
        const auto width{ mDimensions.x };
        const auto height{ mDimensions.y };
        state.mWidth = static_cast<core::u32>( width );
        state.mHeight = static_cast<core::u32>( height );
        state.mInput = pathTracing ? graph.GetData<RayTracingInfo>().mDirect : graph.GetData<RayTracingInfo>().mIndirect;

        state.mOutput = graph.Create( FGTextureDescription{}
            .SetName( "OIDN_FilteredHDR" )
            .SetWidth( width )
            .SetHeight( height )
            .SetFormat( Format::eRGBA32_FLOAT )
            .SetUsage( TextureUsageFlagsBits::ShaderResource | 
                TextureUsageFlagsBits::UnorderedAccess ) );

        state.mDenoiser = graph.Create( FGDenoiserDescription{}
            .SetName( "OIDN_ViewState" )
            .SetType( ImageDenoiserType::eOidn )
            .SetQuality( static_cast<ImageDenoiserQuality>( mSettings.mOidnQuality ) )
            .SetInputSize( { state.mWidth, state.mHeight } )
            .SetOutputSize( { state.mWidth, state.mHeight } ) );

        graph.RegisterPass(
            "OIDN_Initialize",
            FGPassType::eCompute,
            [this]( FGNodeBuilder &builder, core::Blackboard & ) {
                builder.UseResource( mState->mDenoiser, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                builder.UseResource( mState->mDenoiser, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
            },
            [this]( CommandContext &context, core::Blackboard & ) {
                mState->mInitialized = context.InitializeDenoiser( mState->mDenoiser );
                mState->mHasResult = false;
                mState->mError = context.GetDenoiser( mState->mDenoiser )->GetError();
            } );
        graph.SetExecutionPolicy( "OIDN_Initialize", FGExecutionPolicy::eOnce );

        // Read-only storage access gives the SDK GENERAL layout without making
        // the noisy input a writer in the graph's dependency/version tracking.
        graph.RegisterPass(
            "OIDN_Filter",
            FGPassType::eCompute,
            [this]( FGNodeBuilder &builder, core::Blackboard & ) {
                builder.UseResource( mState->mDenoiser, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
                builder.UseResource( mState->mInput, FGPipelineStage::eComputeShader, FGResourceAccess::eReadStorage );
                builder.UseResource( mState->mOutput, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
            },
            [this]( CommandContext &context, core::Blackboard &blackboard ) {
                auto &state{ *mState };
                state.mExecuted = false;
                if ( !state.mInitialized || !state.mReady || !blackboard.Get<RayTracingInfo>().mTraceOutputsReady ) {
                    return;
                }
                const FGDenoiserDispatch dispatch{
                    .mColor = state.mInput,
                    .mOutput = state.mOutput,
                };
                const auto result{ context.DispatchDenoiser( state.mDenoiser, dispatch ) };
                state.mExecuted = result == DenoiserResult::eSuccess;
                if ( !state.mExecuted ) {
                    state.mError = context.GetDenoiser( state.mDenoiser )->GetError();
                    state.mReady = false;
                }
            } );

        const auto pipeline{ graph.Create( FGPipelineDescription{}
            .SetName( "OIDN_CompositePipeline" )
            .SetPipelineType( PipelineType::eCompute )
            .PushShader( "Oidn_Composite.slang", FGStageType::eCompute ) ) };

        graph.RegisterPass(
            "OIDN_Composite",
            FGPassType::eCompute,
            [this]( FGNodeBuilder &builder, core::Blackboard & ) {
                builder.UseResource( mState->mOutput, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( mState->mInput, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
            },
            [this, pipeline]( CommandContext &context, core::Blackboard & ) {
                if ( !mState->mExecuted ) {
                    return;
                }

                struct Parameters {
                    core::u32 mFiltered{};
                    core::u32 mOutput{};
                    core::u32 mWidth{};
                    core::u32 mHeight{};
                    core::f32 mSplit{};
                } parameters{
                    .mFiltered = context.PushTexture_SRV( mState->mOutput ),
                    .mOutput = context.PushTexture_UAV( mState->mInput ),
                    .mWidth = mState->mWidth,
                    .mHeight = mState->mHeight,
                    .mSplit = mSettings.mSplitScreen,
                };

                // Rebind Mikoto's pipeline, descriptor table and push constants
                // after the RHI denoiser dispatch changed those bindings.
                context.BindPipeline( pipeline );
                context.PushConstants( parameters );

                constexpr core::u32 groupSide{ 8 };

                const u32 x{ ( parameters.mWidth + groupSide - 1 ) / groupSide };
                const u32 y{ ( parameters.mHeight + groupSide - 1 ) / groupSide };
                const u32 z{ 1u };

                context.Dispatch( x, y, z );

                mState->mHasResult = true;
            } );
    }
}
