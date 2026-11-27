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

#ifndef MIKOTO_VIDEO_AUDIO_STREAM_HH
#define MIKOTO_VIDEO_AUDIO_STREAM_HH

#include <atomic>

#include <miniaudio.h>

#include <Audio/AudioSource.hh>

#include <Video/Types.hh>

namespace mikoto::video {

    struct PcmBlock {
        Timestamp mPresentationTime{};
        core::u32 mSampleRate{};
        core::u32 mChannels{};
        eastl::vector<float> mSamples{};
    };

    class VideoAudioStream;

    struct VideoPcmDataSource {
        ma_data_source_base mBase{};
        VideoAudioStream *mStream{};
    };

    class VideoAudioStream final : public core::INonCopyable {
    public:
        VideoAudioStream( audio::AudioDevice &device, const AudioTrackInfo &track, const audio::SourceSettings &settings );
        ~VideoAudioStream();

        /** Updates gain/spatial processing without replacing PCM, playback state or its clock. */
        auto SetSettings( const audio::SourceSettings &settings ) -> void;

        auto Push( const PcmBlock &block ) -> bool;
        auto Finish() -> void;
        MKT_NODISCARD auto GetPlaybackTime() const -> eastl::optional<core::f64>;
        MKT_NODISCARD auto GetQueuedSeconds() -> core::f64;
        MKT_NODISCARD auto GetSource() -> audio::AudioSource *;

        // Called by miniaudio; never decode, allocate or wait on the audio thread.
        auto ReadFrames( void *output, core::u64 count, core::u64 &read ) -> ma_result;
        MKT_NODISCARD auto GetSampleRate() const -> core::u32;
        MKT_NODISCARD auto GetChannels() const -> core::u32;
        MKT_NODISCARD auto GetConsumedFrames() const -> core::u64;

    private:
        VideoPcmDataSource mDataSource{};
        ma_pcm_rb mBuffer{};
        audio::AudioSourceHandle mSource{};
        core::u32 mSampleRate{};
        core::u32 mChannels{};
        std::atomic<core::u64> mConsumedFrames{};
        std::atomic<core::f64> mStartTime{};
        std::atomic<bool> mAnchored{};
        std::atomic<bool> mFinished{};
        bool mBufferInitialized{};
        bool mDataSourceInitialized{};
    };
}// namespace mikoto::video

#endif
