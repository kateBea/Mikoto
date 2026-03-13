//    Copyright 2025 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cmath>
#include <utility>

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

#include <Common/Common.hh>
#include <Core/InputService.hh>
#include <Library/Math/Math.hh>
#include <Library/Utility/Types.hh>
#include <Scene/SceneCamera.hh>

namespace Mikoto {

    SceneCamera::SceneCamera( const SceneCameraDescription &desc )
        : Camera{ glm::perspective( glm::radians( desc.Fov ), desc.AspectRatio, desc.NearPlane, desc.FarPlane ) },
        m_TargetWindow{ desc.TargetWindow } {

        m_Position = Vec3F{ 100.0f, 100.5f, 100.0f };
        m_TargetPosition = Vec3F{ 10.0f, 14.5f, 21.0f };
        m_ForwardVector = glm::vec3{ -0.457344413f, -0.443095952f, -0.771039605f };
        m_TargetForwardVector = m_ForwardVector;
    }

    SceneCamera::SceneCamera( const float fov, const float aspectRatio, const float nearClip, const float farClip )
        : Camera{ glm::perspective( glm::radians( fov ), aspectRatio, nearClip, farClip ) } {
        m_NearClip = nearClip;
        m_FarClip = farClip;
        m_FieldOfView = fov;
        m_AspectRatio = aspectRatio;

        m_Position = Vec3F{ 100.0f, 100.5f, 100.0f };
        m_TargetPosition = Vec3F{ 10.0f, 14.5f, 21.0f };
        m_ForwardVector = glm::vec3{ -0.457344413f, -0.443095952f, -0.771039605f };
        m_TargetForwardVector = m_ForwardVector;

        UpdateViewMatrix();
    }
    
    auto SceneCamera::SetTargetWindow( const Window* window ) -> void {
        if (window != nullptr) {
            m_TargetWindow = window;
        }
    }

    auto SceneCamera::UpdateViewMatrix() -> void {
        if (m_LockCameraToTarget ) {
        } else {
            m_ViewMatrix = lookAt( m_Position, m_Position + m_ForwardVector, m_CameraUpVector );
        }
    }

    auto SceneCamera::ProcessMouseInput( const double timeStep ) -> void {
        const glm::vec2 mousePos{ m_TargetWindow->GetMouseX(), m_TargetWindow->GetMouseY() };
        const glm::vec2 delta{ (mousePos - m_LastMousePosition) * m_RotationFactor };

        // Still update the last camera pos,
        // this avoids camera jumping
        m_LastMousePosition = mousePos;

        if (delta == glm::vec2{0.0f}) return;

        m_Pitch = (m_WantCameraRotationX ? delta.y : 0.0f) * m_RotationSpeed * static_cast<float>( timeStep );
        m_Yaw   = (m_WantCameraRotationY ? delta.x : 0.0f) * m_RotationSpeed * static_cast<float>( timeStep );

        const glm::quat pitchQ{ glm::angleAxis(-m_Pitch, m_RightVector) };
        const glm::quat yawQ{ glm::angleAxis(-m_Yaw,   Math::UNIT_VECTOR_Y) };

        // Combine pitch and yaw into a single rotation quaternion.
        // Quaternion multiplication composes rotations in a stable way and avoids gimbal lock.
        // Order matters as quaternion product is non-commutative.
        const glm::quat rotation{ yawQ * pitchQ };

        // Rotate the camera's forward direction using the combined quaternion.
        m_TargetForwardVector = glm::normalize(glm::rotate(rotation, m_TargetForwardVector));
    }

    auto SceneCamera::ProcessKeyboardInput( const double timeStep ) -> void {

        const float speed = m_MovementSpeed * static_cast<float>( timeStep );
        if ( m_TargetWindow->IsKeyPressed( Key_W ) ) m_TargetPosition += m_ForwardVector * speed;
        if ( m_TargetWindow->IsKeyPressed( Key_S ) ) m_TargetPosition -= m_ForwardVector * speed;
        if ( m_TargetWindow->IsKeyPressed( Key_A ) ) m_TargetPosition -= m_RightVector * speed;
        if ( m_TargetWindow->IsKeyPressed( Key_D ) ) m_TargetPosition += m_RightVector * speed;
        if ( m_TargetWindow->IsKeyPressed( Key_Space ) || m_TargetWindow->IsKeyPressed( Key_E ) ) m_TargetPosition.y += speed;
        if ( m_TargetWindow->IsKeyPressed( Key_Q ) ) m_TargetPosition.y -= speed;
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
            m_LastMousePosition = { m_TargetWindow->GetMouseX(), m_TargetWindow->GetMouseY() };

            return;
        }

        if (m_TargetWindow == nullptr) {
            MKT_CORE_LOGGER_WARN( "SceneCamera::UpdateState - Camera has no target window. Forgot to call SetCamera(...)?" );
            return;
        }

        // We apply camera smooth damping with computing the values for the final position and the final forward vector used to determine the rotation
        // Whenever there’s user input (keyboard/mouse), we update the targets, not the current values directly.
        // The interpolation will smoothly move the camera towards the end position, the same applies for the rotation.

        m_RightVector = glm::cross( m_ForwardVector, Math::UNIT_VECTOR_Y );

        ProcessMouseInput( timeStep );
        ProcessKeyboardInput( timeStep );

        Interpolate( timeStep );
    }

    auto SceneCamera::SetViewportSize( const float width, const float height ) -> void {
        if ( m_ViewportWidth == width && m_ViewportHeight == height ) {
            return;
        }

        m_ViewportWidth = width;
        m_ViewportHeight = height;

        UpdateProjection();
    }

    auto SceneCamera::WantRotation( const bool xAxis, const bool yAxis ) -> void {
        m_WantCameraRotationX = xAxis;
        m_WantCameraRotationY = yAxis;
    }

    auto SceneCamera::SetCameraTarget( const Vec3F &position ) -> void {
        m_CameraTarget = position;
    }

    auto SceneCamera::LockCameraToTarget( bool enable ) -> void {
        m_LockCameraToTarget = enable;
    }

    auto SceneCamera::SetOrbitDistance( float orbitDistance ) -> void {
        m_OrbitDistance = orbitDistance;
    }
}
