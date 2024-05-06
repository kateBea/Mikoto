/**
 * SceneCamera.hh
 * Created by kate on 6/25/23.
 * */

#ifndef MIKOTO_SCENE_CAMERA_HH
#define MIKOTO_SCENE_CAMERA_HH

// Third-Party Libraries
#include "glm/glm.hpp"

// Project Headers
#include "Common/Common.hh"
#include "Camera.hh"

namespace Mikoto {
    class SceneCamera : public Camera {
    public:
        explicit SceneCamera(const glm::mat4& projection = glm::mat4(1.0f), const glm::mat4& transform = glm::mat4(1.0f))
            :   Camera{ projection, transform, ProjectionType::ORTHOGRAPHIC } {}

        SceneCamera(const SceneCamera& other) = default;
        SceneCamera(SceneCamera&& other) = default;

        auto operator=(const SceneCamera& other) -> SceneCamera& = default;
        auto operator=(SceneCamera&& other) -> SceneCamera& = default;

        MKT_NODISCARD auto GetOrthographicFarPlane() const -> double;
        auto SetOrthographicFarPlane(double value) -> void;

        MKT_NODISCARD auto GetOrthographicNearPlane() const -> double;
        auto SetOrthographicNearPlane(double value) -> void;

        MKT_NODISCARD auto GetOrthographicSize() const -> double;
        auto SetOrthographicSize(double value) -> void;


        MKT_NODISCARD auto GetPerspectiveFarPlane() const -> double;
        auto SetPerspectiveFarPlane(double value) -> void;

        MKT_NODISCARD auto GetPerspectiveNearPlane() const -> double;
        auto SetPerspectiveNearPlane(double value) -> void;

        MKT_NODISCARD auto GetPerspectiveFOV() const -> double;
        auto SetPerspectiveFOV(double value) -> void;

        ~SceneCamera() = default;

        auto SetOrthographic(double nearPlane, double farPlane, double size) -> void;
        auto SetPerspective(double nearPlane, double farPlane, double fov) -> void;
        auto SetViewportSize(UInt32_T width, UInt32_T height) -> void;
    private:
        auto RecomputeProjection() -> void;

    private:
        // Orthographic camera info
        double m_OrthographicSize{ 3.5f };
        double m_OrthographicNearPlane{ -1.0f };
        double m_OrthographicFarPlane{ 1.0f };

        // Perspective camera stuff
        double m_PerspectiveFieldOfView{ 45.0f };
        double m_PerspectiveNearPlane{ 0.001 };
        double m_PerspectiveFarPlane{ 1000.0f };

        double m_AspectRatio{};
    };
}



#endif // MIKOTO_SCENE_CAMERA_HH
