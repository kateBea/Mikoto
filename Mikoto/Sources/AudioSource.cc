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

#include <Logging/Logger.hh>
#include <Library/Math/Math.hh>

#include <Audio/AudioSource.hh>
#include <Audio/AudioService.hh>

namespace Mikoto {

    // - The maximum number of listeners is restricted to MA_ENGINE_MAX_LISTENERS.
    // - By default, sounds and sound groups have spatialization enabled. If you don't 
    // ever want to spatialize your sounds, initialize the sound with the MA_SOUND_FLAG_NO_SPATIALIZATION flag
    // - By default sounds will be spatialized based on the closest listener. If a sound should 
    // always be spatialized relative to a specific listener it can be pinned to one:
    // https://miniaud.io/docs/manual/index.html
    AudioSource::AudioSource( AudioDevice *device, const Path &path )
        : m_Path{ path }, m_Device{ device } {}

    auto AudioSource::GetVolume() const -> float {
        return m_Volume;
    }

    auto AudioSource::IsMuted() const -> bool {
        return m_Muted;
    }

    auto AudioSource::IsLooping() const -> bool {
        return m_IsLooping;
    }

    auto AudioSource::Mute( const bool value ) -> void {
        m_Muted = value;
        SetVolume( m_Volume );
    }

    auto AudioSource::SetVolume( const float volume ) -> void {
        m_Volume = Math::Clamp( volume, 0.0f, 10.0f );

        if (m_Muted) {
            ma_sound_set_volume( &m_Sound, 0.0f );
        } else {
            ma_sound_set_volume( &m_Sound, m_Volume );
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

        ma_sound_set_pinned_listener_index( MKT_ADDRESSOF( m_Sound ), listener->GetIndex() );
    }

    auto AudioSource::ResetListener() -> void {
    }

    auto AudioSource::SetLooping( const bool value ) -> void {
        m_IsLooping = value;

        ma_sound_set_looping( &m_Sound, m_IsLooping ? MA_TRUE : MA_FALSE );
    }

    auto AudioSource::Play() -> void {
        Continue();
    }

    auto AudioSource::Pause() -> void {
        if ( m_IsPlaying ) {
            ma_sound_get_cursor_in_seconds( &m_Sound, &m_CurrentProgress );
            ma_sound_stop( &m_Sound );

            m_IsPlaying = false;
        }
    }

    auto AudioSource::Continue() -> void {
        if ( !m_IsPlaying ) {

            ma_sound_start( &m_Sound );
            ma_sound_seek_to_second( std::addressof( m_Sound ), m_CurrentProgress );

            m_IsPlaying = true;
        }
    }

    auto AudioSource::Stop() -> void {
        ma_sound_stop( &m_Sound );

        m_IsPlaying = false;

        m_CurrentProgress = 0;
        ma_sound_seek_to_second( std::addressof( m_Sound ), m_CurrentProgress );
    }

    auto AudioSource::IsPlaying() const -> bool {
        return m_IsPlaying && ma_sound_is_playing( &m_Sound ) == MA_TRUE;
    }

    auto AudioSource::SetPitch( const float pitch ) -> void {
        m_Pitch = pitch;
        ma_sound_set_pitch( &m_Sound, m_Pitch );
    }

    auto AudioSource::GetPitch() const -> float {
        return m_Pitch;
    }

    auto AudioSource::SetSpatialization( const bool state ) -> void {
        m_IsSpatialized = state;
        ma_sound_set_spatialization_enabled( &m_Sound, m_IsSpatialized ? MA_TRUE : MA_FALSE );
    }

    auto AudioSource::SetPosition( const float x, const float y, const float z ) -> void {
        if ( m_IsSpatialized ) {
            ma_sound_set_position( &m_Sound, x, y, z );
        }
    }

    auto AudioSource::SetPosition(const Vec3F& pos) -> void {
        SetPosition( pos.x, pos.y, pos.z );
    }

    auto AudioSource::IsSpatialized() const -> bool {
        return m_IsSpatialized;
    }

    auto AudioSource::GetAudioDuration() const -> float {
        float duration{ 0 };

        ma_sound_get_length_in_seconds( &m_Sound, &duration );

        return duration;
    }

    auto AudioSource::GetCurrentProgress() const -> float {
        float duration{ 0 };

        ma_sound_get_cursor_in_seconds( &m_Sound, &duration );

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

        return m_Path == source->m_Path;
    }

    auto AudioSource::SetDopplerFactor( const float value ) -> float {
        m_DopplerEffect = Math::Clamp( value, 0.f, GetMaxVolume() );
        ma_sound_set_doppler_factor(std::addressof( m_Sound ), m_DopplerEffect);

        return m_DopplerEffect;
    }

    auto AudioSource::GetDopplerFactor() const -> float {
        return m_DopplerEffect;
    }

    auto AudioSource::GetMaxVolume() -> float {
        return 20.0f;
    }

    auto AudioSource::Initialize() -> void {
        if ( m_Path.empty() ) {
            return;
        }

        const ma_result result{ ma_sound_init_from_file(
                std::addressof( m_Device->m_AudioEngine ),
                m_Path.string().c_str(),
                MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_ASYNC,
                nullptr, nullptr,
                std::addressof( m_Sound ) ) };

        if ( result != MA_SUCCESS ) {
            MKT_CORE_LOGGER_ERROR( "AudioSource::Initialize - Failed to allocate audio source." );
            return;
        }

        // Pre setup miniaudio
        SetVolume( m_Volume );
        SetLooping( m_IsLooping );
        SetPitch( m_Pitch );
        SetSpatialization( m_IsSpatialized );

        m_IsAllocated = true;
    }

    auto AudioSource::Release() -> void {
        if (IsPlaying()) {
            Stop();
        }

        ma_sound_uninit( &m_Sound );
        m_IsPlaying = false;
        m_Sound = {};

        m_IsAllocated = false;
    }
}