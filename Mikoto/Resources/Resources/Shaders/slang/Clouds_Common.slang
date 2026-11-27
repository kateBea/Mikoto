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

// Per-frame cloud bindings, camera-ray reconstruction and atmosphere-atlas sampling. These
// utilities establish the distance and texture conventions shared by tracing, history and
// composition.

#pragma once

#include "base/Atmosphere_Helpers.slang"
#include "base/Base.slang"
#include "base/Camera_Helpers.slang"
#include "base/Clouds_Helpers.slang"

struct CloudsParameters {
    Pointer mFrame;
    Pointer mModels;
    Pointer mCamera;
    uint mOutput;
    uint mMetadata;
    uint mInput;
    uint mInputMetadata;
    uint mHistory;
    uint mHistoryMetadata;
    uint mDepth;
    uint mVolume;
};

MKT_DECLARE_TEXTURE_SRV( Texture2D<float4> )
cloudImages;
MKT_DECLARE_TEXTURE_UAV( RWTexture2D<float4> )
cloudOutputs;
MKT_DECLARE_TEXTURE_UAV( RWTexture3D<float4> )
cloudVolumeOutputs;
MKT_DECLARE_PUSH_CONSTANT( CloudsParameters, parameters );

uint CloudHash( uint value ) {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    return value ^ ( value >> 16 );
}

float CloudRandom( uint value ) {
    return float( CloudHash( value ) & 0xffffffu ) / 16777216.0;
}

// Reconstruct a camera ray from inverse projection and camera rotation. A direction uses
// rotation only so camera translation cannot bend distant cloud rays.
void CloudCameraRay( CameraInfo camera, float2 uv, out float3 origin, out float3 direction ) {
    float2 clip = float2( uv.x * 2 - 1, 1 - uv.y * 2 );
    // Sub-metre points reconstructed in world space lose their direction when
    // the camera is kilometres from the origin. Keep this subtraction in view space.
    float4 a = mul( camera.mInverseProjection, float4( clip, .25, 1 ) );
    float4 b = mul( camera.mInverseProjection, float4( clip, .75, 1 ) );
    float3 pointA = a.xyz / a.w;
    float3 pointB = b.xyz / b.w;
    bool aIsNear = dot( pointA, pointA ) < dot( pointB, pointB );
    float3 nearPoint = aIsNear ? pointA : pointB;
    float3 farPoint = aIsNear ? pointB : pointA;
    bool orthographic = camera.mProjection[3][3] != 0;
    float3 viewDirection = normalize( orthographic ? farPoint - nearPoint : farPoint );
    float3x3 cameraToWorld = transpose( ( float3x3 )camera.mViewMatrix );
    direction = normalize( mul( cameraToWorld, viewDirection ) );
    float3 viewOrigin = orthographic ? nearPoint - viewDirection * dot( nearPoint, viewDirection ) : float3( 0 );
    origin = camera.mViewPosition.xyz + mul( cameraToWorld, viewOrigin );
}

// Convert opaque device depth into the ray distance used by cloud integration. Clear/background
// depth maps to the configured cloud tracing range.
float CloudSurfaceDistance( CloudsFrameData frame, CameraInfo camera, float2 uv, float depth ) {
    float4 nearClip = mul( camera.mProjection, float4( 0, 0, -camera.mPlanes.x, 1 ) );
    float farDepth = nearClip.z / nearClip.w > .5 ? 0 : 1;
    if ( abs( depth - farDepth ) < 1e-6 ) {
        return frame.mMarch.y;
    }
    float4 view = mul( camera.mInverseProjection, float4( uv.x * 2 - 1, 1 - uv.y * 2, depth, 1 ) );
    float3 point = view.xyz / view.w;
    float distance = camera.mProjection[3][3] != 0 ? abs( point.z ) : length( point );
    return min( frame.mMarch.y, max( 0, distance ) );
}

// Interpolate the two adjacent depth slices of the flattened atmosphere volume. Each slice
// occupies a separate vertical region of the atlas.
float3 CloudAerialVolume( uint index, float2 uv, float slice, SamplerState sourceSampler ) {
    float low = floor( slice );
    float high = min( low + 1, kAtmosAerialLastSlice );
    float2 xy = clamp( uv * kAtmosAerialSliceSize, .5, ( float( kAtmosAerialSliceSize ) - .5 ) );
    float3 a = cloudImages[index].SampleLevel( sourceSampler, float2( xy.x / kAtmosAerialSliceSize, ( xy.y + low * kAtmosAerialSliceSize ) / kAtmosAerialAtlasHeight ), 0 ).rgb;
    float3 b = cloudImages[index].SampleLevel( sourceSampler, float2( xy.x / kAtmosAerialSliceSize, ( xy.y + high * kAtmosAerialSliceSize ) / kAtmosAerialAtlasHeight ), 0 ).rgb;
    return lerp( a, b, frac( slice ) );
}
