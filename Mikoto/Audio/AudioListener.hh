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


#ifndef MIKOTO_AUDIO_LISTENER_HH
#define MIKOTO_AUDIO_LISTENER_HH

#include <glm/glm.hpp>

#include <Common/Common.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {

    inline constexpr Int32 INVALID_LISTENER_INDEX{ -1 };


    /**
     * @brief Represents the listener in a 3D audio scene.
     *
     * The listener acts as the "ears" of the audio system. It defines
     * the position, orientation, and velocity used to spatialize sounds.
     */
    class AudioListener final {
    public:
        explicit AudioListener( Int32 index, float x, float y, float z );

        /**
        * @brief Sets the position of the listener in world space.
        *
        * @param x X-coordinate.
        * @param y Y-coordinate.
        * @param z Z-coordinate.
        */
        auto SetPosition( float x, float y, float z ) -> void;
        auto SetPosition( const Vec3F& pos ) -> void;

        auto SetUp( float x, float y, float z ) -> void;
        auto SetUp( const Vec3F& up ) -> void;

        auto SetForward( float x, float y, float z ) -> void;
        auto SetForward( const Vec3F& forward ) -> void;


        /**
        * @brief Gets the current position of the listener.
        *
        * @return The 3D position as a float3.
        */
        MKT_NODISCARD auto GetPosition() const -> const Vec3F&;
        MKT_NODISCARD auto GetUp() const -> const Vec3F&;
        MKT_NODISCARD auto GetForward() const -> const Vec3F&;
        MKT_NODISCARD auto GetVelocity() const -> const Vec3F&;

        MKT_NODISCARD auto GetIndex() const -> Int32;

        /**
        * @brief Sets the orientation of the listener using forward and up vectors.
        *
        * @param forwardX Forward vector X.
        * @param forwardY Forward vector Y.
        * @param forwardZ Forward vector Z.
        * @param upX Up vector X.
        * @param upY Up vector Y.
        * @param upZ Up vector Z.
        */
        auto SetOrientation( float forwardX, float forwardY, float forwardZ,
                             float upX, float upY, float upZ ) -> void;

        /**
        * @brief Sets the velocity of the listener (used for Doppler effect).
        *
        * @param x Velocity in X.
        * @param y Velocity in Y.
        * @param z Velocity in Z.
        */
        auto SetVelocity( float x, float y, float z ) -> void;
        auto SetVelocity( const Vec3F& vel ) -> void;

        /**
        * @brief Updates the underlying audio backend with the current listener state.
        * Should be called after setting position/orientation/velocity.
        */
        auto Apply() const -> void;

    private:
        Int32 m_Index{ INVALID_LISTENER_INDEX };

        Vec3F m_Up{ 0.0f, 1.0f, 0.0f };
        Vec3F m_Position{ 0.0f, 0.0f, 0.0f };
        Vec3F m_Forward{ 0.0f, 0.0f, -1.0f };
        Vec3F m_Velocity{ 0.0f, 0.0f, 0.0f };
    };

}

#endif // MIKOTO_AUDIO_LISTENER_HH
