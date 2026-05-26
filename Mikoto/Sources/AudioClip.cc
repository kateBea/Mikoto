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

#include <miniaudio.h>

#include <Audio/AudioClip.hh>
#include <Audio/AudioDevice.hh>

#include <Logging/Logger.hh>

namespace mikoto::audio {

    using namespace mikoto::core;

    Audio::Audio( const AudioLoadDescription& description )
        : mFileSource{ description.mFile }
    {}

    auto Audio::CreateSource() -> AudioSourceHandle {
        AudioSourceHandle source{ m_Sources.Allocate( m_Device, mFileSource->GetPath() ) };
        if ( source.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate audio source." );
            return AudioSourceHandle::CreateEmpty();
        }

        source->Initialize();

        return source;
    }

    auto Audio::GetFile() const -> FileHandle {
        return mFileSource;
    }

    auto Audio::GetTrackName() const -> eastl::string_view {
        return mFileSource->GetName();
    }

    auto Audio::Release() -> void {
        m_Sources.Shutdown();
    }

    auto Audio::Initialize() -> void {
        m_Sources.Init( 5 );
        SetIsReady( true );

        mIsAllocated = true;
    }
}// namespace Mikoto