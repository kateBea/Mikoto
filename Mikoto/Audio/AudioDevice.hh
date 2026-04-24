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

#ifndef MIKOTO_AUDIO_DEVICE_HH
#define MIKOTO_AUDIO_DEVICE_HH

#include <miniaudio.h>

#include <EASTL/unique_ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/ResourcePool.hh>

#include <Audio/AudioClip.hh>
#include <Audio/AudioUtility.hh>

namespace mikoto::audio {

    struct AudioDeviceDescription {
        u32 mMaxListeners{ 5 };
    };

    class AudioDevice {
    public:
        explicit AudioDevice(const AudioDeviceDescription& desc);

        auto Init() -> void;
        auto Shutdown() -> void;

        auto LoadAudio( const AudioLoadDescription& description ) -> AudioHandle;

        MKT_NODISCARD static auto Create( const AudioDeviceDescription& description ) -> eastl::unique_ptr<AudioDevice>;

    private:
        friend class Audio;
        friend class AudioListener;
        friend class AudioSource;

    private:
        ma_engine mAudioEngine{};
        ma_engine_config mEngineConfig{};

        ResourcePoolTyped<Audio> mLoadedAudios{};
    };
}

#endif // MIKOTO_AUDIO_DEVICE_HH
