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

#ifndef MIKOTO_AUDIO_SOURCE_HH
#define MIKOTO_AUDIO_SOURCE_HH

#include <miniaudio.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/ResourcePool.hh>

#include <Filesystem/Path.hh>

namespace mikoto::audio {

    using namespace mikoto::core;
    using namespace mikoto::filesystem;

    class AudioDevice;
    class AudioListener;

    class AudioSource final : public IResource {
    public:

        explicit AudioSource( AudioDevice* device, const Path& path );

        auto Mute( bool value ) -> void;
        auto SetVolume( float volume ) -> void;
        auto IncreaseVolume( float delta ) -> void;
        auto DecreaseVolume( float delta ) -> void;
        auto SetLooping( bool value ) -> void;

        // By default sounds will be spatialized based on the closest listener. If a sound should always be 
        // spatialized relative to a specific listener we call this method
        auto SetListener( AudioListener* listener) -> void;

        // Back to default behaviour
        auto ResetListener() -> void;


        auto Play() -> void;
        auto Pause() -> void;
        auto Continue() -> void;
        auto Stop() -> void;

        auto SetPitch( float pitch ) -> void;
        auto SetSpatialization( bool state ) -> void;
        auto SetPosition( const float3& pos ) -> void;
        auto SetPosition( float x, float y, float z ) -> void;

        auto SetDopplerFactor(float value) -> float;

        MKT_NODISCARD auto GetVolume() const -> float;
        MKT_NODISCARD auto IsMuted() const -> bool;
        MKT_NODISCARD auto IsLooping() const -> bool;

        MKT_NODISCARD auto IsPlaying() const -> bool;
        MKT_NODISCARD auto GetPitch() const -> float;

        MKT_NODISCARD auto IsSpatialized() const -> bool;

        // audio duration in seconds
        MKT_NODISCARD auto GetAudioDuration() const -> float;

        // current progress in seconds
        MKT_NODISCARD auto GetCurrentProgress() const -> float;

        MKT_NODISCARD auto IsSameSource( const AudioSource* source ) const -> bool;
        MKT_NODISCARD auto IsSameAudio( const AudioSource* source ) const -> bool;

        MKT_NODISCARD auto GetDopplerFactor() const -> float;
        MKT_NODISCARD static auto GetMaxVolume() -> float;

    private:

        auto Initialize() -> void override;
        auto Release() -> void override;

    private:
        friend class Audio;

    private:
        ma_sound mSound{};
        Path mPath{};

        float mDopplerEffect{ 1.0f };
        float mVolume{ 2.0f };
        bool mMuted{ false };
        float mPitch{ 1.0f };
        float mCurrentProgress{ 0.0f };
        bool mIsLooping{ false };
        bool mIsSpatialized{ false };
        bool mIsPlaying{ false };

        AudioDevice* mDevice{ nullptr };
    };

    using AudioSourceHandle = Ref<AudioSource>;

}// namespace Mikoto


#endif//MIKOTO_AUDIO_SOURCE_HH
