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

// Hillaire, EGSR 2020: https://sebh.github.io/publications/egsr2020.pdf
// Parameterizations and scattering integration adapted from Mirinae,
// Copyright (c) 2023 Sungmin Woo (MIT).
// https://github.com/SausageTaste/mirinae/blob/54293d427b1c107f0566a0db59935950546e1528/LICENSE

// Shared atmospheric integration and LUT addressing. Distances inside the atmosphere model use
// the units established by AtmosphereData; camera/world conversions happen at the boundary.

#pragma once

#include "base/Atmosphere_Helpers.slang"

#if !defined( MKT_ATMOSPHERE_RAY_TRACING )
#include "base/Base.slang"
#include "base/Camera_Helpers.slang"
#include "base/Memory_Helpers.slang"
#endif

static const float AtmosPi = 3.141592653589793;

#if defined( MKT_ATMOSPHERE_RAY_TRACING )
// The ray pipeline supplies these resources through the shared bindless table.
// Reuse the raster solver without importing its bindless push constants.
AtmosphereData Atmosphere() {
    AtmosphereData a;
    a.mRadii = asfloat( LoadBufferValue<uint4>( Parameters.mViewEffects, 0 ) );
    a.mRayleigh = asfloat( LoadBufferValue<uint4>( Parameters.mViewEffects, 16 ) );
    a.mMie = asfloat( LoadBufferValue<uint4>( Parameters.mViewEffects, 32 ) );
    a.mOzone = asfloat( LoadBufferValue<uint4>( Parameters.mViewEffects, 48 ) );
    a.mGround = asfloat( LoadBufferValue<uint4>( Parameters.mViewEffects, 64 ) );
    a.mSunDirection = asfloat( LoadBufferValue<uint4>( Parameters.mViewEffects, 80 ) );
    a.mSunIlluminance = asfloat( LoadBufferValue<uint4>( Parameters.mViewEffects, 96 ) );
    a.mView = asfloat( LoadBufferValue<uint4>( Parameters.mViewEffects, 112 ) );
    return a;
}

float4 AtmosRead( uint textureID, float2 uv ) {
    return AtmosphereMap( textureID ).SampleLevel( GetAtmosphereSampler(), uv, 0 );
}

float3 AtmosSunTransmittance( AtmosphereData a, float3 position, float3 sun ) {
    return AtmosSunTransmittance( a, position, sun, AtmosphereMap( 0 ), GetAtmosphereSampler() );
}
float3 AtmosReadMultiple( float2 uv ) {
    return AtmosRead( 1, uv ).rgb;
}
#else

#if defined( MKT_ATMOSPHERE_MULTIPLE_SCATTERING )
struct AtmosphereComputeParams {
    Pointer mAtmosphere;
    uint mTransmittance;
    uint mSampler;
    uint mOutput;
};

MKT_DECLARE_PUSH_CONSTANT( AtmosphereComputeParams, parameters );
#else
struct AtmosphereDrawParams {
    Pointer mAtmosphere;
    Pointer mCamera;
    uint mTransmittance;
    uint mMultipleScattering;
    uint mSkyView;
    uint mSource;
    uint mDepth;
    uint mSampler;
    uint mStage;
    uint mFace;
    uint mAerialRadiance;
    uint mAerialTransmittance;
};

MKT_DECLARE_PUSH_CONSTANT( AtmosphereDrawParams, parameters );
#endif

MKT_DECLARE_TEXTURE_SRV( Texture2D )
textures;
MKT_DECLARE_SAMPLERS samplers;

AtmosphereData Atmosphere() {
    return ( ( BufferReference<AtmosphereData> )parameters.mAtmosphere )[0];
}

float4 AtmosRead( uint textureID, float2 uv ) {
    return textures[textureID].SampleLevel( samplers[parameters.mSampler], uv, 0 );
}

float3 AtmosSunTransmittance( AtmosphereData a, float3 position, float3 sun ) {
    return AtmosSunTransmittance( a, position, sun, textures[parameters.mTransmittance], samplers[parameters.mSampler] );
}

float3 AtmosReadMultiple( float2 uv ) {
#if defined( MKT_ATMOSPHERE_MULTIPLE_SCATTERING )
    return 0;
#else
    return AtmosRead( parameters.mMultipleScattering, uv ).rgb;
#endif
}
#endif

