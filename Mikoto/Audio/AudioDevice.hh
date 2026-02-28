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

#include <Assets/AudioClip.hh>
#include <Audio/AudioUtility.hh>
#include <Library/Data/ResourcePool.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {

    /**
     * @brief Represents the configuration and description of an audio device.
     *
     * This struct holds various properties that define the configuration of an audio device,
     * such as sample rate, format, and any other settings required to initialize the audio system.
     */
    struct AudioDeviceDescription {
        UInt32 MaxListenersCount{ 5 };
    };

    /**
    * @brief Represents an audio device that manages the audio engine, device configuration,
    *        and handles loading and accessing audio resources.
    *
    * This class is responsible for initializing and shutting down the audio system,
    * as well as loading and accessing audio resources (such as sounds and music) using
    * a resource pool. It interacts with the underlying audio engine and device for playback.
    */
    class AudioDevice {
    public:
        explicit AudioDevice(const AudioDeviceDescription& desc);

        /**
        * @brief Initializes the audio device and its engine.
        *
        * This function sets up the audio engine and device, preparing the system to handle
        * audio playback. It must be called before any audio operations can take place.
        */
        auto Init() -> void;

        /**
        * @brief Shuts down the audio device and releases any resources.
        *
        * This function stops the audio engine, frees any allocated resources, and properly
        * shuts down the device to ensure the system is cleanly deinitialized.
        */
        auto Shutdown() -> void;

        /**
        * @brief Loads an audio resource based on the provided description.
        *
        * This function loads an audio file into memory based on the given `AudioDescription`
        * (which includes the file path and other parameters like volume). It returns a handle
        * to the loaded audio resource.
        *
        * @param description The `AudioDescription` that contains the source file and properties for the audio.
        * @return A handle to the loaded audio resource.
        */
        auto LoadAudio( const AudioLoadDescription& description ) -> AudioHandle;

        /**
        * @brief Retrieves an audio that might have been loaded before
        *
        * Returns an audio that was cached. If the audio does not exist, it returns an empty handle.
        * @param uri Uri to the cached audio.
        * @return Handle to the cached audio.
        */
        auto GetAudio(const std::string& uri) -> AudioHandle;


        MKT_NODISCARD static auto Create( const AudioDeviceDescription& description ) -> Unique<AudioDevice>;

    private:
        friend class Audio;
        friend class AudioListener;
        friend class AudioSource;

    private:
        ma_engine m_AudioEngine{};
        ma_engine_config m_EngineConfig{};

        ResourcePoolTyped<Audio> m_LoadedAudios{};

        // Is this needed here, or should it be in the AssetsService?
        ankerl::unordered_dense::map<std::string, Handle> m_CachedAudios{};
    };
}

#endif // MIKOTO_AUDIO_DEVICE_HH
