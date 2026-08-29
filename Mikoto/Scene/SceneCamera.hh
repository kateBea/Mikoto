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

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/ReferenceCounted.hh>

#include <Scene/Camera.hh>

#include <Platform/Window.hh>

namespace mikoto::scene {

    struct SceneCameraDescription {
        core::f32 mFov{};
        core::f32 mAspectRatio{};
        core::f32 mNearPlane{};
        core::f32 mFarPlane{};

        platform::Window* mWindow{};

        auto SetFarPlane( core::f32 value ) -> SceneCameraDescription&;
        auto SetNearPlane( core::f32 value ) -> SceneCameraDescription&;

        auto SetTargetWindow( platform::Window* window ) -> SceneCameraDescription&;

        auto SetFieldOfView( core::f32 value ) -> SceneCameraDescription&;
        auto SetAspectRatio( core::f32 width, core::f32 height ) -> SceneCameraDescription&;
    };

    class SceneCamera final : public Camera, public core::ReferenceCounted {
    public:
        explicit SceneCamera() = default;
        explicit SceneCamera( const SceneCameraDescription& desc );

        SceneCamera( core::f32 fov, core::f32 aspectRatio, core::f32 nearClip, core::f32 farClip );

        auto Update( core::f64 timeStep ) -> void;

        auto SetMovementSpeed( core::f32 value ) -> void { mMovementSpeed = value; }

        auto SetRotationSpeed( core::f32 value ) -> void { mRotationSpeed = value; }
        auto SetDampingFactor( core::f32 value ) -> void { mDampingFactor = value; }

        auto SetTargetWindow( const platform::Window* window ) -> void;

        auto WantRotation( bool xAxis, bool yAxis ) -> void;

        auto SetCameraTarget(const core::float3& position) -> void;
        auto LockCameraToTarget(bool enable) -> void;
        auto SetOrbitDistance(core::f32 orbitDistance = 10.0f) -> void;

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

        auto Interpolate( core::f64 timeStep ) -> void;

        auto ProcessMouseInput( core::f64 timeStep ) -> void;
        auto ProcessKeyboardInput( core::f64 timeStep ) -> void;

    private:
        // This kind of camera responds to input from a window
        // in order to compute translations and rotations
        const platform::Window* mWindow{ nullptr };

        core::float3 mTargetPosition{ 0.0f, 0.0f, 0.0f };
        core::float3 mTargetForwardVector{ 0.0f, 0.0f, -1.0f };

        // Controls how quickly the camera moves towards the target.
        // Higher values mean faster smoothing.
        core::f32 mDampingFactor{ 15.0f };

        bool mWantCameraRotationX{ true };
        bool mWantCameraRotationY{ true };

        core::float2 mLastMousePosition{ 0.0f, 0.0f };

        core::f32 mRotationSpeed{ 2.f };
        core::f32 mMovementSpeed{ 2.f };

        // Avoid speedy rotations. Compensate rotation speed
        float mRotationFactor{ 0.03f };

        bool mAllowCameraMovementAndRotation{ false };

        core::float3 mCameraTarget{ 0.0f, 0.0f, 0.0f };
        core::f32 mOrbitDistance{ 10.f };
        bool mLockCameraToTarget{ false };
    };
}// namespace Mikoto

#endif// MIKOTO_SCENE_CAMERA_HH