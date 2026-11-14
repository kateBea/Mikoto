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

#include <utility>
#include <cmath>
#include <algorithm>

#include <EASTL/algorithm.h>

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/compatibility.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Logging/Assert.hh>
#include <Logging/Logger.hh>

#include <Math/Math.hh>

#include <Platform/InputSystem.hh>

#include <Scene/SceneCamera.hh>

namespace mikoto::scene {

    using namespace mikoto::core;
    using namespace mikoto::platform;

    auto SceneCameraDescription::SetFarPlane( core::f32 value ) -> SceneCameraDescription& {
        MKT_BEGIN_PROFILER();
        mFarPlane = value;
        return *this;
    }

    auto SceneCameraDescription::SetNearPlane( core::f32 value ) -> SceneCameraDescription& {
        MKT_BEGIN_PROFILER();
        mNearPlane = value;
        return *this;
    }

    auto SceneCameraDescription::SetTargetWindow( platform::IWindow* window ) -> SceneCameraDescription& {
        MKT_BEGIN_PROFILER();
        mWindow = window;
        return *this;
    }

    auto SceneCameraDescription::SetFieldOfView( core::f32 value ) -> SceneCameraDescription& {
        MKT_BEGIN_PROFILER();
        mFov = value;
        return *this;
    }

    auto SceneCameraDescription::SetAspectRatio( core::f32 width, core::f32 height ) -> SceneCameraDescription& {
        MKT_BEGIN_PROFILER();

        // A minimized or unattached window must not invalidate the lens.
        if ( width > 0.0f && height > 0.0f ) {
            const core::f32 aspect{ width / height };

            if ( aspect > 0.0f ) {
                mAspectRatio = aspect;
            }
        }

        return *this;
    }

    SceneCamera::SceneCamera() {
        MKT_BEGIN_PROFILER();
        LookAt( mPosition, mPosition + mForward );
    }

    SceneCamera::SceneCamera( const SceneCameraDescription& desc )
        : Camera{},
          mWindow{ desc.mWindow } {
        MKT_BEGIN_PROFILER();

        MKT_ASSERT( desc.mNearPlane > 0.0f && desc.mNearPlane < desc.mFarPlane,
            "SceneCamera requires 0 < near plane < far plane" );
        MKT_ASSERT( desc.mAspectRatio > 0.0f, "SceneCamera aspect ratio must be positive" );
        MKT_ASSERT( desc.mFov > 0.0f && desc.mFov < 179.0f, "SceneCamera field of view must be between 0 and 179 degrees" );

        mNearPlane = desc.mNearPlane;
        mFarPlane = desc.mFarPlane;
        mFov = desc.mFov;
        mAspectRatio = desc.mAspectRatio;
        // Constructors accept an aspect, not pixel dimensions. Keep the
        // stored viewport consistent for serialization and fixed-aspect cameras.
        mViewportWidth = mViewportHeight * mAspectRatio;
        UpdateProjection();

        LookAt( { 100.0f, 100.5f, 100.0f }, { 101.0f, 101.5f, 101.0f } );
    }

    SceneCamera::SceneCamera( core::f32 fov, core::f32 aspectRatio, core::f32 nearClip, core::f32 farClip )
        : SceneCamera{ SceneCameraDescription{ .mFov = fov, .mAspectRatio = aspectRatio, .mNearPlane = nearClip, .mFarPlane = farClip } } {
    }

    auto SceneCamera::LookAt( const core::float3& position, const core::float3& target ) -> void {
        MKT_BEGIN_PROFILER();
        const core::f32 distance{ glm::length( target - position ) };

        if ( distance < 0.01f ) {
            return;
        }

        mOrbitDistance = glm::min( distance, 1000000.0f );
        SetViewPose( position, target - position );
    }

