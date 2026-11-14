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

#ifndef AUDIO_AUDIO_MIXER
#define AUDIO_AUDIO_MIXER

#include <EASTL/unique_ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Service.hh>

namespace mikoto::audio {

    class AudioDevice;
    class AudioSource;

    class AudioMixer final : public core::IService {
    public:
        explicit AudioMixer( AudioDevice* device );

        auto Initialize() -> void override;
        auto Shutdown() -> void override;

        auto SetVolume( core::f32 volume, bool muted ) -> void;
        auto RouteGroup( core::u64 id, core::u64 output ) -> bool;
        auto RemoveGroup( core::u64 id ) -> void;

        auto BeginUpdate() -> void;
        auto EndUpdate() -> void;

        MKT_NODISCARD auto UpdateGroup( core::u64 id, core::f32 volume, bool muted ) -> bool;

        ~AudioMixer() override;

    private:
        friend class AudioSource;
        struct Impl;

        eastl::unique_ptr<Impl> mImpl{};
        AudioDevice* mDevice{};
    };
}// namespace mikoto::audio

#endif
