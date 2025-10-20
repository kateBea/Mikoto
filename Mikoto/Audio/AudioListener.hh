//
// Created by zanet on 4/7/2025.
//

#ifndef AUDIOLISTENER_HH
#define AUDIOLISTENER_HH

#include <glm/glm.hpp>

#include <Common/Common.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {
    /**
     * @brief Represents the listener in a 3D audio scene.
     *
     * The listener acts as the "ears" of the audio system. It defines
     * the position, orientation, and velocity used to spatialize sounds.
     */
    class AudioListener final {
    public:
        /**
        * @brief Sets the position of the listener in world space.
        *
        * @param x X-coordinate.
        * @param y Y-coordinate.
        * @param z Z-coordinate.
        */
        auto SetPosition( float x, float y, float z ) -> void;

        /**
        * @brief Gets the current position of the listener.
        *
        * @return The 3D position as a float3.
        */
        MKT_NODISCARD auto GetPosition() const -> const glm::vec3 &;

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

        /**
        * @brief Updates the underlying audio backend with the current listener state.
        * Should be called after setting position/orientation/velocity.
        */
        auto Apply() const -> void;

    private:
        Vec3F m_Position{ -15.0f, 0.0f, 0.0f };
        Vec3F m_Forward{ 0.0f, 0.0f, -1.0f };
        Vec3F m_Up{ 0.0f, 1.0f, 0.0f };
        Vec3F m_Velocity{ 0.0f, 0.0f, 0.0f };
    };

}// namespace Mikoto


#endif//AUDIOLISTENER_HH
