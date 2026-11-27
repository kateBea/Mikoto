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

#ifndef MIKOTO_VIDEO_SERVICE_HH
#define MIKOTO_VIDEO_SERVICE_HH

#include <condition_variable>
#include <mutex>

#include <ankerl/unordered_dense.h>

#include <EASTL/deque.h>
#include <EASTL/shared_ptr.h>

#include <Assets/AssetVideo.hh>

#include <Core/Service.hh>
#include <Core/Singleton.hh>

#include <Video/AacDecoder.hh>
#include <Video/H264Parser.hh>
#include <Video/VideoAudioStream.hh>

namespace mikoto::video {

    // A playback owns media cursors and audio. Consumers take parsed pictures in decode order.
    class VideoPlayback final : public core::ReferenceCounted<VideoPlayback> {
    public:
        VideoPlayback( asset::AssetHandle<asset::VideoClip> clip, const PlaybackSettings &settings, bool preview );
        ~VideoPlayback() override;

        auto SetSettings( const PlaybackSettings &settings ) -> void;
        auto SetPosition( core::float3 position ) -> void;
        auto SetFailure( eastl::string_view error ) -> void;

        auto Play() -> void;
        auto Stop() -> void;
        auto Pause() -> void;
        auto Seek( core::f64 seconds ) -> void;
        auto ConsumePicture() -> void;

        MKT_NODISCARD auto GetState() const -> PlaybackState;
        MKT_NODISCARD auto GetTimeSeconds() const -> core::f64;
        MKT_NODISCARD auto GetError() const -> const eastl::string &;
        MKT_NODISCARD auto GetClip() const -> core::Ref<asset::VideoClip>;
        MKT_NODISCARD auto GetPicture() const -> core::Ref<H264Picture>;
        MKT_NODISCARD auto GetEpoch() const -> core::u64;
        MKT_NODISCARD auto GetPreviewSample() const -> core::u64;
        /** Editor ownership stays stable while Inspector controls play or seek the clip. */
        MKT_NODISCARD auto IsPreview() const -> bool;
        /** Whether decoding/presentation should stop after the initial displayable frame. */
        MKT_NODISCARD auto IsPosterFrameOnly() const -> bool;
        MKT_NODISCARD auto IsRegistered() const -> bool;

    private:
        friend class VideoService;

        core::Ref<asset::VideoClip> mClip{};
        asset::AssetHandle<asset::VideoClip> mAsset{};
        PlaybackSettings mSettings{};
        PlaybackState mState{ PlaybackState::eLoading };
        eastl::string mError{};
        eastl::shared_ptr<H264Parser> mParser{};
        eastl::unique_ptr<VideoAudioStream> mAudio{};
        eastl::shared_ptr<AacDecoder> mAudioDecoder{};
        eastl::deque<core::Ref<H264Picture>> mPictures{};
        eastl::deque<PcmBlock> mPendingPcm{};
        core::float3 mPosition{};
        core::u64 mEpoch{};
        core::f64 mTimeSeconds{};
        core::u64 mPreviewSample{};
        core::u64 mNextVideoSample{};
        core::u64 mNextAudioSample{};
        eastl::optional<core::f64> mSeekTime{};
        bool mPreview{};
        bool mPosterFrameOnly{};
        bool mRegistered{};
        bool mJobPending{};
        bool mVideoEnded{};
        bool mAudioEnded{};
        bool mStartedAudio{};
    };

    using VideoPlaybackHandle = core::Ref<VideoPlayback>;

    class VideoService final : public core::IService, public core::Singleton<VideoService> {
    public:
        VideoService() = default;
        ~VideoService() override;

        auto Initialize() -> void override;
        auto Shutdown() -> void override;

        /** Registers an independent media cursor; the caller owns the returned playback. */
        MKT_NODISCARD auto RegisterPlayback( const asset::AssetRef<asset::VideoClip> &clip,
            const PlaybackSettings &settings, bool preview ) -> VideoPlaybackHandle;
        auto UnregisterPlayback( VideoPlayback *playback ) -> void;
        auto Update() -> void;
        auto Update( VideoPlaybackHandle playback, core::f64 timeStep ) -> void;

    private:
        struct CompletedJob {
            core::u64 mEpoch{};
            core::u64 mNextVideoSample{};
            core::u64 mNextAudioSample{};
            VideoPlaybackHandle mPlayback{};
            eastl::vector<PcmBlock> mPcm{};
            eastl::vector<core::Ref<H264Picture>> mPictures{};
            eastl::string mError{};
            eastl::shared_ptr<AacDecoder> mAudioDecoder{};
            bool mAudioEnded{};
        };

        auto CollectJobs() -> void;
        auto ScheduleDecode( VideoPlaybackHandle playback ) -> void;
        auto ResetPlayback( VideoPlayback &playback, core::f64 seconds ) -> void;

        audio::AudioDevice *mAudioDevice{};
        std::mutex mJobsMutex{};
        core::u32 mActiveJobs{};
        std::condition_variable mJobsFinished{};
        eastl::vector<CompletedJob> mCompletedJobs{};
        ankerl::unordered_dense::set<VideoPlayback *> mPlaybacks{};
        bool mAcceptingJobs{};
        bool mMediaFoundationStarted{};
    };
}// namespace mikoto::video

#endif