float2 AtmosTexelToUnit( float2 uv, float2 size ) {
    return saturate( ( uv * size - 0.5 ) / ( size - 1 ) );
}

float AtmosEntryDistance( AtmosphereData a, float3 origin, float3 direction ) {
    if ( dot( origin, origin ) <= a.mRadii.y * a.mRadii.y ) {
        return 0;
    }

    float2 hits;
    return AtmosSphereInterval( origin, direction, a.mRadii.y, hits ) && hits.x >= 0 ? hits.x : -1;
}

struct AtmosMedium {
    float3 mRayleigh;
    float3 mMie;
    float3 mExtinction;
};

// Evaluate altitude-dependent Rayleigh, Mie and ozone coefficients. Scattering adds directional
// radiance while total extinction attenuates the ray.
AtmosMedium AtmosSampleMedium( AtmosphereData a, float3 position ) {
    float height = max( 0, length( position ) - a.mRadii.x );
    float rayleighDensity = exp( -height / a.mRayleigh.w );
    float mieDensity = exp( -height / a.mMie.z );
    // Ozone is a triangular layer spanning 10..40 km, peaking at 25 km.
    float ozoneDensity = saturate( 1 - abs( height - 25 ) / 15 );
    AtmosMedium medium;
    medium.mRayleigh = a.mRayleigh.xyz * rayleighDensity;
    medium.mMie = float3( a.mMie.x * mieDensity );
    medium.mExtinction = medium.mRayleigh + a.mMie.y * mieDensity + a.mOzone.xyz * ozoneDensity;
    return medium;
}

void AtmosTransmittanceRay( AtmosphereData a, float2 uv, out float3 origin, out float3 direction ) {
    uv = AtmosTexelToUnit( uv, float2( 256, 64 ) );
    float H = sqrt( a.mRadii.y * a.mRadii.y - a.mRadii.x * a.mRadii.x );
    float rho = H * uv.y;
    float height = sqrt( rho * rho + a.mRadii.x * a.mRadii.x );
    float distance = lerp( a.mRadii.y - height, rho + H, uv.x );
    float cosine = distance < 1e-5
                       ? 1
                       : clamp( ( H * H - rho * rho - distance * distance ) / ( 2 * height * distance ), -1, 1 );
    origin = float3( 0, height, 0 );
    direction = float3( sqrt( saturate( 1 - cosine * cosine ) ), cosine, 0 );
}

float3 AtmosMultiple( AtmosphereData a, float3 position, float3 sun ) {
    float height = length( position );
    float2 uv =
        float2( dot( position / height, sun ) * 0.5 + 0.5, ( height - a.mRadii.x ) / ( a.mRadii.y - a.mRadii.x ) );
    return AtmosReadMultiple( AtmosUnitToTexel( uv, float2( 32 ) ) );
}

// Integral of constant source over one segment. The limiting value is ds
// when extinction approaches zero; that limit matters for vacuum settings.
// Evaluate the homogeneous-segment scattering integral using its extinction. The small-
// extinction limit avoids a numerically unstable division.
float3 AtmosSegmentIntegral( float3 extinction, float distance ) {
    float3 tau = extinction * distance;
    float3 exact = ( 1 - exp( -tau ) ) / max( extinction, 1e-9 );
    float3 series = distance * ( 1 - 0.5 * tau );
    return float3( tau.x < 1e-3 ? series.x : exact.x, tau.y < 1e-3 ? series.y : exact.y,
        tau.z < 1e-3 ? series.z : exact.z );
}

struct AtmosIntegration {
    float3 mRadiance;
    float3 mTransmittance;
    float3 mScatteringFraction;
};

