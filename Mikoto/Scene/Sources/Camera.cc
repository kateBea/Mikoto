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

#include <glm/glm.hpp>
#include <glm/gtx/compatibility.hpp>

#include <Core/Profiler.hh>

#include <Scene/Camera.hh>

namespace mikoto::scene {

    auto Camera::SetClipPlanes( core::f32 nearPlane, core::f32 farPlane ) -> void {
        MKT_BEGIN_PROFILER();

        if ( nearPlane <= 0 || farPlane <= nearPlane ) {
            return;
        }

        mNearPlane = nearPlane;
        mFarPlane = farPlane;

        UpdateProjection();
    }

    auto Camera::ExtractRenderPose( const core::float4x4& world ) -> core::float4x4 {
        core::float3 backward{ world[2] };

        if ( glm::dot( backward, backward ) < 1e-12f ) {
            backward = { 0, 0, 1 };
        }

        backward = glm::normalize( backward );
        core::float3 up{ world[1] };
        core::float3 right{ glm::cross( up, backward ) };

        if ( glm::dot( right, right ) < 1e-12f ) {
            up = glm::abs( backward.y ) < 0.99f ? core::float3{ 0, 1, 0 } : core::float3{ 1, 0, 0 };
            right = glm::cross( up, backward );
        }

        right = glm::normalize( right );
        up = glm::cross( backward, right );
        return { core::float4{ right, 0 }, core::float4{ up, 0 }, core::float4{ backward, 0 }, core::float4{ core::float3{ world[3] }, 1 } };
    }

    auto Camera::SetRenderPose( const core::float4x4& pose ) -> void {
        MKT_BEGIN_PROFILER();
        mTransform = pose;
        mPosition = core::float3{ pose[3] };
        mForward = -core::float3{ pose[2] };
        mView = glm::inverse( pose );

        UpdateProjection();
    }

    auto Camera::IsOrthographic() const -> bool {
        MKT_BEGIN_PROFILER();
        return mProjectionType == ProjectionType::eOrthographic;
    }

