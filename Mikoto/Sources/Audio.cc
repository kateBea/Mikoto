// //
// // Created by zanet on 1/28/2025.
// //
//
// #include <miniaudio.h>
//
// #include <Assets/Audio.hh>
// #include <Audio/AudioDevice.hh>
// #include <Core/Logger.hh>
// #include <Library/IO/File.hh>
//
// namespace Mikoto {
//     AudioLoadDescription& AudioLoadDescription::WithFile( const File* source ) {
//         this->AudioFile = source;
//         return *this;
//     }
//
//     AudioLoadDescription& AudioLoadDescription::SetVolume( const float volume ) {
//         Volume = volume;
//         return *this;
//     }
//
//     Audio::Audio( const AudioLoadDescription& description )
//         : m_FileSource{ description.AudioFile } {}
//
//     auto Audio::CreateSource() -> AudioSourceHandle {
//         AudioSource* source{ m_Sources.Allocate( m_Device, m_FileSource->GetPath() ) };
//         if ( source == nullptr ) {
//             MKT_CORE_LOGGER_ERROR( "Failed to allocate audio source." );
//             return {};
//         }
//
//         source->Allocate();
//
//         return AudioSourceHandle{ source };
//     }
//
//     auto Audio::Release() -> void {
//         m_Sources.Shutdown( );
//     }
//
//     auto Audio::Allocate() -> void {
//         // Start with 20 instances max
//         m_Sources.Init( 20 );
//         SetIsReady( true );
//     }
// }