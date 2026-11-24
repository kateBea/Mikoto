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

    auto VideoModule::PrepareFrame( const scene::Scene* scene ) -> u64 {
        mScene = scene;

        if ( !mScene ) {
            mPlaybacks.clear();
            return 0;
        }

        u64 revision{};
        const auto& registry{ mScene->GetRegistry() };

        // Prepare material bindings before the raster and ray-tracing scene snapshots are packed.
        for ( const auto [entity, component, tag]: registry.view<scene::VideoComponent, scene::TagComponent>().each() ) {
            auto data{ component.GetRenderData() };

            if ( !tag.IsActive() || !data ) {
                continue;
            }

            const auto* source{ registry.try_get<scene::MaterialComponent>( entity ) };
            data->UpdateMaterial( source ? source->GetMaterial().As<material::SurfaceMaterial>() : material::SurfaceMaterialHandle{}, component.GetOutputMap() );
            data->Prepare();
            revision = revision * 1099511628211ull ^ ( as<u64>( tag.GetGuid() ) ^ data->GetRevision() ^ rc_cast<u64>( data.GetPtr() ) );
        }

        return revision;
    }

    auto VideoModule::GatherPlaybacks() -> void {
        mPlaybacks.clear();

        if ( !mScene ) {
            return;
        }

        // Retain the component-owned render data for this view's upload/decode/presentation passes.
        for ( const auto [entity, component, tag]: mScene->GetRegistry().view<scene::VideoComponent, scene::TagComponent>().each() ) {
            if ( tag.IsActive() ) {
                if ( auto data{ component.GetRenderData() } ) {
                    mPlaybacks.push_back( eastl::move( data ) );
                }
            }
        }
    }

    auto VideoModule::RegisterPasses( FrameGraph& graph ) -> void {
        if ( !RenderSystem::Get()->GetGpuDevice()->GetQueue( QueueType::eVideoDecode ) ) {
            return;
        }

        mGathered = graph.Create( FGVideoDescription{ "Transferred video playbacks" } );
        mUploaded = graph.Create( FGVideoDescription{ "Uploaded video bitstreams" } );
        mDecoded = graph.Create( FGVideoDescription{ "Decoded video pictures" } );
        mConverted = graph.Create( FGVideoDescription{ "Converted video images" } );
        mPresented = graph.Create( FGVideoDescription{ "Presented video materials" } );
        mSampler = graph.Create( FGSamplerDescription{}
            .SetName( "Video plane sampler" )
            .SetFilter( SamplerFilter::eLinear )
            .SetWrap( SamplerWrapMode::eClampToEdge ) );
        mPipeline = graph.Create( FGPipelineDescription{}
            .SetName( "Video_Presentation" )
            .SetPipelineType( PipelineType::eCompute )
            .PushShader( "VideoPresent_Comp.slang", FGStageType::eCompute ) );

        graph.RegisterPass(
            "Video_PlaybackTransfer",
            FGPassType::eVideo,
            [this]( FGNodeBuilder& builder, Blackboard& ) {
                builder.UseResource( mGathered, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this]( CommandContext&, Blackboard& ) {
                GatherPlaybacks();
            } );

        // Host-visible uploads run in the video prelude before any picture is recorded.
        // The render system reuses each clip's immutable buffer on subsequent frames.
        graph.RegisterPass(
            "Video_BitstreamUpload",
            FGPassType::eVideo,
            [this]( FGNodeBuilder& builder, Blackboard& ) {
                builder.UseResource( mGathered, FGPipelineStage::eCopy, FGResourceAccess::eRead );
                builder.UseResource( mUploaded, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this]( CommandContext&, Blackboard& ) {
                for ( auto& playback: mPlaybacks ) {
                    playback->UploadBitstream();
                }
            } );

        graph.RegisterPass(
            "Video_Decode",
            FGPassType::eVideo,
            [this]( FGNodeBuilder& builder, Blackboard& ) {
                builder.UseResource( mUploaded, FGPipelineStage::eCopy, FGResourceAccess::eRead );
                builder.UseResource( mDecoded, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this]( CommandContext& context, Blackboard& ) {
                RecordDecodeWork( context );
            } );

        graph.RegisterPass(
            "Video_Presentation",
            FGPassType::eCompute,
            [this]( FGNodeBuilder& builder, Blackboard& ) {
                builder.UseTextures( mDecoded, FGPipelineStage::eComputeShader, FGResourceAccess::eRead,
                    [this]( eastl::vector<TextureHandle>& textures ) {
                        GatherPresentationTextures( textures, true );
                    } );
                builder.UseTextures( mConverted, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite,
                    [this]( eastl::vector<TextureHandle>& textures ) {
                        GatherPresentationTextures( textures, false );
                    } );
            },
            [this]( CommandContext& context, Blackboard& ) {
                Present( context );
            } );

        // Publish the converted images to bindless material consumers, including views
        // that reuse an already presented frame. No conversion or decode is needed then.
        graph.RegisterPass(
            "Video_MaterialReady",
            FGPassType::eGeneric,
            [this]( FGNodeBuilder& builder, Blackboard& ) {
                const auto resolveTextures{ [this]( eastl::vector<TextureHandle>& textures ) {
                    GatherOutputTextures( textures );
                } };

                builder.UseTextures( mConverted, FGPipelineStage::ePixelShader, FGResourceAccess::eRead, resolveTextures );
                builder.UseTextures( mConverted, FGPipelineStage::eComputeShader, FGResourceAccess::eRead, resolveTextures );

                if ( RenderSystem::Get()->GetGpuDevice()->SupportsRayTracing() ) {
                    builder.UseTextures( mConverted, FGPipelineStage::eRayTracingShader, FGResourceAccess::eRead, resolveTextures );
                }

                builder.UseResource( mPresented, FGPipelineStage::ePixelShader, FGResourceAccess::eWrite );
            },
            []( CommandContext&, Blackboard& ) {
                // Nothing
            } );

        // Batch handles order the fixed graph; runtime texture declarations resolve
        // the actual images. Materials must not read an output before it is published.
        auto& control{ graph.GetNodeControl() };

        for ( auto& [name, node]: control.mNodes ) {
            if ( node.mType != FGPassType::eVideo && name != "Video_Presentation" && name != "Video_MaterialReady" ) {
                FGNodeBuilder builder{ node, control };
                builder.UseResource( mPresented, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
            }
        }
    }

    auto VideoModule::GatherPresentationTextures( eastl::vector<TextureHandle>& textures, bool decodedInputs ) const -> void {
        for ( const auto& playback: mPlaybacks ) {
            const auto frame{ playback->GetCurrentFrame() };

            if ( frame && frame->mRecorded && !playback->HasPresented( frame->mGeneration ) ) {
                // Y and UV are views of one allocation. The graph transitions its full storage.
                textures.push_back( decodedInputs ? frame->mSurface->GetImage() : playback->GetOutputTexture() );
            }
        }
    }

    auto VideoModule::GatherOutputTextures( eastl::vector<TextureHandle>& textures ) const -> void {
        for ( const auto& playback: mPlaybacks ) {
            if ( auto output{ playback->GetOutputTexture() } ) {
                textures.push_back( eastl::move( output ) );
            }
        }
    }

    auto VideoModule::RecordDecodeWork( CommandContext& context ) -> void {
        for ( auto& playback: mPlaybacks ) {
            if ( !playback->mBitstream || playback->mPlayback->GetState() == video::PlaybackState::eFailed ) {
                continue;
            }

            for ( auto& work: playback->mDecodeWork ) {
                work.mDescription.mBitstream = playback->mBitstream->mBuffer;
                work.mDescription.mBitstreamRange = playback->mBitstream->mPictures.at( work.mDescription.mPicture->GetSampleIndex() );
                context.DecodeVideo( mDecoded, work.mDescription );
                work.mFrame->mRecorded = true;
            }

            playback->mDecodeWork.clear();
        }
    }

    auto VideoModule::Present( CommandContext& context ) -> void {
        if ( !mScene ) {
            return;
        }

        for ( auto& playback: mPlaybacks ) {
            auto frame{ playback->GetCurrentFrame() };

            if ( !frame || !frame->mRecorded || playback->HasPresented( frame->mGeneration ) ) {
                continue;
            }

            auto output{ playback->GetOutputTexture() };
            const auto& track{ playback->GetPlayback()->GetClip()->GetInfo().mVideo };

            const auto decodedImage{ frame->mSurface->GetImage() };
            const float2 decodedExtent{ decodedImage->GetWidth(), decodedImage->GetHeight() };

            VideoPresentationConstants constants{};
            constants.mWidth = output->GetWidth();
            constants.mHeight = output->GetHeight();

            constants.mLuma = context.UseImportedTexture( context.ImportSampledTexture( frame->mSurface->GetLumaImage() ) );
            constants.mChroma = context.UseImportedTexture( context.ImportSampledTexture( frame->mSurface->GetChromaImage() ) );
            
            constants.mSampler = context.PushSampler( mSampler );
            constants.mOutput = context.PushTexture_UAV( context.ImportTexture( output ) );
            
            constants.mTransfer = track.mTransferCharacteristics;
            constants.mMatrix = track.mMatrixCoefficients;
            constants.mFullRange = track.mFullRange;

            // Decoder storage can exceed the coded extent 
            // to satisfy backend alignment requirements.
            constants.mUvRect = { 
                track.mWidth / decodedExtent.x, 
                track.mHeight / decodedExtent.y,
                track.mCropLeft / decodedExtent.x, 
                track.mCropTop / decodedExtent.y 
            };

            constants.mChromaOffset = track.mChromaOffset / decodedExtent;

            context.BindPipeline( mPipeline );
            context.PushConstants( constants );
            context.Dispatch( ( output->GetWidth() + 7 ) / 8, ( output->GetHeight() + 7 ) / 8, 1 );

            playback->MarkPresented( frame->mGeneration );
        }
    }
}
