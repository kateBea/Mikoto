//
// Created by kate on 6/12/23.
//

#include <algorithm>

#include <Utility/Common.hh>

#include <Core/Logger.hh>
#include <Core/Events/Event.hh>
#include <Core/TimeManager.hh>

#include <Platform/InputManager.hh>

#include <Renderer/Camera/OrthographicCameraController.hh>

namespace kaTe {

    OrthographicCameraController::OrthographicCameraController(double aspectRatio, bool enableRotation, std::shared_ptr<OrthographicCamera> target)
        :   m_AspectRatio{ (float)aspectRatio }, m_EnableRotation{ enableRotation }
    {
        if (target == nullptr)
            // these are for now magic numbers
            m_TargetCamera = std::make_shared<OrthographicCamera>(-aspectRatio * m_Zoom, aspectRatio * m_Zoom, -m_Zoom, m_Zoom);
        else
            m_TargetCamera = std::move(target);


    }

    OrthographicCameraController::OrthographicCameraController(UInt32_T width, UInt32_T height, bool enableRotation, std::shared_ptr<OrthographicCamera> target)
        :   m_AspectRatio{ (float)width / (float)height }, m_EnableRotation{ enableRotation }
    {
        if (target == nullptr)
            m_TargetCamera = std::make_shared<OrthographicCamera>(-m_AspectRatio * m_Zoom, m_AspectRatio * m_Zoom, -m_Zoom, m_Zoom);
        else
            m_TargetCamera = std::move(target);

    }

    auto OrthographicCameraController::OnUpdate() -> void {
        auto deltaTime{ (float)TimeManager::GetDeltaTime() };

        // Try using the event system instead
        if (m_EnableRotation) {
            if (InputManager::IsKeyPressed(KT_KEY_Q)) m_TargetCameraRotation += m_TargetCameraRotationSpeed * deltaTime;
            if (InputManager::IsKeyPressed(KT_KEY_E)) m_TargetCameraRotation -= m_TargetCameraRotationSpeed * deltaTime;
        }
        else {
            if (InputManager::IsKeyPressed(KT_KEY_Q) || InputManager::IsKeyPressed(KT_KEY_E))
                KATE_CORE_LOGGER_WARN("Trying to rotate camera but rotation is disabled for this controller");

        }

        // Multiply m_TargetCameraMovementSpeed by zoom level to decrease movement speed when objects are very close to the camera
        // NOTE that camera scrolling is currently inverted, because [ m_Zoom += event.GetOffsetY() * m_FieldOfViewSensitivity ]
        // @OrthographicCameraController::OnMouseScrolledEvent(MouseScrollEvent& event)
        if (InputManager::IsKeyPressed(KT_KEY_A))
            m_TargetCameraPosition.x -= (m_TargetCameraMovementSpeed * m_Zoom)  * deltaTime;

        if (InputManager::IsKeyPressed(KT_KEY_D))
            m_TargetCameraPosition.x += (m_TargetCameraMovementSpeed * m_Zoom)  * deltaTime;

        if (InputManager::IsKeyPressed(KT_KEY_W))
            m_TargetCameraPosition.y += (m_TargetCameraMovementSpeed * m_Zoom)  * deltaTime;

        if (InputManager::IsKeyPressed(KT_KEY_S))
            m_TargetCameraPosition.y -= (m_TargetCameraMovementSpeed * m_Zoom)  * deltaTime;


        m_TargetCamera->SetPosition(m_TargetCameraPosition.x, m_TargetCameraPosition.y);
        m_TargetCamera->SetRotation(m_TargetCameraRotation);
    }

    auto OrthographicCameraController::OnEvent(Event &event) -> void {
        EventDispatcher dispatcher{ event };

        dispatcher.Forward<MouseScrollEvent>(KT_BIND_EVENT_FUNC(OrthographicCameraController::OnMouseScrolledEvent));
        dispatcher.Forward<WindowResizedEvent>(KT_BIND_EVENT_FUNC(OrthographicCameraController::OnWindowResized));
    }

    auto OrthographicCameraController::SetProjection(double left, double right, double bottom, double top) -> void {
        m_TargetCamera->SetProjection(left, right, bottom, top);
    }

    auto OrthographicCameraController::OnMouseScrolledEvent(MouseScrollEvent& event) -> bool {
        m_Zoom += event.GetOffsetY() * m_FieldOfViewSensitivity;
        m_Zoom = std::max(m_Zoom, s_MaxZoom);
        m_TargetCamera->SetProjection(-m_AspectRatio * m_Zoom, m_AspectRatio * m_Zoom, -m_Zoom, m_Zoom);
        return false;
    }

    auto OrthographicCameraController::OnWindowResized(WindowResizedEvent& event) -> bool {
        AdjustViewport((double)event.GetWidth(), event.GetHeight());
        return false;
    }

    auto OrthographicCameraController::GetCamera() const -> const std::shared_ptr<OrthographicCamera>& {
        if (!m_TargetCamera)
            KATE_CORE_LOGGER_WARN("Target camera is null. This controller handles an external camera");
        return m_TargetCamera;
    }

    auto OrthographicCameraController::GetCamera() -> std::shared_ptr<OrthographicCamera>& {
        if (!m_TargetCamera)
            KATE_CORE_LOGGER_WARN("Target camera is null. This controller handles an external camera");
        return m_TargetCamera;
    }

    auto OrthographicCameraController::AdjustViewport(UInt32_T width, UInt32_T height) -> void {
        m_AspectRatio = (double)width / height;
        m_TargetCamera->SetProjection(-m_AspectRatio * m_Zoom, m_AspectRatio * m_Zoom, -m_Zoom, m_Zoom);
    }
}