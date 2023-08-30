/**
 * OrthographicCamera.cc
 * Created by kate on 6/7/23.
 * */

// Third-Party Libraries
#include <glm/gtc/matrix_transform.hpp>

// Project Headers
#include <Renderer/Camera/OrthographicCamera.hh>

namespace Mikoto {
    OrthographicCamera::OrthographicCamera(double left, double right, double bottom, double top)
        :   m_ViewMatrix{ 1.0f }, m_Projection{ glm::ortho(left, right, bottom, top, DEFAULT_NEAR_PLANE, DEFAULT_FAR_PLANE) }
    {
        m_ProjectionAndView = m_Projection * m_ViewMatrix;
    }

    OrthographicCamera::OrthographicCamera(double left, double right, double bottom, double top, double zNear, double zFar)
        :   m_ViewMatrix{ 1.0f }, m_Projection{ glm::ortho(left, right, bottom, top, zNear, zFar) }
    {
        m_ProjectionAndView = m_Projection * m_ViewMatrix;
    }

    auto OrthographicCamera::RecomputeViewMatrix() -> void {
        constexpr glm::mat4 identityMatrix(1.0f);
        constexpr glm::vec3 zAxis{ 0.0f, 0.0f, 1.0f };

        glm::mat4 transform{ glm::translate(identityMatrix, m_Position) *
                                glm::rotate(identityMatrix, static_cast<float>(glm::radians(m_Rotation)), zAxis) };

        m_ViewMatrix = glm::inverse(transform);
        m_ProjectionAndView = m_Projection * m_ViewMatrix;
    }

    void OrthographicCamera::UpdateProjection(const std::shared_ptr<Window> &window, double zoom) {
        double width{ static_cast<double>(window->GetWidth()) };
        double height{ static_cast<double>(window->GetHeight()) };
        m_AspectRatio = width / height;
        SetProjection(-m_AspectRatio * zoom, m_AspectRatio * zoom, -zoom, zoom);
    }

    auto OrthographicCamera::SetProjection(double left, double right, double bottom, double top) -> void {
        m_Projection = glm::ortho(left, right, bottom, top, DEFAULT_NEAR_PLANE, DEFAULT_FAR_PLANE);
        m_ProjectionAndView = m_Projection * m_ViewMatrix;
    }
}