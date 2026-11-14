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

#ifndef MIKOTO_AUDIO_AUDIO_SOURCE_HH
#define MIKOTO_AUDIO_AUDIO_SOURCE_HH

#include <EASTL/unique_ptr.h>

#include <miniaudio.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/Resource.hh>

#include <Filesystem/Path.hh>

#include <Audio/AudioSettings.hh>

namespace mikoto::audio {

    class AudioDevice;
    class AudioListener;
    class AudioMixer;

    /**
     * @brief A controllable playback instance created from an Audio clip.
     */
    class AudioSource final : public core::INonCopyable, public core::Resource {
    public:
        auto SetOutput( AudioMixer* mixer, core::u64 group = 0 ) -> bool;
        /**
         * @brief Constructs an uninitialized playback source.
         *
         * @param device Borrowed device; use AudioDevice::CreateSource() to allocate playback
         * state.
         * @param path Audio file path copied into the source.
         */
        explicit AudioSource( AudioDevice* device, const filesystem::Path& path );
        AudioSource( AudioDevice* device, ma_data_source_base* source );


        /**
         * @brief Releases playback state and unregisters the source from its device.
         */
        ~AudioSource() override;


        /**
         * @brief Gets the sanitized playback configuration.
         *
         * @return Borrowed settings reference, valid until this source is destroyed.
         */
        auto GetSettings() const -> const SourceSettings&;


        /**
         * @brief Sanitizes and stores settings, applying them immediately when allocated.
         *
         * @param settings Requested playback and attenuation settings.
         */
        auto SetSettings( SourceSettings settings ) -> void;


        /**
         * @brief Creates an independent source with the same file, settings, and position.
         *
         * @return New stopped source, or an empty handle when unavailable.
         * @note Playback cursor, velocity, pause state, and pinned listener are not copied.
         */
        auto Clone() const -> core::Ref<AudioSource>;


        /**
         * @brief Seeks the playback cursor and resets spatial processing history on success.
         *
         * @param seconds Requested time, clamped to zero and the known duration.
         * @return True on success; false if unallocated or rejected by the backend.
         */
        auto Seek( float seconds ) -> bool;


        /**
         * @brief Checks the cached pause state of an allocated source.
         *
         * @return True only when allocated and explicitly paused.
         */
        auto IsPaused() const -> bool;


        /**
         * @brief Updates position and derives Doppler velocity from consecutive samples.
         *
         * @param position World-space position.
         * @param timeStep Elapsed seconds; invalid intervals or the first sample produce zero
         * velocity.
         */
        auto UpdatePosition( const core::float3& position, float timeStep ) -> void;


        /**
         * @brief Stores velocity and applies it to allocated playback state.
         *
         * @param velocity World-space units per second.
         */
        auto SetVelocity( const core::float3& velocity ) -> void;


        /**
         * @brief Gets the cached world-space position.
         *
         * @return Borrowed position reference valid for this source's lifetime.
         */
        auto GetPosition() const -> const core::float3&;


        /**
         * @brief Gets the cached Doppler velocity.
         *
         * @return Borrowed velocity reference in world-space units per second.
         */
        auto GetVelocity() const -> const core::float3&;


        /**
         * @brief Changes output gain without stopping the playback cursor.
         *
         * @param value True to silence output while retaining the configured volume.
         */
        auto Mute( bool value ) -> void;


        /**
         * @brief Sets sanitized linear gain.
         *
         * @param volume Gain clamped to [0, 20].
         */
        auto SetVolume( float volume ) -> void;


        /**
         * @brief Adds to the configured gain and sanitizes the result.
         *
         * @param delta Signed gain increment.
         */
        auto IncreaseVolume( float delta ) -> void;


        /**
         * @brief Subtracts from the configured gain and sanitizes the result.
         *
         * @param delta Signed gain decrement.
         */
        auto DecreaseVolume( float delta ) -> void;


        /**
         * @brief Controls automatic replay at the end of the file.
         *
         * @param value True to enable looping.
         */
        auto SetLooping( bool value ) -> void;


        /**
         * @brief Pins an allocated source to a listener slot.
         *
         * @param listener Listener belonging to this device. Null or invalid indices are ignored.
         * @note Use ResetListener() to restore automatic selection; this call stores the slot
         * index.
         */
        auto SetListener( AudioListener* listener ) -> void;


        /**
         * @brief Clears listener pinning and selects the closest enabled listener when allocated.
         */
        auto ResetListener() -> void;


        /**
         * @brief Starts playback, or resumes from the retained cursor.
         *
         * @note Already playing or unallocated sources are unchanged. Natural end-of-file restarts
         * at zero.
         */
        auto Play() -> void;


        /**
         * @brief Stops a playing source while retaining its cursor and marking it paused.
         */
        auto Pause() -> void;


        /**
         * @brief Invokes Play() to resume or start playback.
         */
        auto Continue() -> void;


        /**
         * @brief Stops playback, rewinds to zero, and clears pause and spatial history.
         */
        auto Stop() -> void;


        /**
         * @brief Sets sanitized playback pitch.
         *
         * @param pitch Frequency multiplier clamped to [0.01, 4].
         */
        auto SetPitch( float pitch ) -> void;


