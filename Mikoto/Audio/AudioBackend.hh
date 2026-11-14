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

#ifndef MIKOTO_AUDIO_AUDIO_BACKEND_HH
#define MIKOTO_AUDIO_AUDIO_BACKEND_HH

#include <mutex>

#include <EASTL/array.h>
#include <EASTL/atomic.h>
#include <EASTL/vector.h>

#include <ankerl/unordered_dense.h>

#include <miniaudio.h>

#include <phonon.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Audio/AudioMixer.hh>
#include <Audio/AudioDevice.hh>
#include <Audio/AudioSource.hh>
#include <Audio/AudioSettings.hh>

namespace mikoto::audio {

    // https://miniaud.io/docs/manual/index.html
    // https://miniaud.io/docs/examples/engine_advanced.html

    inline constexpr ma_uint32 kAudioBlockFrames{ 256 };

    // Private graph adapter. Each voice owns its convolution
    // history, only the immutable HRTF dataset is shared across voices.
    struct SteamAudioNode {
        ma_node_base mNode{};
        ma_sound* mSound{};

        // Steam Audio resources.
        IPLContext mContext{};
        IPLHRTF mHrtf{};
        IPLBinauralEffect mEffect{};

        // Audio-thread processing buffers and position.
        eastl::array<float, kAudioBlockFrames> mInput{};
        eastl::array<eastl::array<float, kAudioBlockFrames>, 2> mOutput{};

        ma_uint32 mCursor{};
        std::mutex mSettingsMutex;
        SourceSettings mPendingSettings{}, mSettings{};

        eastl::array<eastl::array<float, kAudioBlockFrames>, 2> mDry{};
        float mSampleRate{ 48000 };
        eastl::array<float, 2> mLowPassHistory{};

        // Four damped combs followed by two all-pass diffusers per channel.
        eastl::array<eastl::array<eastl::vector<float>, 6>, 2> mReverb{};
        eastl::array<eastl::array<core::usize, 6>, 2> mReverbCursor{};
        eastl::array<eastl::array<float, 4>, 2> mReverbDamping{};

        // Cross-thread reset requests, consumed by the audio thread.
        eastl::atomic<uint32_t> mResetRevision{};
        ma_uint32 mAppliedReset{};

        // Processing and lifecycle state.
        bool mWasSpatialized{};
        bool mInitialized{};

        // Lifecycle.
        MKT_NODISCARD auto Initialize(
            ma_engine& engine, ma_sound& sound,
            IPLContext context, IPLHRTF hrtf ) -> bool;

        auto Shutdown() -> void;

        // Requests a history reset for the audio thread to apply.
        auto RequestReset() -> void;
    };

    struct AudioDevice::Impl {
        ma_engine mAudioEngine{};
        ma_engine_config mEngineConfig{};

        IPLContext mContext{};
        IPLHRTF mHrtf{};

        // Loading and shutdown can recursively destroy registered resources.
        // This lock is never acquired by the audio processing callback.
        std::recursive_mutex mResourceMutex{};
        eastl::vector<AudioSource*> mLiveSources{};
        eastl::vector<Audio*> mLiveClips{};
        eastl::vector<AudioMixer*> mLiveMixers{};
    };

    struct AudioSource::Impl {
        ma_sound mSound{};
        SteamAudioNode mBinaural{};
    };

    struct AudioMixer::Impl {
        struct Group {
            ma_sound_group mNode{};
            core::u64 mOutput{};
            bool mUsed{};
        };
        ma_sound_group mMaster{};
        ankerl::unordered_dense::map<core::u64, eastl::unique_ptr<Group>> mGroups{};
    };

}// namespace mikoto::audio

#endif