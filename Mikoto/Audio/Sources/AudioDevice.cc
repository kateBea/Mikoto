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

#include <EASTL/utility.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Logging/Logger.hh>

#include <Audio/Utility.hh>
#include <Audio/AudioDevice.hh>
#include <Audio/AudioBackend.hh>

namespace mikoto::audio {

    using namespace mikoto::core;

    AudioDevice::AudioDevice( const AudioDeviceDescription& desc )
        : mImpl{ eastl::make_unique<Impl>() } {
        MKT_BEGIN_PROFILER();
        mImpl->mEngineConfig = ma_engine_config_init();
        mImpl->mEngineConfig.listenerCount = glm::clamp( desc.mMaxListeners, 1u, core::u32( MA_ENGINE_MAX_LISTENERS ) );
        mImpl->mEngineConfig.noDevice = desc.mNoDevice ? MA_TRUE : MA_FALSE;

        // A stereo graph lets 2D voices pass through unchanged and 3D voices
        // deliver Steam Audio's binaural output on the same endpoint.
        mImpl->mEngineConfig.channels = 2;
        mImpl->mEngineConfig.sampleRate = 48000;
        mImpl->mEngineConfig.periodSizeInFrames = kAudioBlockFrames;
    }

    auto AudioDevice::Initialize() -> void {
        MKT_BEGIN_PROFILER();

        // https://valvesoftware.github.io/steam-audio/doc/capi/getting-started.html

        std::lock_guard lock{ mImpl->mResourceMutex };

        if ( mInitialized ) {
            return;
        }

        MKT_CORE_LOGGER_INFO( "Initializing AudioDevice..." );
        const auto result{ ma_engine_init( &mImpl->mEngineConfig, &mImpl->mAudioEngine ) };

        if ( result != MA_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( string::Format( "AudioDevice::Init failed: {}", ma_result_description( result ) ) );
        }

        IPLContextSettings context{};
        context.version = STEAMAUDIO_VERSION;

        IPLAudioSettings audio{ static_cast<IPLint32>( ma_engine_get_sample_rate( &mImpl->mAudioEngine ) ), kAudioBlockFrames };
        IPLHRTFSettings hrtf{};
        hrtf.type = IPL_HRTFTYPE_DEFAULT;
        hrtf.volume = 1.0f;

        if ( iplContextCreate( &context, &mImpl->mContext ) != IPL_STATUS_SUCCESS ||
             iplHRTFCreate( mImpl->mContext, &audio, &hrtf, &mImpl->mHrtf ) != IPL_STATUS_SUCCESS ) {
            if ( mImpl->mHrtf ) {
                iplHRTFRelease( &mImpl->mHrtf );
            }

            if ( mImpl->mContext ) {
                iplContextRelease( &mImpl->mContext );
            }

            ma_engine_uninit( &mImpl->mAudioEngine );
            MKT_THROW_RUNTIME_ERROR( "AudioDevice::Init could not initialize Steam Audio HRTF" );
        }

        mInitialized = true;
    }

    auto AudioDevice::Shutdown() -> void {
        MKT_BEGIN_PROFILER();
        std::lock_guard lock{ mImpl->mResourceMutex };

        if ( !mInitialized ) {
            return;
        }

        MKT_CORE_LOGGER_INFO( "Shutting down AudioDevice..." );
        // Sources detach their graph nodes before the shared HRTF/context die.
        while ( !mImpl->mLiveSources.empty() ) {
            mImpl->mLiveSources.back()->Destroy();
        }

        while ( !mImpl->mLiveClips.empty() ) {
            mImpl->mLiveClips.back()->Destroy();
        }

        while ( !mImpl->mLiveMixers.empty() ) {
            mImpl->mLiveMixers.back()->Shutdown();
        }

        ma_engine_uninit( &mImpl->mAudioEngine );

        iplHRTFRelease( &mImpl->mHrtf );
        iplContextRelease( &mImpl->mContext );
        mInitialized = false;
    }

    auto AudioDevice::LoadAudio( filesystem::FileHandle file, core::f32 volume ) -> AudioHandle {
        MKT_BEGIN_PROFILER();
        std::lock_guard lock{ mImpl->mResourceMutex };

        if ( !mInitialized || !file || !file->GetPath().IsFile() ) {
            return AudioHandle{};
        }

        auto audio = AudioHandle::New( eastl::move( file ), volume );

        if ( audio ) {
            audio->Init( this );
        }

        return audio;
    }

    auto AudioDevice::CreateSource( const filesystem::Path& path ) -> AudioSourceHandle {
        MKT_BEGIN_PROFILER();
        std::lock_guard lock{ mImpl->mResourceMutex };

        if ( !mInitialized || !path.IsFile() ) {
            return AudioSourceHandle{};
        }

        auto source{ AudioSourceHandle::New( this, path ) };
        source->Initialize();

        if ( !source->IsReady() ) {
            return AudioSourceHandle{};
        }

        return source;
    }

    auto AudioDevice::IsInitialized() const -> bool {
        MKT_BEGIN_PROFILER();
        return mInitialized;
    }

    auto AudioDevice::CreateSource( ma_data_source_base* dataSource ) -> AudioSourceHandle {
        std::lock_guard lock{ mImpl->mResourceMutex };

        if ( !mInitialized || !dataSource ) {
            return {};
        }

        auto source{ AudioSourceHandle::New( this, dataSource ) };
        source->Initialize();
        return source->IsReady() ? source : AudioSourceHandle{};
    }

    auto AudioDevice::GetListenerCount() const -> core::u32 {
        MKT_BEGIN_PROFILER();
        return mInitialized ? ma_engine_get_listener_count( &mImpl->mAudioEngine ) : 0;
    }

    auto AudioDevice::SetMasterVolume( float volume ) -> void {
        if ( mInitialized ) {
            ma_engine_set_volume( &mImpl->mAudioEngine, SanitizeAudio( SourceSettings{ .mVolume = volume } ).mVolume );
        }
    }

    auto AudioDevice::GetMasterVolume() -> float {
        return mInitialized ? ma_engine_get_volume( &mImpl->mAudioEngine ) : 0.0f;
    }

    AudioDevice::~AudioDevice() {
        // Empty. Defined for pimpl purposes
        // 'Attempting to call the destructor of an incomplete type'
    }

    auto AudioDevice::RenderOffline( float* output, core::u64 frames ) -> bool {
        MKT_BEGIN_PROFILER();
        return mInitialized && mImpl->mEngineConfig.noDevice && output &&
               ma_engine_read_pcm_frames( &mImpl->mAudioEngine, output, frames, nullptr ) == MA_SUCCESS;
    }

    auto AudioDevice::Create( const AudioDeviceDescription& desc ) -> eastl::unique_ptr<AudioDevice> {
        MKT_BEGIN_PROFILER();
        return eastl::make_unique<AudioDevice>( desc );
    }
}// namespace mikoto::audio
