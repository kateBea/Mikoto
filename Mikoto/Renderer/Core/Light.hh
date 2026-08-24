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

#ifndef MIKOTO_LIGHT_HH
#define MIKOTO_LIGHT_HH

#include <glm/glm.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;

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
        auto SetColor( Args&&... args ) -> void {
            mColor = core::float3( std::forward<Args>( args )... );
        }

        auto SetColor( const core::float3& color ) -> void { mColor = color; }
        MKT_NODISCARD auto GetColor() const -> const core::float3& { return mColor; }

        auto SetIntensity( float intensity ) -> void { mIntensity = intensity; }
        MKT_NODISCARD auto GetIntensity() const -> float { return mIntensity; }

        auto SetIsShadowCaster( bool value ) -> void {
            mIsShadowCaster = value;
        }

        MKT_NODISCARD auto IsShadowCaster() const -> bool { return mIsShadowCaster; }

    protected:
        core::float3 mColor{ 1.0f, 1.0f, 1.0f };
        float mIntensity{ 1.0f };

        bool mIsShadowCaster{};
    };

    class PointLight : public LightObject {
    public:
        explicit PointLight() = default;
        ~PointLight() = default;

        auto SetRadius( float radius ) -> void { mRadius = radius; }
        MKT_NODISCARD auto GetRadius() const -> float { return mRadius; }

    private:
        float mRadius{ 1.0f };// falloff radius
    };

    class DirectionalLight : public LightObject {
    public:
        explicit DirectionalLight() = default;
        ~DirectionalLight() = default;

        template<typename... Args>
        auto SetDirection( Args&&... args ) -> void {
            mDirection = core::float3( std::forward<Args>( args )... );
        }

        MKT_NODISCARD auto GetDirection() const -> const core::float3& { return mDirection; }

    private:
        core::float3 mDirection{ -1.0f, -1.0f, -1.0f };// default pointing diagonally down
    };

    class SpotLight : public LightObject {
    public:
        explicit SpotLight() { UpdateCutoffs(); }

        template<typename... Args>
        auto SetDirection( Args&&... args ) -> void {
            mDirection = glm::normalize( core::float3( std::forward<Args>( args )... ) );
        }

        auto SetAngle( float angleDeg ) -> void {
            mAngle = angleDeg;
            UpdateCutoffs();
        }

        auto SetSoftness( float softness ) -> void {
            mSoftness = glm::clamp( softness, 0.0f, GetMaxSoftness() );
            UpdateCutoffs();
        }

        auto SetRadius( float radius ) -> void { mRadius = radius; }

        MKT_NODISCARD static auto GetMaxSoftness() -> float { return 30.0f; }
        MKT_NODISCARD static auto GetMaxAngle() -> float { return 180.0f; } // In degrees

        MKT_NODISCARD auto GetDirection() const -> const core::float3& { return mDirection; }

        MKT_NODISCARD auto GetAngle() const -> float { return mAngle; }
        MKT_NODISCARD auto GetSoftness() const -> float { return mSoftness; }

        MKT_NODISCARD auto GetCutOff() const -> float { return mCutOff; }
        MKT_NODISCARD auto GetOuterCutOff() const -> float { return mOuterCutOff; }

        MKT_NODISCARD auto GetRadius() const -> float { return mRadius; }

    private:
        auto UpdateCutoffs() -> void {
            const float outer{ glm::radians( mAngle ) };
            const float inner{ outer * ( GetMaxSoftness() - mSoftness ) };

            mCutOff = glm::cos( inner );
            mOuterCutOff = glm::cos( outer );
        }

    private:
        core::float3 mDirection{ 0.0f, -1.0f, 0.0f }; // Pointing downwards by default

        core::f32 mAngle{ 30.0f };  // degrees
        core::f32 mSoftness{ 1.0f };

        core::f32 mCutOff{ 0.0f };
        core::f32 mOuterCutOff{ 0.0f };

        core::f32 mRadius{ 10.0f };
    };
}// namespace Mikoto


#endif//MIKOTO_LIGHT_HH