// March the atmosphere segment while tracking radiance and transmittance. Sun visibility, phase
// functions and the multiple-scattering LUT determine the source term at each sample.
AtmosIntegration AtmosIntegrate( AtmosphereData a, float3 origin, float3 direction, float3 sun, float maximumDistance,
    uint samples, bool multiple, bool isotropic, bool groundBounce ) {
    AtmosIntegration result;
    result.mRadiance = 0;
    result.mTransmittance = 1;
    result.mScatteringFraction = 0;

    // Intersect the ray segment with the shell so exterior cameras march
    // from the atmosphere entry point and skip the vacuum ahead of it.
    // https://github.com/SausageTaste/mirinae/blob/54293d427b1c107f0566a0db59935950546e1528/asset/slang/atmos/cam_vol.slang#L54-L70
    float2 shell;
    if ( !AtmosSphereInterval( origin, direction, a.mRadii.y, shell ) ) {
        return result;
    }

    float entry = max( 0, shell.x );
    float end = min( shell.y, maximumDistance );
    float ground = AtmosSphere( origin, direction, a.mRadii.x );
    if ( ground >= 0 ) {
        end = min( end, ground );
    }

    float distance = end - entry;
    if ( distance <= 0 ) {
        return result;
    }

    origin += entry * direction;

    float cosine = dot( direction, sun );
    float phaseR = 3.0 / ( 16.0 * AtmosPi ) * ( 1 + cosine * cosine );
    float g = a.mMie.w;
    float phaseM = 3.0 / ( 8.0 * AtmosPi ) * ( 1 - g * g ) * ( 1 + cosine * cosine ) /
                   ( ( 2 + g * g ) * pow( max( 1e-4, 1 + g * g - 2 * g * cosine ), 1.5 ) );
    if ( isotropic ) {
        phaseR = phaseM = 1 / ( 4 * AtmosPi );
    }

    // Quadratic step distribution spends more samples near the observer,
    // where density and aerial perspective vary fastest.
    for ( uint i = 0; i < samples; ++i ) {
        float begin = float( i ) / samples;
        float end = float( i + 1 ) / samples;
        float ds = ( end * end - begin * begin ) * distance;
        float3 position = origin + direction * ( ( begin * begin + end * end ) * 0.5 * distance );
        AtmosMedium medium = AtmosSampleMedium( a, position );
        float3 scattering = medium.mRayleigh + medium.mMie;
        float3 source =
            AtmosSunTransmittance( a, position, sun ) * ( medium.mRayleigh * phaseR + medium.mMie * phaseM );
        if ( multiple ) {
            source += AtmosMultiple( a, position, sun ) * scattering;
        }
        float3 integral = AtmosSegmentIntegral( medium.mExtinction, ds );
        result.mRadiance += result.mTransmittance * source * integral;
        result.mScatteringFraction += result.mTransmittance * scattering * integral;
        result.mTransmittance *= exp( -medium.mExtinction * ds );
    }

    if ( groundBounce && ground >= entry && ground <= end + 0.001 ) {
        float3 position = origin + ( ground - entry ) * direction;
        float3 normal = normalize( position );
        result.mRadiance += result.mTransmittance * a.mGround.xyz / AtmosPi * saturate( dot( normal, sun ) ) *
                            AtmosSunTransmittance( a, position + normal * AtmosGroundOffset, sun );
    }

    return result;
}
// Nonlinear sky-view coordinates split at the actual planetary horizon and
// concentrate rows on either side. Azimuth is relative to the projected sun.
// Map a sky direction into the sky-view LUT domain. The horizon-dependent mapping must be
// inverted by AtmosSkyDirection when the LUT is generated.
float2 AtmosSkyUv( AtmosphereData a, float3 origin, float3 direction ) {
    float height = length( origin );
    float3 up = origin / height;
    float beta = asin( saturate( a.mRadii.x / height ) );
    float horizon = AtmosPi - beta;
    float zenithCosine = dot( direction, up );
    float theta = acos( clamp( zenithCosine, -1, 1 ) );
    float y = theta < horizon ? ( 1 - sqrt( saturate( 1 - theta / horizon ) ) ) * 0.5
                              : sqrt( saturate( ( theta - horizon ) / beta ) ) * 0.5 + 0.5;
    float3 viewOnHorizon = direction - up * zenithCosine;
    float3 sunOnHorizon = a.mSunDirection.xyz - up * dot( a.mSunDirection.xyz, up );
    float denominator = length( viewOnHorizon ) * length( sunOnHorizon );
    float cosine = denominator > 1e-5 ? dot( viewOnHorizon, sunOnHorizon ) / denominator : 1;
    return AtmosUnitToTexel( float2( sqrt( saturate( 0.5 - 0.5 * cosine ) ), y ), float2( 192, 108 ) );
}

