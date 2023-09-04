/**
 * EditorCamera.hh
 * Created by kate on 8/29/23.
 * */

// C++ Standard Library
#include <utility>

// Third-Party Libraries
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

// Project Headers
#include <Utility/Common.hh>
#include <Core/KeyCodes.hh>
#include <Core/MouseButtons.hh>
#include <Core/Application.hh>
#include <Scene/EditorCamera.hh>
#include <Platform/InputManager.hh>

namespace Mikoto {
    EditorCamera::EditorCamera(float fov, float aspectRatio, float nearClip, float farClip)
        :   Camera{ glm::perspective(glm::radians(fov), aspectRatio, nearClip, farClip) }
        ,   m_FieldOfView{ fov }, m_AspectRatio{ aspectRatio }, m_NearClip{ nearClip }, m_FarClip{ farClip }
    {
        m_ForwardVector = glm::vec3(0.0f, 0.0f, -10.0f);
        m_Position = glm::vec3(0.0f, 0.0f, -5.0f);

        UpdateView();
    }

    auto EditorCamera::UpdateProjection() -> void {
        m_AspectRatio = m_ViewportWidth / m_ViewportHeight;
        SetProjection(glm::perspective(glm::radians(m_FieldOfView), m_AspectRatio, m_NearClip, m_FarClip));
    }

    auto EditorCamera::UpdateView() -> void {
        m_ViewMatrix = glm::lookAt(m_Position, m_Position + m_ForwardVector, CAMERA_UP_VECTOR);
        m_ViewMatrix = glm::inverse(m_ViewMatrix);
    }

    auto EditorCamera::OnUpdate(double timeStep) -> void {
        m_RightVector = glm::cross(m_ForwardVector, CAMERA_UP_VECTOR);
        const glm::vec2 MOUSE_CURRENT_POSITION{ InputManager::GetMouseX(), InputManager::GetMouseY() };
        glm::vec2 delta{ (MOUSE_CURRENT_POSITION - m_LastMousePosition) * 0.003f };
        m_LastMousePosition = MOUSE_CURRENT_POSITION;

        if (!InputManager::IsMouseKeyPressed(MouseButton::Mouse_Button_Left)) {
            InputManager::SetCursorMode(InputManager::CursorInputMode::CURSOR_NORMAL);
            return;
        }

        InputManager::SetCursorMode(InputManager::CursorInputMode::CURSOR_DISABLED);

        if (InputManager::IsKeyPressed(KeyCode::Key_W)) {
            m_Position -= m_ForwardVector * m_MovementSpeed * (float)timeStep;
        }

        if (InputManager::IsKeyPressed(KeyCode::Key_A)) {
            m_Position += m_RightVector * m_MovementSpeed * (float)timeStep;
        }

        if (InputManager::IsKeyPressed(KeyCode::Key_S)) {
            m_Position += m_ForwardVector * m_MovementSpeed * (float)timeStep;
        }

        if (InputManager::IsKeyPressed(KeyCode::Key_D)) {
            m_Position -= m_RightVector * m_MovementSpeed * (float)timeStep;
        }

        if (InputManager::IsKeyPressed(KeyCode::Key_Space) || InputManager::IsKeyPressed(KeyCode::Key_E)) {
            m_Position.y -= m_MovementSpeed * (float)timeStep;
        }

        if (InputManager::IsKeyPressed(KeyCode::Key_Q)) {
            m_Position.y += m_MovementSpeed * (float)timeStep;
        }

        // Perform rotation
        if (delta.x != 0.0f || delta.y != 0.0f) {
            // The Y offset of the mouse will dictate how much we rotate in the X axis
            m_Pitch = delta.y * m_RotationSpeed * (float)timeStep;
            // The X offset of the mouse will dictate how much we rotate in the Y axis
            m_Yaw = delta.x * m_RotationSpeed * (float)timeStep;

            const glm::quat quaternion{ glm::normalize(glm::cross(glm::angleAxis(-m_Pitch, glm::normalize(m_RightVector)),
                                                                 glm::angleAxis(-m_Yaw, CAMERA_UP_VECTOR))) };

            m_ForwardVector = glm::rotate(quaternion, m_ForwardVector);
        }

        UpdateView();
    }

    auto EditorCamera::OnEvent(Event& event) -> void {
        EventDispatcher dispatcher{ event };
        auto dispatch{ dispatcher.Forward<MouseScrollEvent>(MKT_BIND_EVENT_FUNC(EditorCamera::OnMouseScroll)) };
        (void)dispatch;
    }

    auto EditorCamera::OnMouseScroll(MouseScrollEvent& event) -> bool {
        UpdateView();
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