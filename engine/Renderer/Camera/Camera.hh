/**
 * Camera.hh
 * Created by kate on 6/24/23.
 * */

#ifndef MIKOTO_CAMERA_HH
#define MIKOTO_CAMERA_HH

// Third-Party Libraries
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Project Headers
#include <Utility/Common.hh>

namespace Mikoto {
    class Camera {
    public:
        explicit Camera(const glm::mat4& projection = glm::mat4(1.0f), const glm::mat4& transform = glm::mat4(1.0f))
            :   m_Projection{ projection }, m_Transform{ transform } {}

        Camera(const Camera& other) = default;
        Camera(Camera&& other) = default;

        auto operator=(const Camera& other) -> Camera& = default;
        auto operator=(Camera&& other) -> Camera& = default;

        MKT_NODISCARD auto GetProjection() const -> const glm::mat4& { return m_Projection; }
        auto SetProjection(const glm::mat4& projection = glm::mat4(1.0)) -> void { m_Projection = projection; }

        MKT_NODISCARD auto GetTransform() const -> const glm::mat4& { return m_Transform; }
        MKT_NODISCARD auto GetTransform() -> glm::mat4& { return m_Transform; }
        auto SetTransform(const glm::mat4& transform = glm::mat4(1.0)) -> void { m_Transform = transform; }

        auto SetPosition(const glm::vec3& position, const glm::vec3& angles = glm::vec3(0.0f)) -> void {
            m_Translation = position;
            m_Rotation = angles;

            glm::mat4 rotationX{ glm::rotate(IDENTITY_MATRIX, (float)glm::radians(m_Rotation[0]), X_AXIS) };
            glm::mat4 rotationY{ glm::rotate(rotationX, (float)glm::radians(m_Rotation[1]), Y_AXIS) };
            glm::mat4 rotation{ glm::rotate(rotationY, (float)glm::radians(m_Rotation[2]), Z_AXIS) };

            m_Transform = glm::translate(IDENTITY_MATRIX, position) * rotation;
        }

        ~Camera() = default;

    private:
        static constexpr glm::vec3 X_AXIS{ 1.0f, 0.0f, 0.0f };
        static constexpr glm::vec3 Y_AXIS{ 0.0f, 1.0f, 0.0f };
        static constexpr glm::vec3 Z_AXIS{ 0.0f, 0.0f, 1.0f };
        static constexpr glm::mat4 IDENTITY_MATRIX{ glm::mat4(1.0) };

        glm::mat4 m_Projection{};
        glm::mat4 m_Transform{};
        glm::vec3 m_Translation{};
        glm::vec3 m_Rotation{};
    };
}


#endif//KATE_ENGINE_CAMERA_HH
