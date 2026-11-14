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

#include <cmath>

#include <EASTL/algorithm.h>

#include <Core/Exception.hh>

#include <Logging/Logger.hh>

#include <Threading/TaskService.hh>

#include <Assets/AssetRegistryService.hh>

#include <Audio/AudioService.hh>

#include <Video/AacDecoder.hh>
#include <Video/VideoService.hh>

#include <Renderer/Core/RenderSystem.hh>
#include <Renderer/Core/CommandContext.hh>

#include <Scene/Scene.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )
#include <mfapi.h>
#endif

namespace mikoto::video {

    // https://wickedengine.net/category/devblog/2023/05/07/vulkan-video-decoding/index.html

    using namespace mikoto::core;
    using namespace mikoto::renderer::rhi;

    static auto PictureNumber( i32 frame, i32 current, i32 maximum ) -> i32 {
        return frame > current ? frame - maximum : frame;
    }

    static auto FindPreviewSample( eastl::span<const SampleInfo> samples ) -> u64 {
        // Container presentation order can differ from decode order, including edit-list preroll.
        u64 first{};
        bool found{};

        for ( u64 index{}; index < samples.size(); ++index ) {
            const auto& sample{ samples[index] };

            if ( sample.mPresentationTime.ToSeconds() + sample.mDuration.ToSeconds() <= 0 ) {
                continue;
            }

            if ( !found || sample.mPresentationTime.ToSeconds() < samples[first].mPresentationTime.ToSeconds() ) {
                first = index;
                found = true;
            }
        }

        return first;
    }

    static auto TrimPcmBefore( PcmBlock& block, f64 seconds ) -> void {
        const auto skip{ static_cast<usize>( eastl::max( 0.0, std::ceil( ( seconds - block.mPresentationTime.ToSeconds() ) * block.mSampleRate ) ) ) };
        const auto frames{ block.mSamples.size() / block.mChannels };

        if ( skip ) {
            const auto trimmed{ eastl::min( skip, frames ) };
            block.mSamples.erase( block.mSamples.begin(), block.mSamples.begin() + trimmed * block.mChannels );
            block.mPresentationTime = { static_cast<i64>( seconds * block.mSampleRate ), block.mSampleRate };
        }
    }

    VideoPlayback::VideoPlayback( asset::AssetHandle<asset::VideoClip> clip, const PlaybackSettings& settings )
        : mAsset{ eastl::move( clip ) }, mSettings{ settings } {
    }

    VideoPlayback::~VideoPlayback() = default;

    auto VideoPlayback::Play() -> void {
        if ( mPreview ) {
            return;
        }

        if ( mState == PlaybackState::eEnded ) {
            Seek( 0 );
        }

        if ( mState != PlaybackState::eFailed ) {
            mState = PlaybackState::ePlaying;
        }
    }

    auto VideoPlayback::Pause() -> void {
        if ( mState == PlaybackState::ePlaying ) {
            mState = PlaybackState::ePaused;

            if ( mAudio ) {
                mAudio->GetSource()->Pause();
            }
        }
    }

    auto VideoPlayback::Stop() -> void {
        Pause();
        Seek( 0 );
        mState = PlaybackState::eStopped;
    }

    auto VideoPlayback::Seek( f64 seconds ) -> void {
        if ( mPreview ) {
            return;
        }

        mSeekTime = eastl::max( 0.0, seconds );
    }

    auto VideoPlayback::GetState() const -> PlaybackState {
        return mState;
    }

    auto VideoPlayback::GetError() const -> const eastl::string& {
        return mError;
    }

    auto VideoPlayback::GetTimeSeconds() const -> f64 {
        return mTimeSeconds;
    }

    auto VideoPlayback::GetCurrentFrame() const -> Ref<DecodedVideoFrame> {
        return mCurrentFrame;
    }

    auto VideoPlayback::GetOutputTexture() const -> TextureHandle {
        return mOutput;
    }

    auto VideoPlayback::GetDecoder() const -> VideoDecoderHandle {
        return mDecoder;
    }

    auto VideoPlayback::GetClip() const -> Ref<asset::VideoClip> {
        return mClip;
    }


    auto VideoPlayback::HasPresented( u64 generation ) const -> bool {
        return mPresentedGeneration == generation;
    }


    auto VideoPlayback::MarkPresented( u64 generation ) -> void {
        mPresentedGeneration = generation;
    }

