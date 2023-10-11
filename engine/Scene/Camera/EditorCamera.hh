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
#include <Utility/Random.hh>
#include <Core/Event.hh>
#include <Core/CoreEvents.hh>
#include <Scene/Camera/Camera.hh>

namespace Mikoto {
    /**
     * This camera is based off OpenGL coordinate system which is right handed
     * Will eventually be changed so that it is adjusted properly to the Vulkan backend
     * */
    class EditorCamera : public Camera {
    public:
        explicit EditorCamera() = default;
        EditorCamera(float fov, float aspectRatio, float nearClip, float farClip);

        auto OnUpdate(double timeStep) -> void;
        auto UpdateProjection() -> void;
        auto UpdateViewMatrix() -> void;

        MKT_NODISCARD auto GetViewMatrix() const -> const glm::mat4& { return m_ViewMatrix; }
        MKT_NODISCARD auto GetViewProjection() const -> glm::mat4 { return GetProjection() * m_ViewMatrix; }
        MKT_NODISCARD auto GetPosition() const -> const glm::vec3& { return m_Position; }

        auto SetViewportSize(float width, float height) -> void;
        auto SetMovementSpeed(float value) -> void { m_MovementSpeed = value; }
        auto SetRotationSpeed(float value) -> void { m_RotationSpeed = value; }
        auto SetFieldOfView(float value) -> void { m_FieldOfView = value; }
        auto SetFarPlane(float value) -> void { m_FarClip = value; }
        auto SetNearPlane(float value) -> void { m_NearClip = value; }

        auto SwitchRotation(bool value) { m_AllowCameraMovementAndRotation = value; }

        MKT_NODISCARD constexpr static auto GetMinMovementSpeed() -> float { return 15.0f; }
        MKT_NODISCARD constexpr static auto GetMaxMovementSpeed() -> float { return 15.0f; }
        MKT_NODISCARD constexpr static auto GetMinRotationSpeed() -> float { return 30.0f; }
        MKT_NODISCARD constexpr static auto GetMaxRotationSpeed() -> float { return 90.0f; }

        MKT_NODISCARD constexpr static auto GetMaxNearClip() -> float { return 1.0f; }
        MKT_NODISCARD constexpr static auto GetMinNearClip() -> float { return 0.01f; }
        MKT_NODISCARD constexpr static auto GetMaxFarClip() -> float { return 10000.0f; }
        MKT_NODISCARD constexpr static auto GetMinFarClip() -> float { return 1000.0f; }

        MKT_NODISCARD constexpr static auto GetMaxFov() -> float { return 90.0f; }
        MKT_NODISCARD constexpr static auto GetMinFov() -> float { return 15.0f; }

    private:
        auto ProcessMouseInput(double timeStep) -> void;
        auto ProcessKeyboardInput(double timeStep) -> void;

        MKT_NODISCARD auto OnMouseScroll(MouseScrollEvent& event) -> bool;

    private:
        // Projection data
        float m_FieldOfView{ 45.0f };
        float m_AspectRatio{ 1.778f };
        float m_NearClip{ 0.1f };
        float m_FarClip{ 1000.0f };

        // Camera transform matrices
        glm::mat4 m_ViewMatrix{};
        glm::mat4 m_ProjectionMatrix{};

        // Camera vectors
        glm::vec3 m_ForwardVector{ 0.0f, 0.0f, -1.0f};
        glm::vec3 m_RightVector{ 1.0f, 0.0f, 0.0f};
        glm::vec3 m_CameraUpVector{ 0.0f, 1.0f, 0.0f };
        glm::vec3 m_Position{ 0.0f, 0.0f, 5.0f };

        float m_Pitch{ 0.0f }; // Rotation around X axis in radians
        float m_Yaw{ 0.0f }; // Rotation around Y axis in radians
        float m_Roll{ 0.0f }; // Rotation around Z axis in radians

        glm::vec2 m_LastMousePosition{ 0.0f, 0.0f };

        float m_ViewportWidth{ 1920 };
        float m_ViewportHeight{ 1080 };

        float m_RotationSpeed{ GetMinRotationSpeed() };
        float m_MovementSpeed{ GetMinMovementSpeed() };

        Random::GUID::UUID m_Guid{};
        bool m_AllowCameraMovementAndRotation{ false };
    };
}


#endif // MIKOTO_EDITOR_CAMERA_HH