    auto SceneCamera::SetViewPose( const core::float3& position, const core::float3& direction ) -> void {
        MKT_BEGIN_PROFILER();

        MKT_ASSERT( glm::dot( direction, direction ) > 0.000001f, "Camera target must differ from its position" );

        mViewTransitionTime = 0.5f;
        mPosition = mTargetPosition = position;
        mForward = glm::normalize( direction );

        // Top/bottom orientation-gizmo views are exactly vertical. Their yaw
        // is undefined, so choose a deterministic north-up basis at the pole.
        const bool vertical{ mForward.x * mForward.x + mForward.z * mForward.z < 1e-12f };
        mYaw = vertical ? 0.0f : glm::atan2( mForward.x, -mForward.z );
        mPitch = glm::asin( glm::clamp( mForward.y, -1.0f, 1.0f ) );
        mTargetAngles = { mYaw, mPitch };

        // Rebase both navigation modes on the displayed pose without residual motion.
        mPositionVelocity = {};
        mAngleVelocity = {};

        mOrbitTarget = mTargetOrbitTarget = mPosition + mForward * mOrbitDistance;
        mTargetOrbitDistance = mOrbitDistance;
        mOrbitVelocity = {};
        mOrbitDistanceVelocity = 0.0f;
        mResetMouseSample = true;

        mRightVector = { glm::cos( mYaw ), 0.0f, glm::sin( mYaw ) };
        mUp = math::constants::kUnitVectorY;
        UpdateViewMatrix();
    }

    auto SceneCamera::SetNavigationMode( CameraNavigationMode mode ) -> void {
        if ( mode == mNavigationMode || ( mode != CameraNavigationMode::eFly && mode != CameraNavigationMode::eOrbit ) ) {
            return;
        }

        mNavigationMode = mode;
        // Rebase on the displayed pose, not a spring destination the user has
        // not reached yet. Switching modes must never snap or resume old motion.
        SetViewPose( mPosition, mForward );
    }

    auto SceneCamera::GetNavigationMode() const -> CameraNavigationMode {
        return mNavigationMode;
    }

    auto SceneCamera::GetOrbitTarget() const -> core::float3 {
        return mOrbitTarget;
    }

    auto SceneCamera::GetOrbitDistance() const -> core::f32 {
        return mOrbitDistance;
    }

    auto SceneCamera::SetOrbitDistance( core::f32 distance ) -> void {
        mOrbitDistance = glm::clamp( distance, 0.01f, 1000000.0f );
        SetViewPose( mPosition, mForward );
    }

    auto SceneCamera::SetViewDirection( const core::float3& direction ) -> void {
        const core::f32 lengthSquared{ glm::dot( direction, direction ) };

        if ( lengthSquared < 0.000001f ) {
            return;
        }

        const core::float3 forward{ glm::normalize( direction ) };
        const core::float3 position{ mNavigationMode == CameraNavigationMode::eOrbit ? mOrbitTarget - forward * mOrbitDistance : mPosition };
        SetViewPose( position, forward );
    }

    auto SceneCamera::TransitionViewDirection( const float3& direction ) -> void {
        if ( glm::dot( direction, direction ) < 0.000001f ) { return; }
        const auto forward{ glm::normalize( direction ) };
        const auto up{ glm::abs( forward.y ) > 0.9999f ? float3{ 0, 0, forward.y > 0 ? 1.0f : -1.0f } : float3{ 0, 1, 0 } };
        const auto from{ glm::quat_cast( glm::mat3{ glm::inverse( mView ) } ) };
        const auto to{ glm::quatLookAtRH( forward, up ) };
        mViewTransitionFrom = { from.x, from.y, from.z, from.w };
        mViewTransitionTo = { to.x, to.y, to.z, to.w };
        mViewTransitionEye = mPosition;
        mViewTransitionPivot = mOrbitTarget;
        mViewTransitionTime = 0.0f;
    }

    auto SceneCamera::TransitionProjectionType( ProjectionType type ) -> void {
        if ( type == mProjectionType ) { return; }
        const auto displayed{ mProjection };
        if ( type == ProjectionType::eOrthographic ) {
            SetOrthographicHeight( 2.0f * mOrbitDistance * glm::tan( glm::radians( mFov ) * 0.5f ) );
        }
        SetProjectionType( type );
        mProjectionTransitionFrom = displayed;
        mProjectionTransitionTime = 0.0f;
        mProjection = displayed;
    }

    auto SceneCamera::GetMovementSpeed() const -> core::f32 {
        return mMovementSpeed;
    }

    auto SceneCamera::GetRotationSpeed() const -> core::f32 {
        return mRotationSpeed;
    }

    auto SceneCamera::GetSmoothTime() const -> core::f32 {
        return mSmoothTime;
    }

    auto SceneCamera::WantsXAxisRotation() const -> bool {
        return mWantCameraRotationX;
    }

    auto SceneCamera::WantsYAxisRotation() const -> bool {
        return mWantCameraRotationY;
    }

