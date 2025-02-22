/**
 * EditorCamera.hh
 * Created by kate on 8/29/23.
 * */

// C++ Standard Library
#include <cmath>
#include <utility>

// Third-Party Libraries
#include <glm/glm.hpp>

// Quaternions with extensions
#include <glm/gtx/quaternion.hpp>

// Project Headers
#include <Common/Common.hh>
#include <Common/Constants.hh>
#include <Core/Events/CoreEvents.hh>
#include <Core/Input/KeyCodes.hh>
#include <Core/Input/MouseCodes.hh>
#include <Core/System/EventSystem.hh>
#include <Core/System/InputSystem.hh>
#include <Library/Utility/Types.hh>
#include <Scene/Camera/SceneCamera.hh>

namespace Mikoto {
    SceneCamera::SceneCamera( const float fov, const float aspectRatio, const float nearClip, const float farClip)
        :   Camera{ glm::perspective(glm::radians(fov), aspectRatio, nearClip, farClip) }
    {
        m_NearClip = nearClip;
        m_FarClip = farClip;
        m_FieldOfView = fov;
        m_AspectRatio = aspectRatio;

        m_Position = glm::vec3{ -31.07f, 48.06f, 100.0f };
        m_ForwardVector = glm::vec3{ 15.0f, -10.0f, -30.0f };

        // Starting value for forward is the opposite of the
        // position to make the camera look at the center.
        // Forward vector must be normalized as it starts with
        // the position value which represents a position in world space
        m_ForwardVector = normalize(m_ForwardVector);

        UpdateViewMatrix();
    }

    auto SceneCamera::UpdateProjection() -> void {
        m_AspectRatio = m_ViewportWidth / m_ViewportHeight;

        SetProjectionFromType();
    }

    auto SceneCamera::UpdateViewMatrix() -> void {
        m_ViewMatrix = lookAt(m_Position,                      // The camera is located here
                                   m_Position + m_ForwardVector,    // This is where the camera is looking at
                                   m_CameraUpVector);               // This is the camera's up vector (normalized)

    }

    auto SceneCamera::ProcessMouseInput( const double timeStep) -> void {
        static InputSystem& inputSystem{ Engine::GetSystem<InputSystem>() };

        // Get mouse angle rotation values
        const glm::vec2 MOUSE_CURRENT_POSITION{ inputSystem.GetMouseX(), inputSystem.GetMouseY() };
        const glm::vec2 delta{ (MOUSE_CURRENT_POSITION - m_LastMousePosition) * 0.03f };

        m_LastMousePosition = MOUSE_CURRENT_POSITION;

        if (delta.x != 0.0f || delta.y != 0.0f) {
            // The Y offset of the mouse will dictate how much we rotate in the X axis
            m_Pitch = delta.y * m_RotationSpeed * static_cast<float>( timeStep );
            // The X offset of the mouse will dictate how much we rotate in the Y axis
            m_Yaw = delta.x * m_RotationSpeed * static_cast<float>( timeStep );

            // We can optionally rotate the camera on the X and Y axis
            m_Pitch = m_WantCameraRotationX ? m_Pitch : 0.0f;
            m_Yaw = m_WantCameraRotationY ? m_Yaw : 0.0f;

            const glm::quat quat{ glm::normalize(glm::cross(glm::angleAxis(-m_Pitch, m_RightVector), glm::angleAxis(-m_Yaw, GLM_UNIT_VECTOR_Y))) };
            m_ForwardVector = glm::rotate(quat, m_ForwardVector);
        }
    }

    auto SceneCamera::ProcessKeyboardInput(double timeStep) -> void {
        static InputSystem& inputSystem{ Engine::GetSystem<InputSystem>() };

        m_CameraUpVector = GLM_UNIT_VECTOR_Y;

        // Move forward
        if (inputSystem.IsKeyPressed(Key_W)) {
            m_Position += m_ForwardVector * m_MovementSpeed * static_cast<float>( timeStep );
        }

        // Move backwards
        if (inputSystem.IsKeyPressed(Key_S)) {
            m_Position -= m_ForwardVector * m_MovementSpeed * static_cast<float>( timeStep );
        }

        // Move left
        if (inputSystem.IsKeyPressed(Key_A)) {
            m_Position -= m_RightVector * m_MovementSpeed * static_cast<float>( timeStep );
        }

        // Move right
        if (inputSystem.IsKeyPressed(Key_D)) {
            m_Position += m_RightVector * m_MovementSpeed * static_cast<float>( timeStep );
        }

        // Move up
        if (inputSystem.IsKeyPressed(Key_Space) || inputSystem.IsKeyPressed(Key_E)) {
            m_Position.y += m_MovementSpeed * static_cast<float>( timeStep );
        }

        // Move down
        if (inputSystem.IsKeyPressed(Key_Q)) {
            m_Position.y -= m_MovementSpeed * static_cast<float>( timeStep );
        }
    }

    auto SceneCamera::UpdateState( const double timeStep) -> void {
        UpdateViewMatrix();
        UpdateProjection();

        if (!m_AllowCameraMovementAndRotation) {
            static InputSystem& inputSystem{ Engine::GetSystem<InputSystem>() };
            const glm::vec2 MOUSE_CURRENT_POSITION{ inputSystem.GetMouseX(), inputSystem.GetMouseY() };
            m_LastMousePosition = MOUSE_CURRENT_POSITION;
            return;
        }

        if (m_ProjectionType != PERSPECTIVE) {
            return;
        }

        m_CameraUpVector = GLM_UNIT_VECTOR_Y;
        m_RightVector = glm::cross(m_ForwardVector, m_CameraUpVector);

        ProcessMouseInput(timeStep);
        ProcessKeyboardInput(timeStep);
    }

    auto SceneCamera::SetViewportSize( const float width, const float height ) -> void {
        if ( m_ViewportWidth == width && m_ViewportHeight == height )
            return;

        m_ViewportWidth = width;
        m_ViewportHeight = height;

        UpdateProjection();
    }
    auto SceneCamera::WantRotation( const bool xAxis, const bool yAxis ) -> void {
        m_WantCameraRotationX = xAxis;
        m_WantCameraRotationY = yAxis;
    }
}// namespace Mikoto