//
// Created by kate on 6/7/23.
//

#ifndef KATE_ENGINE_ORTHOGRAPHIC_CAMERA_HH
#define KATE_ENGINE_ORTHOGRAPHIC_CAMERA_HH

// Force radians always
#include <glm/glm.hpp>

#include <Platform/Window/Window.hh>
#include <Utility/Common.hh>

namespace kaTe {
    class OrthographicCamera {
    public:
        OrthographicCamera(double left, double right, double bottom, double top);
        OrthographicCamera(double left, double right, double bottom, double top, double zNear, double zFar);

        KT_NODISCARD auto GetFieldOfView() const -> double { return m_FieldOfView; }
        KT_NODISCARD auto GetAspectRatio() const -> double { return m_AspectRatio; }
        KT_NODISCARD auto GetRotation() const -> double { return m_Rotation; }
        KT_NODISCARD auto GetPosition() const -> const glm::vec3& { return m_Position; }
        KT_NODISCARD auto GetProjection() const -> const glm::mat4& { return m_Projection; }
        KT_NODISCARD auto GetView() const -> const glm::mat4& { return m_ViewMatrix; }
        KT_NODISCARD auto GetProjectionView() const -> const glm::mat4& { return m_ProjectionAndView; }

        auto SetFieldOfView(double fov) -> void { m_FieldOfView = fov; RecomputeViewMatrix(); }
        auto SetAspectRatio(double ar) -> void { m_AspectRatio = ar; RecomputeViewMatrix(); }
        // Takes rotation angles in degrees
        auto SetRotation(double rotation) -> void { m_Rotation = rotation; RecomputeViewMatrix(); }
        auto SetPosition(const glm::vec3& pos) -> void { m_Position = pos; RecomputeViewMatrix(); }
        auto SetPosition(double x, double y) -> void { m_Position = { x, y, 0.0 }; RecomputeViewMatrix(); }
        auto SetProjection(glm::mat4 proj) -> void { m_Projection = proj; RecomputeViewMatrix(); }
        auto SetProjection(double left, double right, double bottom, double top) -> void;
        auto SetView(glm::mat4 view) -> void { m_ViewMatrix = view; RecomputeViewMatrix(); }
        auto UpdateProjection(const std::shared_ptr<Window> &window, double zoom = 1.0) -> void;
    private:
        auto RecomputeViewMatrix() -> void;
    private:
        static constexpr double s_DefaultNearPlane{ -1.0 };
        static constexpr double s_DefaultFarPlane{ 1.0 };

        glm::mat4 m_ViewMatrix{};
        glm::mat4 m_Projection{};
        glm::mat4 m_ProjectionMatrix{};
        glm::vec3 m_Position{};

        // To avoid recomputing (projection * view) when it is not necessary
        glm::mat4 m_ProjectionAndView{};

        double m_FieldOfView{};
        double m_AspectRatio{};

        // Rotation in degrees
        double m_Rotation{};
    };
}

#endif//KATE_ENGINE_ORTHOGRAPHIC_CAMERA_HH