    auto VideoService::Initialize() -> void {
        mDevice = renderer::RenderSystem::Get()->GetGpuDevice();
        mAudioDevice = audio::AudioService::Get()->GetDevice();

#if defined( MIKOTO_PLATFORM_WINDOWS )
        if ( FAILED( MFStartup( MF_VERSION, MFSTARTUP_LITE ) ) ) {
            MKT_THROW_RUNTIME_ERROR( "Cannot initialize Media Foundation for AAC audio." );
        }

        mMediaFoundationStarted = true;
#endif

        mAcceptingJobs = true;
        mIsInitialized = true;
    }

    auto VideoService::Shutdown() -> void {
        {
            std::unique_lock lock{ mJobsMutex };
            mAcceptingJobs = false;
            mJobsFinished.wait( lock, [&] { return !mActiveJobs; } );
            mCompletedJobs.clear();
        }

        mScenes.clear();

#if defined( MIKOTO_PLATFORM_WINDOWS )
        if ( mMediaFoundationStarted ) {
            MFShutdown();
            mMediaFoundationStarted = false;
        }
#endif

        mDevice = nullptr;
        mAudioDevice = nullptr;
        mIsInitialized = false;
    }

    VideoService::~VideoService() {
        if ( mIsInitialized ) {
            Shutdown();
        }
    }

    auto VideoService::CollectJobs() -> void {
        eastl::vector<CompletedJob> jobs{};

        {
            std::lock_guard lock{ mJobsMutex };
            jobs.swap( mCompletedJobs );
        }

        for ( auto& job: jobs ) {
            auto& playback{ *job.mPlayback };

            if ( job.mEpoch != playback.mEpoch ) {
                continue;
            }

            playback.mJobPending = false;

            if ( !job.mError.empty() ) {
                playback.mError = eastl::move( job.mError );
                playback.mState = PlaybackState::eFailed;
                playback.mAudio.reset();
                MKT_CORE_LOGGER_ERROR( "Video playback: {}", playback.mError );
                continue;
            }

            playback.mNextVideoSample = job.mNextVideoSample;
            playback.mNextAudioSample = job.mNextAudioSample;
            playback.mAudioDecoder = eastl::move( job.mAudioDecoder );
            playback.mAudioEnded = job.mAudioEnded;
            playback.mVideoEnded = playback.mNextVideoSample == playback.mClip->GetSamples( TrackKind::eVideo ).size();

            for ( auto& picture: job.mPictures ) {
                playback.mPictures.push_back( eastl::move( picture ) );
            }

            for ( auto& pcm: job.mPcm ) {
                TrimPcmBefore( pcm, playback.mTimeSeconds );

                if ( !pcm.mSamples.empty() ) {
                    playback.mPendingPcm.push_back( eastl::move( pcm ) );
                }
            }
        }
    }

    auto VideoService::ResetPlayback( VideoPlayback& playback, f64 seconds ) -> void {
        // Replacing parser/audio state lets old worker results expire without blocking a seek.
        ++playback.mEpoch;
        playback.mJobPending = false;
        playback.mPictures.clear();
        playback.mPendingPcm.clear();
        playback.mDisplayQueue.clear();
        playback.mDecodeWork.clear();
        playback.mCurrentFrame.Reset();
        playback.mFramePool.clear();
        playback.mDpb.clear();
        playback.mDecoder.Reset();
        playback.mAudio.reset();
        playback.mAudioDecoder.reset();
        playback.mParser = eastl::make_shared<H264Parser>( playback.mClip->GetInfo().mVideo );
        playback.mTimeSeconds = eastl::min( seconds, playback.mClip->GetInfo().mDurationSeconds );
        playback.mNextVideoSample = playback.mClip->FindRandomAccessSample( playback.mTimeSeconds );
        playback.mNextAudioSample = 0;
        playback.mPreviousPocMsb = playback.mPreviousPocLsb = playback.mPreviousFrameNumber = playback.mFrameNumberOffset = 0;
        playback.mVideoEnded = playback.mAudioEnded = playback.mStartedAudio = false;
        playback.mResetDecoder = true;
        playback.mSeekTime.reset();

        // Decode from the initial IDR through the first displayable picture, then hold it.
        // Wicked also decodes an initial frame independently of its Playing flag:
        // https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/wiVideo.cpp (IsDecodingRequired)
        if ( playback.mPreview ) {
            playback.mPreviewSample = FindPreviewSample( playback.mClip->GetSamples( TrackKind::eVideo ) );
            playback.mNextVideoSample = 0;
            playback.mTimeSeconds = 0;
        }

        if ( !playback.mPreview && playback.mSettings.mAudioEnabled && playback.mClip->GetInfo().mAudio.mCodec != AudioCodec::eNone ) {
            const auto& track{ playback.mClip->GetInfo().mAudio };

            if ( track.mCodec != AudioCodec::eAac ) {
                MKT_THROW_RUNTIME_ERROR( "This video's audio codec is not supported." );
            }

            const auto samples{ playback.mClip->GetSamples( TrackKind::eAudio ) };

            while ( playback.mNextAudioSample + 1 < samples.size() &&
                    samples[playback.mNextAudioSample + 1].mPresentationTime.ToSeconds() < playback.mTimeSeconds ) {
                ++playback.mNextAudioSample;
            }

            playback.mAudio = eastl::make_unique<VideoAudioStream>( *mAudioDevice, track, playback.mSettings.mAudio );
        } else {
            playback.mAudioEnded = true;
        }
    }

