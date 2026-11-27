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

#ifndef MIKOTO_RENDERER_CORE_LIGHT_HH
#define MIKOTO_RENDERER_CORE_LIGHT_HH

#include <cmath>

#include <EASTL/utility.h>

#include <glm/glm.hpp>
#include <glm/gtx/compatibility.hpp>

#include <Core/Core.hh>
#include <Core/Profiler.hh>
#include <Core/Types.hh>

namespace mikoto::renderer {

    enum class LightType {
        eDirectional,
        ePoint,
        eSpot,
        eAreaLight,
        eSkyLight
    };

    // Base class for shared light data (color)
    class LightObject {
    public:
        template<typename... Args>
        auto SetColor( Args &&...args ) -> void {
            mColor = core::float3( eastl::forward<Args>( args )... );
        }

        auto SetColor( const core::float3 &color ) -> void;
        auto SetIntensity( float intensity ) -> void;
        auto SetIsShadowCaster( bool value ) -> void;

        MKT_NODISCARD auto GetColor() const -> const core::float3 &;
        MKT_NODISCARD auto GetIntensity() const -> float;
        MKT_NODISCARD auto IsShadowCaster() const -> bool;

    protected:
        // Invalid and zero-length directions consistently fall back to world down.
        MKT_NODISCARD static auto NormalizeDirection( core::float3 direction ) -> core::float3;

    private:
        core::float3 mColor{ 1.0f, 1.0f, 1.0f };
        float mIntensity{ 1.0f };

        bool mIsShadowCaster{};
    };

    class PointLight : public LightObject {
    public:
        explicit PointLight() = default;
        ~PointLight() = default;

        auto SetRadius( float radius ) -> void;
        auto SetSourceRadius( float radius ) -> void;
        MKT_NODISCARD auto GetRadius() const -> float;
        MKT_NODISCARD auto GetSourceRadius() const -> float;

    private:
        float mRadius{ 1.0f };      // falloff radius
        float mSourceRadius{ 0.1f };// Path-traced emitting sphere radius, in world units.
    };

    class DirectionalLight : public LightObject {
    public:
        explicit DirectionalLight() = default;
        ~DirectionalLight() = default;

        template<typename... Args>
        auto SetDirection( Args &&...args ) -> void {
            SetNormalizedDirection( core::float3( eastl::forward<Args>( args )... ) );
        }
        auto SetAngularRadius( float degrees ) -> void;

        MKT_NODISCARD auto GetDirection() const -> const core::float3 &;
        MKT_NODISCARD auto GetAngularRadius() const -> float;

        /** @brief Returns a normalized direction after applying the entity's world transform.
         * @param world Entity-to-world transform, including its parents; translation is ignored.
         * @return Unit world direction, or world down if the transformed direction is invalid.
         */
        MKT_NODISCARD auto GetWorldDirection( const core::float4x4 &world ) const -> core::float3;

    private:
        auto SetNormalizedDirection( core::float3 direction ) -> void;

        core::float3 mDirection{ -1.0f, -1.0f, -1.0f };// default pointing diagonally down
        float mAngularRadius{ 0.266f };                // Path-traced solar half-angle; zero retains an ideal distant light.
    };

    class SpotLight : public LightObject {
    public:
        explicit SpotLight();

        template<typename... Args>
        auto SetDirection( Args &&...args ) -> void {
            SetNormalizedDirection( core::float3( eastl::forward<Args>( args )... ) );
        }

        auto SetAngle( float angleDeg ) -> void;

        auto SetSoftness( float softness ) -> void;

        auto SetRadius( float radius ) -> void;

        MKT_NODISCARD static auto GetMaxSoftness() -> float;
        MKT_NODISCARD static auto GetMaxAngle() -> float;// Outer half-angle in degrees

        MKT_NODISCARD auto GetDirection() const -> const core::float3 &;

        /** @brief Returns a normalized direction after applying the entity's world transform.
         * @param world Entity-to-world transform, including its parents; translation is ignored.
         * @return Unit world direction, or world down if the transformed direction is invalid.
         */
        MKT_NODISCARD auto GetWorldDirection( const core::float4x4 &world ) const -> core::float3;

        MKT_NODISCARD auto GetAngle() const -> float;
        MKT_NODISCARD auto GetSoftness() const -> float;

        MKT_NODISCARD auto GetCutOff() const -> float;
        MKT_NODISCARD auto GetOuterCutOff() const -> float;

        MKT_NODISCARD auto GetRadius() const -> float;

    private:
        auto UpdateCutoffs() -> void;

    private:
        auto SetNormalizedDirection( core::float3 direction ) -> void;

        core::float3 mDirection{ 0.0f, -1.0f, 0.0f };// Pointing downwards by default

        core::f32 mAngle{ 30.0f };// degrees
        core::f32 mSoftness{ 1.0f };

        core::f32 mCutOff{ 0.0f };
        core::f32 mOuterCutOff{ 0.0f };

        core::f32 mRadius{ 10.0f };
    };

    enum class AreaLightShape : core::u8 {
        eRectangle,
        eMesh,
        eSphere,
        eTube
    };

    // Analytic shapes support raster lighting. Rectangles and mesh hierarchies also
    // support path tracing; mesh emitters retain their authored emissive textures.
    class AreaLight : public LightObject {
    public:
        explicit AreaLight() = default;

        auto SetRadius( float radius ) -> void;
        auto SetRange( float range ) -> void;
        auto SetTwoSided( bool twoSided ) -> void;
        auto SetSize( const core::float2 &size ) -> void;
        auto SetShape( AreaLightShape shape ) -> void;
        /** Enables material/video texturing for rectangular raster emission. */
        auto SetMaterialEmissionEnabled( bool enabled ) -> void;
        /** Selects an entity's emission material. Zero uses the light entity itself. */
        auto SetEmissionSourceEntityID( core::u64 entityID ) -> void;

        MKT_NODISCARD auto GetRadius() const -> float;
        MKT_NODISCARD auto GetRange() const -> float;
        MKT_NODISCARD auto IsTwoSided() const -> bool;
        MKT_NODISCARD auto GetSize() const -> const core::float2 &;
        MKT_NODISCARD auto GetShape() const -> AreaLightShape;
        MKT_NODISCARD auto IsMaterialEmissionEnabled() const -> bool;
        MKT_NODISCARD auto GetEmissionSourceEntityID() const -> core::u64;

        ~AreaLight() = default;

    private:
        core::float2 mSize{ 1.0f, 1.0f };
        float mRadius{ 0.5f };
        float mRange{ 10.0f };// Raster influence distance from the emitting shape, in metres.

        bool mIsTwoSided{ false };
        bool mMaterialEmissionEnabled{ true };
        core::u64 mEmissionSourceEntityID{};
        AreaLightShape mShape{ AreaLightShape::eRectangle };
    };
}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_CORE_LIGHT_HH
