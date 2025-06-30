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

#include "Camera.hh"
#include "Common/Common.hh"
#include "Library/Random/Random.hh"

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
         *
         * @param fov Field of view.
         * @param aspectRatio Aspect ratio.
         * @param nearClip Near clipping plane.
         * @param farClip Far clipping plane.
         * */
        SceneCamera( float fov, float aspectRatio, float nearClip, float farClip );

        auto SetTargetWindow( const Window* window ) -> void;


        /**
         * @brief Updates the camera state using the elapsed time.
         *
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
         *
         * @param value The new far clipping plane value to set.
         * */
        auto SetFarPlane( float value ) -> void { m_FarClip = value; }


        /**
         * @brief Sets the near clipping plane distance of the camera.
         *
         * @param value The new near clipping plane value to set.
         * */
        auto SetNearPlane( float value ) -> void { m_NearClip = value; }


        /**
         * @brief Enables or disables camera movement and rotation.
         *
         * @param value If true, camera movement and rotation are allowed; otherwise, they are not.
         * */
        auto EnableCamera( const bool value ) { m_AllowCameraMovementAndRotation = value; }


        MKT_NODISCARD constexpr static auto GetMinMovementSpeed() -> float { return 15.0f; }
        MKT_NODISCARD constexpr static auto GetMaxMovementSpeed() -> float { return 250.0f; }

        MKT_NODISCARD constexpr static auto GetMinRotationSpeed() -> float { return 15.0f; }
        MKT_NODISCARD constexpr static auto GetMaxRotationSpeed() -> float { return 250.0f; }

        MKT_NODISCARD constexpr static auto GetMaxNearClip() -> float { return 2500.0f; }
        MKT_NODISCARD constexpr static auto GetMinNearClip() -> float { return 0.01f; }

        MKT_NODISCARD constexpr static auto GetMaxFarClip() -> float { return 10000.0f; }
        MKT_NODISCARD constexpr static auto GetMinFarClip() -> float { return 1000.0f; }

        MKT_NODISCARD constexpr static auto GetMaxFov() -> float { return 90.0f; }
        MKT_NODISCARD constexpr static auto GetMinFov() -> float { return 15.0f; }

        MKT_NODISCARD constexpr static auto GetMaxDampingFactor() -> float { return 100.0f; }
        MKT_NODISCARD constexpr static auto GetMinDampingFactor() -> float { return 1.0f; }


    private:
        /**
         * @brief Updates the projection matrix based on stored parameters.
         * */
        auto UpdateProjection() -> void;


        /**
      * @brief Updates the view matrix of the camera.
      * */
        auto UpdateViewMatrix() -> void;

        /**
         * @brief Processes mouse input to update the camera's state.
         *
         * @param timeStep The time elapsed since the last frame.
         * */
        auto ProcessMouseInput( double timeStep ) -> void;


        /**
         * @brief Processes keyboard input to update the camera's state.
         *
         * @param timeStep The time elapsed since the last frame.
         * */
        auto ProcessKeyboardInput( double timeStep ) -> void;

        auto Interpolate( double timeStep ) -> void;


    private:
        // This kind of camera responds to input from a window
        // in order to compute translations and rotations
        const Window* m_TargetWindow{ nullptr };


        glm::vec3 m_TargetPosition{ 0.0f, 0.0f, 0.0f };
        glm::vec3 m_TargetForwardVector{ 0.0f, 0.0f, -1.0f };

        // Controls how quickly the camera moves towards the target. Higher values mean faster smoothing.
        float m_DampingFactor{ 5.0f };

        bool m_WantCameraRotationX{ true };
        bool m_WantCameraRotationY{ true };

        glm::vec2 m_LastMousePosition{ 0.0f, 0.0f };

        float m_RotationSpeed{ GetMinRotationSpeed() };
        float m_MovementSpeed{ GetMinMovementSpeed() };

        // Avoid speedy rotations. Compensate rotation speed
        float m_RotationFactor{ 0.03f };

        bool m_AllowCameraMovementAndRotation{ false };
    };
}// namespace Mikoto

#endif// MIKOTO_EDITOR_CAMERA_HH