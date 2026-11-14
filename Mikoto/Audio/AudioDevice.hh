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

#ifndef MIKOTO_AUDIO_AUDIO_DEVICE_HH
#define MIKOTO_AUDIO_AUDIO_DEVICE_HH

#include <EASTL/unique_ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/Resource.hh>

#include <Audio/Utility.hh>
#include <Audio/AudioClip.hh>

namespace mikoto::audio {

    /**
     * @brief Parameters controlling creation of an AudioDevice.
     */
    struct AudioDeviceDescription {
        /**
         * @brief Requested listener capacity, clamped to the backend's supported limits.
         *
         * The closest enabled listener hears each spatialized source unless explicitly pinned.
         */
        core::u32 mMaxListeners{ 4 };

        /**
         * @brief Offline mixing/tests: do not open hardware or start an audio thread.
         */
        bool mNoDevice{};
    };

    /**
     * @brief Owns audio playback and the shared headphone spatialization data.
     *
     * miniaudio decodes, mixes and sends stereo samples to the device. Steam
     * Audio spatializes 3D sources using HRTF; 2D sources preserve their stereo
     * channels. Native resources stay private so clients need neither SDK.
     * Sources are invalidated before the shared backend is shut down.
     */
    class AudioDevice final : core::INonCopyable {
    public:
        /**
         * @brief Stores configuration and prepares an uninitialized stereo device.
         *
         * @param desc Listener capacity and offline mode. Listener count is clamped to backend
         * limits.
         */
        explicit AudioDevice( const AudioDeviceDescription& desc );


        /**
         * @brief Initializes the mixer and shared Steam Audio HRTF resources.
         *
         * @note Repeated calls are a no-op. Offline mode creates no hardware playback thread.
         * @throws core::RuntimeException If mixer or HRTF initialization fails.
         */
        auto Initialize() -> void;


        /**
         * @brief Invalidates registered sources and clips before releasing shared backend state.
         *
         * @note Repeated calls are a no-op. Outstanding handles may remain, but their resources are
         * no longer ready.
         */
        auto Shutdown() -> void;


        /**
         * @brief Creates and registers a clip referencing an encoded audio file.
         *
         * @param file Encoded audio file to retain.
         * @param volume Default linear source gain, clamped to [0,20].
         * @return Clip handle, or empty if the device is uninitialized or the file is missing.
         * @note Decoding is performed when a playback source is created.
         */
        MKT_NODISCARD auto LoadAudio( filesystem::FileHandle file, core::f32 volume ) -> AudioHandle;


        /**
         * @brief Creates and initializes an independent playback source.
         *
         * @param path Existing audio file to decode.
         * @return Ready stopped source, or empty for an uninitialized device, invalid file, or
         * initialization failure.
         */
        MKT_NODISCARD auto CreateSource( const filesystem::Path &path ) -> AudioSourceHandle;


        /**
         * @brief Creates and initializes playback from a borrowed miniaudio data source.
         *
         * @param source Initialized, caller-owned data source that must outlive the playback source.
         * @return Ready stopped source, or empty for an uninitialized device, null source, or
         * initialization failure.
         */
        MKT_NODISCARD auto CreateSource( ma_data_source_base *source ) -> AudioSourceHandle;


        /**
         * @brief Applies sanitized master gain when initialized.
         *
         * @param volume Linear gain clamped to [0,20].
         */
        auto SetMasterVolume( float volume ) -> void;


        /**
         * @brief Mixes audio synchronously into a caller-owned stereo buffer.
         *
         * @param output Writable interleaved float buffer with space for at least frames * 2
         * samples.
         * @param frames Number of stereo frames to produce.
         * @return True on success; false for null output, uninitialized/non-offline devices, or
         * backend failure.
         */
        MKT_NODISCARD auto RenderOffline( float *output, core::u64 frames ) -> bool;


        /**
         * @brief Checks backend initialization state.
         *
         * @return True after successful Init() until Shutdown().
         */
        MKT_NODISCARD auto IsInitialized() const -> bool;


        /**
         * @brief Queries the initialized backend's listener capacity.
         *
         * @return Actual listener slot count, or zero before initialization/after shutdown.
         */
        MKT_NODISCARD auto GetListenerCount() const -> core::u32;


        /**
         * @brief Queries master output gain.
         *
         * @return Linear gain, or zero when uninitialized.
         */
        MKT_NODISCARD auto GetMasterVolume() -> float;


        /**
         * @brief Calls Shutdown() before destroying backend storage.
         * Defined for pimpl purposes
         */
        ~AudioDevice();


        /**
         * @brief Constructs a device with exclusive EASTL ownership.
         *
         * @param description Device configuration.
         * @return Owning pointer to an uninitialized device; call Init() before use.
         * Construction/allocation errors propagate.
         */
        MKT_NODISCARD static auto Create( const AudioDeviceDescription& description ) -> eastl::unique_ptr<AudioDevice>;

    private:
        friend class Audio;
        friend class AudioListener;
        friend class AudioSource;
        friend class AudioMixer;

    private:
        struct Impl;
        eastl::unique_ptr<Impl> mImpl;
        bool mInitialized{};
    };
}// namespace mikoto::audio

#endif// MIKOTO_AUDIO_AUDIO_DEVICE_HH
