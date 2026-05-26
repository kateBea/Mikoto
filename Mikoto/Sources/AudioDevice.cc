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

#include <EASTL/unique_ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Exception.hh>
#include <Core/ResourcePool.hh>

#include <Logging/Logger.hh>

#include <Audio/AudioClip.hh>
#include <Audio/AudioDevice.hh>

#include <Memory/Allocator.hh>

namespace mikoto::audio {

    AudioDevice::AudioDevice(const AudioDeviceDescription& desc) {
        mEngineConfig = ma_engine_config_init();
        mEngineConfig.listenerCount = desc.mMaxListeners;
    }

    auto AudioDevice::Init() -> void {
        ma_result result{ ma_engine_init( nullptr, MKT_ADDRESSOF( mAudioEngine ) ) };
        if( result != MA_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "AudioDevice::Init - Failed to initialize audio device" );
        }

        mLoadedAudios.Init( 10 );
    }

    auto AudioDevice::Shutdown() -> void {
        ma_engine_uninit( MKT_ADDRESSOF( mAudioEngine ) );
        mLoadedAudios.Shutdown();
    }

    auto AudioDevice::LoadAudio( const AudioLoadDescription& description ) -> AudioHandle {
        if ( !description.mFile ) {
            MKT_CORE_LOGGER_ERROR( "AudioDevice::LoadAudio - Audio file is null." );
            return AudioHandle::CreateEmpty();
        }

        AudioHandle audio{ mLoadedAudios.Allocate( description ) };
        if ( audio.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "AudioDevice::LoadAudio - Failed to allocate audio resource." );
            return AudioHandle::CreateEmpty();
        }

        audio->Init( this );

        return audio;
    }

    auto AudioDevice::Create( const AudioDeviceDescription& description ) -> eastl::unique_ptr<AudioDevice> {
        return eastl::make_unique<AudioDevice>( description );
    }
}