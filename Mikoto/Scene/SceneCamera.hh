//    Copyright 2026 ケイト
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

#ifndef MIKOTO_SCENE_CAMERA_HH
#define MIKOTO_SCENE_CAMERA_HH

#include <Platform/Window.hh>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Scene/Camera.hh>

namespace mikoto::scene {
    using namespace mikoto::core;

    struct SceneCameraDescription {
        float mFov{};
        float mAspectRatio{};
        float mNearPlane{};
        float mFarPlane{};

        platform::Window* mWindow{};
    };

    class SceneCamera final : public Camera {
    public:
        explicit SceneCamera() = default;
        explicit SceneCamera( const SceneCameraDescription& desc );

        SceneCamera( float fov, float aspectRatio, float nearClip, float farClip );

        /**
         * @brief Updates the camera state using the elapsed time.
         * @param timeStep Elapsed time since the last frame.
         * */
        auto Update( double timeStep ) -> void;

        /**
         * @brief Sets the size of the viewport for the camera.
         * @param width The width of the viewport.
         * @param height The height of the viewport.
         * */
        auto SetViewportSize( float width, float height ) -> void;

        /**
         * @brief Sets the movement speed of the camera.
         * This value determines the speed at which we can move the camera
         * with Key_W, Key_A, Key_S and Key_D.
         * @param value The new movement speed value to set.
         * */
        auto SetMovementSpeed( float value ) -> void { m_MovementSpeed = value; }

        /**
         * @brief Sets the rotation speed of the camera. Sets the speed at which we can rotate the camera with the mouse.
         * @param value The new rotation speed value to set.
         * */
        auto SetRotationSpeed( float value ) -> void { m_RotationSpeed = value; }
        auto SetDampingFactor( float value ) -> void { m_DampingFactor = value; }

        /**
         * @brief Sets the field of view of the camera.
         * @param value The new field of view value to set.
         * */
        auto SetFieldOfView( float value ) -> void { m_FieldOfView = value; }

        auto SetTargetWindow( const platform::Window* window ) -> void;

        auto WantRotation( bool xAxis, bool yAxis ) -> void;

        auto SetCameraTarget(const float3& position) -> void;
        auto LockCameraToTarget(bool enable) -> void;
        auto SetOrbitDistance(float orbitDistance = 10.0f) -> void;

        /**
         * @brief Sets the far clipping plane distance of the camera.
         * @param value The new far clipping plane value to set.
         * */
        auto SetFarPlane( float value ) -> void { m_FarClip = value; }

        /**
         * @brief Sets the near clipping plane distance of the camera.
         * @param value The new near clipping plane value to set.
         * */
        auto SetNearPlane( float value ) -> void { m_NearClip = value; }

        /**
         * @brief Enables or disables camera movement and rotation.
         * @param value If true, camera movement and rotation are allowed; otherwise, they are not.
         * */
        auto EnableCamera( const bool value ) { m_AllowCameraMovementAndRotation = value; }

        // Vignette
        // Size
        // Shape

        // Depth of field
        // Aperture ( F-Stop )
        // Focal length
        // Depht blur

    private:
        // [Internal]
        auto UpdateViewMatrix() -> void;

        auto Interpolate( double timeStep ) -> void;

        auto ProcessMouseInput( double timeStep ) -> void;
        auto ProcessKeyboardInput( double timeStep ) -> void;

    private:
        // This kind of camera responds to input from a window
        // in order to compute translations and rotations
        const platform::Window* m_TargetWindow{ nullptr };

        float3 m_TargetPosition{ 0.0f, 0.0f, 0.0f };
        float3 m_TargetForwardVector{ 0.0f, 0.0f, -1.0f };

        // Controls how quickly the camera moves towards the target. Higher values mean faster smoothing.
        float m_DampingFactor{ 15.0f };

        bool m_WantCameraRotationX{ true };
        bool m_WantCameraRotationY{ true };

        float2 m_LastMousePosition{ 0.0f, 0.0f };

        float m_RotationSpeed{ 2.f };
        float m_MovementSpeed{ 2.f };

        // Avoid speedy rotations. Compensate rotation speed
        float m_RotationFactor{ 0.03f };

        bool m_AllowCameraMovementAndRotation{ false };

        float3 m_CameraTarget{ 0.0f, 0.0f, 0.0f };
        float m_OrbitDistance{ 10.f };
        bool m_LockCameraToTarget{ false };
    };
}// namespace Mikoto

#endif// MIKOTO_SCENE_CAMERA_HH