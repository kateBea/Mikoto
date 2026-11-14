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

#include <glm/gtx/compatibility.hpp>

#include <miniaudio.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Logging/Logger.hh>

#include <Audio/Utility.hh>
#include <Audio/AudioSource.hh>
#include <Audio/AudioDevice.hh>
#include <Audio/AudioBackend.hh>
#include <Audio/AudioListener.hh>

namespace mikoto::audio {

    auto AudioSource::SetOutput( AudioMixer* mixer, core::u64 group ) -> bool {
        if ( !mIsAllocated || !mDevice ) {
            return false;
        }

        if ( mixer == mMixer && group == mMixerGroup ) {
            return true;
        }

        if ( mixer && ( !mixer->IsInitialized() || mixer->mDevice != mDevice || ( group && !mixer->mImpl->mGroups.contains( group ) ) ) ) {
            return false;
        }

        ma_node* target{ ma_engine_get_endpoint( &mDevice->mImpl->mAudioEngine ) };

        if ( mixer ) {
            target = group ? static_cast<ma_node*>( &mixer->mImpl->mGroups.at( group )->mNode ) : static_cast<ma_node*>( &mixer->mImpl->mMaster );
        }

        if ( ma_node_attach_output_bus( &mImpl->mBinaural.mNode, 0, target, 0 ) != MA_SUCCESS ) {
            return false;
        }

        mMixer = mixer;
        mMixerGroup = mixer ? group : 0;
        return true;
    }

    AudioSource::AudioSource( AudioDevice* device, const filesystem::Path& path )
        : mImpl{ eastl::make_unique<Impl>() }, mPath{ path }, mDevice{ device } {
    }

    AudioSource::AudioSource( AudioDevice* device, ma_data_source_base* source )
        : mImpl{ eastl::make_unique<Impl>() }, mDataSource{ source }, mDevice{ device } {
    }

    AudioSource::~AudioSource() {
        MKT_BEGIN_PROFILER();
        // Failed initialization can still leave a live-list registration.
        // Destroy checks mIsAllocated before releasing the native sound.
        if ( mDevice ) {
            Destroy();
        }
    }

    auto AudioSource::SetSettings( SourceSettings settings ) -> void {
        MKT_BEGIN_PROFILER();
        mSettings = SanitizeAudio( settings );
        {
            std::lock_guard lock{ mImpl->mBinaural.mSettingsMutex };
            mImpl->mBinaural.mPendingSettings = mSettings;
        }

        if ( !mIsAllocated ) {
            return;
        }

        ma_sound_set_volume( &mImpl->mSound, mSettings.mMuted ? 0.0f : mSettings.mVolume );
        ma_sound_set_pitch( &mImpl->mSound, mSettings.mPitch );
        // Apply pan after the HRTF for 3D voices so mono folding cannot erase it.
        ma_sound_set_pan( &mImpl->mSound, mSettings.mSpatialized ? 0.f : mSettings.mPan );
        ma_sound_set_looping( &mImpl->mSound, mSettings.mLooping ? MA_TRUE : MA_FALSE );
        ma_sound_set_spatialization_enabled( &mImpl->mSound, mSettings.mSpatialized ? MA_TRUE : MA_FALSE );
        ma_sound_set_doppler_factor( &mImpl->mSound, mSettings.mDoppler );
        ma_sound_set_attenuation_model( &mImpl->mSound, mSettings.mCurves[0].mEnabled ? ma_attenuation_model_none : static_cast<ma_attenuation_model>( mSettings.mAttenuation ) );
        ma_sound_set_min_distance( &mImpl->mSound, mSettings.mMinDistance );
        ma_sound_set_max_distance( &mImpl->mSound, mSettings.mMaxDistance );
        ma_sound_set_rolloff( &mImpl->mSound, mSettings.mRolloff );
    }

    auto AudioSource::GetVolume() const -> float {
        MKT_BEGIN_PROFILER();
        return mSettings.mVolume;
    }

