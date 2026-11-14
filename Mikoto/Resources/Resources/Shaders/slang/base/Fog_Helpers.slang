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

#include "BufferReference.slang"
#include "Camera_Helpers.slang"

// Integrated scattering/transmission sampling follows Godot's camera volume;
// interpolation within a homogeneous cell uses PBRT's Beer transmittance.
// https://github.com/godotengine/godot/blob/master/servers/rendering/renderer_rd/shaders/environment/volumetric_fog_process.glsl
// https://pbr-book.org/4ed/Volume_Scattering/Transmittance

struct FogFrameData {
    float4x4 PreviousViewProjection;
    float4x4 PreviousInverseViewProjection;
    float4 PreviousCamera;
    uint4 Grid;
    float4 Range;
    uint4 Frame;
    uint4 Textures;
};

float4 ReadFogSlice( Texture2D<float4> texture, SamplerState sampler, float2 uv, uint slice, uint4 grid ) {
    float2 pixel = clamp( uv * float2( grid.xy ), 0.5, float2( grid.xy ) - 0.5 );
    pixel += float2( slice % grid.w, slice / grid.w ) * float2( grid.xy );
    return texture.SampleLevel( sampler, pixel / float2( grid.xy * grid.w ), 0.0 );
}

void FogCameraRay( float4x4 inverseViewProjection, float3 camera, bool orthographic, float2 uv,
                   out float3 origin, out float3 direction ) {
    float2 ndc = float2( uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0 );
    float4 nearPoint = mul( inverseViewProjection, float4( ndc, 0.0, 1.0 ) );
    float4 farPoint = mul( inverseViewProjection, float4( ndc, 1.0, 1.0 ) );
    origin = orthographic ? nearPoint.xyz / nearPoint.w : camera;
    direction = normalize( farPoint.xyz / farPoint.w - origin );
}

float FogSliceDistance( float slice, FogFrameData frame ) {
    float t = slice / float( frame.Grid.z );
    return frame.Range.y * t * t;
}

// Interpolate in physical distance with Beer transmission within the final cell.
// This also handles camera-near fragments without linearly interpolating opacity.
void ReadIntegratedFog( Texture2D<float4> radianceImage, Texture2D<float4> transmittanceImage,
                        SamplerState sampler, float2 uv, float distance, FogFrameData frame, out float3 radiance, out float3 transmittance ) {
    distance = clamp( distance, 0.0, frame.Range.y );
    uint cell = min( uint( sqrt( distance / frame.Range.y ) * frame.Grid.z ), frame.Grid.z - 1 );
    float start = FogSliceDistance( cell, frame );
    float end = FogSliceDistance( cell + 1, frame );
    float fraction = saturate( ( distance - start ) / ( end - start ) );
    float3 a = cell == 0 ? float3( 0.0 ) : ReadFogSlice( radianceImage, sampler, uv, cell - 1, frame.Grid ).rgb;
    float3 ta = cell == 0 ? float3( 1.0 ) : ReadFogSlice( transmittanceImage, sampler, uv, cell - 1, frame.Grid ).rgb;
    float3 b = ReadFogSlice( radianceImage, sampler, uv, cell, frame.Grid ).rgb;
    float3 tb = ReadFogSlice( transmittanceImage, sampler, uv, cell, frame.Grid ).rgb;
    float3 ratio = saturate( tb / max( ta, 1e-30 ) );
    float3 partial = fraction <= 0.0 ? float3( 1.0 ) : pow( ratio, fraction );
    radiance = a;
    for ( uint channel = 0; channel < 3; ++channel ) {
        float factor = ratio[channel] > 0.9999 ? fraction : ( 1.0 - partial[channel] ) / ( 1.0 - ratio[channel] );
        radiance[channel] += ( b[channel] - a[channel] ) * factor;
    }
    transmittance = ta * partial;
}
