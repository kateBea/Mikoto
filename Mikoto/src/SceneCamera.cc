/**
 * EditorCamera.hh
 * Created by kate on 8/29/23.
 * */

// C++ Standard Library
#include <cmath>
#include <utility>

// Third-Party Libraries
#include <glm/glm.hpp>

// Quaternions with extensions
#include <glm/gtx/quaternion.hpp>

// Project Headers
#include <Common/Common.hh>
#include <Common/Constants.hh>
#include <Core/Events/CoreEvents.hh>
#include <Core/Input/KeyCodes.hh>
#include <Core/Input/MouseCodes.hh>
#include <Core/System/EventSystem.hh>
#include <Core/System/InputSystem.hh>
#include <Library/Utility/Types.hh>
#include <Scene/Camera/SceneCamera.hh>
namespace Mikoto {

    SceneCamera::SceneCamera( const float fov, const float aspectRatio, const float nearClip, const float farClip )
        : Camera{ glm::perspective( glm::radians( fov ), aspectRatio, nearClip, farClip ) } {
        m_NearClip = nearClip;
        m_FarClip = farClip;
        m_FieldOfView = fov;
        m_AspectRatio = aspectRatio;

        m_Position = glm::vec3{ -31.07f, 48.06f, 100.0f };
        m_TargetPosition = m_Position;
        m_ForwardVector = glm::normalize( glm::vec3{ 15.0f, -10.0f, -30.0f } );
        m_TargetForwardVector = m_ForwardVector;

        UpdateViewMatrix();
    }

    auto SceneCamera::UpdateProjection() -> void {
        m_AspectRatio = m_ViewportWidth / m_ViewportHeight;
        SetProjectionFromType();
    }

    auto SceneCamera::UpdateViewMatrix() -> void {
        m_ViewMatrix = lookAt( m_Position, m_Position + m_ForwardVector, m_CameraUpVector );
    }

    auto SceneCamera::ProcessMouseInput( const double timeStep ) -> void {
        m_CameraUpVector = GLM_UNIT_VECTOR_Y;

        InputSystem& inputSystem{ Engine::GetSystem<InputSystem>() };

        const glm::vec2 mouseCurrentPosition{ inputSystem.GetMouseX(), inputSystem.GetMouseY() };
        const glm::vec2 delta{ ( mouseCurrentPosition - m_LastMousePosition ) * m_RotationFactor };

        m_LastMousePosition = mouseCurrentPosition;

        if ( delta.x != 0.0f || delta.y != 0.0f ) {
            m_Pitch = ( m_WantCameraRotationX ? delta.y : 0.0f ) * m_RotationSpeed * static_cast<float>( timeStep );
            m_Yaw = ( m_WantCameraRotationY ? delta.x : 0.0f ) * m_RotationSpeed * static_cast<float>( timeStep );

            const glm::quat rotation{ glm::normalize( glm::cross( glm::angleAxis( -m_Pitch, m_RightVector ), glm::angleAxis( -m_Yaw, GLM_UNIT_VECTOR_Y ) ) ) };
            m_TargetForwardVector = glm::rotate( rotation, m_TargetForwardVector );
        }
    }

    auto SceneCamera::ProcessKeyboardInput( const double timeStep ) -> void {
        static InputSystem& inputSystem{ Engine::GetSystem<InputSystem>() };

        const float speed = m_MovementSpeed * static_cast<float>( timeStep );
        if ( inputSystem.IsKeyPressed( Key_W ) ) m_TargetPosition += m_ForwardVector * speed;
        if ( inputSystem.IsKeyPressed( Key_S ) ) m_TargetPosition -= m_ForwardVector * speed;
        if ( inputSystem.IsKeyPressed( Key_A ) ) m_TargetPosition -= m_RightVector * speed;
        if ( inputSystem.IsKeyPressed( Key_D ) ) m_TargetPosition += m_RightVector * speed;
        if ( inputSystem.IsKeyPressed( Key_Space ) || inputSystem.IsKeyPressed( Key_E ) ) m_TargetPosition.y += speed;
        if ( inputSystem.IsKeyPressed( Key_Q ) ) m_TargetPosition.y -= speed;
    }

    auto SceneCamera::Interpolate( const double timeStep ) -> void {
        m_Position = glm::mix( m_Position, m_TargetPosition, 1.0f - glm::exp( -m_DampingFactor * static_cast<float>( timeStep ) ) );
        m_ForwardVector = glm::normalize( glm::mix( m_ForwardVector, m_TargetForwardVector, 1.0f - glm::exp( -m_DampingFactor * static_cast<float>( timeStep ) ) ) );
    }

    auto SceneCamera::UpdateState( const double timeStep ) -> void {
        UpdateProjection();
        UpdateViewMatrix();

        // Continue interpolation if they aren't equal
        if ( m_Position != m_TargetPosition || m_ForwardVector != m_TargetForwardVector ) {
            Interpolate( timeStep );
        }

        if ( !m_AllowCameraMovementAndRotation ) {
            InputSystem& inputSystem{ Engine::GetSystem<InputSystem>() };
            m_LastMousePosition = { inputSystem.GetMouseX(), inputSystem.GetMouseY() };
            return;
        }

        // We apply camera smooth damping with computing the values for the final position and the final forward vector used to determine the rotation
        // Whenever there’s user input (keyboard/mouse), we update the targets, not the current values directly.
        // The interpolation will smoothly move the camera towards the end position, the same applies for the rotation.

        m_RightVector = glm::cross( m_ForwardVector, GLM_UNIT_VECTOR_Y );

        ProcessMouseInput( timeStep );
        ProcessKeyboardInput( timeStep );

        Interpolate( timeStep );
    }

    auto SceneCamera::SetViewportSize( const float width, const float height ) -> void {
        if ( m_ViewportWidth == width && m_ViewportHeight == height ) return;
        m_ViewportWidth = width;
        m_ViewportHeight = height;
        UpdateProjection();
    }

    auto SceneCamera::WantRotation( const bool xAxis, const bool yAxis ) -> void {
        m_WantCameraRotationX = xAxis;
        m_WantCameraRotationY = yAxis;
    }
}// namespace Mikoto
