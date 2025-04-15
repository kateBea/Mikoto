//
// Created by zanet on 3/27/2025.
//

#ifndef AUDIODEVICE_HH
#define AUDIODEVICE_HH

#include <miniaudio/miniaudio.h>

#include <Audio/AudioUtility.hh>
#include <Library/Data/ResourcePool.hh>
#include <Library/Utility/Types.hh>

#ifdef MIKOTO_USE_AUDIO_ENGINE_INTERFACE
#define MIKOTO_USE_MINIAUDIO_ENGINE
#else
#define MIKOTO_USE_MINIAUDIO_DEVICE
#endif

namespace Mikoto {

    class Audio;

    using AudioHandle = Ref<Audio>;

    /**
     * @brief Represents the configuration and description of an audio device.
     *
     * This struct holds various properties that define the configuration of an audio device,
     * such as sample rate, format, and any other settings required to initialize the audio system.
     */
    struct AudioDeviceDescription {
        /**
        * @brief Flag to enable or disable the use of spatialization.
        *
        * If enabled, the device will allow spatial audio effects (3D positioning, attenuation, etc.).
        */
        bool EnableSpatialization{ true }; ///< Default to spatialization enabled

        auto WithSpatialization(bool value) -> AudioDeviceDescription&;
    };

    /**
    * @brief Represents an audio device that manages the audio engine, device configuration,
    *        and handles loading and accessing audio resources.
    *
    * This class is responsible for initializing and shutting down the audio system,
    * as well as loading and accessing audio resources (such as sounds and music) using
    * a resource pool. It interacts with the underlying audio engine and device for playback.
    */
    class AudioDevice {
    public:
        explicit AudioDevice();

        /**
        * @brief Initializes the audio device and its engine.
        *
        * This function sets up the audio engine and device, preparing the system to handle
        * audio playback. It must be called before any audio operations can take place.
        */
        auto Init() -> void;

        /**
        * @brief Shuts down the audio device and releases any resources.
        *
        * This function stops the audio engine, frees any allocated resources, and properly
        * shuts down the device to ensure the system is cleanly deinitialized.
        */
        auto Shutdown() -> void;

        /**
        * @brief Loads an audio resource based on the provided description.
        *
        * This function loads an audio file into memory based on the given `AudioDescription`
        * (which includes the file path and other parameters like volume). It returns a handle
        * to the loaded audio resource.
        *
        * @param description The `AudioDescription` that contains the source file and properties
        *                    for the audio.
        * @return A handle to the loaded audio resource.
        */
        auto LoadAudio( const AudioLoadDescription& description ) -> AudioHandle;


        MKT_NODISCARD static auto Create( const AudioDeviceDescription& description ) -> Scope_T<AudioDevice>;

    private:
        friend class Audio;
        friend class AudioListener;
        friend class AudioSource;

    private:
        ma_engine m_AudioEngine{};
        ma_device_config m_DeviceConfig{};

#ifndef MIKOTO_USE_AUDIO_ENGINE_INTERFACE
        ma_device m_Device{};
#endif

        ResourcePoolTyped<Audio> m_LoadedAudios{};
    };


}// namespace Mikoto


#endif//AUDIODEVICE_HH
