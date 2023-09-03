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
        UpdateView();
    }

    auto EditorCamera::UpdateProjection() -> void {
        m_AspectRatio = m_ViewportWidth / m_ViewportHeight;
        SetProjection(glm::perspective(glm::radians(m_FieldOfView), m_AspectRatio, m_NearClip, m_FarClip));
    }

    auto EditorCamera::UpdateView() -> void {
        // m_Yaw = m_Pitch = 0.0f; // Lock the camera's rotation

        m_Position = CalculatePosition();

        glm::quat orientation{ GetOrientation() };
        m_ViewMatrix = glm::translate(glm::mat4(1.0f), m_Position) * glm::toMat4(orientation);
        m_ViewMatrix = glm::inverse(m_ViewMatrix);
    }

    auto EditorCamera::PanSpeed() const -> std::pair<float, float> {
        float x = std::min(m_ViewportWidth / 1000.0f, 2.4f); // max = 2.4f
        float xFactor = 0.0366f * (x * x) - 0.1778f * x + 0.3021f;

        float y = std::min(m_ViewportHeight / 1000.0f, 2.4f); // max = 2.4f
        float yFactor = 0.0366f * (y * y) - 0.1778f * y + 0.3021f;

        return std::make_pair(xFactor, yFactor);
    }

    auto EditorCamera::RotationSpeed() const -> float {
        return m_RotationSpeed;
    }

    auto EditorCamera::ZoomSpeed() const -> float {
        float distance{ m_Distance * 0.2f };
        distance = std::max(distance, 0.0f);

        float speed = distance * distance;
        speed = std::min(speed, 100.0f); // max speed = 100

        return speed;
    }

    auto EditorCamera::OnUpdate(double ts) -> void {
        const glm::vec2 mouse{ InputManager::GetMouseX(), InputManager::GetMouseY() };
        glm::vec2 delta{ (mouse - m_InitialMousePosition) * 0.003f };
        m_InitialMousePosition = mouse;

        if (InputManager::IsMouseKeyPressed(MouseButton::Mouse_Button_Right)) {
            MousePan(delta);
        }
        else if (InputManager::IsMouseKeyPressed(MouseButton::Mouse_Button_Middle)) {
            MouseZoom(delta.y);
        }
        else if (InputManager::IsMouseKeyPressed(MouseButton::Mouse_Button_Left)) {
            MouseRotate(delta);
        }

        UpdateView();
    }

    auto EditorCamera::OnEvent(Event& event) -> void {
        EventDispatcher dispatcher{ event };
        auto dispatch{ dispatcher.Forward<MouseScrollEvent>(MKT_BIND_EVENT_FUNC(EditorCamera::OnMouseScroll)) };
        (void)dispatch;
    }

    auto EditorCamera::OnMouseScroll(MouseScrollEvent& event) -> bool {
        float delta{ (float)event.GetOffsetY() * 0.1f };

        MouseZoom(delta);
        UpdateView();

        return false;
    }

    auto EditorCamera::MousePan(const glm::vec2& delta) -> void {
        auto [xSpeed, ySpeed]{ PanSpeed() };

        m_FocalPoint += -GetRightDirection() * delta.x * xSpeed * m_Distance;
        m_FocalPoint += GetUpDirection() * delta.y * ySpeed * m_Distance;
    }

    auto EditorCamera::MouseRotate(const glm::vec2 &delta) -> void {
        float yawSign = GetUpDirection().y < 0 ? -1.0f : 1.0f;
        m_Yaw += yawSign * delta.x * RotationSpeed();
        m_Pitch += delta.y * RotationSpeed();
    }

    auto EditorCamera::MouseZoom(float delta) -> void {
        m_Distance -= delta * ZoomSpeed();

        if (m_Distance < 1.0f) {
            m_FocalPoint += GetForwardDirection();
            m_Distance = 1.0f;
        }
    }

    auto EditorCamera::GetUpDirection() const -> glm::vec3 {
        return glm::rotate(GetOrientation(), glm::vec3(0.0f, 1.0f, 0.0f));
    }

    auto EditorCamera::GetRightDirection() const -> glm::vec3 {
        return glm::rotate(GetOrientation(), glm::vec3(1.0f, 0.0f, 0.0f));
    }

    auto EditorCamera::GetForwardDirection() const -> glm::vec3 {
        return glm::rotate(GetOrientation(), glm::vec3(0.0f, 0.0f, -1.0f));
    }

    auto EditorCamera::CalculatePosition() const -> glm::vec3 {
        return m_FocalPoint - GetForwardDirection() * m_Distance;
    }

    auto EditorCamera::GetOrientation() const -> glm::quat {
        return { glm::vec3(-m_Pitch, -m_Yaw, 0.0f) };
    }

    auto EditorCamera::SetViewportSize(float width, float height) -> void {
        m_ViewportWidth = width;
        m_ViewportHeight = height;

        UpdateProjection();
    }
}