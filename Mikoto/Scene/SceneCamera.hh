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

        auto Update( f64 timeStep ) -> void;

        auto SetMovementSpeed( float value ) -> void { mMovementSpeed = value; }

        auto SetRotationSpeed( float value ) -> void { mRotationSpeed = value; }
        auto SetDampingFactor( float value ) -> void { mDampingFactor = value; }

        auto SetTargetWindow( const platform::Window* window ) -> void;

        auto WantRotation( bool xAxis, bool yAxis ) -> void;

        auto SetCameraTarget(const float3& position) -> void;
        auto LockCameraToTarget(bool enable) -> void;
        auto SetOrbitDistance(float orbitDistance = 10.0f) -> void;

        // Enable camera rotation and movement
        auto EnableCamera( bool value ) -> void;

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
        const platform::Window* mTargetWindow{ nullptr };

        float3 mTargetPosition{ 0.0f, 0.0f, 0.0f };
        float3 mTargetForwardVector{ 0.0f, 0.0f, -1.0f };

        // Controls how quickly the camera moves towards the target. Higher values mean faster smoothing.
        float mDampingFactor{ 15.0f };

        bool mWantCameraRotationX{ true };
        bool mWantCameraRotationY{ true };

        float2 mLastMousePosition{ 0.0f, 0.0f };

        float mRotationSpeed{ 2.f };
        float mMovementSpeed{ 2.f };

        // Avoid speedy rotations. Compensate rotation speed
        float mRotationFactor{ 0.03f };

        bool mAllowCameraMovementAndRotation{ false };

        float3 mCameraTarget{ 0.0f, 0.0f, 0.0f };
        float mOrbitDistance{ 10.f };
        bool mLockCameraToTarget{ false };
    };
}// namespace Mikoto

#endif// MIKOTO_SCENE_CAMERA_HH