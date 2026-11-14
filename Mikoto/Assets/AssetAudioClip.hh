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

#ifndef MIKOTO_ASSETS_ASSET_AUDIO_CLIP_HH
#define MIKOTO_ASSETS_ASSET_AUDIO_CLIP_HH

#include <Assets/Asset.hh>

#include <Audio/AudioClip.hh>
#include <Audio/AudioSource.hh>

namespace mikoto::asset {

    struct AudioLoadDescription {
        float mVolume{ 1.4f };
        filesystem::Path mPath{};
        filesystem::FileHandle mFile{};

        LoadPriority mPriority{ LoadPriority::eNormal };

        auto SetFile( filesystem::FileHandle source ) -> AudioLoadDescription&;
        auto SetVolume( core::f32 volume ) -> AudioLoadDescription&;

    };

    class AudioClip final : public core::ReferenceCounted<AudioClip> {
    public:
        explicit AudioClip( core::Ref<audio::Audio> resource );

        MKT_NODISCARD auto GetResource() const -> core::Ref<audio::Audio>;
        MKT_NODISCARD auto GetFile() const -> filesystem::FileHandle;
        MKT_NODISCARD auto GetTrackName() const -> eastl::string_view;
        MKT_NODISCARD auto CreateSource() const -> audio::AudioSourceHandle;
        MKT_NODISCARD auto CanEvict() const -> bool;
        MKT_NODISCARD auto GetResidentMemoryUsage() const -> AssetMemoryUsage;

        ~AudioClip() override;
    private:
        core::Ref<audio::Audio> mResource{};
    };

    template<>
    struct AssetTraits<AudioClip> {
        using Asset = AudioClip;
        using Description = AudioLoadDescription;

        static constexpr AssetType kType{ AssetType::eAudio };
    };

    template<>
    struct AssetTraits<audio::Audio> : public AssetTraits<AudioClip> {

    };
}// namespace mikoto::asset

#endif// MIKOTO_ASSETS_ASSET_AUDIO_CLIP_HH