    auto SceneCamera::ResetSettings() -> void {
        SetMovementSpeed( 70.0f );
        SetRotationSpeed( 15.0f );
        SetSmoothTime( 2.0f / 27.0f );
        WantRotation( true, true );
        SetNavigationMode( CameraNavigationMode::eFly );
        SetProjectionType( ProjectionType::ePerspective );
        SetClipPlanes( 0.1f, 5000.0f );
        SetFieldOfView( 45.0f );
        SetOrthographicHeight( 20.0f );
    }

    auto SceneCamera::ResetView() -> void {
        SetViewPose( { 8.0f, 6.5f, 10.0f }, { -8.0f, -5.5f, -10.0f } );
        SetOrbitDistance( 10.0f );
    }

    auto SceneCamera::SetMovementSpeed( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();
        mMovementSpeed = glm::clamp( value, 0.01f, 5000.0f );
    }

    auto SceneCamera::SetRotationSpeed( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        mRotationSpeed = glm::clamp( value, 0.1f, 500.0f );
    }

    auto SceneCamera::SetTargetWindow( const platform::IWindow* window ) -> void {
        MKT_BEGIN_PROFILER();

        if ( window != nullptr ) {
            mWindow = window;
            mResetMouseSample = true;
        }
    }

    auto SceneCamera::EnableCamera( bool value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( value != mAllowCameraMovementAndRotation ) {
            mResetMouseSample = true;
        }

        mAllowCameraMovementAndRotation = value;
    }

    auto SceneCamera::SetSmoothTime( float seconds ) -> void {
        MKT_BEGIN_PROFILER();
        mSmoothTime = glm::min( seconds, 2.0f );
    }

    auto SceneCamera::SetDampingFactor( float value ) -> void {
        MKT_BEGIN_PROFILER();
        SetSmoothTime( 2.0f / value );
    }

    auto SceneCamera::GetVignetteSettings() const -> const VignetteSettings& {
        MKT_BEGIN_PROFILER();
        return mVignette;
    }

    auto SceneCamera::GetFilmGrainSettings() const -> const FilmGrainSettings& {
        return mFilmGrain;
    }

    auto SceneCamera::SetFilmGrainSettings( const FilmGrainSettings& settings ) -> void {
        mFilmGrain.mAmount = glm::clamp( settings.mAmount, 0.0f, 1.0f );
    }

    auto SceneCamera::SetVignetteSettings( const VignetteSettings& settings ) -> void {
        MKT_BEGIN_PROFILER();

        const VignetteSettings defaults{};
        const auto sanitize{ []( core::f32 value, core::f32 fallback, core::f32 minimum, core::f32 maximum ) {
            return glm::clamp( value, minimum, maximum );
        } };

        mVignette.mEnabled = settings.mEnabled;
        mVignette.mShape = settings.mShape == VignetteShape::eRectangle ? VignetteShape::eRectangle : VignetteShape::eCircle;
        mVignette.mCenter = {
            sanitize( settings.mCenter.x, defaults.mCenter.x, 0.0f, 1.0f ),
            sanitize( settings.mCenter.y, defaults.mCenter.y, 0.0f, 1.0f )
        };
        mVignette.mHalfSize = {
            sanitize( settings.mHalfSize.x, defaults.mHalfSize.x, 0.0001f, 4.0f ),
            sanitize( settings.mHalfSize.y, defaults.mHalfSize.y, 0.0001f, 4.0f )
        };
        mVignette.mFeather = sanitize( settings.mFeather, defaults.mFeather, 0.0001f, 4.0f );
        mVignette.mStrength = sanitize( settings.mStrength, defaults.mStrength, 0.0f, 1.0f );
        mVignette.mColor = {
            sanitize( settings.mColor.r, defaults.mColor.r, 0.0f, 1.0f ),
            sanitize( settings.mColor.g, defaults.mColor.g, 0.0f, 1.0f ),
            sanitize( settings.mColor.b, defaults.mColor.b, 0.0f, 1.0f )
        };
        mVignette.mAspectCorrect = settings.mAspectCorrect;
    }

    auto SceneCamera::SetVignetteSize( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        auto settings{ mVignette };
        settings.mHalfSize = core::float2{ value };
        SetVignetteSettings( settings );
    }

    auto SceneCamera::SetVignetteShape( VignetteShape value ) -> void {
        MKT_BEGIN_PROFILER();

        auto settings{ mVignette };
        settings.mShape = value;
        SetVignetteSettings( settings );
    }

