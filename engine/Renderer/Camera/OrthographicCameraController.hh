//
// Created by kate on 6/12/23.
//

#ifndef KATE_ENGINE_ORTHOGRAPHIC_CAMERA_CONTROLLER_HH
#define KATE_ENGINE_ORTHOGRAPHIC_CAMERA_CONTROLLER_HH

#include <memory>

// Force radians always
#include <glm/glm.hpp>

#include <Utility/Common.hh>
#include <Core/Events/Event.hh>

#include <Core/Events/AppEvents.hh>
#include <Core/Events/MouseEvents.hh>
#include <Renderer/Camera/OrthographicCamera.hh>

// TODO: move to OrthographicCamera.hh file
namespace kaTe {
    /**
     * This class controls an orthographic camera. The camera can be an entity independent to
     * it or be part of it as an aggregate relationship, we specify so upon construction
     * */
    class OrthographicCameraController {
    public:
        /**
         * We can optionally pass this controller a camera if we want it to handle a specific target
         * */
        explicit OrthographicCameraController(double aspectRatio, bool enableRotation = false, std::shared_ptr<OrthographicCamera> target = nullptr);
        OrthographicCameraController(UInt32_T width, UInt32_T height, bool enableRotation = false, std::shared_ptr<OrthographicCamera> target = nullptr);

        auto OnUpdate() -> void;
        auto OnEvent(Event& event) -> void;

        auto SetProjection(double left, double right, double bottom, double top) -> void;
        auto AdjustViewport(UInt32_T width, UInt32_T height) -> void;

        /**
         * This function tells whether this OrthographicCameraController controls and external camera or not
         * meaning the camera is an independent entity to this OrthographicCameraController
         * */
        auto HasCamera() -> bool { return m_TargetCamera != nullptr; }

        KT_NODISCARD auto GetCamera() const -> const std::shared_ptr<OrthographicCamera>&;
        KT_NODISCARD auto GetCamera() -> std::shared_ptr<OrthographicCamera>&;
        KT_NODISCARD auto GetZoomLevel() const -> double { return m_Zoom; }

    private:
        auto OnMouseScrolledEvent(MouseScrollEvent& event) -> bool;
        auto OnWindowResized(WindowResizedEvent& event) -> bool;
    public:
        OrthographicCameraController(const OrthographicCameraController&) = delete;
        auto operator=(const OrthographicCameraController&) = delete;

        OrthographicCameraController(OrthographicCameraController&&) = delete;
        auto operator=(OrthographicCameraController&&) = delete;
    private:
        static constexpr float s_MaxZoom{ 0.25f };

        float m_AspectRatio{};
        float m_Zoom{ 1.0f };
        float m_FieldOfViewSensitivity{ 0.08f };
        bool m_EnableRotation{};

        glm::vec3 m_TargetCameraPosition{};
        float m_TargetCameraMovementSpeed{ 1.5f };

        // In radians
        float m_TargetCameraRotation{};
        // Represents the rotation in degrees so radians/time_unit (time_unit could be seconds,
        // milliseconds, etc.). The default units are seconds
        float m_TargetCameraRotationSpeed{ 90.0f };

        std::shared_ptr<OrthographicCamera> m_TargetCamera{};
    };
}


#endif//KATE_ENGINE_ORTHOGRAPHIC_CAMERA_CONTROLLER_HH
