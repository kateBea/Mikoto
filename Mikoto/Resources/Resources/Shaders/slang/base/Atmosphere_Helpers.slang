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

// Atmosphere parameterization adapted from Mirinae, Copyright (c) 2023 Sungmin Woo (MIT).
// https://github.com/SausageTaste/mirinae/blob/54293d427b1c107f0566a0db59935950546e1528/LICENSE

#pragma once

static const float AtmosGroundOffset = 0.01;

struct AtmosphereData {
    float4 mRadii;
    float4 mRayleigh;
    float4 mMie;
    float4 mOzone;
    float4 mGround;
    float4 mSunDirection;
    float4 mSunIlluminance;
    float4 mView;
};

// As in Mirinae, world up is radial at the scene origin and the planet is
// centered one ground radius below it. Mikoto supplies its own world scale.
// https://github.com/SausageTaste/mirinae/blob/54293d427b1c107f0566a0db59935950546e1528/asset/slang/atmos/sky_view.slang#L24-L45
float3 AtmosPlanetPosition( AtmosphereData a, float3 world ) {
    return ( world - float3( 0, a.mRadii.w, 0 ) ) * a.mRadii.z + float3( 0, a.mRadii.x, 0 );
}

float3 AtmosClampGround( AtmosphereData a, float3 position ) {
    float radius = length( position );
    float minimum = a.mRadii.x + AtmosGroundOffset;
    if ( radius >= minimum ) {
        return position;
    }

    float3 up = radius > 1e-4 ? position / radius : float3( 0, 1, 0 );
    return up * minimum;
}

bool AtmosSphereInterval( float3 origin, float3 direction, float radius, out float2 hits ) {
    float projected = dot( origin, direction );
    float3 closest = origin - projected * direction;
    float discriminant = radius * radius - dot( closest, closest );
    if ( discriminant < 0 ) {
        hits = 0;
        return false;
    }

    float halfChord = sqrt( discriminant );
    hits = -projected + float2( -halfChord, halfChord );
    return true;
}

// Nearest forward intersection. A negative result means the ray misses.
float AtmosSphere( float3 origin, float3 direction, float radius ) {
    float2 hits;
    if ( !AtmosSphereInterval( origin, direction, radius, hits ) || hits.y < 0 ) {
        return -1;
    }
    return hits.x >= 0 ? hits.x : hits.y;
}

// Texel centers span [0,1] in the mathematical LUT domain.
float2 AtmosUnitToTexel( float2 uv, float2 size ) {
    return ( saturate( uv ) * ( size - 1 ) + 0.5 ) / size;
}

// Bruneton's parameterization, used by Hillaire, gives long horizon paths
// more resolution. https://sebh.github.io/publications/egsr2020.pdf
float2 AtmosTransmittanceUv( AtmosphereData a, float height, float cosine ) {
    float H = sqrt( max( 0, a.mRadii.y * a.mRadii.y - a.mRadii.x * a.mRadii.x ) );
    float rho = sqrt( max( 0, height * height - a.mRadii.x * a.mRadii.x ) );
    float d = max( 0, -height * cosine +
                              sqrt( max( 0, height * height * ( cosine * cosine - 1 ) + a.mRadii.y * a.mRadii.y ) ) );
    float minimum = a.mRadii.y - height;
    return AtmosUnitToTexel( float2( ( d - minimum ) / max( 1e-4, rho + H - minimum ), rho / H ), float2( 256, 64 ) );
}

float3 AtmosSunTransmittance( AtmosphereData a, float3 position, float3 sun, Texture2D<float4> lut, SamplerState lutSampler ) {
    if ( AtmosSphere( position, sun, a.mRadii.x ) >= 0 ) {
        return 0;
    }

    float height = length( position );
    if ( height > a.mRadii.y ) {
        float2 hits;
        if ( !AtmosSphereInterval( position, sun, a.mRadii.y, hits ) || hits.x < 0 ) {
            return 1;
        }

        position += hits.x * sun;
        height = a.mRadii.y;
    }

    float2 uv = AtmosTransmittanceUv( a, height, dot( position / height, sun ) );
    return lut.SampleLevel( lutSampler, uv, 0 ).rgb;
}

// Mirinae evaluates the sunlight's path through air at each surface:
// https://github.com/SausageTaste/mirinae/blob/54293d427b1c107f0566a0db59935950546e1528/asset/slang/compo/atmos_surface.slang#L100-L124
float3 AtmosSurfaceSun( AtmosphereData a, float3 world, Texture2D<float4> lut, SamplerState lutSampler ) {
    float3 position = AtmosClampGround( a, AtmosPlanetPosition( a, world ) );
    return a.mSunIlluminance.xyz * AtmosSunTransmittance( a, position, a.mSunDirection.xyz, lut, lutSampler );
}