    auto SceneCamera::SetVignetteIntensity( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        auto settings{ mVignette };
        settings.mStrength = value;
        SetVignetteSettings( settings );
    }

    auto SceneCamera::SetDepthOfFieldSettings( const DepthOfFieldSettings& settings ) -> void {
        MKT_BEGIN_PROFILER();
        const DepthOfFieldSettings defaults{};
        const auto sanitize = []( core::f32 value, core::f32 fallback, core::f32 minimum, core::f32 maximum ) -> core::f32 {
            return eastl::clamp( value, minimum, maximum );
        };

        mDepthOfField.mEnabled = settings.mEnabled;
        mDepthOfField.mMaxAperture = sanitize( settings.mMaxAperture, defaults.mMaxAperture, 0.7f, 64.0f );
        mDepthOfField.mAperture = sanitize( settings.mAperture, defaults.mAperture, mDepthOfField.mMaxAperture, 64.0f );
        mDepthOfField.mAperture = glm::max( mDepthOfField.mAperture, mDepthOfField.mMaxAperture );
        mDepthOfField.mSensorWidth = sanitize( settings.mSensorWidth, defaults.mSensorWidth, 1.0f, 100.0f );
        mDepthOfField.mUsePhysicalLens = settings.mUsePhysicalLens;
        mDepthOfField.mFocalLength = sanitize( settings.mFocalLength, defaults.mFocalLength, 1.0f, 300.0f );

        // Keep the focus plane beyond the focal length for thin-lens calculations.
        const core::f32 minimumFocus{ mDepthOfField.mFocalLength * 0.001f + 0.001f };
        mDepthOfField.mFocusDistance = sanitize( settings.mFocusDistance, defaults.mFocusDistance, minimumFocus, 100000.0f );
        mDepthOfField.mMaxBlur = sanitize( settings.mMaxBlur, defaults.mMaxBlur, 0.0f, 64.0f );
        mDepthOfField.mBladeCount = glm::clamp( settings.mBladeCount, 3u, 16u );
        mDepthOfField.mBladeCurvature = sanitize( settings.mBladeCurvature, defaults.mBladeCurvature, 0.0f, 1.0f );
        mDepthOfField.mBladeRotation = sanitize( settings.mBladeRotation, defaults.mBladeRotation, -180.0f, 180.0f );

        UpdateProjection();
    }

    auto SceneCamera::SetAperture( core::f32 fStop ) -> void {
        MKT_BEGIN_PROFILER();

        auto settings{ mDepthOfField };
        settings.mAperture = fStop;
        SetDepthOfFieldSettings( settings );
    }

    auto SceneCamera::SetFocalLength( core::f32 millimeters ) -> void {
        MKT_BEGIN_PROFILER();

        auto settings{ mDepthOfField };
        settings.mFocalLength = millimeters;
        SetDepthOfFieldSettings( settings );
    }

    auto SceneCamera::SetFocusDistance( core::f32 distance ) -> void {
        MKT_BEGIN_PROFILER();

        auto settings{ mDepthOfField };
        settings.mFocusDistance = distance;
        SetDepthOfFieldSettings( settings );
    }

    auto SceneCamera::SetDepthOfFieldBlur( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        auto settings{ mDepthOfField };
        settings.mMaxBlur = value;
        SetDepthOfFieldSettings( settings );
    }

    auto SceneCamera::ProcessMouseInput( const double ) -> void {
        MKT_BEGIN_PROFILER();

        const float2 mousePos{ mWindow->GetMouseX(), mWindow->GetMouseY() };
        const float2 delta{ mResetMouseSample ? core::float2{} : mousePos - mLastMousePosition };

        mResetMouseSample = false;
        mLastMousePosition = mousePos;

        // Keep an exact top/bottom snap until the user actually rotates.
        // A stationary mouse must not clamp its pitch back to 89 degrees.
        if ( delta.x == 0.0f && delta.y == 0.0f ) {
            return;
        }

        const float sensitivity{ mRotationFactor * mRotationSpeed };

        if ( mWantCameraRotationY ) {
            mTargetAngles.x += delta.x * sensitivity;
        }

        if ( mWantCameraRotationX ) {
            mTargetAngles.y -= delta.y * sensitivity;
        }

        mTargetAngles.x = std::remainder( mTargetAngles.x, glm::two_pi<float>() );
        mTargetAngles.y = glm::clamp( mTargetAngles.y, -glm::radians( 89.0f ), glm::radians( 89.0f ) );
    }

