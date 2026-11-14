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
#include <Renderer/Core/RenderSystem.hh>
#include <Renderer/ForwardRenderer/Passes/VideoModule.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::renderer::rhi;

    struct VideoPresentationConstants {
        u32 mWidth{};
        u32 mHeight{};
        u32 mLuma{};
        u32 mChroma{};
        u32 mOutput{};
        u32 mSampler{};
        u32 mTransfer{};
        u32 mMatrix{};
        u32 mFullRange{};
        u32 mPadding[3]{};
        float4 mUvRect{};
        float2 mChromaOffset{};
        float2 mPadding2{};
    };

    auto VideoModule::SetCamera( const scene::Camera& camera ) -> void {
        mCamera = &camera;
    }

    auto VideoModule::SetScene( const scene::Scene& scene ) -> void {
        mScene = &scene;
    }

    auto VideoModule::RegisterPasses( FrameGraph& graph ) -> void {
        if ( !RenderSystem::Get()->GetGpuDevice()->GetQueue( QueueType::eVideoDecode ) ) {
            return;
        }

        mDecoded = graph.Create( FGVideoDescription{ "Decoded video pictures" } );
        mPresented = graph.Create( FGVideoDescription{ "Presented video materials" } );
        mSampler = graph.Create( FGSamplerDescription{}
            .SetName( "Video plane sampler" )
            .SetFilter( SamplerFilter::eLinear )
            .SetWrap( SamplerWrapMode::eClampToEdge ) );
        mPipeline = graph.Create( FGPipelineDescription{}
            .SetName( "Video presentation" )
            .SetPipelineType( PipelineType::eCompute )
            .PushShader( "VideoPresent_Comp.slang", FGStageType::eCompute ) );

        graph.RegisterPass(
            "Video decode",
            FGPassType::eVideo,
            [this]( FGNodeBuilder& builder, Blackboard& ) {
                builder.UseResource( mDecoded, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this]( CommandContext& context, Blackboard& ) {
                if ( mScene ) {
                    video::VideoService::Get()->RecordDecodeWork( *mScene, context, mDecoded );
                }
            } );

        graph.RegisterPass(
            "Video presentation",
            FGPassType::eCompute,
            [this]( FGNodeBuilder& builder, Blackboard& ) {
                builder.UseResource( mDecoded, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( mPresented, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
            },
            [this]( CommandContext& context, Blackboard& ) {
                Present( context );
            } );

        // Frame-graph limitation: runtime bindless material images cannot yet declare
        // per-picture dependencies during setup. These video tokens stand in for that
        // changing image set; ordinary resource reads still derive every graph edge.
        auto& control{ graph.GetNodeControl() };

        for ( auto& [name, node]: control.mNodes ) {
            if ( node.mType != FGPassType::eVideo && name != "Video presentation" ) {
                FGNodeBuilder builder{ node, control };
                builder.UseResource( mPresented, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
            }
        }
    }

    auto VideoModule::Present( CommandContext& context ) -> void {
        if ( !mScene ) {
            return;
        }

        for ( auto playback: video::VideoService::Get()->GetPlaybacks( *mScene ) ) {
            auto frame{ playback->GetCurrentFrame() };

            if ( !frame || !frame->mRecorded || playback->HasPresented( frame->mGeneration ) ) {
                continue;
            }

            auto output{ playback->GetOutputTexture() };
            context.PrepareVideoSampling( frame->mSurface, output );
            const auto& track{ playback->GetClip()->GetInfo().mVideo };
            const auto decodedImage{ frame->mSurface->GetImage() };
            const float2 decodedExtent{ decodedImage->GetWidth(), decodedImage->GetHeight() };
            VideoPresentationConstants constants{};
            constants.mWidth = output->GetWidth();
            constants.mHeight = output->GetHeight();
            constants.mLuma = context.UseImportedTexture( context.ImportSampledTexture( frame->mSurface->GetLumaImage() ) );
            constants.mChroma = context.UseImportedTexture( context.ImportSampledTexture( frame->mSurface->GetChromaImage() ) );
            constants.mOutput = context.PushTexture_UAV( context.ImportTexture( output ) );
            constants.mSampler = context.PushSampler( mSampler );
            constants.mTransfer = track.mTransferCharacteristics;
            constants.mMatrix = track.mMatrixCoefficients;
            constants.mFullRange = track.mFullRange;
            // Decoder storage can exceed the coded extent to satisfy backend alignment requirements.
            constants.mUvRect = { track.mWidth / decodedExtent.x, track.mHeight / decodedExtent.y,
                track.mCropLeft / decodedExtent.x, track.mCropTop / decodedExtent.y };
            constants.mChromaOffset = track.mChromaOffset / decodedExtent;

            context.BindPipeline( mPipeline );
            context.PushConstants( constants );
            context.Dispatch( ( output->GetWidth() + 7 ) / 8, ( output->GetHeight() + 7 ) / 8, 1 );
            context.FinishVideoPresentation( output );
            playback->MarkPresented( frame->mGeneration );
        }
    }
}
