//
// Created by zanet on 1/28/2025.
//

#include <miniaudio.h>

#include <Assets/AudioClip.hh>
#include <Audio/AudioDevice.hh>
#include <Library/IO/File.hh>
#include <Logging/Logger.hh>

namespace Mikoto {
    AudioLoadDescription& AudioLoadDescription::WithFile( const File* source ) {
        this->AudioFile = source;
        return *this;
    }

    AudioLoadDescription& AudioLoadDescription::SetVolume( const float volume ) {
        Volume = volume;
        return *this;
    }

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