    auto SceneCamera::ProcessMouseScroll() -> void {
        MKT_BEGIN_PROFILER();
        const auto* input{ platform::InputSystem::GetPtr() };

        if ( !input ) {
            return;
        }

        // Wheel offsets are displacement events, so do not multiply them by frame time.
        // https://wiki.libsdl.org/SDL3/SDL_MouseWheelEvent
        const f32 distance{ input->GetMouseScrollDelta().second * mMovementSpeed * 0.1f };

        if ( distance == 0.0f ) {
            return;
        }

        // Reuse keyboard dolly targets and springs; the lens and orbit pivot stay fixed.
        if ( mNavigationMode == CameraNavigationMode::eOrbit ) {
            mTargetOrbitDistance = glm::clamp( mTargetOrbitDistance - distance, 0.01f, 1000000.0f );
        }
        else {
            mTargetPosition += mForward * distance;
        }
    }

    auto SceneCamera::ProcessKeyboardInput( const double timeStep ) -> void {
        MKT_BEGIN_PROFILER();

        const float speed{ mMovementSpeed * static_cast<float>( timeStep ) };

        if ( mNavigationMode == CameraNavigationMode::eOrbit ) {
            // Pan the pivot and eye together; dolly changes only radius. Mouse
            // rotation remains independent of both, even while inputs overlap.
            if ( mWindow->IsKeyPressed( platform::Key_W ) ) {
                mTargetOrbitDistance -= speed;
            }

            if ( mWindow->IsKeyPressed( platform::Key_S ) ) {
                mTargetOrbitDistance += speed;
            }

            mTargetOrbitDistance = glm::clamp( mTargetOrbitDistance, 0.01f, 1000000.0f );

            if ( mWindow->IsKeyPressed( platform::Key_A ) ) {
                mTargetOrbitTarget -= mRightVector * speed;
            }

            if ( mWindow->IsKeyPressed( platform::Key_D ) ) {
                mTargetOrbitTarget += mRightVector * speed;
            }

            if ( mWindow->IsKeyPressed( platform::Key_Space ) || mWindow->IsKeyPressed( platform::Key_E ) ) {
                mTargetOrbitTarget.y += speed;
            }

            if ( mWindow->IsKeyPressed( platform::Key_Q ) ) {
                mTargetOrbitTarget.y -= speed;
            }

            return;
        }

        if ( mWindow->IsKeyPressed( platform::Key_W ) ) {
            mTargetPosition += mForward * speed;
        }

        if ( mWindow->IsKeyPressed( platform::Key_S ) ) {
            mTargetPosition -= mForward * speed;
        }

        if ( mWindow->IsKeyPressed( platform::Key_A ) ) {
            mTargetPosition -= mRightVector * speed;
        }

        if ( mWindow->IsKeyPressed( platform::Key_D ) ) {
            mTargetPosition += mRightVector * speed;
        }

        if ( mWindow->IsKeyPressed( platform::Key_Space ) || mWindow->IsKeyPressed( platform::Key_E ) ) {
            mTargetPosition.y += speed;
        }

        if ( mWindow->IsKeyPressed( platform::Key_Q ) ) {
            mTargetPosition.y -= speed;
        }
    }

    auto SceneCamera::Interpolate( const double timeStep ) -> void {
        MKT_BEGIN_PROFILER();

        const float dt{ static_cast<float>( timeStep ) };
        mYaw = std::remainder( math::SmoothDampAngle( mYaw, mTargetAngles.x, mAngleVelocity.x, mSmoothTime, dt ), glm::two_pi<float>() );
        mPitch = math::SmoothDamp( mPitch, mTargetAngles.y, mAngleVelocity.y, mSmoothTime, dt );

        const float cosPitch{ glm::cos( mPitch ) };
        mForward = { glm::sin( mYaw ) * cosPitch, glm::sin( mPitch ), -glm::cos( mYaw ) * cosPitch };

        // Derive right from yaw: cross(forward, worldUp) vanishes at the poles.
        mRightVector = { glm::cos( mYaw ), 0.0f, glm::sin( mYaw ) };

        if ( mNavigationMode == CameraNavigationMode::eOrbit ) {
            mOrbitTarget = math::SmoothDamp( mOrbitTarget, mTargetOrbitTarget, mOrbitVelocity, mSmoothTime, dt );
            mOrbitDistance = math::SmoothDamp( mOrbitDistance, mTargetOrbitDistance, mOrbitDistanceVelocity, mSmoothTime, dt );

            // Derive the eye from the smoothed orientation and radius rather than
            // smoothing eye positions along a chord, which would drift off target.
            mPosition = mOrbitTarget - mForward * mOrbitDistance;
            mTargetPosition = mPosition;
        }
        else {
            mPosition = math::SmoothDamp( mPosition, mTargetPosition, mPositionVelocity, mSmoothTime, dt );
            mOrbitTarget = mTargetOrbitTarget = mPosition + mForward * mOrbitDistance;
        }
    }

