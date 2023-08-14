//
// Created by kate on 6/24/23.
//

#ifndef KATE_ENGINE_CAMERA_HH
#define KATE_ENGINE_CAMERA_HH

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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

        KT_NODISCARD auto GetProjection() const -> const glm::mat4& { return m_Projection; }
        auto SetProjection(const glm::mat4& projection = glm::mat4(1.0)) -> void { m_Projection = projection; }

        KT_NODISCARD auto GetTransform() const -> const glm::mat4& { return m_Transform; }
        KT_NODISCARD auto GetTransform() -> glm::mat4& { return m_Transform; }
        auto SetTransform(const glm::mat4& transform = glm::mat4(1.0)) -> void { m_Transform = transform; }

        auto SetPosition(const glm::vec3& position, const glm::vec3& angles = glm::vec3(0.0f)) -> void {
            m_Translation = position;
            m_Rotation = angles;

            glm::mat4 rotationX{ glm::rotate(identMat, (float)glm::radians(m_Rotation[0]), xAxis) };
            glm::mat4 rotationY{ glm::rotate(rotationX, (float)glm::radians(m_Rotation[1]), yAxis) };
            glm::mat4 rotation{ glm::rotate(rotationY, (float)glm::radians(m_Rotation[2]), zAxis) };

            m_Transform = glm::translate(identMat, position) * rotation;
        }

        virtual ~Camera() = default;

    private:
        // Constants
        static constexpr glm::vec3 xAxis{ 1.0f, 0.0f, 0.0f };
        static constexpr glm::vec3 yAxis{ 0.0f, 1.0f, 0.0f };
        static constexpr glm::vec3 zAxis{ 0.0f, 0.0f, 1.0f };
        static constexpr glm::mat4 identMat{ glm::mat4(1.0) };

        glm::vec3 m_Translation{};
        glm::vec3 m_Rotation{};

        glm::mat4 m_Projection{};
        glm::mat4 m_Transform{};
    };
}


#endif//KATE_ENGINE_CAMERA_HH
