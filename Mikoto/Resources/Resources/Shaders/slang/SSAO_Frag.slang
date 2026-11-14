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

#include "base/Base.slang"
#include "base/Camera_Helpers.slang"
#include "base/Memory_Helpers.slang"

struct DrawParams {
    uint mSamplerID;
    uint mKernelBufferID;
    uint mNoiseTextureID;
    uint mGBufferNormalsID;
    uint mGBufferPositionsID;
    uint mCameraBufferID;
};

MKT_DECLARE_SAMPLERS samplers;
MKT_DECLARE_STRUCTURED_SRV buffers;
MKT_DECLARE_TEXTURE_SRV( Texture2D ) textures;

MKT_DECLARE_PUSH_CONSTANT( DrawParams, parameters );

[shader( "fragment" )]
float main( float2 uv : TEXCOORD0 ) : SV_Target0 {
    CameraInfo camera = buffers[parameters.mCameraBufferID].Read<CameraInfo>( 0, 0 );

    uint width, height;
    textures[parameters.mGBufferPositionsID].GetDimensions( width, height );

    int2 pixel = clamp( int2( uv * float2( width, height ) ), int2( 0 ), int2( width, height ) - 1 );
    float4 position = textures[parameters.mGBufferPositionsID].Load( int3( pixel, 0 ) );

    if ( position.z >= -1e-5 ) {
        return 1.0;
    }

    float3 encodedNormal = textures[parameters.mGBufferNormalsID].Load( int3( pixel, 0 ) ).xyz;
    float3 normal = normalize( mul( ( float3x3 )camera.mViewMatrix, encodedNormal * 2 - 1 ) );

    uint noiseWidth, noiseHeight;
    textures[parameters.mNoiseTextureID].GetDimensions( noiseWidth, noiseHeight );

    // Noise is already signed. Wrap only noise, never projected scene samples.
    float3 randomVector = textures[parameters.mNoiseTextureID]
        .Load( int3( pixel % int2( noiseWidth, noiseHeight ), 0 ) ).xyz;
    float3 tangent = randomVector - normal * dot( randomVector, normal );

    if ( dot( tangent, tangent ) < 1e-6 ) {
        tangent = cross( abs( normal.z ) < 0.99 ? float3( 0, 0, 1 ) : float3( 0, 1, 0 ), normal );
    }

    tangent = normalize( tangent );

    float occlusion = 0;
    const float radius = 0.5;
    const float bias = 0.025;
    float3 bitangent = cross( normal, tangent );

    for ( uint i = 0; i < 64; ++i ) {
        float3 kernel = buffers[parameters.mKernelBufferID].Read<float4>( 0, i ).xyz;

        // Explicit basis combination avoids transposing the tangent frame.
        float3 samplePosition = position.xyz + radius * ( kernel.x * tangent + kernel.y * bitangent + kernel.z * normal );
        float4 projected = mul( camera.mProjection, float4( samplePosition, 1 ) );
        if ( projected.w <= 0 ) {
            continue;
        }

        float2 sampleUv = projected.xy / projected.w * float2( 0.5, -0.5 ) + 0.5;
        if ( any( sampleUv < 0 ) || any( sampleUv >= 1 ) ) {
            continue;
        }

        float sampleDepth = textures[parameters.mGBufferPositionsID]
            .Load( int3( int2( sampleUv * float2( width, height ) ), 0 ) )
                                    .z;
        if ( sampleDepth >= -1e-5 ) {
            continue;
        }

        float rangeWeight = smoothstep( 0, 1, radius / max( abs( position.z - sampleDepth ), 1e-5 ) );
        occlusion += ( sampleDepth >= samplePosition.z + bias ? 1.0 : 0.0 ) * rangeWeight;
    }

    return saturate( 1 - occlusion / 64.0 );
}