    auto VideoService::ScheduleDecode( VideoPlaybackHandle playback ) -> void {
        const auto& state{ *playback };
        const auto videoSamples{ state.mClip->GetSamples( TrackKind::eVideo ) };

        const u64 videoEnd{ state.mPreview ? state.mPreviewSample + 1 : videoSamples.size() };
        const bool videoNeeded{ !state.mVideoEnded && state.mNextVideoSample < videoEnd && state.mPictures.size() < 4 &&
            ( state.mPreview || videoSamples[state.mNextVideoSample].mDecodeTime.ToSeconds() <= state.mTimeSeconds + 0.25 ) };

        const bool audioNeeded{ state.mAudio && !state.mAudioEnded && state.mPendingPcm.empty() && state.mAudio->GetQueuedSeconds() < 0.5 };

        if ( state.mJobPending || ( !videoNeeded && !audioNeeded ) ) {
            return;
        }

        {
            std::lock_guard lock{ mJobsMutex };
            if ( !mAcceptingJobs || mActiveJobs >= 4 ) {
                return;
            }

            ++mActiveJobs;
        }

        playback->mJobPending = true;
        
        auto clip{ state.mClip };
        auto parser{ state.mParser };
        auto audioDecoder{ state.mAudioDecoder };

        CompletedJob job{};
        job.mPlayback = playback;
        job.mEpoch = state.mEpoch;
        job.mNextVideoSample = state.mNextVideoSample;
        job.mNextAudioSample = state.mNextAudioSample;
        job.mAudioEnded = state.mAudioEnded;

        try {
            threading::TaskService::Get()->ExecuteWorkerTask( [this, job = eastl::move( job ), clip, parser, audioDecoder, videoNeeded, videoEnd, audioNeeded]() mutable {
#if defined( MIKOTO_PLATFORM_WINDOWS )
                const auto com{ CoInitializeEx( nullptr, COINIT_MULTITHREADED ) };
#endif
                try {
                    EncodedPacket packet{};

                    for ( u32 count{}; videoNeeded && count < 4 && job.mNextVideoSample < videoEnd &&
                        clip->ReadSample( TrackKind::eVideo, job.mNextVideoSample, packet ); ++count ) {
                        job.mPictures.push_back( parser->ParsePicture( packet ) );
                        ++job.mNextVideoSample;
                    }

                    if ( audioNeeded ) {
                        if ( !audioDecoder ) {
                            audioDecoder = eastl::make_shared<AacDecoder>( clip->GetInfo().mAudio );
                        }

                        for ( u32 count{}; count < 24 && clip->ReadSample( TrackKind::eAudio, job.mNextAudioSample, packet ); ++count ) {
                            auto blocks{ audioDecoder->Decode( packet ) };

                            for ( auto& block: blocks ) {
                                job.mPcm.push_back( eastl::move( block ) );
                            }

                            ++job.mNextAudioSample;
                        }

                        if ( job.mNextAudioSample == clip->GetSamples( TrackKind::eAudio ).size() ) {
                            for ( auto& block: audioDecoder->Drain() ) {
                                job.mPcm.push_back( eastl::move( block ) );
                            }

                            job.mAudioEnded = true;
                        }
                    }

                    job.mAudioDecoder = eastl::move( audioDecoder );
                } catch ( const std::exception& error ) {
                    job.mError = error.what();
                }

#if defined( MIKOTO_PLATFORM_WINDOWS )
                if ( SUCCEEDED( com ) ) {
                    CoUninitialize();
                }
#endif

                std::lock_guard lock{ mJobsMutex };
                mCompletedJobs.push_back( eastl::move( job ) );
                --mActiveJobs;
                mJobsFinished.notify_all();
            } );
        } catch ( ... ) {
            std::lock_guard lock{ mJobsMutex };
            playback->mJobPending = false;
            --mActiveJobs;
            mJobsFinished.notify_all();
            throw;
        }
    }

