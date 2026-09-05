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

#ifndef MIKOTO_CAMERA_HH
#define MIKOTO_CAMERA_HH

#include <utility>

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Math/Random.hh>
#include <Math/Math.hh>

namespace mikoto::scene {

    enum class ProjectionType {
        eOrthographic,
        ePerspective,
    };

    class Camera {
    public:

        Camera(const Camera& other) = default;
        Camera(Camera&& other) = default;

        auto operator=(const Camera& other) -> Camera& = default;
        auto operator=(Camera&& other) -> Camera& = default;

        auto SetViwMatrix(const glm::mat4& matrix) -> void { mView = matrix; }

        MKT_NODISCARD auto GetProjection() const -> const glm::mat4& { return mProjection; }
        MKT_NODISCARD auto GetViewMatrix() const -> const glm::mat4& { return mView; }

        MKT_NODISCARD auto GetTransform() const -> const glm::mat4& { return mTransform; }
        MKT_NODISCARD auto GetTransform() -> glm::mat4& { return mTransform; }
        auto SetTransform(const glm::mat4& transform) -> void { mTransform = transform; }

        MKT_NODISCARD auto GetTargetForward() const -> const glm::vec3& { return mForward; }
        MKT_NODISCARD auto GetFOV() const -> float { return mFov; }
        MKT_NODISCARD auto GetNearPlane() const -> float { return mNearPlane; }
        MKT_NODISCARD auto GetFarPlane() const -> float { return mFarPlane; }
        MKT_NODISCARD auto GetAspectRatio() const -> float { return mViewportWidth / mViewportHeight; }
        MKT_NODISCARD auto GetViewPort() const -> decltype(auto) { return std::make_pair(mViewportWidth, mViewportHeight); }


        MKT_NODISCARD auto GetViewProjection() const -> glm::mat4 { return GetProjection() * mView; }

        MKT_NODISCARD auto GetPosition() const -> const glm::vec3& { return mPosition; }
        MKT_NODISCARD auto GetRotation() const -> const glm::vec3& { return mRotation; }

        auto SetFieldOfView( float value ) -> void { mFov = value; }

        auto SetFarPlane( float value ) -> void { mFarPlane = value; }
        auto SetNearPlane( float value ) -> void { mNearPlane = value; }

        auto SetPosition(const core::float3& position) -> void {
            mTranslation = position;
            mTransform = glm::translate(mTransform, mTranslation);
        }

        auto SetRotation(const core::float3& angles = glm::vec3(0.0f)) -> void {
            mRotation = angles;

            // ADL, namespace skipped
            mTransform = rotate(mTransform, glm::radians( mRotation[0] ), core::float3{ 1.0f, 0.0f, 0.0f });
            mTransform =  rotate(mTransform, glm::radians( mRotation[1] ), core::float3{ .0f, 1.0f, 0.0f } );
            mTransform =  rotate(mTransform, glm::radians( mRotation[2] ), core::float3{ 0.0f, 0.0f, 1.0f });
        }

        auto SetViewportSize( const core::f32 width, const core::f32 height ) -> void {
            if ( mViewportWidth == width && mViewportHeight == height ) {
                return;
            }

            mViewportWidth = width;
            mViewportHeight = height;
        }

        MKT_NODISCARD auto GetProjectionType() const -> ProjectionType { return mProjectionType; }
        MKT_NODISCARD auto IsOrthographic() const -> bool { return mProjectionType == ProjectionType::eOrthographic; }

        auto SetProjectionType( const ProjectionType type ) -> void {
            mProjectionType = type;
        }

        ~Camera() = default;

    protected:
        explicit Camera(const glm::mat4& projection = glm::mat4(1.0f), const glm::mat4& transform = glm::mat4(1.0f), ProjectionType projectionType = ProjectionType::ePerspective )
            :   mProjection{ projection }, mTransform{ transform }, mProjectionType{ projectionType }
        {
            UpdateProjection();
        }

        auto UpdateProjection() -> void {
            mAspectRatio = mViewportWidth / mViewportHeight;

            switch(mProjectionType) {
                case ProjectionType::eOrthographic:
                    mProjection = glm::ortho(0.0f, mViewportWidth, 0.0f, mViewportHeight);
                break;
                case ProjectionType::ePerspective:
                    mProjection = glm::perspective(glm::radians(mFov), mAspectRatio, mNearPlane, mFarPlane);
                break;
            }
        }

    protected:
        float mViewportWidth{ 1920 };
        float mViewportHeight{ 1080 };

        // [Projection Data]
        float mNearPlane{ 0.1f };
        float mFarPlane{ 1000.0f };
        float mFov{ 45.0f };
        float mAspectRatio{ mViewportWidth / mViewportHeight };

        // [Matrices]
        core::float4x4 mView{};
        core::float4x4 mProjection{};
        core::float4x4 mTransform{};

        // [Vectors]
        core::float3 mPosition{ -15.0f, 5.0f, 30.0f };
        core::float3 mRightVector{ 1.0f, 0.0f, 0.0f };
        core::float3 mUp{ core::float3{ .0f, 1.0f, 0.0f } };
        core::float3 mForward{ 15.0f, -5.0f, -30.0f };
        core::float3 mTranslation{};
        core::float3 mRotation{};

        // [Rotations]
        float mYaw{ 0.0f };
        float mRoll{ 0.0f };
        float mPitch{ 0.0f };

        ProjectionType mProjectionType{ ProjectionType::ePerspective };
    };
}


#endif // MIKOTO_CAMERA_HH
