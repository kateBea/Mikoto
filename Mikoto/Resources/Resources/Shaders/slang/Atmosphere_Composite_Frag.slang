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

#include "Atmosphere_Common.slang"

float3 AtmosVolume( uint textureID, float2 uv, float slice ) {
    float low = floor( slice );
    float high = min( low + 1, 31 );

    // Clamp inside each tile before sampling. Otherwise bilinear filtering
    // bleeds a tile's last row into the next depth slice's first row.
    float2 xy = clamp( uv * 32, float2( 0.5 ), float2( 31.5 ) );
    float3 a = AtmosRead( textureID, float2( xy.x / 32, ( xy.y + low * 32 ) / 1024 ) ).rgb;
    float3 b = AtmosRead( textureID, float2( xy.x / 32, ( xy.y + high * 32 ) / 1024 ) ).rgb;
    return lerp( a, b, frac( slice ) );
}

[shader( "fragment" )]
float4 main( float2 uv : TEXCOORD0 ) : SV_Target0 {
    float4 color = AtmosRead( parameters.mSource, uv );
    if ( parameters.mStage == 5 ) {
        return color;
    }

    AtmosphereData a = Atmosphere();
    CameraInfo camera = ( ( BufferReference<CameraInfo> )parameters.mCamera )[0];
    float3 observer = AtmosClampGround( a, AtmosPlanetPosition( a, camera.mViewPosition.xyz ) );
    float3 origin, direction;
    AtmosCameraRay( camera, uv, a, origin, direction );

    if ( parameters.mStage == 7 ) {
        if ( a.mSunIlluminance.w < 0.5 ) {
            discard;
        }
        float angle = acos( clamp( dot( direction, a.mSunDirection.xyz ), -1, 1 ) );
        float edge = max( fwidth( angle ), 1e-5 );
        float disk = 1 - smoothstep( a.mSunDirection.w - edge, a.mSunDirection.w + edge, angle );
        if ( disk <= 0 ) {
            discard;
        }
        float3 sky = AtmosRead( parameters.mSkyView, AtmosSkyUv( a, observer, direction ) ).rgb;
        float solidAngle = AtmosPi * pow( sin( a.mSunDirection.w ), 2 );
        sky += disk * a.mSunIlluminance.xyz / max( solidAngle, 1e-6 ) * AtmosSunTransmittance( a, origin, direction );
        return float4( min( sky, float3( 60000 ) ), color.a );
    }
    uint width, height;
    textures[parameters.mDepth].GetDimensions( width, height );
    int2 pixel = clamp( int2( uv * float2( width, height ) ), int2( 0 ), int2( width, height ) - 1 );
    float depth = textures[parameters.mDepth].Load( int3( pixel, 0 ) ).r;

    if ( depth >= 1 ) {
        if ( a.mSunIlluminance.w < 0.5 ) {
            return color;
        }

        float3 sky = AtmosRead( parameters.mSkyView, AtmosSkyUv( a, observer, direction ) ).rgb;
        return float4( min( sky, float3( 60000 ) ), color.a );
    }

    if ( a.mView.y < 0.5 ) {
        return color;
    }

    float2 ndc = float2( uv.x * 2 - 1, 1 - uv.y * 2 );
    float4 world = mul( camera.mInverseViewProjection, float4( ndc, depth, 1 ) );
    world /= world.w;

    float3 rayStart = camera.mViewPosition.xyz;
    if ( a.mView.z > 0.5 ) {
        float4 nearPoint = mul( camera.mInverseViewProjection, float4( ndc, 0, 1 ) );
        rayStart = nearPoint.xyz / nearPoint.w;
    }

    float distance = length( world.xyz - rayStart ) * a.mRadii.z;
    float entry = AtmosEntryDistance( a, origin, direction );
    if ( entry < 0 || distance <= entry ) {
        return color;
    }

    float slice = sqrt( saturate( ( distance - entry ) / a.mGround.w ) ) * 31;
    float3 radiance = AtmosVolume( parameters.mAerialRadiance, uv, slice );
    float3 transmittance = AtmosVolume( parameters.mAerialTransmittance, uv, slice );

    // The first interval is linear in physical distance, not sqrt(distance).
    // This prevents excessive fog immediately in front of the camera.
    if ( slice < 1 ) {
        radiance *= slice;
        transmittance = lerp( float3( 1 ), transmittance, slice );
    }

    return float4( color.rgb * transmittance + radiance, color.a );
}