    auto AudioSource::IsMuted() const -> bool {
        MKT_BEGIN_PROFILER();
        return mSettings.mMuted;
    }

    auto AudioSource::IsLooping() const -> bool {
        MKT_BEGIN_PROFILER();
        return mSettings.mLooping;
    }

    auto AudioSource::GetPitch() const -> float {
        MKT_BEGIN_PROFILER();
        return mSettings.mPitch;
    }

    auto AudioSource::IsSpatialized() const -> bool {
        MKT_BEGIN_PROFILER();
        return mSettings.mSpatialized;
    }

    auto AudioSource::GetDopplerFactor() const -> float {
        MKT_BEGIN_PROFILER();
        return mSettings.mDoppler;
    }

    auto AudioSource::GetMaxVolume() -> float {
        MKT_BEGIN_PROFILER();
        return 20.0f;
    }

    auto AudioSource::Mute( bool value ) -> void {
        MKT_BEGIN_PROFILER();
        auto s = mSettings;
        s.mMuted = value;
        SetSettings( s );
    }

    auto AudioSource::SetVolume( float value ) -> void {
        MKT_BEGIN_PROFILER();
        auto s = mSettings;
        s.mVolume = value;
        SetSettings( s );
    }

    auto AudioSource::IncreaseVolume( float delta ) -> void {
        MKT_BEGIN_PROFILER();
        SetVolume( GetVolume() + delta );
    }

    auto AudioSource::DecreaseVolume( float delta ) -> void {
        MKT_BEGIN_PROFILER();
        SetVolume( GetVolume() - delta );
    }

    auto AudioSource::SetLooping( bool value ) -> void {
        MKT_BEGIN_PROFILER();
        auto s = mSettings;
        s.mLooping = value;
        SetSettings( s );
    }

    auto AudioSource::SetPitch( float value ) -> void {
        MKT_BEGIN_PROFILER();
        auto s = mSettings;
        s.mPitch = value;
        SetSettings( s );
    }

    auto AudioSource::SetSpatialization( bool value ) -> void {
        MKT_BEGIN_PROFILER();
        auto s = mSettings;
        s.mSpatialized = value;
        SetSettings( s );
    }

    auto AudioSource::SetDopplerFactor( float value ) -> float {
        MKT_BEGIN_PROFILER();
        auto s = mSettings;
        s.mDoppler = value;
        SetSettings( s );
        return mSettings.mDoppler;
    }

    auto AudioSource::SetListener( AudioListener* listener ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !mIsAllocated || !listener || listener->GetIndex() < 0 || !mDevice ||
             static_cast<core::u32>( listener->GetIndex() ) >= mDevice->GetListenerCount() ) {
            return;
        }