    auto Camera::GetOrthographicHeight() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mOrthographicHeight;
    }

    auto Camera::SetOrthographicHeight( core::f32 height ) -> void {
        MKT_BEGIN_PROFILER();

        if ( height > 0.001f ) {
            mOrthographicHeight = height;
            UpdateProjection();
        }
    }

    auto Camera::SetRotation( const core::float3& angles ) -> void {
        MKT_BEGIN_PROFILER();
        mRotation = angles;

        // ADL, namespace skipped
        mTransform = rotate( mTransform, glm::radians( mRotation[0] ), core::float3{ 1.0f, 0.0f, 0.0f } );
        mTransform = rotate( mTransform, glm::radians( mRotation[1] ), core::float3{ .0f, 1.0f, 0.0f } );
        mTransform = rotate( mTransform, glm::radians( mRotation[2] ), core::float3{ 0.0f, 0.0f, 1.0f } );
    }

    auto Camera::SetViewMatrix( const core::float4x4& matrix ) -> void {
        MKT_BEGIN_PROFILER();
        mView = matrix;
    }

    auto Camera::GetProjection() const -> const core::float4x4& {
        MKT_BEGIN_PROFILER();
        return mProjection;
    }

    auto Camera::GetViewMatrix() const -> const core::float4x4& {
        MKT_BEGIN_PROFILER();
        return mView;
    }

    auto Camera::GetTransform() const -> const core::float4x4& {
        MKT_BEGIN_PROFILER();
        return mTransform;
    }

    auto Camera::GetTransform() -> core::float4x4& {
        MKT_BEGIN_PROFILER();
        return mTransform;
    }

    auto Camera::SetTransform( const core::float4x4& transform ) -> void {
        MKT_BEGIN_PROFILER();
        mTransform = transform;
    }

    auto Camera::GetTargetForward() const -> const core::float3& {
        MKT_BEGIN_PROFILER();
        return mForward;
    }

    auto Camera::GetFOV() const -> float {
        MKT_BEGIN_PROFILER();
        return mFov;
    }

    auto Camera::GetNearPlane() const -> float {
        MKT_BEGIN_PROFILER();
        return mNearPlane;
    }

    auto Camera::GetFarPlane() const -> float {
        MKT_BEGIN_PROFILER();
        return mFarPlane;
    }

    auto Camera::GetAspectRatio() const -> float {
        MKT_BEGIN_PROFILER();
        return mAspectRatio;
    }

    auto Camera::GetViewPort() const -> std::pair<core::f32, core::f32> {
        MKT_BEGIN_PROFILER();
        return { mViewportWidth, mViewportHeight };
    }

    auto Camera::GetViewProjection() const -> core::float4x4 {
        MKT_BEGIN_PROFILER();
        return GetProjection() * mView;
    }

    auto Camera::GetPosition() const -> const core::float3& {
        MKT_BEGIN_PROFILER();
        return mPosition;
    }

    auto Camera::GetRotation() const -> const core::float3& {
        MKT_BEGIN_PROFILER();
        return mRotation;
    }

    auto Camera::GetProjectionType() const -> ProjectionType {
        MKT_BEGIN_PROFILER();
        return mProjectionType;
    }

    auto Camera::GetDepthOfFieldSettings() const -> const DepthOfFieldSettings& {
        MKT_BEGIN_PROFILER();
        return mDepthOfField;
    }

    Camera::Camera( const core::float4x4& projection, const core::float4x4& transform, ProjectionType projectionType )
        : mProjection{ projection },
          mTransform{ transform },
          mProjectionType{ projectionType } {
        MKT_BEGIN_PROFILER();
        UpdateProjection();
    }

    auto Camera::UpdateProjection() -> void {
        MKT_BEGIN_PROFILER();

        switch ( mProjectionType ) {
            case ProjectionType::eOrthographic:
                // A scene camera frames world units around its view axis.
                // The four-argument pixel-space ortho clipped everything
                // outside [-1,1] depth and placed the origin at a corner.
                mProjection = glm::orthoRH_ZO( -mOrthographicHeight * mAspectRatio * 0.5f,
                    mOrthographicHeight * mAspectRatio * 0.5f, -mOrthographicHeight * 0.5f,
                    mOrthographicHeight * 0.5f, mNearPlane, mFarPlane );
                break;
            case ProjectionType::ePerspective:
                if ( mDepthOfField.mUsePhysicalLens ) {
                    const core::f32 sensorHeight{ mDepthOfField.mSensorWidth / mAspectRatio };
                    mFov = glm::degrees( 2.0f * glm::atan( sensorHeight / ( 2.0f * mDepthOfField.mFocalLength ) ) );
                }

                mProjection = glm::perspectiveRH_ZO( glm::radians( mFov ), mAspectRatio, mNearPlane, mFarPlane );
                break;
        }
    }

    auto Camera::SetFieldOfView( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( value <= 0 || value >= 179 ) {
            return;
        }

        mFov = value;

        if ( mDepthOfField.mUsePhysicalLens ) {
            const core::f32 sensorHeight{ mDepthOfField.mSensorWidth / mAspectRatio };
            mDepthOfField.mFocalLength = glm::clamp( sensorHeight / ( 2.0f * glm::tan( glm::radians( value ) * 0.5f ) ), 1.0f, 300.0f );
            mDepthOfField.mFocusDistance = glm::max( mDepthOfField.mFocusDistance, mDepthOfField.mFocalLength * 0.001f + 0.001f );
        }

        UpdateProjection();
    }

    auto Camera::SetFarPlane( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( value <= mNearPlane ) {
            return;
        }

        mFarPlane = value;

        UpdateProjection();
    }

    auto Camera::SetNearPlane( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( value <= 0 || value >= mFarPlane ) {
            return;
        }

        mNearPlane = value;

        UpdateProjection();
    }

    auto Camera::SetViewportSize( const core::f32 width, const core::f32 height ) -> void {
        MKT_BEGIN_PROFILER();

        if ( width <= 0 || height <= 0 ) {
            return;
        }

        const core::f32 aspect{ width / height };

        if ( aspect <= 0.0f ) {
            return;
        }

        if ( mViewportWidth == width && mViewportHeight == height && mAspectRatio == aspect ) {
            return;
        }

        mViewportWidth = width;
        mViewportHeight = height;
        mAspectRatio = aspect;

        UpdateProjection();
    }

    auto Camera::SetPosition( const core::float3& position ) -> void {
        MKT_BEGIN_PROFILER();
        mTranslation = position;
        mTransform = glm::translate( mTransform, mTranslation );
    }

    auto Camera::SetProjectionType( const ProjectionType type ) -> void {
        MKT_BEGIN_PROFILER();
        mProjectionType = type;

        UpdateProjection();
    }
}// namespace mikoto::scene
