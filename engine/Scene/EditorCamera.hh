/**
 * EditorCamera.hh
 * Created by kate on 8/29/23.
 * */

#ifndef MIKOTO_EDITOR_CAMERA_HH
#define MIKOTO_EDITOR_CAMERA_HH

// Third-Party Libraries
#include <glm/glm.hpp>

// Project Headers
#include <Utility/Common.hh>
#include <Core/Events/Event.hh>
#include <Core/Events/MouseEvents.hh>
#include <Renderer/Camera/Camera.hh>

namespace Mikoto {
    class EditorCamera : public Camera {
    public:
        explicit EditorCamera() = default;
        EditorCamera(float fov, float aspectRatio, float nearClip, float farClip);

        auto OnUpdate(double ts) -> void;
        auto OnEvent(Event& event) -> void;

        auto SetDistance(float distance) -> void { m_Distance = distance; }
        auto SetViewportSize(float width, float height) -> void;

        MKT_NODISCARD auto GetViewMatrix() const -> const glm::mat4 & { return m_ViewMatrix; }
        MKT_NODISCARD auto GetViewProjection() const -> glm::mat4 { return GetProjection() * m_ViewMatrix; }

        MKT_NODISCARD auto GetUpDirection() const -> glm::vec3;
        MKT_NODISCARD auto GetRightDirection() const -> glm::vec3;
        MKT_NODISCARD auto GetForwardDirection() const -> glm::vec3;
        MKT_NODISCARD auto GetPosition() const -> const glm::vec3 & { return m_Position; }
        MKT_NODISCARD auto GetOrientation() const -> glm::quat;

        MKT_NODISCARD auto GetDistance() const -> float { return m_Distance; }

        MKT_NODISCARD auto GetPitch() const -> float { return m_Pitch; }
        MKT_NODISCARD auto GetYaw() const -> float { return m_Yaw; }

    private:
        auto UpdateProjection() -> void;
        auto UpdateView() -> void;

        auto MousePan(const glm::vec2 &delta) -> void;
        auto MouseRotate(const glm::vec2 &delta) -> void;
        auto MouseZoom(float delta) -> void;

        MKT_NODISCARD auto CalculatePosition() const -> glm::vec3;
        MKT_NODISCARD auto OnMouseScroll(MouseScrollEvent& event) -> bool;

        MKT_NODISCARD auto PanSpeed() const -> std::pair<float, float>;
        MKT_NODISCARD auto RotationSpeed() const -> float;
        MKT_NODISCARD auto ZoomSpeed() const -> float;

    private:
        float m_FieldOfView{ 45.0f };
        float m_AspectRatio{ 1.778f };
        float m_NearClip{ 0.1f };
        float m_FarClip{ 1000.0f };

        glm::mat4 m_ViewMatrix{};
        glm::vec3 m_Position{ 0.0f, 0.0f, 0.0f };
        glm::vec3 m_FocalPoint{ 0.0f, 0.0f, 0.0f };

        glm::vec2 m_InitialMousePosition{ 0.0f, 0.0f };

        glm::vec2 m_InitialMovePosition{ 0.0f, 0.0f };

        float m_Distance{ 10.0f };
        float m_Pitch{ 0.0f };
        float m_Yaw{ 0.0f };

        float m_ViewportWidth{ 1280 };
        float m_ViewportHeight{ 720 };

        float m_RotationSpeed{ 0.8f };
    };
}


#endif // MIKOTO_EDITOR_CAMERA_HH
