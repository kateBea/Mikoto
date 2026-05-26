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

#include <Filesystem/Path.hh>
#include <Filesystem/File.hh>

namespace mikoto::audio {

    using namespace mikoto::filesystem;

    struct AudioLoadDescription {
        FileHandle mFile{};
        float mVolume{ 1.4f };

        auto SetFile( FileHandle source ) -> AudioLoadDescription&;
        auto SetVolume( float volume ) -> AudioLoadDescription&;
    };

}
#endif //MIKOTO_AUDIO_UTILITY_HH