        /**
         * @brief Enables listener-relative headphone HRTF processing.
         *
         * @param state True for 3D audio; false preserves the stereo image for music/UI.
         * @note Distance attenuation and Doppler remain available for 3D voices. The 256-frame
         * adapter adds about 5.33 ms at 48 kHz. Scene occlusion and reflections are not simulated.
         */
        auto SetSpatialization( bool state ) -> void;


        /**
         * @brief Stores position and updates allocated playback and listener selection.
         *
         * @param pos World-space position.
         * @note Does not derive velocity; use UpdatePosition() for moving sources.
         */
        auto SetPosition( const core::float3& pos ) -> void;


        /**
         * @brief Stores position and updates allocated playback and listener selection.
         *
         * @param x World-space X coordinate.
         * @param y World-space Y coordinate.
         * @param z World-space Z coordinate.
         * @note Does not derive velocity; use UpdatePosition() for moving sources.
         */
        auto SetPosition( float x, float y, float z ) -> void;


        /**
         * @brief Sets sanitized Doppler scaling.
         *
         * @param value Factor clamped to [0, 5].
         * @return Applied factor.
         */
        auto SetDopplerFactor( float value ) -> float;


        /**
         * @brief Gets the configured gain, including while muted.
         *
         * @return Linear gain in [0, 20].
         */
        MKT_NODISCARD auto GetVolume() const -> float;


        /**
         * @brief Checks whether output is muted.
         *
         * @return True when muted.
         */
        MKT_NODISCARD auto IsMuted() const -> bool;


        /**
         * @brief Checks whether looping is enabled.
         *
         * @return True when the source repeats at end-of-file.
         */
        MKT_NODISCARD auto IsLooping() const -> bool;


        /**
         * @brief Queries the native playback state.
         *
         * @return True when allocated and playing; false after natural completion or teardown.
         */
        MKT_NODISCARD auto IsPlaying() const -> bool;


        /**
         * @brief Gets the configured pitch multiplier.
         *
         * @return Sanitized frequency multiplier.
         */
        MKT_NODISCARD auto GetPitch() const -> float;


        /**
         * @brief Checks whether spatial processing is configured.
         *
         * @return True when HRTF spatialization is enabled.
         */
        MKT_NODISCARD auto IsSpatialized() const -> bool;


        /**
         * @brief Queries the clip duration.
         *
         * @return Duration in seconds, or zero when unavailable.
         */
        MKT_NODISCARD auto GetAudioDuration() const -> float;


        /**
         * @brief Queries the current playback cursor.
         *
         * @return Cursor time in seconds, or zero when unavailable.
         */
        MKT_NODISCARD auto GetCurrentProgress() const -> float;


        /**
         * @brief Compares source object identity.
         *
         * @param source Source to compare; may be null.
         * @return True only when source points to this object.
         */
        MKT_NODISCARD auto IsSameSource( const AudioSource* source ) const -> bool;


        /**
         * @brief Compares the stored audio paths.
         *
         * @param source Source to compare; may be null.
         * @return True for equal paths, false for null. This does not compare decoded samples.
         */
        MKT_NODISCARD auto IsSameAudio( const AudioSource* source ) const -> bool;


        /**
         * @brief Gets the configured Doppler scale.
         *
         * @return Sanitized Doppler multiplier.
         */
        MKT_NODISCARD auto GetDopplerFactor() const -> float;


        /**
         * @brief Gets the supported source gain limit.
         *
         * @return Maximum linear gain, 20.
         */
        MKT_NODISCARD static auto GetMaxVolume() -> float;


    private:
        /**
         * @brief Allocates decoding and spatial playback state and registers the source.
         *
         * @note Requires an initialized device and regular file. Failed initialization leaves
         * IsReady() false.
         */
        auto Initialize() -> void override;


        /**
         * @brief Unregisters and releases native playback state, then detaches from the device.
         */
        auto Destroy() -> void override;


        /**
         * @brief Selects the closest enabled listener for an allocated, unpinned source.
         */
        auto UpdateListenerSelection() -> void;

    private:
        friend class Audio;
        friend class AudioDevice;
        friend class AudioMixer;

    private:
        struct Impl;

        // Native playback state and source asset.
        eastl::unique_ptr<Impl> mImpl;
        filesystem::Path mPath{};
        ma_data_source_base* mDataSource{}; // Borrowed; streaming owner releases this source first.

        // Playback configuration.
        SourceSettings mSettings{};

        // Spatial state used to update position and Doppler velocity.
        core::float3 mPosition{};
        core::float3 mVelocity{};
        bool mHasPosition{};

        // Playback and listener selection.
        bool mPaused{};
        bool mListenerPinned{};

        // Device registration; the device releases sources before shutting down.
        AudioDevice* mDevice{ nullptr };
        AudioMixer* mMixer{};
        core::u64 mMixerGroup{};
    };

    using AudioSourceHandle = core::Ref<AudioSource>;

}// namespace mikoto::audio

#endif// MIKOTO_AUDIO_AUDIO_SOURCE_HH