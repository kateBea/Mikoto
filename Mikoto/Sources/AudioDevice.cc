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

#include <miniaudio.h>

#include <Core/Exception.hh>

#include <Memory/Allocator.hh>

#include <Assets/AudioClip.hh>
#include <Audio/AudioDevice.hh>
#include <Library/Data/ResourcePool.hh>

namespace Mikoto {

    AudioDevice::AudioDevice(const AudioDeviceDescription& desc) {
        m_EngineConfig = ma_engine_config_init();
        m_EngineConfig.listenerCount = desc.MaxListenersCount;
    }

    auto AudioDevice::Init() -> void {
        ma_result result{ ma_engine_init( nullptr, MKT_ADDRESSOF( m_AudioEngine ) ) };
        if( result != MA_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "AudioDevice::Init - Failed to initialize audio device" );
        }

        m_LoadedAudios.Init( 10 );
    }

    auto AudioDevice::Shutdown() -> void {
        ma_engine_uninit( MKT_ADDRESSOF( m_AudioEngine ) );
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