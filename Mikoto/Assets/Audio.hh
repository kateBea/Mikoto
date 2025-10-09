//
// Created by zanet on 1/28/2025.
//

#ifndef AUDIO_HH
#define AUDIO_HH

#include <miniaudio.h>

#include <Audio/AudioDeviceObject.hh>
#include <Audio/AudioSource.hh>
#include <Audio/AudioUtility.hh>
#include <Library/IO/File.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {

    using AudioSourceHandle = Ref<AudioSource>;

    /**
    * @class AudioDeviceObject
    * @brief Represents a resource associated with an audio device.
    *
    * This class serves as a base for audio assets. To reproduce them,
    * we need to create a source and a listener. Audio assets are loaded
    * to the main device which is part of the Audio service.
    */
    class Audio final : public AudioDeviceObject {
    public:
        /**
         * @brief Constructs an Audio object with a given description.
         *
         * @param description The audio description containing source file and volume settings.
         */
        explicit Audio( const AudioLoadDescription& description );

        auto GetFile() const -> const File* { return m_FileSource; }

        auto CreateSource() -> AudioSourceHandle;

    private:
        /**
        * @brief Releases system resources associated with the audio.
        *
        * This function is called when the audio resource is removed from the resource pool.
        */
        auto Release() -> void override;

        auto Allocate() -> void override;

        const File* m_FileSource{};

        ResourcePoolTyped<AudioSource> m_Sources{};
    };

}// namespace Mikoto


#endif//AUDIO_HH
