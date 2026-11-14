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

#pragma once

#include "../../../slang/base/Math_Helpers.slang"

// D3D11 can sample the same 2D atlas as OpenGL. Layer zero contains diffuse
// irradiance, followed by increasing GGX roughness levels with one-texel gutters.
static const uint kEnvironmentGutter = 1u;
static const uint kEnvironmentSpecularLevels = 5u;
static const uint kEnvironmentDiffuseLayer = 0u;

static const float kEquirectangularEnvironment = 1.0;
static const float kCubeFacesEnvironment = 2.0;

[[vk::binding( 3, 0 )]] SamplerState environmentSampler : register( s3 );
[[vk::binding( 9, 0 )]] Texture2D panorama : register( t9 );
[[vk::binding( 10, 0 )]] Texture2D rightFace : register( t10 );
[[vk::binding( 11, 0 )]] Texture2D leftFace : register( t11 );
[[vk::binding( 12, 0 )]] Texture2D topFace : register( t12 );
[[vk::binding( 13, 0 )]] Texture2D bottomFace : register( t13 );
[[vk::binding( 14, 0 )]] Texture2D backFace : register( t14 );
[[vk::binding( 15, 0 )]] Texture2D frontFace : register( t15 );

float2 EnvironmentUV( float3 direction ) {
    return float2( atan2( direction.z, direction.x ) / ( 2.0 * PI_Value ) + 0.5,
        0.5 - asin( clamp( direction.y, -1.0, 1.0 ) ) / PI_Value );
}

float3 EnvironmentDirection( float2 uv ) {
    float azimuth = ( uv.x - 0.5 ) * ( 2.0 * PI_Value );
    float elevation = ( 0.5 - uv.y ) * PI_Value;
    float horizontal = cos( elevation );
    return float3( cos( azimuth ) * horizontal, sin( elevation ), sin( azimuth ) * horizontal );
}

float3 ReadEnvironment( float3 direction, float lod, float source ) {
    if ( source == kEquirectangularEnvironment ) {
        float2 uv = EnvironmentUV( direction );

        return panorama.SampleLevel( environmentSampler, uv, lod ).rgb;
    }

    if ( source != kCubeFacesEnvironment ) {
        return 0.0;
    }

    float3 axis = abs( direction );
    if ( axis.x >= axis.y && axis.x >= axis.z ) {
        float2 uv = float2( direction.x > 0.0 ? -direction.z : direction.z, -direction.y ) / axis.x * 0.5 + 0.5;
        return direction.x > 0.0 ? rightFace.SampleLevel( environmentSampler, uv, lod ).rgb
                                : leftFace.SampleLevel( environmentSampler, uv, lod ).rgb;
    }
    if ( axis.y >= axis.z ) {
        float2 uv = float2( direction.x, direction.y > 0.0 ? direction.z : -direction.z ) / axis.y * 0.5 + 0.5;
        return direction.y > 0.0 ? topFace.SampleLevel( environmentSampler, uv, lod ).rgb
                                : bottomFace.SampleLevel( environmentSampler, uv, lod ).rgb;
    }

    float2 uv = float2( direction.z > 0.0 ? direction.x : -direction.x, -direction.y ) / axis.z * 0.5 + 0.5;
    return direction.z > 0.0 ? backFace.SampleLevel( environmentSampler, uv, lod ).rgb
                            : frontFace.SampleLevel( environmentSampler, uv, lod ).rgb;
}

float4 EncodeSceneColor( float3 radiance ) {
    // Keep the HDR attachment finite and mark covered pixels for tone mapping.
    const float maxHalf = 65504.0;
    return float4( clamp( radiance, 0.0, maxHalf ), 1.0 );
}
