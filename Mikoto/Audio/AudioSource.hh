//
// Created by zanet on 4/7/2025.
//

#ifndef AUDIOSOURCE_HH
#define AUDIOSOURCE_HH

#include <miniaudio.h>

#include <Common/Common.hh>
#include <Library/Data/ResourcePool.hh>
#include <Library/Utility/Types.hh>

#include "AudioDevice.hh"

namespace Mikoto {
    class AudioDevice;

    /**
   * @class AudioSource
   * @brief Represents an audio source that can be played, paused, or stopped.
   *
   * This class encapsulates the properties and behavior of an audio source,
   * including spatialization, volume, pitch, and looping control. AudioSource
   * instances are tied to a specific file path and must be allocated before playback.
   */
    class AudioSource final : public IResource {
    public:
        /**
        * @brief Constructs an AudioSource from the specified audio file path.
        *
        * @param device The audio device to which this source belongs.
        * @param path The file system path to the audio resource.
        */
        explicit AudioSource( AudioDevice* device, const Path_T& path );

        /**
         * @brief Gets the current volume level.
         *
         * @return A float value in the range [0.0, 1.0] representing the volume.
         */
        MKT_NODISCARD auto GetVolume() const -> float;

        /**
         * @brief Checks if the audio source is muted.
         *
         * @return True if muted, false otherwise.
         */
        MKT_NODISCARD auto IsMuted() const -> bool;

        /**
         * @brief Checks if the audio is set to loop.
         *
         * @return True if looping is enabled, false otherwise.
         */
        MKT_NODISCARD auto IsLooping() const -> bool;

        /**
         * @brief Mutes or unmutes the audio source.
         *
         * @param value True to mute the source, false to unmute it.
         */
        auto Mute( bool value ) -> void;

        /**
         * @brief Sets the playback volume.
         *
         * @param volume A float value in the range [0.0, 1.0]. Values <= 0 are ignored.
         */
        auto SetVolume( float volume ) -> void;

        /**
         * @brief Enables or disables looping for the audio playback.
         *
         * @param value True to enable looping, false to disable.
         */
        auto SetLooping( bool value ) -> void;

        /**
         * @brief Starts audio playback from the beginning.
         */
        auto Play() -> void;

        /**
        * @brief Pauses audio playback.
        */
        auto Pause() -> void;

        /**
        * @brief Resumes playback from the current paused position.
        */
        auto Continue() -> void;

        /**
        * @brief Stops playback and resets the position to the start.
        */
        auto Stop() -> void;

        /**
        * @brief Checks if the audio is currently playing.
        *
        * @return True if the audio is actively playing, false otherwise.
        */
        MKT_NODISCARD auto IsPlaying() const -> bool;

        /**
        * @brief Sets the pitch multiplier of the audio.
        *
        * @param pitch The desired pitch factor (1.0 = normal pitch).
        */
        auto SetPitch( float pitch ) -> void;

        /**
        * @brief Gets the current pitch multiplier.
        *
        * @return The current pitch as a float.
        */
        MKT_NODISCARD auto GetPitch() const -> float;

        /**
        * @brief Enables or disables 3D spatialization for the audio source.
        *
        * @param state True to enable spatialization, false to disable.
        */
        auto SetSpatialization( bool state ) -> void;

        /**
       * @brief Sets the 3D position of the audio source.
       *
       * @param x X coordinate in 3D space.
       * @param y Y coordinate in 3D space.
       * @param z Z coordinate in 3D space.
       *
       * @note This only has an effect if spatialization is enabled.
       */
        auto SetPosition( float x, float y, float z ) -> void;

        /**
       * @brief Checks if spatialization is enabled.
       *
       * @return True if spatialization is active, false otherwise.
       */
        MKT_NODISCARD auto IsSpatialized() const -> bool;

        /**
       * @brief Allocates internal audio resources and prepares for playback.
       *
       * @note Must be called before invoking Play().
       */
        auto Allocate() -> void override;

        /**
       * @brief Releases internal audio resources and frees memory.
       *
       * @note Should be called when the AudioSource is no longer needed.
       */
        auto Release() -> void override;

    private:
        friend class Audio;

    private:
        ma_sound m_Sound{};
        Path_T m_Path{};

        float m_Volume{};
        bool m_Muted{};
        float m_Pitch{ 1.0f };
        bool m_IsLooping{ false };
        bool m_IsSpatialized{ false };
        bool m_IsPlaying{ false };

        AudioDevice* m_Device{ nullptr };
    };

}// namespace Mikoto


#endif//AUDIOSOURCE_HH
