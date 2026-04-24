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

#ifndef MIKOTO_AUDIO_CLIP_HH
#define MIKOTO_AUDIO_CLIP_HH

#include <miniaudio.h>

#include <EASTL/string.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/ResourcePool.hh>

#include <Filesystem/File.hh>

#include <Audio/AudioSource.hh>
#include <Audio/AudioUtility.hh>
#include <Audio/AudioDeviceObject.hh>

namespace mikoto::audio {

    using namespace mikoto::core;
    using namespace mikoto::filesystem;

    /**
    * @class AudioDeviceObject
    * @brief Represents a resource associated with an audio device.
    *
    * This class serves as a base for audio assets. To reproduce them,
    * we need to create a source and a listener. Audio assets are loaded
    * to the main device which is part of the Audio service.
    */
    class Audio final : public AudioDeviceObject {
    public:
        /**
         * @brief Constructs an Audio object with a given description.
         *
         * @param description The audio description containing source file and volume settings.
         */
        explicit Audio( const AudioLoadDescription& description );

        MKT_NODISCARD auto GetFile() const -> FileHandle;
        MKT_NODISCARD auto CreateSource() -> AudioSourceHandle;
        MKT_NODISCARD auto GetTrackName() const -> eastl::string_view;

    private:
        auto Release() -> void override;
        auto Initialize() -> void override;

    private:
        FileHandle mFileSource{};
        ResourcePoolTyped<AudioSource> m_Sources{};
    };

    using AudioHandle = Ref<Audio>;

}

#endif //MIKOTO_AUDIO_CLIP_HH
