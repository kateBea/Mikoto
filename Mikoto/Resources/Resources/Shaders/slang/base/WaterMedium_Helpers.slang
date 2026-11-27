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

// Water-medium bounds, extinction and in-scattering integration. Caustic and sun transmittance
// queries use the same water plane and optical coefficients as surface transmission.

#pragma once

#include "../vendor/WickedEngine/Water.slang"
#include "Volume_Helpers.slang"
#include "Water_Helpers.slang"

static const uint kWaterShoreFoam = 4;
static const uint kWaterUnderwaterScattering = 8;
static const uint kWaterCaustics = 16;
static const uint kWaterDepthRefraction = 32;

float3 WaterAbsorption( WaterSurface surface ) {
    return -log( clamp( surface.mAttenuation.rgb, .0001, 1.0 ) ) / max( surface.mAttenuation.a, .001 );
}

float3 WaterExtinction( WaterSurface surface ) {
    return WaterAbsorption( surface ) + max( surface.mScattering.rgb, 0.0 );
}

float3 WaterWorldPoint( CameraInfo camera, float2 uv, float depth ) {
    float4 view = mul( camera.mInverseProjection, float4( uv * float2( 2, -2 ) + float2( -1, 1 ), depth, 1 ) );
    float3 position = view.xyz / ( abs( view.w ) > 1e-8 ? view.w : 1e-8 );
    return camera.mViewPosition.xyz + mul( transpose( ( float3x3 )camera.mViewMatrix ), position );
}

float3 WaterUp( WaterSurface surface ) {
    return normalize( mul( ( float3x3 )surface.mWorld, float3( 0, 1, 0 ) ) );
}

float WaterDepth( WaterSurface surface, float3 position ) {
    float3 origin = mul( surface.mWorld, float4( 0, 0, 0, 1 ) ).xyz;
    return dot( origin - position, WaterUp( surface ) );
}

bool WaterContains( WaterSurface surface, float3 position ) {
    float3 local = mul( surface.mWorldToLocal, float4( position, 1 ) ).xyz;
    float depth = WaterDepth( surface, position );
    return ( ( surface.mGeometry.w & 64u ) != 0 || ( all( local.xz >= 0 ) && all( local.xz <= surface.mBounds.xy ) ) ) && depth > 0 && depth < surface.mBounds.z;
}

// The box limits the medium at coast-demo water body edges and at its authored bottom.
// Find where the camera/light ray leaves the finite water medium. The integration interval is
// clipped to this boundary before applying optical coefficients.
float WaterExitDistance( WaterSurface surface, float3 origin, float3 direction ) {
    float3 local = mul( surface.mWorldToLocal, float4( origin, 1 ) ).xyz;
    float3 ray = mul( ( float3x3 )surface.mWorldToLocal, direction );
    float axisScale = length( mul( ( float3x3 )surface.mWorld, float3( 0, 1, 0 ) ) );
    float3 lower = float3( 0, -surface.mBounds.z / max( axisScale, 1e-5 ), 0 );
    float3 upper = float3( surface.mBounds.x, 0, surface.mBounds.y );
    float exit = 1e30;
    for ( uint axis = 0; axis < 3; ++axis ) {
        if ( abs( ray[axis] ) > 1e-8 ) {
            exit = min( exit, ( ( ray[axis] > 0 ? upper[axis] : lower[axis] ) - local[axis] ) / ray[axis] );
        }
    }
    return max( exit, 0.0 );
}

float3 WaterLightDirection( WaterSurface surface, float3 incomingAir ) {
    float3 down = refract( -incomingAir, WaterUp( surface ), 1.0 / max( surface.mOptics.x, 1.001 ) );
    return -normalize( down );
}

float3 WaterSunTransmittance( WaterSurface surface, float3 position, float3 incomingAir ) {
    float cosine = dot( WaterUp( surface ), incomingAir );
    if ( cosine <= 0 ) {
        return 0;
    }
    float3 incomingWater = WaterLightDirection( surface, incomingAir );
    float distance = max( WaterDepth( surface, position ), 0.0 ) / max( dot( incomingWater, WaterUp( surface ) ), .05 );
    float ratio = ( surface.mOptics.x - 1 ) / ( surface.mOptics.x + 1 );
    float f0 = ratio * ratio;
    float fresnel = f0 + ( 1 - f0 ) * pow( 1 - saturate( cosine ), 5.0 );
    return ( 1 - fresnel ) * exp( -WaterExtinction( surface ) * distance );
}

// Wicked's RGB caustic shader, projected along the refracted sun onto submerged
// receivers. Mikoto supplies depth falloff, extinction and existing shadowing.
// Evaluate the adapted caustic intensity in the underwater receiver domain. Sun direction and
// depth determine how the pattern is projected through the interface.
float3 WaterCausticPattern( WaterSurface surface, float3 position, float3 incomingAir ) {
    float depth = WaterDepth( surface, position );
    if ( ( surface.mGeometry.w & kWaterCaustics ) == 0 || dot( WaterUp( surface ), incomingAir ) <= 0 || depth <= .02 || depth >= surface.mCaustics.z ) {
        return 0;
    }
    float3 incomingWater = WaterLightDirection( surface, incomingAir );
    float3 entry = position + incomingWater * depth / max( dot( incomingWater, WaterUp( surface ) ), .05 );
    float2 p = mul( surface.mWorldToLocal, float4( entry, 1 ) ).xz * surface.mCaustics.y;
    float fade = smoothstep( .02, .25, depth ) * ( 1 - smoothstep( surface.mCaustics.z * .5, surface.mCaustics.z, depth ) );
    return WickedCaustics( p, surface.mBounds.w ) * surface.mCaustics.x * fade;
}

// Analytic Beer-Lambert integration per segment avoids step-size-dependent fog.
// Both HG directions point away from the sample, matching PBRT's convention.
// https://pbr-book.org/4ed/Volume_Scattering/Phase_Functions
// Integrate homogeneous water scattering over a finite segment. The closed-form extinction
// integral avoids dividing directly by zero optical depth.
float3 WaterSegmentIntegral( float3 extinction, float distance ) {
    float3 integral;
    for ( uint channel = 0; channel < 3; ++channel ) {
        integral[channel] = extinction[channel] > 1e-6 ? ( 1 - exp( -extinction[channel] * distance ) ) / extinction[channel] : distance;
    }
    return integral;
}
