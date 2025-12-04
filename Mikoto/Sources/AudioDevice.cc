//
// Created by zanet on 3/27/2025.
//

#include <miniaudio.h>

#include <Core/Exception.hh>
#include <Assets/AudioClip.hh>
#include <Audio/AudioDevice.hh>
#include <Library/Data/ResourcePool.hh>

namespace Mikoto {


    AudioDevice::AudioDevice(const AudioDeviceDescription& desc) {
        m_EngineConfig = ma_engine_config_init();
        m_EngineConfig.listenerCount = desc.MaxListenersCount;
    }

    auto AudioDevice::Init() -> void {
        ma_result result{ ma_engine_init( nullptr, &m_AudioEngine ) };
        if( result != MA_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "AudioDevice::Init - Failed to initialize audio device" );
        }

        m_LoadedAudios.Init( 10 );
    }

    auto AudioDevice::Shutdown() -> void {
        ma_engine_uninit( &m_AudioEngine );
        m_LoadedAudios.Shutdown();
    }

    auto AudioDevice::LoadAudio( const AudioLoadDescription& description ) -> AudioHandle {
        if ( !description.AudioFile ) {
            MKT_CORE_LOGGER_ERROR( "AudioDevice::LoadAudio - Audio file is null." );
            return AudioHandle::CreateEmpty();
        }

        AudioHandle audio{ m_LoadedAudios.Allocate( description ) };
        if ( audio.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "AudioDevice::LoadAudio - Failed to allocate audio resource." );
            return AudioHandle::CreateEmpty();
        }

        audio->Init( this );

        m_CachedAudios[description.AudioFile->GetPath()] = audio->GetHandle();

        return audio;
    }

    auto AudioDevice::GetAudio( const std::string& uri ) -> AudioHandle {
        const auto it{ m_CachedAudios.find( uri ) };
        if ( it != m_CachedAudios.end() ) {
            return m_LoadedAudios.Get( it->second );
        }

        return AudioHandle::CreateEmpty();
    }

    auto AudioDevice::Create( const AudioDeviceDescription& description ) -> Unique<AudioDevice> {
        return CreateScope<AudioDevice>( description );
    }

}