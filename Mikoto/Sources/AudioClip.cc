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

#include <Logging/Logger.hh>
#include <Library/IO/File.hh>

#include <Assets/AudioClip.hh>
#include <Audio/AudioDevice.hh>

namespace Mikoto {

    Audio::Audio( const AudioLoadDescription& description )
        : m_FileSource{ description.AudioFile },
          m_TrackName{
          Path{ description.AudioFile->GetPath() }
              .replace_extension()
              .filename()
              .string() }
    {}

    auto Audio::CreateSource() -> AudioSourceHandle {
        AudioSourceHandle source{ m_Sources.Allocate( m_Device, m_FileSource->GetPath() ) };
        if ( source.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate audio source." );
            return AudioSourceHandle::CreateEmpty();
        }

        source->Initialize();

        return source;
    }

    auto Audio::GetFile() const -> const File* { 
        return m_FileSource; 
    }

    auto Audio::GetTrackName() const -> const std::string& {
        return m_TrackName;
    }

    auto Audio::Release() -> void {
        m_Sources.Shutdown();
    }

    auto Audio::Initialize() -> void {
        m_Sources.Init( 5 );
        SetIsReady( true );

        m_IsAllocated = true;
    }
}// namespace Mikoto