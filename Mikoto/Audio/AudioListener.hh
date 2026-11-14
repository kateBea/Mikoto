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

#ifndef MIKOTO_AUDIO_AUDIO_LISTENER_HH
#define MIKOTO_AUDIO_AUDIO_LISTENER_HH

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

namespace mikoto::audio {

    /**
     * @brief Sentinel used when a listener is not assigned a device slot.
     */
    inline constexpr core::i32 kInvalidListenerIndex{ -1 };
    class AudioDevice;

    /**
     * @brief Describes a listener pose and velocity consumed by 3D audio spatialization.
     */
    class AudioListener final {
    public:
        /**
         * @brief Stores a listener slot and initial pose without applying it.
         *
         * @param index Device listener slot.
         * @param x Initial world-space X coordinate.
         * @param y Initial world-space Y coordinate.
         * @param z Initial world-space Z coordinate.
         * @param device Borrowed device, or null for a detached listener.
         */
        explicit AudioListener( core::i32 index, float x, float y, float z, AudioDevice* device = nullptr );


        /**
         * @brief Sets participation in listener selection and immediately applies all cached state.
         *
         * @param enabled True to enable this listener.
         */
        auto SetEnabled( bool enabled ) -> void;


        /**
         * @brief Caches position and derives Doppler velocity; call Apply() to publish.
         *
         * @param position World-space position.
         * @param timeStep Elapsed seconds; first samples and invalid intervals produce zero
         * velocity.
         */
        auto UpdatePosition( const core::float3& position, float timeStep ) -> void;


        /**
         * @brief Clears the borrowed device pointer, making Apply() a no-op.
         *
         * @note The stored slot index and pose remain unchanged.
         */
        auto Detach() -> void;


        /**
         * @brief Caches world-space position without changing velocity; call Apply() to publish.
         *
         * @param x World-space X coordinate.
         * @param y World-space Y coordinate.
         * @param z World-space Z coordinate.
         */
        auto SetPosition( float x, float y, float z ) -> void;


        /**
         * @brief Caches world-space position without changing velocity; call Apply() to publish.
         *
         * @param pos World-space position.
         */
        auto SetPosition( const core::float3& pos ) -> void;


        /**
         * @brief Caches a normalized up direction; call Apply() to publish.
         *
         * @param x Up-vector X component.
         * @param y Up-vector Y component.
         * @param z Up-vector Z component.
         */
        auto SetUp( float x, float y, float z ) -> void;


        /**
         * @brief Caches a normalized up direction; call Apply() to publish.
         *
         * @param up Requested direction; invalid or near-zero vectors retain the previous
         * direction.
         */
        auto SetUp( const core::float3& up ) -> void;


        /**
         * @brief Caches a normalized forward direction; call Apply() to publish.
         *
         * @param x Forward-vector X component.
         * @param y Forward-vector Y component.
         * @param z Forward-vector Z component.
         */
        auto SetForward( float x, float y, float z ) -> void;


        /**
         * @brief Caches a normalized forward direction; call Apply() to publish.
         *
         * @param forward Requested direction; invalid or near-zero vectors retain the previous
         * direction.
         */
        auto SetForward( const core::float3& forward ) -> void;


        /**
         * @brief Gets the cached position.
         *
         * @return Borrowed world-space position reference.
         */
        MKT_NODISCARD auto GetPosition() const -> const core::float3&;


        /**
         * @brief Gets the cached up direction.
         *
         * @return Borrowed up-vector reference.
         */
        MKT_NODISCARD auto GetUp() const -> const core::float3&;


        /**
         * @brief Gets the cached forward direction.
         *
         * @return Borrowed forward-vector reference.
         */
        MKT_NODISCARD auto GetForward() const -> const core::float3&;


        /**
         * @brief Gets the cached velocity.
         *
         * @return Borrowed velocity reference in world-space units per second.
         */
        MKT_NODISCARD auto GetVelocity() const -> const core::float3&;


        /**
         * @brief Gets the stored listener slot.
         *
         * @return Constructor-supplied index; Detach() does not change it.
         */
        MKT_NODISCARD auto GetIndex() const -> core::i32;


        /**
         * @brief Caches a normalized, orthogonal forward/up basis; call Apply() to publish.
         *
         * @param forwardX Forward X component.
         * @param forwardY Forward Y component.
         * @param forwardZ Forward Z component.
         * @param upX Up X component.
         * @param upY Up Y component.
         * @param upZ Up Z component.
         */
        auto SetOrientation( float forwardX, float forwardY, float forwardZ,
                             float upX, float upY, float upZ ) -> void;


        /**
         * @brief Caches Doppler velocity; call Apply() to publish.
         *
         * @param x X velocity in world-space units per second.
         * @param y Y velocity in world-space units per second.
         * @param z Z velocity in world-space units per second.
         */
        auto SetVelocity( float x, float y, float z ) -> void;


        /**
         * @brief Caches Doppler velocity; call Apply() to publish.
         *
         * @param vel Velocity vector in world units per second.
         */
        auto SetVelocity( const core::float3& vel ) -> void;


        /**
         * @brief Publishes cached enabled state, position, orientation, and velocity to the device.
         *
         * @note Does nothing when detached, the device is uninitialized, or the slot is out of
         * range.
         */
        auto Apply() const -> void;


    private:
        core::i32 mIndex{ kInvalidListenerIndex };
        AudioDevice* mDevice{};
        bool mEnabled{ true };
        bool mHasPosition{};

        core::float3 mUp{ 0.0f, 1.0f, 0.0f };
        core::float3 mPosition{ 0.0f, 0.0f, 0.0f };
        core::float3 mForward{ 0.0f, 0.0f, -1.0f };
        core::float3 mVelocity{ 0.0f, 0.0f, 0.0f };
    };
}// namespace mikoto::audio

#endif// MIKOTO_AUDIO_AUDIO_LISTENER_HH
