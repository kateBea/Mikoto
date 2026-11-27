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

#include <EASTL/algorithm.h>

#include <Renderer/Core/Light.hh>

namespace mikoto::renderer {
    auto LightObject::SetColor( const core::float3 &color ) -> void {
        MKT_BEGIN_PROFILER();
        mColor = color;
    }


    auto LightObject::GetColor() const -> const core::float3 & {
        MKT_BEGIN_PROFILER();
        return mColor;
    }


    auto LightObject::SetIntensity( float intensity ) -> void {
        MKT_BEGIN_PROFILER();
        mIntensity = eastl::max( intensity, 0.0f );
    }


    auto LightObject::GetIntensity() const -> float {
        MKT_BEGIN_PROFILER();
        return mIntensity;
    }


    auto LightObject::SetIsShadowCaster( bool value ) -> void {
        MKT_BEGIN_PROFILER();
        mIsShadowCaster = value;
    }


    auto LightObject::IsShadowCaster() const -> bool {
        MKT_BEGIN_PROFILER();
        return mIsShadowCaster;
    }


    auto PointLight::SetRadius( float radius ) -> void {
        MKT_BEGIN_PROFILER();
        mRadius = eastl::max( radius, .02f );
    }


    auto PointLight::GetRadius() const -> float {
        MKT_BEGIN_PROFILER();
        return mRadius;
    }

    auto PointLight::SetSourceRadius( float radius ) -> void {
        mSourceRadius = glm::max( radius, 0.0f );
    }

    auto PointLight::GetSourceRadius() const -> float {
        return mSourceRadius;
    }

    auto DirectionalLight::SetAngularRadius( float degrees ) -> void {
        mAngularRadius = glm::clamp( degrees, 0.0f, 10.0f );
    }

    auto DirectionalLight::GetAngularRadius() const -> float {
        return mAngularRadius;
    }


    auto DirectionalLight::GetDirection() const -> const core::float3 & {
        MKT_BEGIN_PROFILER();
        return mDirection;
    }


    SpotLight::SpotLight() {
        MKT_BEGIN_PROFILER();
        UpdateCutoffs();
    }


    auto SpotLight::SetAngle( float angleDeg ) -> void {
        MKT_BEGIN_PROFILER();
        mAngle = glm::clamp( angleDeg, 0.1f, GetMaxAngle() );
        UpdateCutoffs();
    }


    auto SpotLight::SetSoftness( float softness ) -> void {
        MKT_BEGIN_PROFILER();
        mSoftness = glm::clamp( softness, 0.0f, GetMaxSoftness() );
        UpdateCutoffs();
    }


    auto SpotLight::SetRadius( float radius ) -> void {
        MKT_BEGIN_PROFILER();
        mRadius = eastl::max( radius, .02f );
    }


    auto SpotLight::GetMaxSoftness() -> float {
        MKT_BEGIN_PROFILER();
        return 30.0f;
    }


    auto SpotLight::GetMaxAngle() -> float {
        MKT_BEGIN_PROFILER();
        return 89.0f;
    }


    auto SpotLight::GetDirection() const -> const core::float3 & {
        MKT_BEGIN_PROFILER();
        return mDirection;
    }


    auto SpotLight::GetAngle() const -> float {
        MKT_BEGIN_PROFILER();
        return mAngle;
    }


    auto SpotLight::GetSoftness() const -> float {
        MKT_BEGIN_PROFILER();
        return mSoftness;
    }


    auto SpotLight::GetCutOff() const -> float {
        MKT_BEGIN_PROFILER();
        return mCutOff;
    }


    auto SpotLight::GetOuterCutOff() const -> float {
        MKT_BEGIN_PROFILER();
        return mOuterCutOff;
    }


    auto SpotLight::GetRadius() const -> float {
        MKT_BEGIN_PROFILER();
        return mRadius;
    }


    auto SpotLight::UpdateCutoffs() -> void {
        MKT_BEGIN_PROFILER();
        const float outer{ glm::radians( mAngle ) };
        const float inner{ glm::radians( eastl::max( 0.0f, mAngle - mSoftness ) ) };

        mCutOff = glm::cos( inner );
        mOuterCutOff = glm::cos( outer );
    }

    auto DirectionalLight::SetNormalizedDirection( core::float3 direction ) -> void {
        MKT_BEGIN_PROFILER();
        mDirection = NormalizeDirection( direction );
    }

    auto SpotLight::SetNormalizedDirection( core::float3 direction ) -> void {
        MKT_BEGIN_PROFILER();
        mDirection = NormalizeDirection( direction );
    }

    auto LightObject::NormalizeDirection( core::float3 direction ) -> core::float3 {
        const core::f32 length2{ glm::dot( direction, direction ) };
        return length2 > 1e-12f ? direction / glm::sqrt( length2 ) : core::float3{ 0, -1, 0 };
    }

    auto DirectionalLight::GetWorldDirection( const core::float4x4 &world ) const -> core::float3 {
        return NormalizeDirection( core::float3{ world * core::float4{ mDirection, 0.0f } } );
    }

    auto SpotLight::GetWorldDirection( const core::float4x4 &world ) const -> core::float3 {
        return NormalizeDirection( core::float3{ world * core::float4{ mDirection, 0.0f } } );
    }

    auto AreaLight::SetSize( const core::float2 &size ) -> void {
        for ( int axis{}; axis < 2; ++axis ) {
            mSize[axis] = eastl::max( size[axis], 0.0f );
        }
    }

    auto AreaLight::GetSize() const -> const core::float2 & {
        return mSize;
    }

    auto AreaLight::SetRadius( float radius ) -> void {
        mRadius = eastl::max( radius, 0.0f );
    }

    auto AreaLight::GetRadius() const -> float {
        return mRadius;
    }

    auto AreaLight::SetRange( float range ) -> void {
        mRange = eastl::max( range, 0.0f );
    }

    auto AreaLight::GetRange() const -> float {
        return mRange;
    }

    auto AreaLight::SetTwoSided( bool twoSided ) -> void {
        mIsTwoSided = twoSided;
    }

    auto AreaLight::IsTwoSided() const -> bool {
        return mIsTwoSided;
    }

    auto AreaLight::SetShape( AreaLightShape shape ) -> void {
        mShape = shape <= AreaLightShape::eTube ? shape : AreaLightShape::eRectangle;
    }

    auto AreaLight::SetMaterialEmissionEnabled( bool enabled ) -> void {
        mMaterialEmissionEnabled = enabled;
    }

    auto AreaLight::SetEmissionSourceEntityID( core::u64 entityID ) -> void {
        mEmissionSourceEntityID = entityID;
    }

    auto AreaLight::GetEmissionSourceEntityID() const -> core::u64 {
        return mEmissionSourceEntityID;
    }

    auto AreaLight::IsMaterialEmissionEnabled() const -> bool {
        return mMaterialEmissionEnabled;
    }

    auto AreaLight::GetShape() const -> AreaLightShape {
        return mShape;
    }
}// namespace mikoto::renderer
