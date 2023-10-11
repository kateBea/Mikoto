/**
 * SceneCamera.cc
 * Created by kate on 6/25/23.
 * */

// Third-Party Libraries
#include <glm/glm.hpp>

// Project Headers
#include <Scene/Camera/SceneCamera.hh>

namespace Mikoto {

    auto SceneCamera::SetOrthographic(double nearPlane, double farPlane, double size) -> void {
        SetProjectionType(ProjectionType::ORTHOGRAPHIC);
        m_OrthographicSize = size;
        m_OrthographicNearPlane = nearPlane;
        m_OrthographicFarPlane = farPlane;
        RecomputeProjection();
    }

    auto SceneCamera::SetPerspective(double nearPlane, double farPlane, double fov) -> void {
        SetProjectionType(ProjectionType::PERSPECTIVE);
        m_PerspectiveNearPlane = nearPlane;
        m_PerspectiveFarPlane = farPlane;
        m_PerspectiveFieldOfView = fov;
        RecomputeProjection();
    }

    auto SceneCamera::SetViewportSize(UInt32_T width, UInt32_T height) -> void {
        m_AspectRatio = width / (double)height;
        RecomputeProjection();
    }

    auto SceneCamera::RecomputeProjection() -> void {
        if (GetProjectionType()  == ProjectionType::ORTHOGRAPHIC) {
            double zoom{ .5 };
            double orthographicLeft{ -m_OrthographicSize * m_AspectRatio * zoom };
            double orthographicRight{ m_OrthographicSize * m_AspectRatio * zoom };
            double orthographicBottom{ -m_OrthographicSize * zoom };
            double orthographicTop{ m_OrthographicSize * zoom };
            SetProjection(glm::ortho(orthographicLeft, orthographicRight, orthographicBottom,
                                     orthographicTop, m_OrthographicNearPlane, m_OrthographicFarPlane));
        }
        else if (GetProjectionType() == ProjectionType::PERSPECTIVE) {
            SetProjection(glm::perspective(glm::radians(m_PerspectiveFieldOfView), m_AspectRatio, m_PerspectiveNearPlane, m_PerspectiveFarPlane));
        }
    }

    auto SceneCamera::GetOrthographicSize() const -> double {
        return m_OrthographicSize;
    }
    auto SceneCamera::SetOrthographicSize(double value) -> void {
        m_OrthographicSize = value;
        RecomputeProjection();
    }

    auto SceneCamera::GetOrthographicFarPlane() const -> double {
        return m_OrthographicFarPlane;
    }

    auto SceneCamera::SetOrthographicFarPlane(double value) -> void {
        m_OrthographicFarPlane = value;
        RecomputeProjection();
    }

    auto SceneCamera::GetOrthographicNearPlane() const -> double {
        return m_OrthographicNearPlane;
    }

    auto SceneCamera::SetOrthographicNearPlane(double value) -> void {
        m_OrthographicNearPlane = value;
        RecomputeProjection();
    }

    auto SceneCamera::GetPerspectiveFarPlane() const -> double {
        return m_PerspectiveFarPlane;
    }

    auto SceneCamera::SetPerspectiveFarPlane(double value) -> void {
        m_PerspectiveFarPlane = value;
        RecomputeProjection();
    }

    auto SceneCamera::GetPerspectiveNearPlane() const -> double {
        return m_PerspectiveNearPlane;
    }

    auto SceneCamera::SetPerspectiveNearPlane(double value) -> void {
        m_PerspectiveNearPlane = value;
        RecomputeProjection();
    }

    auto SceneCamera::GetPerspectiveFOV() const -> double {
        return m_PerspectiveFieldOfView;
    }

    auto SceneCamera::SetPerspectiveFOV(double value) -> void {
        m_PerspectiveFieldOfView = value;
        RecomputeProjection();
    }
}