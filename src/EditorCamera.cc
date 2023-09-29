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
#include "Renderer/Camera/EditorCamera.hh"
#include <Core/KeyCodes.hh>
#include <Core/MouseButtons.hh>
#include <Platform/InputManager.hh>
#include <Utility/Common.hh>
#include <Utility/Constants.hh>

namespace Mikoto {
    EditorCamera::EditorCamera(float fov, float aspectRatio, float nearClip, float farClip)
        :   Camera{ glm::perspective(glm::radians(fov), aspectRatio, nearClip, farClip) }
        ,   m_FieldOfView{ fov }, m_AspectRatio{ aspectRatio }, m_NearClip{ nearClip }, m_FarClip{ farClip }
    {
        UpdateViewMatrix();
    }

    auto EditorCamera::UpdateProjection() -> void {
        m_AspectRatio = m_ViewportWidth / m_ViewportHeight;
        SetProjection(glm::perspective(glm::radians(m_FieldOfView), m_AspectRatio, m_NearClip, m_FarClip));
    }

    auto EditorCamera::UpdateViewMatrix() -> void {
        m_ViewMatrix = glm::lookAt(m_Position,                      // The camera is located here
                                   m_Position + m_ForwardVector,    // This is where the camera is looking at
                                   m_CameraUpVector);               // This is the camera's up vector (normalized)

    }

    auto EditorCamera::ProcessMouseInput(double timeStep) -> void {
        // Get mouse angle rotation values
        const glm::vec2 MOUSE_CURRENT_POSITION{ InputManager::GetMouseX(), InputManager::GetMouseY() };
        glm::vec2 delta{ (MOUSE_CURRENT_POSITION - m_LastMousePosition) * 0.03f };
        m_LastMousePosition = MOUSE_CURRENT_POSITION;

        // TODO: temporary (avoid camera jumping)
        // Offset that indicates there will be a camera jump when rotating
        static constexpr auto JUMP_THRESHOLD{ 8.0f };
        if (std::abs(glm::length(delta)) > JUMP_THRESHOLD)
            return;

        if (!InputManager::IsMouseKeyPressed(MouseButton::Mouse_Button_Right))
            return;

        // Perform rotation
        if (delta.x != 0.0f || delta.y != 0.0f) {
            // The Y offset of the mouse will dictate how much we rotate in the X axis
            m_Pitch = delta.y * m_RotationSpeed * (float)timeStep;
            // The X offset of the mouse will dictate how much we rotate in the Y axis
            m_Yaw = delta.x * m_RotationSpeed * (float)timeStep;

            glm::quat q{ glm::normalize(glm::cross(glm::angleAxis(-m_Pitch, m_RightVector), glm::angleAxis(-m_Yaw, GLM_UNIT_VECTOR_Y))) };
            m_ForwardVector = glm::rotate(q, m_ForwardVector);

        }
    }

    auto EditorCamera::ProcessKeyboardInput(double timeStep) -> void {
        m_CameraUpVector = GLM_UNIT_VECTOR_Y;

        if (!InputManager::IsMouseKeyPressed(MouseButton::Mouse_Button_Right))
            return;

        // Move forward
        if (InputManager::IsKeyPressed(KeyCode::Key_W)) {
            m_Position += m_ForwardVector * m_MovementSpeed * (float)timeStep;
        }

        // Move backwards
        if (InputManager::IsKeyPressed(KeyCode::Key_S)) {
            m_Position -= m_ForwardVector * m_MovementSpeed * (float)timeStep;
        }

        // Move left
        if (InputManager::IsKeyPressed(KeyCode::Key_A)) {
            m_Position -= m_RightVector * m_MovementSpeed * (float)timeStep;
        }

        // Move right
        if (InputManager::IsKeyPressed(KeyCode::Key_D)) {
            m_Position += m_RightVector * m_MovementSpeed * (float)timeStep;
        }

        // Move up
        if (InputManager::IsKeyPressed(KeyCode::Key_Space) || InputManager::IsKeyPressed(KeyCode::Key_E)) {
            m_Position.y += m_MovementSpeed * (float)timeStep;
        }

        // Move down
        if (InputManager::IsKeyPressed(KeyCode::Key_Q)) {
            m_Position.y -= m_MovementSpeed * (float)timeStep;
        }
    }

    auto EditorCamera::OnUpdate(double timeStep) -> void {
        m_CameraUpVector = GLM_UNIT_VECTOR_Y;
        m_RightVector = glm::cross(m_ForwardVector, m_CameraUpVector);

        ProcessMouseInput(timeStep);
        ProcessKeyboardInput(timeStep);
    }

    auto EditorCamera::OnEvent(Event& event) -> void {
        EventDispatcher dispatcher{ event };
        auto dispatch{ dispatcher.Forward<MouseScrollEvent>(MKT_BIND_EVENT_FUNC(EditorCamera::OnMouseScroll)) };
        (void)dispatch;
    }

    auto EditorCamera::OnMouseScroll(MouseScrollEvent& event) -> bool {
        UpdateViewMatrix();
        return false;
    }

    auto EditorCamera::SetViewportSize(float width, float height) -> void {
        if (m_ViewportWidth == width && m_ViewportHeight == height)
            return;

        m_ViewportWidth = width;
        m_ViewportHeight = height;

        UpdateProjection();
    }
}