/**
 * @file SceneCamera.hh
 * @brief Definition of the Editor Camera class
 * @details Defines the camera used for editing scenes
 * @date 8/29/23.
 * @author kate
 * */

#ifndef MIKOTO_EDITOR_CAMERA_HH
#define MIKOTO_EDITOR_CAMERA_HH

// Third-Party Libraries
#include "glm/glm.hpp"

// Project Headers
#include <Platform/Window.hh>

#include "Scene/Camera.hh"
#include "Common/Common.hh"
#include "Library/Random/Random.hh"
#include "Library/Utility/Types.hh"

namespace Mikoto {
    /**
     * @class SceneCamera. Camera used for editing scenes.
     * @brief This camera is based off OpenGL coordinate system which is right handed
     * Will eventually be changed so that it is adjusted properly to the Vulkan backend
     * */
    class SceneCamera final : public Camera {
    public:
        /**
         * @brief Default constructor. Initializes an instance of EditorCamera with default values.
         * */
        explicit SceneCamera() = default;


        /**
         * @brief Constructs an EditorCamera with specific projection parameters.
         * @param fov Field of view.
         * @param aspectRatio Aspect ratio.
         * @param nearClip Near clipping plane.
         * @param farClip Far clipping plane.
         * */
        SceneCamera( float fov, float aspectRatio, float nearClip, float farClip );

        auto SetTargetWindow( const Window* window ) -> void;


        /**
         * @brief Updates the camera state using the elapsed time.
         * @param timeStep Elapsed time since the last frame.
         * */
        auto UpdateState( double timeStep ) -> void;


        /**
         * @brief Sets the size of the viewport for the camera.
         * @param width The width of the viewport.
         * @param height The height of the viewport.
         * */
        auto SetViewportSize( float width, float height ) -> void;


        /**
         * @brief Sets the movement speed of the camera.
         * This value determines the speed at which we can move the camera
         * with Key_W, Key_A, Key_S and Key_D.
         * @param value The new movement speed value to set.
         * */
        auto SetMovementSpeed( float value ) -> void { m_MovementSpeed = value; }


        /**
         * @brief Sets the rotation speed of the camera. Sets the speed at which we can rotate the camera with the mouse.
         * @param value The new rotation speed value to set.
         * */
        auto SetRotationSpeed( float value ) -> void { m_RotationSpeed = value; }
        auto SetDampingFactor( float value ) -> void { m_DampingFactor = value; }


        /**
         * @brief Sets the field of view of the camera.
         * @param value The new field of view value to set.
         * */
        auto SetFieldOfView( float value ) -> void { m_FieldOfView = value; }


        auto WantRotation( bool xAxis, bool yAxis ) -> void;


        /**
         * @brief Sets the far clipping plane distance of the camera.
         * @param value The new far clipping plane value to set.
         * */
        auto SetFarPlane( float value ) -> void { m_FarClip = value; }


        /**
         * @brief Sets the near clipping plane distance of the camera.
         * @param value The new near clipping plane value to set.
         * */
        auto SetNearPlane( float value ) -> void { m_NearClip = value; }


        /**
         * @brief Enables or disables camera movement and rotation.
         * @param value If true, camera movement and rotation are allowed; otherwise, they are not.
         * */
        auto EnableCamera( const bool value ) { m_AllowCameraMovementAndRotation = value; }

    private:

        auto UpdateViewMatrix() -> void;

        auto Interpolate( double timeStep ) -> void;

        auto ProcessMouseInput( double timeStep ) -> void;
        auto ProcessKeyboardInput( double timeStep ) -> void;

    private:
        // This kind of camera responds to input from a window
        // in order to compute translations and rotations
        const Window* m_TargetWindow{ nullptr };


        Vec3F m_TargetPosition{ 0.0f, 0.0f, 0.0f };
        Vec3F m_TargetForwardVector{ 0.0f, 0.0f, -1.0f };

        // Controls how quickly the camera moves towards the target. Higher values mean faster smoothing.
        float m_DampingFactor{ 5.0f };

        bool m_WantCameraRotationX{ true };
        bool m_WantCameraRotationY{ true };

        Vec2F m_LastMousePosition{ 0.0f, 0.0f };

        float m_RotationSpeed{ 2.f };
        float m_MovementSpeed{ 2.f };

        // Avoid speedy rotations. Compensate rotation speed
        float m_RotationFactor{ 0.03f };

        bool m_AllowCameraMovementAndRotation{ false };
    };
}// namespace Mikoto

#endif// MIKOTO_EDITOR_CAMERA_HH