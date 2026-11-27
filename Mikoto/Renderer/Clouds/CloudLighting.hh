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

#ifndef MIKOTO_RENDERER_CLOUDS_CLOUD_LIGHTING_HH
#define MIKOTO_RENDERER_CLOUDS_CLOUD_LIGHTING_HH

#include <EASTL/vector.h>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include <Renderer/Clouds/Clouds.hh>

namespace mikoto::renderer {

    /** Fit cloud receivers in a fixed light basis, with room upstream for off-camera casters. */
    // A 1024-square map per cloud reaches upstream by Clouds Max distance, allowing
    // off-camera mesh casters. Masked materials, skinning and cloth share normal shadow
    // vertex/coverage paths; terrain uses separate exact heightfield visibility. Local
    // point lights use six 256-square faces and spotlights one. At eight clouds the
    // maps are about 44 MiB, plus a 4 MiB arena (262144 indirect commands). Overflow
    // uses the complete caster list. Sub-texel casters and narrow beams remain limited
    // by map/cache resolution.
    inline auto BuildCloudSunShadow( const core::float4x4 &cloud, core::float3 toSun,
        core::u32 resolution, core::f32 upstreamDistance ) -> core::float4x4 {
        using namespace core;
        toSun = glm::normalize( toSun );
        const float3 up{ glm::abs( toSun.y ) > .99f ? float3{ 0, 0, 1 } : float3{ 0, 1, 0 } };
        const auto view{ glm::lookAtRH( float3{}, -toSun, up ) * glm::translate( float4x4{ 1 }, -float3{ cloud[3] } ) };
        float3 minimum{ 1e30f }, maximum{ -1e30f };
        for ( u32 i{}; i < 8; ++i ) {
            const float3 corner{ view * cloud * float4{ i & 1 ? .5f : -.5f, i & 2 ? .5f : -.5f, i & 4 ? .5f : -.5f, 1 } };
            minimum = glm::min( minimum, corner );
            maximum = glm::max( maximum, corner );
        }
        const float2 span{ glm::max( float2{ maximum - minimum }, float2{ 1 } ) };
        const float2 texel{ span / float( resolution - 4 ) };
        const float2 center{ glm::round( float2{ minimum + maximum } * .5f / texel ) * texel };
        const float2 half{ texel * float( resolution ) * .5f };
        return glm::orthoRH_ZO( center.x - half.x, center.x + half.x, center.y - half.y, center.y + half.y,
                   -maximum.z - glm::max( upstreamDistance, 1.0f ), -minimum.z + 1 ) *
               view;
    }

    inline auto BuildCloudLocalShadow( core::float3 position, core::float3 direction,
        core::f32 range, core::f32 outerCosine, core::u32 face, bool point ) -> core::float4x4 {
        using namespace core;
        constexpr float3 directions[]{ { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
        constexpr float3 ups[]{ { 0, -1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 }, { 0, -1, 0 }, { 0, -1, 0 } };
        const float3 forward{ point ? directions[face] : glm::normalize( direction ) };
        const float3 up{ point ? ups[face] : glm::abs( forward.y ) > .99f ? float3{ 0, 0, 1 }
                                                                          : float3{ 0, 1, 0 } };
        const f32 angle{ point ? glm::radians( 90.0f ) : 2 * glm::acos( glm::clamp( outerCosine, .001f, .99999f ) ) };
        return glm::perspectiveRH_ZO( angle, 1.0f, glm::max( range * .001f, .001f ), glm::max( range, .02f ) ) *
               glm::lookAtRH( float3{}, forward, up ) * glm::translate( float4x4{ 1 }, -position );
    }

    /** Stable geometry within one flash; envelope changes only radiance, never the branch topology. */
    // Seeded branching ribbons follow a multiple-pulse radiance envelope. Scene depth
    // clips bolts and intervening clouds attenuate them; HDR compositing happens before
    // bloom/tonemapping. Length, width, branches and visibility serialize on the cloud.
    // The 512-segment budget bounds geometry. This is an artistic discharge model,
    // not an electrical simulation. Scene volume time advances in Play/Simulate.
    inline auto BuildCloudBolt( const CloudLightningSettings &settings, const CloudLightningState &state,
        eastl::vector<CloudBoltSegment> &segments ) -> void {
        using namespace core;
        if ( !settings.mBoltsEnabled || state.mColorIntensity.w <= 0 || segments.size() >= kMaxCloudBoltSegments ) {
            return;
        }
        u32 seed{ state.mBoltSeed };
        const auto random{ [&]() -> f32 {
            seed += 0x9e3779b9u;
            u32 x{ seed };
            x ^= x >> 16;
            x *= 0x7feb352du;
            x ^= x >> 15;
            x *= 0x846ca68bu;
            x ^= x >> 16;
            return float( x >> 8 ) / 16777216.0f;
        } };
        const auto append{ [&]( float3 a, float3 b, f32 width, f32 energy ) {
            if ( segments.size() < kMaxCloudBoltSegments && glm::length( b - a ) > .001f ) {
                segments.push_back( { float4{ a, width }, float4{ b, state.mColorIntensity.w * energy }, float4{ settings.mColor, 1 } } );
            }
        } };
        const f32 length{ glm::clamp( settings.mBoltLength, 1.0f, 10000.0f ) };
        const f32 width{ glm::clamp( settings.mBoltWidth, .01f, 20.0f ) };
        float3 points[25]{};
        points[0] = float3{ state.mPositionRadius };
        const float3 drift{ random() - .5f, 0, random() - .5f };
        for ( u32 i{ 1 }; i <= 24; ++i ) {
            const f32 t{ float( i ) / 24 };
            const float3 jitter{ random() - .5f, ( random() - .5f ) * .35f, random() - .5f };
            points[i] = float3{ state.mPositionRadius } + length * ( float3{ drift.x * t * .3f, -t, drift.z * t * .3f } + jitter * .035f );
            append( points[i - 1], points[i], width * ( 1 - .45f * t ), 40 );
        }
        for ( u32 branch{}; branch < glm::min( settings.mBoltBranches, 8u ); ++branch ) {
            const u32 root{ 3 + u32( random() * 15 ) };
            float3 a{ points[root] };
            const float3 forward{ glm::normalize( float3{ ( random() - .5f ) * 2, -.5f - random(), ( random() - .5f ) * 2 } ) };
            for ( u32 i{}; i < 8; ++i ) {
                const float3 jitter{ random() - .5f, random() - .5f, random() - .5f };
                const float3 b{ a + length * ( forward * .025f + jitter * .012f ) };
                append( a, b, width * .45f * ( 1 - float( i ) / 9 ), 16 );
                a = b;
            }
        }
    }

}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_CLOUDS_CLOUD_LIGHTING_HH
