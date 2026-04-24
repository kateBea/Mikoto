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

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Math/Math.hh>

#include <Memory/Allocator.hh>

#include <Logging/Logger.hh>

#include <Audio/AudioSource.hh>
#include <Audio/AudioService.hh>

namespace mikoto::audio {

    // - The maximum number of listeners is restricted to MA_ENGINE_MAX_LISTENERS.
    // - By default, sounds and sound groups have spatialization enabled. If you don't
    // ever want to spatialize your sounds, initialize the sound with the MA_SOUND_FLAG_NO_SPATIALIZATION flag
    // - By default sounds will be spatialized based on the closest listener. If a sound should
    // always be spatialized relative to a specific listener it can be pinned to one:
    // https://miniaud.io/docs/manual/index.html
    AudioSource::AudioSource( AudioDevice *device, const Path &path )
        : mPath{ path }, mDevice{ device } {}

    auto AudioSource::GetVolume() const -> float {
        return mVolume;
    }

    auto AudioSource::IsMuted() const -> bool {
        return mMuted;
    }

    auto AudioSource::IsLooping() const -> bool {
        return mIsLooping;
    }

    auto AudioSource::Mute( const bool value ) -> void {
        mMuted = value;
        SetVolume( mVolume );
    }

    auto AudioSource::SetVolume( const float volume ) -> void {
        mVolume = math::Clamp( volume, 0.0f, 10.0f );

        if (mMuted) {
            ma_sound_set_volume( &mSound, 0.0f );
        } else {
            ma_sound_set_volume( &mSound, mVolume );
        }
    }

    auto AudioSource::IncreaseVolume( float delta ) -> void {
        SetVolume( GetVolume() + delta );
    }

    auto AudioSource::DecreaseVolume( float delta ) -> void {
        SetVolume( GetVolume() - delta );
    }

    auto AudioSource::SetListener( AudioListener *listener ) -> void {
        if (listener == nullptr) {
            return;
        }

        ma_sound_set_pinned_listener_index( MKT_ADDRESSOF( mSound ), listener->GetIndex() );
    }

    auto AudioSource::ResetListener() -> void {
    }

    auto AudioSource::SetLooping( const bool value ) -> void {
        mIsLooping = value;

        ma_sound_set_looping( &mSound, mIsLooping ? MA_TRUE : MA_FALSE );
    }

    auto AudioSource::Play() -> void {
        Continue();
    }

    auto AudioSource::Pause() -> void {
        if ( mIsPlaying ) {
            ma_sound_get_cursor_in_seconds( &mSound, &mCurrentProgress );
            ma_sound_stop( &mSound );

            mIsPlaying = false;
        }
    }

    auto AudioSource::Continue() -> void {
        if ( !mIsPlaying ) {

            ma_sound_start( &mSound );
            ma_sound_seek_to_second( std::addressof( mSound ), mCurrentProgress );

            mIsPlaying = true;
        }
    }

    auto AudioSource::Stop() -> void {
        ma_sound_stop( &mSound );

        mIsPlaying = false;

        mCurrentProgress = 0;
        ma_sound_seek_to_second( std::addressof( mSound ), mCurrentProgress );
    }

    auto AudioSource::IsPlaying() const -> bool {
        return mIsPlaying && ma_sound_is_playing( &mSound ) == MA_TRUE;
    }

    auto AudioSource::SetPitch( const float pitch ) -> void {
        mPitch = pitch;
        ma_sound_set_pitch( &mSound, mPitch );
    }

    auto AudioSource::GetPitch() const -> float {
        return mPitch;
    }

    auto AudioSource::SetSpatialization( const bool state ) -> void {
        mIsSpatialized = state;
        ma_sound_set_spatialization_enabled( &mSound, mIsSpatialized ? MA_TRUE : MA_FALSE );
    }

    auto AudioSource::SetPosition( const float x, const float y, const float z ) -> void {
        if ( mIsSpatialized ) {
            ma_sound_set_position( &mSound, x, y, z );
        }
    }

    auto AudioSource::SetPosition(const float3& pos) -> void {
        SetPosition( pos.x, pos.y, pos.z );
    }

    auto AudioSource::IsSpatialized() const -> bool {
        return mIsSpatialized;
    }

    auto AudioSource::GetAudioDuration() const -> float {
        float duration{ 0 };

        ma_sound_get_length_in_seconds( &mSound, &duration );

        return duration;
    }

    auto AudioSource::GetCurrentProgress() const -> float {
        float duration{ 0 };

        ma_sound_get_cursor_in_seconds( &mSound, &duration );

        return duration;
    }

    auto AudioSource::IsSameSource( const AudioSource * source) const -> bool {
        if (source == nullptr) {
            return false;
        }

        return source == this;
    }

    auto AudioSource::IsSameAudio( const AudioSource * source) const -> bool {
        if (source == nullptr) {
            return false;
        }

        return mPath == source->mPath;
    }

    auto AudioSource::SetDopplerFactor( const float value ) -> float {
        mDopplerEffect = math::Clamp( value, 0.f, GetMaxVolume() );
        ma_sound_set_doppler_factor(std::addressof( mSound ), mDopplerEffect);

        return mDopplerEffect;
    }

    auto AudioSource::GetDopplerFactor() const -> float {
        return mDopplerEffect;
    }

    auto AudioSource::GetMaxVolume() -> float {
        return 20.0f;
    }

    auto AudioSource::Initialize() -> void {
        if ( !mPath.IsFile() ) {
            return;
        }

        const ma_result result{ ma_sound_init_from_file(
                std::addressof( mDevice->mAudioEngine ),
                mPath.GetC_Str(),
                MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_ASYNC,
                nullptr, nullptr,
                std::addressof( mSound ) ) };

        if ( result != MA_SUCCESS ) {
            MKT_CORE_LOGGER_ERROR( "AudioSource::Initialize - Failed to allocate audio source." );
            return;
        }

        // Pre setup miniaudio
        SetVolume( mVolume );
        SetLooping( mIsLooping );
        SetPitch( mPitch );
        SetSpatialization( mIsSpatialized );

        mIsAllocated = true;
    }

    auto AudioSource::Release() -> void {
        if (IsPlaying()) {
            Stop();
        }

        ma_sound_uninit( &mSound );
        mIsPlaying = false;
        mSound = {};

        mIsAllocated = false;
    }
}