/**
 * Camera.hh
 * Created by kate on 6/24/23.
 * */

#ifndef MIKOTO_CAMERA_HH
#define MIKOTO_CAMERA_HH

#include <utility>

// Third-Party Libraries
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

// Project Headers
#include "Common/Common.hh"
#include <Library/Random/Random.hh>
#include <Library/Math/Math.hh>

namespace Mikoto {

    enum class ProjectionType {
        ORTHOGRAPHIC,
        PERSPECTIVE,
    };

    class Camera {
    public:

        Camera(const Camera& other) = default;
        Camera(Camera&& other) = default;

        auto operator=(const Camera& other) -> Camera& = default;
        auto operator=(Camera&& other) -> Camera& = default;

        MKT_NODISCARD auto GetProjection() const -> const glm::mat4& { return m_Projection; }
        MKT_NODISCARD auto GetViewMatrix() const -> const glm::mat4& { return m_ViewMatrix; }

        auto SetProjection(const glm::mat4& projection = glm::mat4(1.0)) -> void { m_Projection = projection; }

        MKT_NODISCARD auto GetTransform() const -> const glm::mat4& { return m_Transform; }
        MKT_NODISCARD auto GetTransform() -> glm::mat4& { return m_Transform; }
        auto SetTransform(const glm::mat4& transform) -> void { m_Transform = transform; }

        MKT_NODISCARD auto GetTargetForward() const -> const glm::vec3& { return m_ForwardVector; }
        MKT_NODISCARD auto GetFOV() const -> float { return m_FieldOfView; }
        MKT_NODISCARD auto GetNearPlane() const -> float { return m_NearClip; }
        MKT_NODISCARD auto GetFarPlane() const -> float { return m_FarClip; }
        MKT_NODISCARD auto GetAspectRatio() const -> float { return m_ViewportWidth / m_ViewportHeight; }
        MKT_NODISCARD auto GetViewPort() const -> decltype(auto) { return std::make_pair(m_ViewportWidth, m_ViewportHeight); }


        /**
         * @brief Retrieve the view projection matrix.
         * @return The result of the projection matrix multiplied by the view matrix.
         * */
        MKT_NODISCARD auto GetViewProjection() const -> glm::mat4 { return GetProjection() * m_ViewMatrix; }

        MKT_NODISCARD auto GetPosition() const -> const glm::vec3& { return m_Position; }
        MKT_NODISCARD auto GetRotation() const -> const glm::vec3& { return m_Rotation; }

        auto SetPosition(const glm::vec3& position) -> void {
            m_Translation = position;
            m_Transform = glm::translate(m_Transform, m_Translation);
        }

        auto SetRotation(const glm::vec3& angles = glm::vec3(0.0f)) -> void {
            m_Rotation = angles;

            m_Transform = rotate(m_Transform, glm::radians( m_Rotation[0] ), Math::UNIT_VECTOR_X);
            m_Transform =  rotate(m_Transform, glm::radians( m_Rotation[1] ), Math::UNIT_VECTOR_Y);
            m_Transform =  rotate(m_Transform, glm::radians( m_Rotation[2] ), Math::UNIT_VECTOR_Z);
        }

        MKT_NODISCARD auto GetProjectionType() const -> ProjectionType { return m_ProjectionType; }
        MKT_NODISCARD auto IsOrthographic() const -> bool { return m_ProjectionType == ProjectionType::ORTHOGRAPHIC; }

        auto SetProjectionType( const ProjectionType type ) -> void {
            m_ProjectionType = type;

            UpdateProjection();
        }

        ~Camera() = default;

    protected:
        explicit Camera(const glm::mat4& projection = glm::mat4(1.0f), const glm::mat4& transform = glm::mat4(1.0f), ProjectionType projectionType = ProjectionType::PERSPECTIVE )
            :   m_Projection{ projection }, m_Transform{ transform }, m_ProjectionType{ projectionType }
        {
            UpdateProjection();
        }

        auto UpdateProjection() -> void {
            m_AspectRatio = m_ViewportWidth / m_ViewportHeight;

            switch(m_ProjectionType) {
                case ProjectionType::ORTHOGRAPHIC:
                    SetProjection(glm::ortho(0.0f, m_ViewportWidth, 0.0f, m_ViewportHeight));
                break;
                case ProjectionType::PERSPECTIVE:
                    SetProjection(glm::perspective(glm::radians(m_FieldOfView), m_AspectRatio, m_NearClip, m_FarClip));
                break;
            }
        }

    protected:
        float m_ViewportWidth{ 1920 };
        float m_ViewportHeight{ 1080 };

        // [Projection Data]
        float m_NearClip{ 0.1f };
        float m_FarClip{ 1000.0f };
        float m_FieldOfView{ 45.0f };
        float m_AspectRatio{ m_ViewportWidth / m_ViewportHeight };

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

        glm::mat4 m_Projection{};
        glm::mat4 m_Transform{};

        glm::vec3 m_Translation{};
        glm::vec3 m_Rotation{};

        ProjectionType m_ProjectionType{ ProjectionType::PERSPECTIVE };
    };
}


#endif // MIKOTO_CAMERA_HH
