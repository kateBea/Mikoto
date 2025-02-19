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
        SceneCamera(float fov, float aspectRatio, float nearClip, float farClip);


        /**
         * @brief Updates the camera state using the elapsed time.
         *
         * @param timeStep Elapsed time since the last frame.
         * */
        auto UpdateState(double timeStep) -> void;


        /**
         * @brief Retrieve the position of the camera.
         * @return The position of the camera.
         * */
        MKT_NODISCARD auto GetPosition() const -> const glm::vec3& { return m_Position; }
        MKT_NODISCARD auto GetFOV() const -> float { return m_FieldOfView; }
        MKT_NODISCARD auto GetNearPlane() const -> float { return m_NearClip; }
        MKT_NODISCARD auto GetFarPlane() const -> float { return m_FarClip; }


        /**
         * @brief Retrieve the view matrix of the camera.
         * */
        MKT_NODISCARD auto GetViewMatrix() const -> const glm::mat4& { return m_ViewMatrix; }


        /**
         * @brief Retrieve the view projection matrix.
         * @return The result of the projection matrix multiplied by the view matrix.
         * */
        MKT_NODISCARD auto GetViewProjection() const -> glm::mat4 { return GetProjection() * m_ViewMatrix; }


        /**
         * @brief Sets the size of the viewport for the camera.
         * @param width The width of the viewport.
         * @param height The height of the viewport.
         * */
        auto SetViewportSize(float width, float height) -> void;


        /**
         * @brief Sets the movement speed of the camera.
         * This value determines the speed at which we can move the camera
         * with Key_W, Key_A, Key_S and Key_D.
         * @param value The new movement speed value to set.
         * */
        auto SetMovementSpeed(float value) -> void { m_MovementSpeed = value; }


        /**
         * @brief Sets the rotation speed of the camera. Sets the speed at which we can rotate the camera with the mouse.
         * @param value The new rotation speed value to set.
         * */
        auto SetRotationSpeed(float value) -> void { m_RotationSpeed = value; }


        /**
         * @brief Sets the field of view of the camera.
         * @param value The new field of view value to set.
         * */
        auto SetFieldOfView(float value) -> void { m_FieldOfView = value; }


        /**
         * @brief Sets the far clipping plane distance of the camera.
         *
         * @param value The new far clipping plane value to set.
         * */
        auto SetFarPlane(float value) -> void { m_FarClip = value; }


        /**
         * @brief Sets the near clipping plane distance of the camera.
         *
         * @param value The new near clipping plane value to set.
         * */
        auto SetNearPlane(float value) -> void { m_NearClip = value; }


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
        auto ProcessMouseInput(double timeStep) -> void;


        /**
         * @brief Processes keyboard input to update the camera's state.
         *
         * @param timeStep The time elapsed since the last frame.
         * */
        auto ProcessKeyboardInput(double timeStep) -> void;


    private:
        // [Projection Data]
        float m_NearClip{ 0.1f };
        float m_FarClip{ 1000.0f };
        float m_FieldOfView{ 45.0f };
        float m_AspectRatio{ 1.778f }; // 1920 / 1080

        // [Matrices]
        glm::mat4 m_ViewMatrix{};
        glm::mat4 m_ProjectionMatrix{};

        // [Vectors]
        glm::vec3 m_Position{ -15.0f, 5.0f, 30.0f };
        glm::vec3 m_RightVector{ 1.0f, 0.0f, 0.0f };
        glm::vec3 m_CameraUpVector{ 0.0f, 1.0f, 0.0f };
        glm::vec3 m_ForwardVector{ 15.0f, -5.0f, -30.0f };

        // [Rotations]
        float m_Yaw{ 0.0f };
        float m_Roll{ 0.0f };
        float m_Pitch{ 0.0f };

        // [Misc]
        GlobalUniqueID m_Guid{};

        float m_ViewportWidth{ 1920 };
        float m_ViewportHeight{ 1080 };

        glm::vec2 m_LastMousePosition{ 0.0f, 0.0f };

        float m_RotationSpeed{ GetMinRotationSpeed() };
        float m_MovementSpeed{ GetMinMovementSpeed() };

        bool m_AllowCameraMovementAndRotation{ false };
    };
}

#endif // MIKOTO_EDITOR_CAMERA_HH