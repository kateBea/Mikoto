//    Copyright 2025 ケイト
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

#ifndef MIKOTO_AUDIO_UTILITY_HH
#define MIKOTO_AUDIO_UTILITY_HH

#include <Library/IO/File.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {

    /**
    * @brief Defines the properties of an audio resource.
    *
    * This structure encapsulates details such as the source file and volume level
    * for an audio resource.
    */
    struct AudioLoadDescription {
        const File* AudioFile{}; ///< The file path to the audio source.
        float Volume{ 1.4f };///< The default volume level.

        /**
         * @brief Sets the audio source file.
         *
         * @param source The path to the audio file.
         * @return Reference to the modified AudioDescription.
         */
        auto WithFile( const File* source ) -> AudioLoadDescription&;

        /**
         * @brief Sets the volume level.
         *
         * @param volume The volume value (range typically 0.0 to 1.0).
         * @return Reference to the modified AudioDescription.
         */
        auto SetVolume( float volume ) -> AudioLoadDescription&;
    };
}
#endif //MIKOTO_AUDIO_UTILITY_HH
