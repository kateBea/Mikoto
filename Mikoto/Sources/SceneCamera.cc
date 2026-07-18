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

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/InputSystem.hh>

#include <Logging/Assert.hh>
#include <Logging/Logger.hh>

#include <Scene/SceneCamera.hh>

namespace mikoto::scene {

    using namespace mikoto::core;
    using namespace mikoto::platform;

    auto SceneCameraDescription::SetFarPlane( core::f32 value ) -> SceneCameraDescription & {
        mFarPlane = value;
        return *this;
    }

    auto SceneCameraDescription::SetNearPlane( core::f32 value ) -> SceneCameraDescription & {
        mNearPlane = value;
        return *this;
    }

    auto SceneCameraDescription::SetTargetWindow( platform::Window *window ) -> SceneCameraDescription & {
        mWindow = window;
        return *this;
    }

    auto SceneCameraDescription::SetFieldOfView( core::f32 value ) -> SceneCameraDescription & {
        mFov = value;
        return *this;
    }

    auto SceneCameraDescription::SetAspectRatio( core::f32 width, core::f32 height ) -> SceneCameraDescription & {
        mAspectRatio = width / height;
        return *this;
    }

    SceneCamera::SceneCamera( const SceneCameraDescription &desc )
        : Camera{ glm::perspective( glm::radians( desc.mFov ), desc.mAspectRatio, desc.mNearPlane, desc.mFarPlane ) },
        mWindow{ desc.mWindow } {

        mPosition = float3{ 100.0f, 100.5f, 100.0f };
        mForward = float3{ 1.0f, 1.0f, 1.0f };
        mTargetForwardVector = mForward;

        UpdateViewMatrix();
    }

    SceneCamera::SceneCamera( f32 fov, f32 aspectRatio, f32 nearClip, f32 farClip )
        : Camera{ glm::perspective( glm::radians( fov ), aspectRatio, nearClip, farClip ) } {
        mNearPlane = nearClip;
        mFarPlane = farClip;
        mFov = fov;
        mAspectRatio = aspectRatio;

        mPosition = float3{ 100.0f, 100.5f, 100.0f };
        mForward = float3{ 1.0f, 1.0f, 1.0f };
        mTargetForwardVector = mForward;

        UpdateViewMatrix();
    }

    auto SceneCamera::SetTargetWindow( const Window* window ) -> void {
        if (window != nullptr) {
            mWindow = window;
        }
    }

    auto SceneCamera::EnableCamera( bool value ) -> void {
        mAllowCameraMovementAndRotation = value;
    }

    auto SceneCamera::UpdateViewMatrix() -> void {
        if (mLockCameraToTarget ) {
        } else {
            mView = lookAt( mPosition, mPosition + mForward, mUp );
        }
    }

    auto SceneCamera::ProcessMouseInput( const double timeStep ) -> void {
        const glm::vec2 mousePos{ mWindow->GetMouseX(), mWindow->GetMouseY() };
        const glm::vec2 delta{ (mousePos - mLastMousePosition) * mRotationFactor };

        // Still update the last camera pos,
        // this avoids camera jumping
        mLastMousePosition = mousePos;

        if (delta == float2{ 0.0f }) {
            return;
        }

        mPitch = (mWantCameraRotationX ? delta.y : 0.0f) * mRotationSpeed * static_cast<float>( timeStep );
        mYaw   = (mWantCameraRotationY ? delta.x : 0.0f) * mRotationSpeed * static_cast<float>( timeStep );

        const glm::quat pitchQ{ glm::angleAxis(-mPitch, mRightVector) };
        const glm::quat yawQ{ glm::angleAxis(-mYaw, math::constants::kUnitVectorY) };

        // Combine pitch and yaw into a single rotation quaternion.
        // Order matters as quaternion product is non-commutative.
        const glm::quat rotation{ yawQ * pitchQ };

        // Rotate the camera's forward direction using the combined quaternion.
        mTargetForwardVector = glm::normalize(glm::rotate(rotation, mTargetForwardVector));
    }

    auto SceneCamera::ProcessKeyboardInput( const double timeStep ) -> void {

        const float speed{ mMovementSpeed * static_cast<float>( timeStep ) };
        if ( mWindow->IsKeyPressed( Key_W ) ) mTargetPosition += mForward * speed;
        if ( mWindow->IsKeyPressed( Key_S ) ) mTargetPosition -= mForward * speed;
        if ( mWindow->IsKeyPressed( Key_A ) ) mTargetPosition -= mRightVector * speed;
        if ( mWindow->IsKeyPressed( Key_D ) ) mTargetPosition += mRightVector * speed;
        if ( mWindow->IsKeyPressed( Key_Space ) || mWindow->IsKeyPressed( Key_E ) ) mTargetPosition.y += speed;
        if ( mWindow->IsKeyPressed( Key_Q ) ) mTargetPosition.y -= speed;
    }

    auto SceneCamera::Interpolate( const double timeStep ) -> void {
        const auto interpolationFactor{ 1.0f - glm::exp( -(mDampingFactor * static_cast<float>( timeStep ) ) ) };

        mPosition = glm::mix( mPosition, mTargetPosition, interpolationFactor );
        mForward = glm::normalize( glm::mix( mForward, mTargetForwardVector, interpolationFactor ) );
    }

    auto SceneCamera::Update( f64 timeStep ) -> void {
        UpdateProjection();
        UpdateViewMatrix();

        if ( !mAllowCameraMovementAndRotation ) {
            mLastMousePosition = { mWindow->GetMouseX(), mWindow->GetMouseY() };

            // Continue interpolation if they aren't equal
            if ( mPosition != mTargetPosition || mForward != mTargetForwardVector ) {
                Interpolate( timeStep );
            }

            return;
        }

        if (mWindow == nullptr) {
            MKT_CORE_LOGGER_WARN( "SceneCamera::UpdateState - Camera has no target window. Forgot to call SetTargetWindow(...)?" );
            return;
        }

        mRightVector = glm::normalize( glm::cross( mForward, math::constants::kUnitVectorY ) );

        ProcessMouseInput( timeStep );
        ProcessKeyboardInput( timeStep );

        Interpolate( timeStep );
    }

    auto SceneCamera::WantRotation( const bool xAxis, const bool yAxis ) -> void {
        mWantCameraRotationX = xAxis;
        mWantCameraRotationY = yAxis;
    }

    auto SceneCamera::SetCameraTarget( const float3 &position ) -> void {
        mCameraTarget = position;
    }

    auto SceneCamera::LockCameraToTarget( bool enable ) -> void {
        mLockCameraToTarget = enable;
    }

    auto SceneCamera::SetOrbitDistance( float orbitDistance ) -> void {
        mOrbitDistance = orbitDistance;
    }
}
