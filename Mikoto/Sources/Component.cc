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

#include <Scene/Component.hh>

namespace mikoto::scene {

    using namespace mikoto::core;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;

    auto HighlightComponent::SetHighlighted( bool value ) -> void {
        mIsHighlighted = value;
    }

    auto HighlightComponent::IsHighlighted() const -> bool {
        return mIsHighlighted;
    }

    TransformComponent::TransformComponent() {
        ComputeTransform( mTranslation, mScale, mRotation );
        mWorldTransform = mTransform;
    }

    TransformComponent::TransformComponent( const core::float3 &position, const core::float3 &size, const core::float3 &angles ) {
        ComputeTransform( position, size, angles );
    }

    auto TransformComponent::GetTranslation() const -> const core::float3& {
        return mTranslation;
    }

    auto TransformComponent::GetRotation() const -> const core::float3& {
        return mRotation;
    }

    auto TransformComponent::GetScale() const -> const core::float3& {
        return mScale;
    }

    auto TransformComponent::GetTransform() const -> const float4x4& {
        return mTransform;
    }

    auto TransformComponent::HasUniformScale() const -> bool {
        return mHasUniformScale;
    }

    auto TransformComponent::GetRotationQuat() const -> quat {
        return quat( mRotation );
    }

    auto TransformComponent::GetWorldTransform() const -> const float4x4 & {
        return mWorldTransform;
    }

    auto TransformComponent::SetWorldTransform( const float4x4 &worldTransform ) -> void {
        mWorldTransform = worldTransform;
    }

    auto TransformComponent::ComputeTransform( const core::float3 &position, const core::float3 &size, const core::float3 &angles ) -> void {
        mTranslation = position;
        mRotation = angles;
        mScale = size;

        // NOTE:
        // Euler rotations composed via successive glm::rotate calls can produce incorrect
        // behavior because rotations are applied in world space, causing axis misalignment.
        // Use quaternion-based rotation (or consistent multiplication order) to ensure
        // rotations happen in local space around the object's pivot.
        // Pivot (center of rotation)

        // NOTE:
        // Rotations must be applied in local space (object axes), not world axes.
        // Chaining glm::rotate on an accumulating matrix can cause rotations to be
        // applied in world space, leading to incorrect behavior.
        // Use quaternions (glm::quat) or proper multiplication order to ensure
        // rotations follow the object's local axes.

        // TODO: See comment in RecomputeTransform, use RecomputeTransform that takes pivot
        mTransform = math::RecomputeTransform( position, size, angles );
    }

    auto CameraComponent::SetClearFlags( CameraClearFlags bg ) -> void {
        mClearFlags = bg;
    }

    auto CameraComponent::GetClearFlags() const -> CameraClearFlags {
        return mClearFlags;
    }

    auto CameraComponent::SetClearColor( const renderer::rhi::Color& color ) -> void {
        mColor = color;
    }

    auto CameraComponent::GetClearColor() const -> const renderer::rhi::Color & {
        return mColor;
    }

    auto CameraComponent::IsMainCamera() const -> bool {
        return mMainCam;
    }

    CameraComponent::CameraComponent( platform::Window* window ) {
        SceneCameraDescription cameraDescription{
            .mFov = 45.0,
            .mNearPlane = 0.1f,
            .mFarPlane = 3000.0f,
            .mWindow = window };

        if (window) {
            cameraDescription.mAspectRatio = as<f32>( window->GetWidth() ) / as<f32>( window->GetHeight() );
        }

        mCamera = Ref<SceneCamera>::New( cameraDescription );
    }

    auto CameraComponent::HasCamera() const -> bool {
        return mCamera != nullptr;
    }

    auto CameraComponent::GetCamera() -> SceneCamera& {
        return *mCamera;
    }

    auto CameraComponent::GetCamera() const -> const SceneCamera& {
        return *mCamera;
    }

    auto CameraComponent::IsAspectRatioFixed() const -> bool {
        return mFixedAspectRatio;
    }

    auto CameraComponent::SetGamma( float gamma ) -> void {
        mGamma = gamma;
    }

    auto CameraComponent::SetExposure( float exposure ) -> void {
        mExposure = exposure;
    }

    auto CameraComponent::GetGamma() const -> float {
        return mGamma;
    }

    auto CameraComponent::GetExposure() const -> float {
        return mExposure;
    }

    auto CameraComponent::SetFixedAspectRatio( const bool value ) -> void {
        mFixedAspectRatio = value;
    }


    auto TransformComponent::SetTransform( const glm::mat4 &transform ) -> void {
        mTransform = transform;
        math::Decompose( mTransform, mTranslation, mRotation, mScale );
    }

    auto TransformComponent::SetTranslation( const float3 &value ) -> void {
        mTranslation = value;
        mTransform = math::RecomputeTransform( mTranslation, mScale, mRotation );
    }

    auto TransformComponent::SetRotation( const float3 &value ) -> void {
        mRotation = value;
        mTransform = math::RecomputeTransform( mTranslation, mScale, mRotation );
    }

    auto TransformComponent::SetRotation( const glm::quat &quaternion ) -> void {
        mRotation = glm::eulerAngles( quaternion );
        mTransform = math::RecomputeTransform( mTranslation, mScale, mRotation );
    }

    auto TransformComponent::SetScale( const float3 &value ) -> void {
        if ( !mHasUniformScale ) {
            mScale = value;
        } else {
            float offSet{ 0 };

            if ( value.x != mScale.x ) {
                offSet = value.x - mScale.x;
            } else if ( value.y != mScale.y ) {
                offSet = value.y - mScale.y;
            } else if ( value.z != mScale.z ) {
                offSet = value.z - mScale.z;
            }

            if ( offSet != 0 ) {
                mScale.x += offSet;
                mScale.y += offSet;
                mScale.z += offSet;
            }
        }

        mTransform = math::RecomputeTransform( mTranslation, mScale, mRotation );
    }

    auto TransformComponent::SetUniformSale( const bool value ) -> void {
        mHasUniformScale = value;
    }
}// namespace mikoto::scene