    auto VideoService::Update( scene::Scene& scene, f64 timeStep ) -> void {
        CollectJobs();

        if ( scene.GetRegistry().view<scene::VideoComponent>().empty() && !mScenes.contains( &scene ) ) {
            return;
        }

        auto& state{ mScenes[&scene] };
        const bool preview{ !scene.IsPlaying() };
        ankerl::unordered_dense::set<u64> present{};

        for ( auto [entity, component, tag]: scene.GetRegistry().view<scene::VideoComponent, scene::TagComponent>().each() ) {
            if ( !tag.IsActive() || !component.GetClip().GetID() ) {
                continue;
            }

            const u64 id{ tag.GetGuid() };
            present.insert( id );
            auto& player{ state.mPlayers[id] };

            if ( !player || player->mAsset.GetRef().GetID() != component.GetClip().GetID() || player->mPreview != preview ) {
                if ( player ) {
                    RemovePlayback( *player );
                }

                player = VideoPlaybackHandle::New( asset::AssetRegistryService::Get()->Request<asset::VideoClip>( component.GetClip() ), component.GetPlaybackSettings() );
                player->mComponentRevision = component.GetRevision();
                player->mOutputMap = component.GetOutputMap();
                player->mPreview = preview;
            }

            auto& playback{ *player };

            if ( playback.mState == PlaybackState::eFailed ) {
                continue;
            }

            try {
                if ( !playback.mClip ) {
                    playback.mClip = playback.mAsset.GetIfReady();

                    if ( !playback.mClip ) {
                        if ( playback.mAsset.GetState() == asset::AssetState::eFailed ) {
                            MKT_THROW_RUNTIME_ERROR( playback.mAsset.GetError() );
                        }

                        continue;
                    }

                    if ( !mDevice->GetQueue( QueueType::eVideoDecode ) ) {
                        MKT_THROW_RUNTIME_ERROR( "This renderer has no H.264 hardware decode queue." );
                    }

                    ResetPlayback( playback, 0 );
                    playback.mState = !preview && playback.mSettings.mPlayOnStart ? PlaybackState::ePlaying : PlaybackState::eStopped;
                }

                if ( playback.mSeekTime ) {
                    ResetPlayback( playback, *playback.mSeekTime );
                }

                if ( playback.mComponentRevision != component.GetRevision() ) {
                    if ( playback.mOutputMap != component.GetOutputMap() && playback.mPlaybackMaterial ) {
                        playback.mPlaybackMaterial.Reset();
                    }

                    playback.mSettings = component.GetPlaybackSettings();
                    playback.mOutputMap = component.GetOutputMap();
                    playback.mComponentRevision = component.GetRevision();

                    // Preview settings reuse the poster instead of restarting the decoder.
                    if ( !preview ) {
                        ResetPlayback( playback, playback.mTimeSeconds );
                    }
                }

                while ( playback.mAudio && !playback.mPendingPcm.empty() && playback.mAudio->Push( playback.mPendingPcm.front() ) ) {
                    playback.mPendingPcm.pop_front();
                }

                if ( playback.mAudio && playback.mAudioEnded && playback.mPendingPcm.empty() ) {
                    playback.mAudio->Finish();
                }

                if ( playback.mState == PlaybackState::ePlaying ) {
                    if ( playback.mAudio ) {
                        auto* sound{ playback.mAudio->GetSource() };
                        const bool drained{ playback.mStartedAudio && playback.mAudioEnded && playback.mPendingPcm.empty() &&
                            playback.mAudio->GetQueuedSeconds() == 0 };

                        // A shorter audio track must not freeze the final video frames or looping.
                        if ( drained ) {
                            playback.mTimeSeconds += timeStep;
                        } else {
                            if ( playback.mStartedAudio || playback.mAudio->GetQueuedSeconds() >= 0.1 || playback.mAudioEnded ) {
                                sound->Play();
                                playback.mStartedAudio = true;
                            }

                            playback.mTimeSeconds = playback.mAudio->GetPlaybackTime().value_or( playback.mTimeSeconds );
                        }
                    } else {
                        playback.mTimeSeconds += timeStep;
                    }
                }

                if ( playback.mAudio ) {
                    if ( const auto* transform{ scene.GetRegistry().try_get<scene::TransformComponent>( entity ) } ) {
                        playback.mAudio->GetSource()->UpdatePosition( float3{ transform->GetWorldTransform()[3] }, static_cast<float>( timeStep ) );
                    }
                }

                // Only rendering sees this clone. Inspector edits, scene saves and Play copies
                // continue to use the authored material and its original texture bindings.
                if ( playback.mCurrentFrame && playback.mOutput && playback.HasPresented( playback.mCurrentFrame->mGeneration ) ) {
                    if ( auto* material{ scene.GetRegistry().try_get<scene::MaterialComponent>( entity ) } ) {
                        const auto source{ material->GetMaterial().template As<material::SurfaceMaterial>() };

                        if ( source && ( !playback.mPlaybackMaterial || source != playback.mOriginalMaterial ||
                            source->GetRevision() != playback.mMaterialRevision ) ) {
                            playback.mOriginalMaterial = source;
                            playback.mMaterialRevision = source->GetRevision();
                            playback.mPlaybackMaterial = material::SurfaceMaterialHandle::New( *source );
                        }

                        if ( playback.mPlaybackMaterial ) {
                            const auto map{ playback.mOutputMap == OutputMap::eEmissive ? material::MapType::eEmissive : material::MapType::eBaseColor };

                            if ( playback.mPlaybackMaterial->GetTexture( map ) != playback.mOutput ) {
                                playback.mPlaybackMaterial->SetTexture( map, playback.mOutput );
                                playback.mPlaybackMaterial->UseTexture( map );
                                ++state.mRevision;
                            }
                        }
                    }
                }

                if ( !preview && playback.mTimeSeconds >= playback.mClip->GetInfo().mDurationSeconds ) {
                    if ( playback.mSettings.mLooping ) {
                        ResetPlayback( playback, 0 );
                    } else {
                        playback.mState = PlaybackState::eEnded;
                    }
                }

                ScheduleDecode( player );
            } catch ( const std::exception& error ) {
                playback.mError = error.what();
                playback.mState = PlaybackState::eFailed;
                playback.mAudio.reset();

                MKT_CORE_LOGGER_ERROR( "Video '{}': {}", component.GetClip().GetPath().GetC_Str(), error.what() );
            }
        }

        for ( auto it{ state.mPlayers.begin() }; it != state.mPlayers.end(); ) {
            if ( !present.contains( it->first ) ) {
                RemovePlayback( *it->second );
                it = state.mPlayers.erase( it );
            } else {
                ++it;
            }
        }
    }