    auto SceneCamera::TranslateView( const core::float3& delta ) -> void {
        mPosition += delta;
        mTargetPosition += delta;
        mOrbitTarget += delta;
        mTargetOrbitTarget += delta;

        UpdateViewMatrix();
    }

    auto SceneCamera::Update( core::f64 timeStep, bool allowKeyboardMovement ) -> void {
        MKT_BEGIN_PROFILER();
        UpdateProjection();

        // Adapt nvpro's snapshot/goal animation and Perlin smootherstep timing.
        // Quaternion interpolation also handles opposite axes without crossing a
        // zero look direction; orbiting preserves the pivot and camera distance.
        // https://github.com/nvpro-samples/nvpro_core/blob/master/nvh/cameramanipulator.cpp
        constexpr f32 duration{ 0.5f };
        const auto smootherStep{ []( f32 t ) { return t * t * t * ( t * ( t * 6.0f - 15.0f ) + 10.0f ); } };
        if ( mProjectionTransitionTime < duration ) {
            mProjectionTransitionTime = glm::min( duration, mProjectionTransitionTime + as<f32>( glm::max( timeStep, 0.0 ) ) );
            const auto t{ smootherStep( mProjectionTransitionTime / duration ) };
            mProjection = mProjectionTransitionFrom * ( 1.0f - t ) + mProjection * t;
        }
        if ( mViewTransitionTime < duration ) {
            const auto elapsed{ glm::min( duration, mViewTransitionTime + as<f32>( glm::max( timeStep, 0.0 ) ) ) };
            const auto t{ smootherStep( elapsed / duration ) };
            const auto a{ mViewTransitionFrom }, b{ mViewTransitionTo };
            const auto orientation{ glm::slerp( glm::quat{ a.w, a.x, a.y, a.z }, glm::quat{ b.w, b.x, b.y, b.z }, t ) };
            const float3 forward{ orientation * float3{ 0, 0, -1 } };
            const float3 eye{ mNavigationMode == CameraNavigationMode::eOrbit ? mViewTransitionPivot - forward * mOrbitDistance : mViewTransitionEye };
            SetViewPose( eye, forward );
            mRightVector = orientation * float3{ 1, 0, 0 };
            mViewTransitionTime = elapsed;
            UpdateViewMatrix();
            return;
        }

        if ( mWindow && mAllowCameraMovementAndRotation ) {
            ProcessMouseInput( timeStep );

            if ( allowKeyboardMovement ) {
                ProcessKeyboardInput( eastl::min( timeStep, 0.1 ) );
                ProcessMouseScroll();
            }
        }
        else {
            mResetMouseSample = true;
        }

        if ( !allowKeyboardMovement ) {
            mTargetPosition = mPosition;
            mPositionVelocity = {};
            mTargetOrbitTarget = mOrbitTarget;
            mOrbitVelocity = {};
            mTargetOrbitDistance = mOrbitDistance;
            mOrbitDistanceVelocity = 0.0f;
        }

        Interpolate( timeStep );

        UpdateViewMatrix();
    }

    auto SceneCamera::WantRotation( const bool xAxis, const bool yAxis ) -> void {
        MKT_BEGIN_PROFILER();
        mWantCameraRotationX = xAxis;
        mWantCameraRotationY = yAxis;
    }

    auto SceneCamera::UpdateViewMatrix() -> void {
        MKT_BEGIN_PROFILER();

        // Build orientation at the origin before adding translation. At large
        // coordinates, position + a short direction can round into a vertical
        // or zero vector and make LookAt's basis degenerate.
        const core::float3 viewUp{ glm::cross( mRightVector, mForward ) };
        mView = glm::translate( glm::lookAt( core::float3{ 0.0f }, mForward, viewUp ), -mPosition );
    }

}// namespace mikoto::scene
