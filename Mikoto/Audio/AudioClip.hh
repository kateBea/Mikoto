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

#ifndef MIKOTO_AUDIO_AUDIO_CLIP_HH
#define MIKOTO_AUDIO_AUDIO_CLIP_HH

#include <EASTL/string.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/Resource.hh>

#include <Filesystem/File.hh>

#include <Audio/Utility.hh>
#include <Audio/AudioSource.hh>
#include <Audio/AudioDeviceObject.hh>

namespace mikoto::audio {

    /**
     * @brief Retains an audio file and default gain associated with an audio device.
     *
     * Create an AudioSource to decode and play the file. Loading the clip itself
     * does not decode audio. Clips are registered with their owning AudioDevice.
     */
    class Audio final : public AudioDeviceObject {
    public:
        /**
         * @brief Stores a source file handle and sanitized default gain.
         *
         * @param file Encoded audio file to retain; no decoding is performed here.
         * @param volume Default linear gain, clamped to [0,20].
         */
        explicit Audio( filesystem::FileHandle file, core::f32 volume );


        /**
         * @brief Gets the retained source file.
         *
         * @return Shared file handle, possibly empty.
         */
        MKT_NODISCARD auto GetFile() const -> filesystem::FileHandle;


        /**
         * @brief Creates an independent playback source with this clip's default gain.
         *
         * @return Ready source, or empty if the clip/device/file is unavailable or source
         * initialization fails.
         */
        MKT_NODISCARD auto CreateSource() const -> AudioSourceHandle;


        /**
         * @brief Gets the filename used as the track name.
         *
         * @return Borrowed filename including extension, or an empty view without a source file.
         */
        MKT_NODISCARD auto GetTrackName() const -> eastl::string_view;


        /**
         * @brief Unregisters the clip if allocated.
         */
        ~Audio() override;

    private:
        /**
         * @brief Unregisters the clip and clears its device association.
         */
        auto Destroy() -> void override;


        /**
         * @brief Registers the clip with its initialized device without decoding audio.
         */
        auto Initialize() -> void override;

    private:
        friend class AudioDevice;

    private:
        core::f32 mDefaultVolume{ 1.0f };
        filesystem::FileHandle mFileSource{};
    };

    using AudioHandle = core::Ref<Audio>;

}

#endif// MIKOTO_AUDIO_AUDIO_CLIP_HH