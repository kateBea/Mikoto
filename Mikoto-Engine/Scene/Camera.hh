/**
 * Camera.hh
 * Created by kate on 6/24/23.
 * */

#ifndef MIKOTO_CAMERA_HH
#define MIKOTO_CAMERA_HH

// Third-Party Libraries
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

// Project Headers
#include "Common/Common.hh"
#include "Common/Constants.hh"

namespace Mikoto {
    class Camera {
    public:
        enum ProjectionType {
            ORTHOGRAPHIC    = 0,
            PERSPECTIVE     = 1,
        };

        explicit Camera(const glm::mat4& projection = glm::mat4(1.0f), const glm::mat4& transform = glm::mat4(1.0f), ProjectionType projectionType = ProjectionType::PERSPECTIVE)
            :   m_Projection{ projection }, m_Transform{ transform }, m_ProjectionType{ projectionType }
        {
            SetProjectionType(m_ProjectionType);
        }

        Camera(const Camera& other) = default;
        Camera(Camera&& other) = default;

        auto operator=(const Camera& other) -> Camera& = default;
        auto operator=(Camera&& other) -> Camera& = default;

        MKT_NODISCARD auto GetProjection() const -> const glm::mat4& { return m_Projection; }

        // TODO: review
        auto SetProjection(const glm::mat4& projection = glm::mat4(1.0)) -> void { m_Projection = projection; }

        MKT_NODISCARD auto GetTransform() const -> const glm::mat4& { return m_Transform; }
        MKT_NODISCARD auto GetTransform() -> glm::mat4& { return m_Transform; }
        auto SetTransform(const glm::mat4& transform) -> void { m_Transform = transform; }

        auto SetPosition(const glm::vec3& position) -> void {
            m_Translation = position;
            m_Transform = glm::translate(m_Transform, m_Translation);
        }

        auto SetRotation(const glm::vec3& angles = glm::vec3(0.0f)) -> void {
            m_Rotation = angles;

            m_Transform = glm::rotate(m_Transform, (float)glm::radians(m_Rotation[0]), GLM_UNIT_VECTOR_X);
            m_Transform =  glm::rotate(m_Transform, (float)glm::radians(m_Rotation[1]), GLM_UNIT_VECTOR_Y);
            m_Transform =  glm::rotate(m_Transform, (float)glm::radians(m_Rotation[2]), GLM_UNIT_VECTOR_Z);
        }

        MKT_NODISCARD auto GetProjectionType() -> ProjectionType { return m_ProjectionType; }
        MKT_NODISCARD auto IsOrthographic() const -> bool { return m_ProjectionType == ProjectionType::ORTHOGRAPHIC; }

        auto SetProjectionType(ProjectionType type) -> void {
            m_ProjectionType = type;

#if 0
            // TODO: set perspective properly (no hard-coding values)
            switch(type) {
                case ORTHOGRAPHIC:
                    SetProjection(glm::ortho(-1.778, 1.778, -1.0, 1.0));
                    break;
                case PERSPECTIVE:
                    SetProjection(glm::perspective(glm::radians(45.0), 1.778, 1.0, 1000.0));
                    break;
            }
#endif
        }

        ~Camera() = default;

    private:
        glm::mat4 m_Projection{};
        glm::mat4 m_Transform{};

        glm::vec3 m_Translation{};
        glm::vec3 m_Rotation{};

        ProjectionType m_ProjectionType{};
    };
}


#endif // MIKOTO_CAMERA_HH