float3 AtmosSkyDirection( AtmosphereData a, float3 origin, float2 uv ) {
    uv = AtmosTexelToUnit( uv, float2( 192, 108 ) );

    float height = length( origin );
    float3 up = origin / height;
    float beta = asin( saturate( a.mRadii.x / height ) );
    float horizon = AtmosPi - beta;
    float theta = uv.y < 0.5 ? horizon * ( 1 - pow( 1 - uv.y * 2, 2 ) ) : horizon + beta * pow( uv.y * 2 - 1, 2 );
    float azimuthCos = 1 - 2 * uv.x * uv.x;

    float3 sunOnHorizon = a.mSunDirection.xyz - up * dot( a.mSunDirection.xyz, up );
    float3 axis = abs( up.y ) < 0.99 ? float3( 0, 1, 0 ) : float3( 0, 0, 1 );
    float3 forward = length( sunOnHorizon ) > 1e-5 ? normalize( sunOnHorizon ) : normalize( cross( up, axis ) );
    float3 horizontal = forward * azimuthCos + cross( forward, up ) * sqrt( saturate( 1 - azimuthCos * azimuthCos ) );

    return normalize( up * cos( theta ) + horizontal * sin( theta ) );
}

#if !defined( MKT_ATMOSPHERE_RAY_TRACING ) && !defined( MKT_ATMOSPHERE_MULTIPLE_SCATTERING )
// Reconstruct directions before adding the camera translation. Subtracting two
// nearby world positions loses the small ray vector at kilometre coordinates,
// which makes the sky's horizon and sun wobble as the camera moves.
// Reconstruct an orientation-only world ray from the current inverse projection. This shares
// the projection used by sky, sun and aerial-perspective composition.
void AtmosCameraRay( CameraInfo camera, float2 uv, AtmosphereData a, out float3 origin, out float3 direction ) {
    float2 ndc = float2( uv.x * 2 - 1, 1 - uv.y * 2 );
    float4 first = mul( camera.mInverseProjection, float4( ndc, .25, 1 ) );
    float4 second = mul( camera.mInverseProjection, float4( ndc, .75, 1 ) );
    float3 pointA = first.xyz / first.w;
    float3 pointB = second.xyz / second.w;
    bool aIsNear = dot( pointA, pointA ) < dot( pointB, pointB );
    float3 nearPoint = aIsNear ? pointA : pointB;
    float3 farPoint = aIsNear ? pointB : pointA;
    bool orthographic = camera.mProjection[3][3] != 0;
    float3 viewDirection = normalize( orthographic ? farPoint - nearPoint : farPoint );
    float3x3 cameraToWorld = transpose( ( float3x3 )camera.mViewMatrix );
    direction = normalize( mul( cameraToWorld, viewDirection ) );
    float3 viewOrigin = orthographic ? nearPoint - viewDirection * dot( nearPoint, viewDirection ) : float3( 0 );
    float3 worldOffset = mul( cameraToWorld, viewOrigin );
    origin = AtmosClampGround( a, AtmosPlanetPosition( a, camera.mViewPosition.xyz ) + worldOffset * a.mRadii.z );
}

float AtmosCameraDistance( CameraInfo camera, float2 uv, float depth ) {
    float4 view = mul( camera.mInverseProjection, float4( uv.x * 2 - 1, 1 - uv.y * 2, depth, 1 ) );
    float3 position = view.xyz / view.w;
    return camera.mProjection[3][3] != 0 ? abs( position.z ) : length( position );
}

#endif

float3 AtmosCubeDirection( float2 uv, uint face ) {
    float2 p = uv * 2 - 1;
    switch ( face ) {
        case 0: {
            return normalize( float3( 1, -p.y, -p.x ) );
        }
        case 1: {
            return normalize( float3( -1, -p.y, p.x ) );
        }
        case 2: {
            return normalize( float3( p.x, 1, p.y ) );
        }
        case 3: {
            return normalize( float3( p.x, -1, -p.y ) );
        }
        case 4: {
            return normalize( float3( p.x, -p.y, 1 ) );
        }
        default: {
            return normalize( float3( -p.x, -p.y, -1 ) );
        }
    }
}
