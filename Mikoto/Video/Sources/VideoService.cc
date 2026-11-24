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



#if defined( MIKOTO_PLATFORM_WINDOWS )
#include <mfapi.h>
#endif

namespace mikoto::video {

    // https://wickedengine.net/category/devblog/2023/05/07/vulkan-video-decoding/index.html

    using namespace mikoto::core;

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

    VideoPlayback::VideoPlayback( asset::AssetHandle<asset::VideoClip> clip, const PlaybackSettings& settings, bool preview )
        : mAsset{ eastl::move( clip ) }, mSettings{ settings }, mPreview{ preview } {
    }

    VideoPlayback::~VideoPlayback() {
        if ( auto* service{ VideoService::GetPtr() }; service && mRegistered ) {
            service->UnregisterPlayback( this );
        }
    }

    auto VideoPlayback::SetSettings( const PlaybackSettings& settings ) -> void {
        mSettings = settings;

        if ( mClip && !mPreview ) {
            mSeekTime = mTimeSeconds;
        }
    }

    auto VideoPlayback::SetPosition( float3 position ) -> void {
        mPosition = position;
    }

    auto VideoPlayback::SetFailure( eastl::string_view error ) -> void {
        mError.assign( error.data(), error.size() );
        mState = PlaybackState::eFailed;
        mAudio.reset();
    }

    auto VideoPlayback::ConsumePicture() -> void {
        mPictures.pop_front();
    }

    auto VideoPlayback::GetPicture() const -> Ref<H264Picture> {
        return mPictures.empty() ? Ref<H264Picture>{} : mPictures.front();
    }

    auto VideoPlayback::GetEpoch() const -> u64 {
        return mEpoch;
    }

    auto VideoPlayback::GetPreviewSample() const -> u64 {
        return mPreviewSample;
    }

    auto VideoPlayback::IsPreview() const -> bool {
        return mPreview;
    }

    auto VideoPlayback::IsRegistered() const -> bool {
        return mRegistered;
    }

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

    auto VideoPlayback::GetClip() const -> Ref<asset::VideoClip> {
        return mClip;
    }


    auto VideoService::Initialize() -> void {
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

        for ( auto* playback: mPlaybacks ) {
            playback->mRegistered = false;
            ++playback->mEpoch;
            playback->mJobPending = false;
            playback->mAudio.reset();
        }

        mPlaybacks.clear();

#if defined( MIKOTO_PLATFORM_WINDOWS )
        if ( mMediaFoundationStarted ) {
            MFShutdown();
            mMediaFoundationStarted = false;
        }
#endif

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
        playback.mAudio.reset();
        playback.mAudioDecoder.reset();
        playback.mParser = eastl::make_shared<H264Parser>( playback.mClip->GetInfo().mVideo );
        playback.mTimeSeconds = eastl::min( seconds, playback.mClip->GetInfo().mDurationSeconds );
        playback.mNextVideoSample = playback.mClip->FindRandomAccessSample( playback.mTimeSeconds );
        playback.mNextAudioSample = 0;
        playback.mVideoEnded = playback.mAudioEnded = playback.mStartedAudio = false;
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

    auto VideoService::RegisterPlayback( const asset::AssetRef<asset::VideoClip>& clip,
        const PlaybackSettings& settings, bool preview ) -> VideoPlaybackHandle {
        auto playback{ VideoPlaybackHandle::New( asset::AssetRegistryService::Get()->Request<asset::VideoClip>( clip ), settings, preview ) };
        mPlaybacks.insert( playback.GetPtr() );
        playback->mRegistered = true;
        return playback;
    }

    auto VideoService::UnregisterPlayback( VideoPlayback* playback ) -> void {
        if ( !mPlaybacks.erase( playback ) ) {
            return;
        }

        playback->mRegistered = false;
        ++playback->mEpoch;
        playback->mJobPending = false;
        playback->mAudio.reset();
        playback->mState = PlaybackState::eStopped;
    }

    auto VideoService::Update() -> void {
        CollectJobs();
    }

    auto VideoService::Update( VideoPlaybackHandle player, f64 timeStep ) -> void {
        if ( !player || !player->mRegistered || player->mState == PlaybackState::eFailed ) {
            return;
        }

        // Each owner advances its media clock once, independently of how many views consume it.
        auto& playback{ *player.GetPtr() };
        const bool preview{ playback.mPreview };

        try {
            if ( !playback.mClip ) {
                playback.mClip = playback.mAsset.GetIfReady();

                if ( !playback.mClip ) {
                    if ( playback.mAsset.GetState() == asset::AssetState::eFailed ) {
                        MKT_THROW_RUNTIME_ERROR( playback.mAsset.GetError() );
                    }

                    return;
                }

                ResetPlayback( playback, 0 );
                playback.mState = !preview && playback.mSettings.mPlayOnStart ? PlaybackState::ePlaying : PlaybackState::eStopped;
            }

            if ( playback.mSeekTime ) {
                ResetPlayback( playback, *playback.mSeekTime );
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
                playback.mAudio->GetSource()->UpdatePosition( playback.mPosition, static_cast<float>( timeStep ) );
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
            playback.SetFailure( error.what() );
            MKT_CORE_LOGGER_ERROR( "Video playback: {}", error.what() );
        }
    }
}