//
// Created by zanet on 3/28/2025.
//

#ifndef AUDIORESOURCE_HH
#define AUDIORESOURCE_HH

#include <Library/Utility/Types.hh>

namespace Mikoto {

    /**
    * @brief Defines the properties of an audio resource.
    *
    * This structure encapsulates details such as the source file and volume level
    * for an audio resource.
    */
    struct AudioLoadDescription {
        const File* AudioFile{}; ///< The file path to the audio source.
        float Volume{ 1.4f };///< The default volume level.

        /**
         * @brief Sets the audio source file.
         *
         * @param source The path to the audio file.
         * @return Reference to the modified AudioDescription.
         */
        auto WithFile( const File* source ) -> AudioLoadDescription&;

        /**
         * @brief Sets the volume level.
         *
         * @param volume The volume value (range typically 0.0 to 1.0).
         * @return Reference to the modified AudioDescription.
         */
        auto SetVolume( float volume ) -> AudioLoadDescription&;
    };
}
#endif //AUDIORESOURCE_HH
