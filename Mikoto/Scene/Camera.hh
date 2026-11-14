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

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Logging/Assert.hh>

#include <Math/Math.hh>
#include <Math/Random.hh>

namespace mikoto::scene {
    /// Fly rotates at the eye; orbit rotates the eye around a retained look-at point.
    enum class CameraNavigationMode : core::u8 {
        eFly,
        eOrbit
    };

    enum class ProjectionType {
        eOrthographic,
        ePerspective,
    };

    struct DepthOfFieldSettings {
        bool mEnabled{ false };
        core::f32 mAperture{ 2.8f };
        core::f32 mFocalLength{ 50.0f };  // Millimeters.
        core::f32 mFocusDistance{ 10.0f };// World units (meters).
        core::f32 mMaxBlur{ 12.0f };      // Maximum blur radius in pixels.
        core::f32 mMaxAperture{ 1.4f };   // Minimum f-stop (widest opening).
        core::f32 mSensorWidth{ 36.0f };  // Millimeters; height follows viewport aspect.
        bool mUsePhysicalLens{};        // Opt-in preserves existing scene framing.
        core::u32 mBladeCount{ 7 };
        core::f32 mBladeCurvature{ 0.8f };// 0: polygon, 1: circular aperture.
        core::f32 mBladeRotation{};      // Degrees.
    };

    class Camera {
    public:
        Camera( const Camera& other ) = default;
        Camera( Camera&& other ) = default;

        auto operator=( const Camera& other ) -> Camera& = default;
        auto operator=( Camera&& other ) -> Camera& = default;

        MKT_NODISCARD auto GetProjection() const -> const core::float4x4&;
        MKT_NODISCARD auto GetViewMatrix() const -> const core::float4x4&;

        MKT_NODISCARD auto GetTransform() const -> const core::float4x4&;
        MKT_NODISCARD auto GetTransform() -> core::float4x4&;

        MKT_NODISCARD auto GetTargetForward() const -> const core::float3&;

        MKT_NODISCARD auto GetFOV() const -> float;
        MKT_NODISCARD auto GetNearPlane() const -> float;
        MKT_NODISCARD auto GetFarPlane() const -> float;
        MKT_NODISCARD auto GetAspectRatio() const -> float;

        MKT_NODISCARD auto GetViewPort() const -> std::pair<core::f32, core::f32>;

        MKT_NODISCARD auto GetViewProjection() const -> core::float4x4;

        MKT_NODISCARD auto GetPosition() const -> const core::float3&;
        MKT_NODISCARD auto GetRotation() const -> const core::float3&;

        MKT_NODISCARD auto GetProjectionType() const -> ProjectionType;
        MKT_NODISCARD auto IsOrthographic() const -> bool;

        MKT_NODISCARD auto GetOrthographicHeight() const -> core::f32;
        MKT_NODISCARD auto GetDepthOfFieldSettings() const -> const DepthOfFieldSettings&;

        auto SetFieldOfView( core::f32 value ) -> void;

        auto SetFarPlane( core::f32 value ) -> void;
        auto SetNearPlane( core::f32 value ) -> void;
        auto SetClipPlanes( core::f32 nearPlane, core::f32 farPlane ) -> void;
        auto SetViewportSize( core::f32 width, core::f32 height ) -> void;

        auto SetPosition( const core::float3& position ) -> void;
        auto SetTransform( const core::float4x4& transform ) -> void;

        auto SetRotation( const core::float3& angles ) -> void;

        auto SetViewMatrix( const core::float4x4& matrix ) -> void;

        auto SetProjectionType( ProjectionType type ) -> void;

        auto SetOrthographicHeight( core::f32 height ) -> void;

        /** @brief Extracts a rigid camera pose from an entity world matrix, excluding scale and shear.
         * @param world Entity world transform. @return Orthonormal pose retaining translation and forward.
         */
        static auto ExtractRenderPose( const core::float4x4& world ) -> core::float4x4;

        auto SetRenderPose( const core::float4x4& pose ) -> void;

        virtual ~Camera() = default;

    protected:
        explicit Camera( const core::float4x4& projection = core::float4x4( 1.0f ),
            const core::float4x4& transform = core::float4x4( 1.0f ),
            ProjectionType projectionType = ProjectionType::ePerspective );

        auto UpdateProjection() -> void;

    protected:
        core::f32 mViewportWidth{ 1920 };
        core::f32 mViewportHeight{ 1080 };

        // [Projection Data]
        DepthOfFieldSettings mDepthOfField{};

        core::f32 mFov{ 45.0f };
        core::f32 mNearPlane{ 0.1f };
        core::f32 mFarPlane{ 1000.0f };
        core::f32 mOrthographicHeight{ 20.0f };
        core::f32 mAspectRatio{ mViewportWidth / mViewportHeight };

        // [Matrices]
        core::float4x4 mView{};
        core::float4x4 mTransform{};
        core::float4x4 mProjection{};

        // [Vectors]
        core::float3 mPosition{ -15.0f, 5.0f, 30.0f };
        core::float3 mRightVector{ 1.0f, 0.0f, 0.0f };
        core::float3 mUp{ .0f, 1.0f, 0.0f};
        core::float3 mForward{ 15.0f, -5.0f, -30.0f };
        core::float3 mTranslation{};
        core::float3 mRotation{};

        // [Rotations]
        core::f32 mYaw{ 0.0f };
        core::f32 mRoll{ 0.0f };
        core::f32 mPitch{ 0.0f };

        ProjectionType mProjectionType{ ProjectionType::ePerspective };
    };
}// namespace mikoto::scene

#endif// MIKOTO_SCENE_CAMERA_HH
