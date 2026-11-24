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

#ifndef MIKOTO_VIDEO_TYPES_HH
#define MIKOTO_VIDEO_TYPES_HH

#include <EASTL/vector.h>
#include <EASTL/optional.h>

#include <Core/Types.hh>

#include <Audio/AudioSettings.hh>

namespace mikoto::video {

    struct Timestamp {
        core::i64 mTicks{};
        core::u32 mTimeScale{ 1 };

        auto ToSeconds() const -> core::f64;
    };

    enum class TrackKind : core::u32 { eVideo, eAudio };
    enum class AudioCodec : core::u32 { eNone, eAac, eUnsupported };
    enum class PlaybackState : core::u32 { eLoading, eStopped, ePlaying, ePaused, eEnded, eFailed };

    struct VideoTrackInfo {
        core::u32 mTrackIndex{};
        core::u32 mWidth{};
        core::u32 mHeight{};
        core::u32 mCodedWidth{};
        core::u32 mCodedHeight{};
        core::u32 mCropLeft{};
        core::u32 mCropTop{};
        core::u32 mTransferCharacteristics{ 1 };
        core::u32 mMatrixCoefficients{ 1 };
        bool mFullRange{};
        core::float2 mChromaOffset{};
        core::u32 mTimeScale{};
        core::u32 mNalLengthBytes{};

        eastl::vector<eastl::vector<core::ubyte>> mSequenceParameterSets{};
        eastl::vector<eastl::vector<core::ubyte>> mPictureParameterSets{};
    };

    struct AudioTrackInfo {
        AudioCodec mCodec{ AudioCodec::eNone };
        core::u32 mTrackIndex{};
        core::u32 mSampleRate{};
        core::u32 mChannels{};
        core::u32 mTimeScale{};
        eastl::vector<core::ubyte> mCodecConfiguration{};
    };

    struct ClipInfo {
        VideoTrackInfo mVideo{};
        AudioTrackInfo mAudio{};
        core::f64 mDurationSeconds{};
    };

    struct SampleInfo {
        core::u64 mOffset{};
        core::u32 mSize{};
        Timestamp mDecodeTime{};
        Timestamp mPresentationTime{};
        Timestamp mDuration{};
        bool mRandomAccess{};
    };

    struct EncodedPacket {
        TrackKind mTrack{ TrackKind::eVideo };
        core::u64 mSampleIndex{};
        Timestamp mDecodeTime{};
        Timestamp mPresentationTime{};
        Timestamp mDuration{};
        eastl::vector<core::ubyte> mBytes{};
        bool mRandomAccess{};
    };

    struct PlaybackSettings {
        bool mPlayOnStart{ true };
        bool mLooping{};
        bool mAudioEnabled{ true };
        audio::SourceSettings mAudio{};
    };
}

#endif
