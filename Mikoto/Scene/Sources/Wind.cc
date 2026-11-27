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

#include <cmath>

#include <EASTL/sort.h>

#include <glm/gtc/matrix_transform.hpp>

#include <Scene/Scene.hh>
#include <Scene/Wind.hh>

namespace mikoto::scene {

    // Shared force-field idea: Gilbert Sanders, Guerrilla Games, GDC 2018.
    // https://www.gdcvault.com/play/1025530/contactUs
    // https://media.gdcvault.com/gdc2018/presentations/gilbert_sanders_between_tech_and.pdf
    // AMD documents a sine-wave grass variant inspired by that presentation:
    // https://gpuopen.com/learn/mesh_shaders/mesh_shaders-procedural_grass_rendering/
    // These analytic traveling waves are Mikoto's own implementation, not a fluid solver.
    auto SampleWind( const WindField &field, const core::float3 &position ) -> core::float3 {
        core::float3 result{};
        for ( const auto &area: field.mAreas ) {
            float weight{ 1.0f };
            if ( area.mControl.x != core::as<core::u32>( WindAreaShape::eDirectional ) ) {
                const core::float3 local{ area.mWorldToLocal * core::float4{ position, 1.0f } };
                const float distance{ area.mControl.x == core::as<core::u32>( WindAreaShape::eSphere ) ? glm::length( local ) * 2.0f : glm::max( glm::abs( local.x ), glm::max( glm::abs( local.y ), glm::abs( local.z ) ) ) * 2.0f };
                if ( distance >= 1.0f ) {
                    continue;
                }
                weight = glm::smoothstep( 0.0f, glm::max( area.mVelocityFalloff.w, .001f ), 1.0f - distance );
            }
            const core::float3 velocity{ area.mVelocityFalloff };
            const float speed{ glm::length( velocity ) };
            const core::float3 direction{ speed > .001f ? velocity / speed : core::float3{ 1, 0, 0 } };
            const core::float3 side{ glm::normalize( core::float3{ -direction.z, .15f, direction.x } ) };
            const float phase{ static_cast<float>( area.mControl.y % 4096 ) * .017f };
            const float spatial{ glm::dot( position, direction ) * 6.283185307f / glm::max( area.mWaves.z, .1f ) };
            const float clock{ field.mTime * area.mWaves.y * 6.283185307f };
            const float broad{ std::sin( spatial - clock + phase ) };
            const float detail{ std::sin( spatial * 2.17f - clock * 1.31f + phase * 1.7f ) };
            const float gust{ glm::max( 0.0f, 1.0f + area.mWaves.x * ( broad * .7f + detail * .3f ) ) };
            const float lateral{ std::sin( glm::dot( position, side ) * .37f - clock * 1.73f + phase ) };
            result += ( velocity * gust + side * lateral * area.mWaves.w ) * weight;
        }
        return result;
    }

    auto Scene::GetWindField() const -> WindField {
        WindField field{};
        field.mTime = core::as<core::f32>( mVolumeTime );
        eastl::vector<entt::entity> entities{};
        for ( const auto entity: mRegistry.view<WindAreaComponent, TransformComponent, TagComponent>() ) {
            if ( mRegistry.get<TagComponent>( entity ).IsActive() && mRegistry.get<WindAreaComponent>( entity ).IsEnabled() ) {
                entities.push_back( entity );
            }
        }
        eastl::sort( entities.begin(), entities.end(), [&]( auto a, auto b ) {
            return mRegistry.get<TagComponent>( a ).GetGuid() < mRegistry.get<TagComponent>( b ).GetGuid();
        } );
        for ( const auto entity: entities ) {
            if ( field.mAreas.size() == kMaxWindAreas ) {
                break;
            }
            const auto &source{ mRegistry.get<WindAreaComponent>( entity ) };
            const auto transform{ mRegistry.get<TransformComponent>( entity ).GetWorldTransform() };
            auto world{ glm::scale( transform, glm::max( source.GetDimensions(), core::float3{ .001f } ) ) };
            const float determinant{ glm::determinant( world ) };
            if ( !std::isfinite( determinant ) || glm::abs( determinant ) < 1e-9f ) {
                continue;
            }
            core::float3 velocity{ glm::mat3{ transform } * source.GetVelocity() };
            const float speed{ glm::length( source.GetVelocity() ) };
            const float transformedSpeed{ glm::length( velocity ) };
            if ( !std::isfinite( transformedSpeed ) ) {
                continue;
            }
            if ( transformedSpeed > .001f ) {
                velocity *= speed / transformedSpeed;
            }
            WindAreaData area{};
            area.mWorldToLocal = glm::inverse( world );
            area.mVelocityFalloff = { velocity, glm::clamp( source.GetFalloff(), 0.0f, 1.0f ) };
            area.mWaves = { glm::clamp( source.GetGustStrength(), 0.0f, 1.0f ), glm::clamp( source.GetGustFrequency(), 0.0f, 4.0f ),
                glm::clamp( source.GetGustWavelength(), .1f, 1000.0f ), glm::clamp( source.GetTurbulence(), 0.0f, 20.0f ) };
            area.mControl = { core::as<core::u32>( source.GetShape() ), source.GetSeed(), 0, 0 };
            field.mAreas.push_back( area );
        }
        return field;
    }

    auto WindAreaComponent::SetShape( WindAreaShape value ) -> void {
        mShape = value;
    }

    auto WindAreaComponent::SetVelocity( const core::float3 &value ) -> void {
        mVelocity = value;
    }

    auto WindAreaComponent::SetDimensions( const core::float3 &value ) -> void {
        mDimensions = value;
    }

    auto WindAreaComponent::SetFalloff( core::f32 value ) -> void {
        mFalloff = value;
    }

    auto WindAreaComponent::SetGustStrength( core::f32 value ) -> void {
        mGustStrength = value;
    }

    auto WindAreaComponent::SetGustFrequency( core::f32 value ) -> void {
        mGustFrequency = value;
    }

    auto WindAreaComponent::SetGustWavelength( core::f32 value ) -> void {
        mGustWavelength = value;
    }

    auto WindAreaComponent::SetTurbulence( core::f32 value ) -> void {
        mTurbulence = value;
    }

    auto WindAreaComponent::SetSeed( core::u32 value ) -> void {
        mSeed = value;
    }

    auto WindAreaComponent::SetEnabled( bool value ) -> void {
        mEnabled = value;
    }

    auto WindAreaComponent::GetShape() const -> WindAreaShape {
        return mShape;
    }

    auto WindAreaComponent::GetVelocity() const -> const core::float3 & {
        return mVelocity;
    }

    auto WindAreaComponent::GetDimensions() const -> const core::float3 & {
        return mDimensions;
    }

    auto WindAreaComponent::GetFalloff() const -> core::f32 {
        return mFalloff;
    }

    auto WindAreaComponent::GetGustStrength() const -> core::f32 {
        return mGustStrength;
    }

    auto WindAreaComponent::GetGustFrequency() const -> core::f32 {
        return mGustFrequency;
    }

    auto WindAreaComponent::GetGustWavelength() const -> core::f32 {
        return mGustWavelength;
    }

    auto WindAreaComponent::GetTurbulence() const -> core::f32 {
        return mTurbulence;
    }

    auto WindAreaComponent::GetSeed() const -> core::u32 {
        return mSeed;
    }

    auto WindAreaComponent::IsEnabled() const -> bool {
        return mEnabled;
    }

}// namespace mikoto::scene