        ma_sound_set_pinned_listener_index( &mImpl->mSound, static_cast<core::u32>( listener->GetIndex() ) );
        mListenerPinned = true;
    }

    auto AudioSource::ResetListener() -> void {
        MKT_BEGIN_PROFILER();
        mListenerPinned = false;
        UpdateListenerSelection();
    }

    auto AudioSource::UpdateListenerSelection() -> void {
        MKT_BEGIN_PROFILER();

        if ( !mIsAllocated || !mDevice || mListenerPinned ) {
            return;
        }

        // This miniaudio version rejects MA_LISTENER_INDEX_CLOSEST in its setter.
        // Select the closest enabled listener through the public API each scene
        // update instead, including when the listener moves and the sound does not.
        const auto index{ ma_engine_find_closest_listener( &mDevice->mImpl->mAudioEngine, mPosition.x, mPosition.y, mPosition.z ) };
        ma_sound_set_pinned_listener_index( &mImpl->mSound, index );
    }

    auto AudioSource::Play() -> void {
        MKT_BEGIN_PROFILER();

        if ( !mIsAllocated || IsPlaying() ) {
            return;
        }

        UpdateListenerSelection();
        // The backend stops on natural EOF; an extra cached playing flag would
        // incorrectly prevent this voice from ever starting again.
        if ( ma_sound_at_end( &mImpl->mSound ) ) {
            ma_sound_seek_to_pcm_frame( &mImpl->mSound, 0 );
            mImpl->mBinaural.RequestReset();
        }

        if ( ma_sound_start( &mImpl->mSound ) == MA_SUCCESS ) {
            mPaused = false;
        }
    }

    auto AudioSource::Continue() -> void {
        MKT_BEGIN_PROFILER();
        Play();
    }

    auto AudioSource::Pause() -> void {
        MKT_BEGIN_PROFILER();

        if ( !IsPlaying() ) {
            return;
        }

        ma_sound_stop( &mImpl->mSound );// The native cursor already retains the exact frame.
        mPaused = true;
    }

    auto AudioSource::Stop() -> void {
        MKT_BEGIN_PROFILER();

        if ( !mIsAllocated ) {
            return;
        }

        ma_sound_stop( &mImpl->mSound );
        ma_sound_seek_to_pcm_frame( &mImpl->mSound, 0 );
        mImpl->mBinaural.RequestReset();
        mPaused = false;
    }

    auto AudioSource::Seek( float seconds ) -> bool {
        MKT_BEGIN_PROFILER();

        if ( !mIsAllocated ) {
            return false;
        }

        const float duration = GetAudioDuration();
        seconds = glm::max( 0.0f, seconds );

        if ( duration > 0 ) {
            seconds = glm::min( seconds, duration );
        }

        const bool success{ ma_sound_seek_to_second( &mImpl->mSound, seconds ) == MA_SUCCESS };

        if ( success ) {
            mImpl->mBinaural.RequestReset();
        }

        return success;
    }

    auto AudioSource::IsPlaying() const -> bool {
        MKT_BEGIN_PROFILER();
        return mIsAllocated && ma_sound_is_playing( &mImpl->mSound ) == MA_TRUE;
    }

    auto AudioSource::SetPosition( float x, float y, float z ) -> void {
        MKT_BEGIN_PROFILER();
        SetPosition( core::float3{ x, y, z } );
    }

    auto AudioSource::SetPosition( const core::float3& value ) -> void {
        MKT_BEGIN_PROFILER();
        mPosition = value;
        mHasPosition = true;

        if ( mIsAllocated ) {
            ma_sound_set_position( &mImpl->mSound, value.x, value.y, value.z );
        }

        UpdateListenerSelection();
    }

    auto AudioSource::SetVelocity( const core::float3& value ) -> void {
        MKT_BEGIN_PROFILER();
        mVelocity = value;

        if ( mIsAllocated ) {
            ma_sound_set_velocity( &mImpl->mSound, value.x, value.y, value.z );
        }
    }

    auto AudioSource::UpdatePosition( const core::float3& value, float dt ) -> void {
        MKT_BEGIN_PROFILER();
        SetVelocity( AudioVelocity( value, mPosition, dt, mHasPosition ) );
        SetPosition( value );
    }

    auto AudioSource::GetAudioDuration() const -> float {
        MKT_BEGIN_PROFILER();
        float value{};

        if ( mIsAllocated ) {
            ma_sound_get_length_in_seconds( &mImpl->mSound, &value );
        }

        return value;
    }

    auto AudioSource::GetCurrentProgress() const -> float {
        MKT_BEGIN_PROFILER();
        float value{};

        if ( mIsAllocated ) {
            ma_sound_get_cursor_in_seconds( &mImpl->mSound, &value );
        }

        return value;
    }

    auto AudioSource::IsSameSource( const AudioSource* source ) const -> bool {
        MKT_BEGIN_PROFILER();
        return source == this;
    }

    auto AudioSource::IsSameAudio( const AudioSource* source ) const -> bool {
        MKT_BEGIN_PROFILER();
        return source && mPath == source->mPath;
    }

    auto AudioSource::Clone() const -> AudioSourceHandle {
        MKT_BEGIN_PROFILER();

        if ( !mIsAllocated || !mDevice || mDataSource ) {
            return AudioSourceHandle{};
        }

        auto copy = mDevice->CreateSource( mPath );

        if ( copy ) {
            copy->SetSettings( mSettings );
            copy->SetPosition( mPosition );
            copy->mHasPosition = false;
        }

        return copy;
    }

    auto AudioSource::Initialize() -> void {
        MKT_BEGIN_PROFILER();

        if ( mIsAllocated || !mDevice || !mDevice->IsInitialized() || ( !mDataSource && !mPath.IsFile() ) ) {
            return;
        }

        std::lock_guard lock{ mDevice->mImpl->mResourceMutex };
        mDevice->mImpl->mLiveSources.push_back( this );

        // Offline tests decode synchronously. Interactive sources wait for a
        // valid decoder, then complete decoding through miniaudio's job queue.
        const core::u32 flags = MA_SOUND_FLAG_NO_DEFAULT_ATTACHMENT | MA_SOUND_FLAG_DECODE | ( mDevice->mImpl->mEngineConfig.noDevice ? 0 : MA_SOUND_FLAG_ASYNC | MA_SOUND_FLAG_WAIT_INIT );
        const auto result{ mDataSource ?
            ma_sound_init_from_data_source( &mDevice->mImpl->mAudioEngine, mDataSource,
                MA_SOUND_FLAG_NO_DEFAULT_ATTACHMENT, nullptr, &mImpl->mSound ) :
            ma_sound_init_from_file( &mDevice->mImpl->mAudioEngine, mPath.GetC_Str(), flags, nullptr, nullptr, &mImpl->mSound ) };

        if ( result != MA_SUCCESS ) {
            MKT_CORE_LOGGER_ERROR( "Could not load audio '{}': {}", mPath.GetC_Str(), ma_result_description( result ) );
            return;
        }

        // Retain distance attenuation and Doppler in miniaudio, but let HRTF
        // handle direction so the sound is not panned twice.
        ma_sound_set_directional_attenuation_factor( &mImpl->mSound, 0.0f );

        if ( !mImpl->mBinaural.Initialize( mDevice->mImpl->mAudioEngine, mImpl->mSound,
                 mDevice->mImpl->mContext, mDevice->mImpl->mHrtf ) ) {
            ma_sound_uninit( &mImpl->mSound );
            MKT_CORE_LOGGER_ERROR( "Could not initialize Steam Audio source '{}'", mPath.GetC_Str() );
            return;
        }

        mIsAllocated = true;
        SetSettings( mSettings );
        SetPosition( mPosition );
        mHasPosition = false;
    }

    auto AudioSource::Destroy() -> void {
        MKT_BEGIN_PROFILER();

        if ( !mDevice ) {
            return;
        }

        std::lock_guard lock{ mDevice->mImpl->mResourceMutex };
        // Keep teardown linear in the number of sources, not quadratic scans
        // of a shrinking list. Individual removals retain registration order.
        auto& sources{ mDevice->mImpl->mLiveSources };

        if ( !sources.empty() && sources.back() == this ) {
            sources.pop_back();
        }
        else {
            eastl::erase( sources, this );
        }

        if ( mIsAllocated ) {
            ma_sound_stop( &mImpl->mSound );
            mImpl->mBinaural.Shutdown();
            ma_sound_uninit( &mImpl->mSound );
        }

        mIsAllocated = false;
        mPaused = false;
        mHasPosition = false;
        mDevice = nullptr;
        mMixer = nullptr;
        mMixerGroup = 0;
        mImpl->mSound = {};
    }

    auto AudioSource::GetSettings() const -> const SourceSettings& {
        MKT_BEGIN_PROFILER();
        return mSettings;
    }


    auto AudioSource::IsPaused() const -> bool {
        MKT_BEGIN_PROFILER();
        return mIsAllocated && mPaused;
    }


    auto AudioSource::GetPosition() const -> const core::float3& {
        MKT_BEGIN_PROFILER();
        return mPosition;
    }


    auto AudioSource::GetVelocity() const -> const core::float3& {
        MKT_BEGIN_PROFILER();
        return mVelocity;
    }
}