    auto VideoService::PreparePicture( VideoPlayback& playback, Ref<H264Picture> picture, Ref<DecodedVideoFrame> frame ) -> void {
        // Match Wicked's decode-order DPB bookkeeping; presentation still follows MP4 PTS.
        // https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/wiVideo.cpp
        const auto& sps{ picture->GetSps() };
        const auto& slice{ picture->GetSlices().front() };
        const i32 maximumFrameNumber{ 1 << ( sps.log2_max_frame_num_minus4 + 4 ) };

        if ( picture->IsIdr() ) {
            for ( auto& slot: playback.mDpb ) {
                slot.mUsed = false;
            }

            playback.mPreviousPocMsb = playback.mPreviousPocLsb = playback.mPreviousFrameNumber = playback.mFrameNumberOffset = 0;
        }

        i32 poc{};
        i32 pocMsb{};

        if ( sps.pic_order_cnt_type == 2 ) {
            if ( !picture->IsIdr() && playback.mPreviousFrameNumber > slice.frame_num ) {
                playback.mFrameNumberOffset += maximumFrameNumber;
            }

            poc = picture->IsIdr() ? 0 : 2 * ( playback.mFrameNumberOffset + slice.frame_num ) - ( picture->IsReference() ? 0 : 1 );
        } else if ( sps.pic_order_cnt_type == 0 && !slice.delta_pic_order_cnt_bottom ) {
            const i32 maximumPoc{ 1 << ( sps.log2_max_pic_order_cnt_lsb_minus4 + 4 ) };
            pocMsb = playback.mPreviousPocMsb;

            if ( slice.pic_order_cnt_lsb < playback.mPreviousPocLsb && playback.mPreviousPocLsb - slice.pic_order_cnt_lsb >= maximumPoc / 2 ) {
                pocMsb += maximumPoc;
            } else if ( slice.pic_order_cnt_lsb > playback.mPreviousPocLsb && slice.pic_order_cnt_lsb - playback.mPreviousPocLsb > maximumPoc / 2 ) {
                pocMsb -= maximumPoc;
            }

            poc = pocMsb + slice.pic_order_cnt_lsb;
        } else {
            MKT_THROW_RUNTIME_ERROR( "This H.264 picture-order mode is not supported yet." );
        }

        auto free{ playback.mDecoder->UsesCoincidentOutput() ? playback.mDpb.begin() + frame->mSlot :
            eastl::find_if( playback.mDpb.begin(), playback.mDpb.end(), []( const auto& slot ) { return !slot.mUsed; } ) };

        if ( free == playback.mDpb.end() ) {
            MKT_THROW_RUNTIME_ERROR( "H.264 decoded-picture buffer has no free reconstruction slot." );
        }

        const auto slotIndex{ as<u32>( free - playback.mDpb.begin() ) };

        VideoDecodeDescription operation{};
        operation.mDecoder = playback.mDecoder;
        operation.mPicture = picture;
        operation.mOutput = frame->mSurface;
        operation.mReconstruction = free->mPicture;
        operation.mReconstruction.mSlot = slotIndex;
        operation.mReconstruction.mFrameNumber = slice.frame_num;
        operation.mReconstruction.mPictureOrderCount = poc;
        operation.mResetSession = playback.mResetDecoder;

        for ( const auto& slot: playback.mDpb ) {
            if ( slot.mUsed ) {
                operation.mReferences.push_back( slot.mPicture );
            }
        }

        // Apply marking after taking the reference snapshot needed by this picture.
        bool currentLongTerm{ picture->IsIdr() && slice.drpm.long_term_reference_flag };

        bool resetOrder{ false };
        i32 currentLongTermIndex{ 0 };

        if ( picture->IsReference() && !picture->IsIdr() && slice.drpm.adaptive_ref_pic_marking_mode_flag ) {
            for ( u32 index{}; index < h264::MAX_MEMORY_MANAGEMENT_CONTROL_OPS; ++index ) {
                const auto op{ slice.drpm.memory_management_control_operation[index] };

                if ( !op ) {
                    break;
                }

                const i32 shortNumber{ slice.frame_num - slice.drpm.difference_of_pic_nums_minus1[index] - 1 };
                const i32 longIndex{ slice.drpm.long_term_frame_idx[index] };

                for ( auto& slot: playback.mDpb ) {
                    if ( !slot.mUsed ) {
                        continue;
                    }

                    auto& reference{ slot.mPicture };
                    const bool matchingShort{ !reference.mLongTerm &&
                        PictureNumber( reference.mFrameNumber, slice.frame_num, maximumFrameNumber ) == shortNumber };
                    const bool replacedLong{ reference.mLongTerm && static_cast<i32>( reference.mFrameNumber ) == longIndex };

                    if ( ( op == 1 && matchingShort ) ||
                         ( op == 2 && reference.mLongTerm && static_cast<i32>( reference.mFrameNumber ) == slice.drpm.long_term_pic_num[index] ) ||
                         ( ( op == 3 || op == 6 ) && replacedLong ) ||
                         ( op == 4 && reference.mLongTerm && reference.mFrameNumber >= static_cast<u32>( slice.drpm.max_long_term_frame_idx_plus1[index] ) ) || op == 5 ) {
                        slot.mUsed = false;
                    }

                    if ( op == 3 && matchingShort ) {
                        reference.mLongTerm = true;
                        reference.mFrameNumber = longIndex;
                        slot.mUsed = true;
                    }
                }

                if ( op == 5 ) {
                    resetOrder = true;
                } else if ( op == 6 ) {
                    currentLongTerm = true;
                    currentLongTermIndex = longIndex;
                } else if ( op < 1 || op > 6 ) {
                    MKT_THROW_RUNTIME_ERROR( "Invalid H.264 reference marking operation." );
                }
            }
        } else if ( picture->IsReference() && !picture->IsIdr() ) {
            const auto used{ eastl::count_if( playback.mDpb.begin(), playback.mDpb.end(), []( const auto& slot ) { return slot.mUsed; } ) };

            if ( used >= eastl::max( sps.num_ref_frames, 1 ) ) {
                auto oldest{ playback.mDpb.end() };

                for ( auto slot{ playback.mDpb.begin() }; slot != playback.mDpb.end(); ++slot ) {
                    if ( slot->mUsed && !slot->mPicture.mLongTerm &&
                         ( oldest == playback.mDpb.end() || PictureNumber( slot->mPicture.mFrameNumber, slice.frame_num, maximumFrameNumber ) <
                             PictureNumber( oldest->mPicture.mFrameNumber, slice.frame_num, maximumFrameNumber ) ) ) {
                        oldest = slot;
                    }
                }

                if ( oldest == playback.mDpb.end() ) {
                    MKT_THROW_RUNTIME_ERROR( "H.264 sliding window has no short-term reference to retire." );
                }

                oldest->mUsed = false;
            }
        }

        free->mPicture = operation.mReconstruction;
        free->mPicture.mLongTerm = currentLongTerm;
        free->mPicture.mFrameNumber = currentLongTerm ? currentLongTermIndex : ( resetOrder ? 0 : slice.frame_num );
        free->mPicture.mPictureOrderCount = resetOrder ? 0 : poc;
        free->mUsed = picture->IsReference();

        if ( picture->IsReference() ) {
            playback.mPreviousPocMsb = resetOrder ? 0 : pocMsb;
            playback.mPreviousPocLsb = resetOrder ? 0 : slice.pic_order_cnt_lsb;
            playback.mPreviousFrameNumber = resetOrder ? 0 : slice.frame_num;
            playback.mFrameNumberOffset = resetOrder ? 0 : playback.mFrameNumberOffset;
        }

        frame->mPresentationTime = picture->GetPresentationTime();
        frame->mGeneration = ++playback.mGeneration;
        frame->mRecorded = false;
        playback.mDecodeWork.push_back( { eastl::move( operation ), frame } );
        playback.mDisplayQueue.push_back( frame );
        playback.mResetDecoder = false;
    }

