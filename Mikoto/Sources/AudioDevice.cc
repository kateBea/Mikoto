// //
// // Created by zanet on 3/27/2025.
// //
//
// #include <miniaudio/miniaudio.h>
//
// #include <Assets/Audio.hh>
// #include <Audio/AudioDevice.hh>
// #include <Library/Data/ResourcePool.hh>
//
// namespace Mikoto {
//
// #ifdef MIKOTO_USE_MINIAUDIO_DEVICE
//     static auto DataCallback( ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount ) -> void {
//         auto* pAudio = static_cast<Audio*>( pDevice->pUserData );
//         if ( pAudio == nullptr || !pAudio->IsLoaded() ) return;
//
//         ma_decoder_read_pcm_frames( &pAudio->m_Decoder, pOutput, frameCount, nullptr );
//     }
// #endif
//
//     AudioDevice::AudioDevice(const AudioDeviceDescription& desc) {
// #ifdef MIKOTO_USE_MINIAUDIO_DEVICE
//         m_DeviceConfig = ma_device_config_init( ma_device_type_playback );
// #endif
//
//     }
//
//     auto AudioDevice::Init() -> void {
// #ifdef MIKOTO_USE_MINIAUDIO_DEVICE
//         m_DeviceConfig.sampleRate = 44100;// Default sample rate
//         m_DeviceConfig.dataCallback = DataCallback;
//
//         if ( ma_device_init( nullptr, &m_DeviceConfig, &m_Device ) != MA_SUCCESS ) {
//             // Handle initialization failure
//         }
// #else
//         ma_engine_init( nullptr, &m_AudioEngine );
// #endif
//     }
//
//     auto AudioDevice::Shutdown() -> void {
// #ifdef MIKOTO_USE_MINIAUDIO_DEVICE
//         ma_device_uninit( &m_Device );
// #else
//         ma_engine_uninit( &m_AudioEngine );
// #endif
//     }
//
//     auto AudioDevice::LoadAudio( const AudioLoadDescription& description ) -> AudioHandle {
//         Audio* audio{ m_LoadedAudios.Allocate( description ) };
//         if ( audio == nullptr ) {
//             MKT_CORE_LOGGER_ERROR( "AudioDevice::LoadAudio - Failed to allocate audio resource." );
//             return {};
//         }
//
//         audio->Init( this );
//
//         return AudioHandle::Create(m_LoadedAudios.Get( audio->GetHandle() ));
//     }
//
//     auto AudioDevice::Create( const AudioDeviceDescription& description ) -> Scope_T<AudioDevice> {
//         return CreateScope<AudioDevice>( description );
//     }
//
// }