    auto VideoService::PrepareDecodeWork( const scene::Scene& scene ) -> void {
        const auto found{ mScenes.find( &scene ) };

        if ( found == mScenes.end() ) {
            return;
        }

        for ( auto& [id, handle]: found->second.mPlayers ) {
            auto& playback{ *handle };

            if ( playback.mState == PlaybackState::eFailed || !playback.mClip ) {
                continue;
            }

            if ( playback.mPreview && playback.mCurrentFrame ) {
                continue;
            }

            try {
                if ( !playback.mDecoder && !playback.mPictures.empty() ) {
                    const VideoDecoderDescription description{ .mConfiguration = playback.mPictures.front() };
                    playback.mDecoder = mDevice->CreateVideoDecoder( description );

                    if ( !playback.mDecoder ) {
                        MKT_THROW_RUNTIME_ERROR( "Cannot create an H.264 hardware decoder for this video." );
                    }

                    playback.mDpb.resize( playback.mDecoder->GetDpbSlotCount() );

                    for ( u32 slot{}; slot < playback.mDpb.size(); ++slot ) {
                        playback.mDpb[slot].mPicture.mSurface = playback.mDecoder->CreateSurface( true );
                        playback.mDpb[slot].mPicture.mSlot = slot;

                        if ( playback.mDecoder->UsesCoincidentOutput() ) {
                            auto frame{ Ref<DecodedVideoFrame>::New() };
                            frame->mSurface = playback.mDpb[slot].mPicture.mSurface;
                            frame->mSlot = slot;
                            playback.mFramePool.push_back( eastl::move( frame ) );
                        }
                    }

                    if ( !playback.mOutput ) {
                        const auto& info{ playback.mClip->GetInfo().mVideo };
                        playback.mOutput = mDevice->CreateTexture( TextureCreateDescription{}
                            .SetName( "Video material output" )
                            .SetWidth( info.mWidth )
                            .SetHeight( info.mHeight )
                            .SetFormat( Format::eRGBA16_FLOAT )
                            .SetUsage( TextureUsageFlagsBits::ShaderResource | TextureUsageFlagsBits::UnorderedAccess ) );
                    }
                }

                while ( playback.mDecoder && !playback.mPictures.empty() && playback.mDecodeWork.size() < 4 ) {
                    auto free{ eastl::find_if( playback.mFramePool.begin(), playback.mFramePool.end(), [&]( const auto& frame ) {
                        // A coincident picture is reusable only after both reference and display users release it.
                        return frame->GetRefCount() == 1 &&
                            ( !playback.mDecoder->UsesCoincidentOutput() || !playback.mDpb[frame->mSlot].mUsed );
                    } ) };

                    if ( free == playback.mFramePool.end() ) {
                        if ( playback.mDecoder->UsesCoincidentOutput() || playback.mFramePool.size() >= 8 ) {
                            break;
                        }

                        auto frame{ Ref<DecodedVideoFrame>::New() };
                        frame->mSurface = playback.mDecoder->CreateSurface( false );
                        playback.mFramePool.push_back( eastl::move( frame ) );
                        free = playback.mFramePool.end() - 1;
                    }

                    PreparePicture( playback, playback.mPictures.front(), *free );
                    playback.mPictures.pop_front();
                }

                // Select by presentation timestamp after
                // preparing every required decode reference.
                Ref<DecodedVideoFrame> selected{};

                for ( auto entry{ playback.mDisplayQueue.begin() }; entry != playback.mDisplayQueue.end(); ) {
                    if ( playback.mPreview ) {
                        const auto samples{ playback.mClip->GetSamples( TrackKind::eVideo ) };

                        if ( ( *entry )->mPresentationTime.ToSeconds() == samples[playback.mPreviewSample].mPresentationTime.ToSeconds() ) {
                            selected = *entry;
                        }

                        entry = playback.mDisplayQueue.erase( entry );
                        continue;
                    }

                    if ( ( *entry )->mPresentationTime.ToSeconds() <= playback.mTimeSeconds ) {
                        if ( !selected || ( *entry )->mPresentationTime.ToSeconds() >= selected->mPresentationTime.ToSeconds() ) {
                            selected = *entry;
                        }

                        entry = playback.mDisplayQueue.erase( entry );
                    } else {
                        ++entry;
                    }
                }

                if ( selected && ( !playback.mCurrentFrame || selected->mPresentationTime.ToSeconds() >= playback.mCurrentFrame->mPresentationTime.ToSeconds() ) &&
                     selected != playback.mCurrentFrame ) {
                    playback.mCurrentFrame = eastl::move( selected );
                    ++found->second.mRevision;
                }
            } catch ( const std::exception& error ) {
                playback.mError = error.what();
                playback.mState = PlaybackState::eFailed;
                playback.mAudio.reset();

                MKT_CORE_LOGGER_ERROR( "Prepare video: {}", error.what() );
            }
        }
    }

    auto VideoService::RecordDecodeWork( const scene::Scene& scene, renderer::CommandContext& context, renderer::FGVideoHandle handle ) -> void {
        for ( auto& playback: GetPlaybacks( scene ) ) {
            for ( auto& work: playback->mDecodeWork ) {
                context.DecodeVideo( handle, work.mDescription );
                work.mFrame->mRecorded = true;
            }

            playback->mDecodeWork.clear();
        }
    }

    auto VideoService::GetPlaybacks( const scene::Scene& scene ) const -> eastl::vector<VideoPlaybackHandle> {
        eastl::vector<VideoPlaybackHandle> players{};
        const auto found{ mScenes.find( &scene ) };

        if ( found != mScenes.end() ) {
            for ( const auto& [id, player]: found->second.mPlayers ) {
                players.push_back( player );
            }
        }

        return players;
    }

    auto VideoService::GetPlayback( const scene::Scene& scene, u64 entity ) const -> VideoPlaybackHandle {
        const auto found{ mScenes.find( &scene ) };

        if ( found == mScenes.end() ) {
            return {};
        }

        const auto player{ found->second.mPlayers.find( entity ) };
        return player != found->second.mPlayers.end() ? player->second : VideoPlaybackHandle{};
    }

    auto VideoService::GetSceneRevision( const scene::Scene& scene ) const -> u64 {
        const auto found{ mScenes.find( &scene ) };
        return found == mScenes.end() ? 0 : found->second.mRevision;
    }

    auto VideoService::GetMaterialOverride( const scene::Scene& scene, u64 entity ) const -> material::MaterialHandle {
        const auto playback{ GetPlayback( scene, entity ) };
        return playback ? playback->mPlaybackMaterial : Ref<material::SurfaceMaterial>{};
    }

    auto VideoService::RemovePlayback( VideoPlayback& playback ) -> void {
        ++playback.mEpoch;
        playback.mAudio.reset();
    }

    auto VideoService::ReleaseScene( scene::Scene& scene ) -> void {
        const auto found{ mScenes.find( &scene ) };

        if ( found == mScenes.end() ) {
            return;
        }

        for ( auto& [id, playback]: found->second.mPlayers ) {
            RemovePlayback( *playback );
        }

        mScenes.erase( found );
    }
}