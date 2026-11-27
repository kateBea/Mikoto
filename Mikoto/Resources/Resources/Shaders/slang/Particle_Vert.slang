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

// Expand each visible particle record into a camera-facing billboard. Per-particle rotation,
// size, tint and texture data are passed to the fragment stage.

#pragma once

#include "base/Camera_Helpers.slang"
#include "base/Memory_Helpers.slang"
#include "base/Particle.slang"

struct DrawParams {
    Pointer mParticles;
    Pointer mCamera;
    uint mSampler;
};

struct VSOutput {
    float4 mPosition : SV_Position;
    float2 mLocal : TEXCOORD0;
    float4 mColor : COLOR0;
    nointerpolation float mSoftness : TEXCOORD1;
    nointerpolation uint mTextureIndex : TEXCOORD2;
    nointerpolation float mBloomIntensity : TEXCOORD3;
};

MKT_DECLARE_PUSH_CONSTANT( DrawParams, parameters );

[shader( "vertex" )]
VSOutput main( uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID ) {
    static const float2 corners[kVerticesPerQuad] = { float2( -1, -1 ), float2( 1, -1 ), float2( 1, 1 ),
        float2( -1, -1 ), float2( 1, 1 ), float2( -1, 1 ) };
    VSOutput o;

    float s;
    float c;
    float2 local = corners[vertexID];
    BufferReference<CameraInfo> camera = ( BufferReference<CameraInfo> )parameters.mCamera;
    ParticleRenderData p = ( ( BufferReference<ParticleRenderData> )parameters.mParticles )[instanceID];

    // GPU simulation draws its bounded slot range without a CPU count readback.
    // Dead slots must not project through the camera origin (w == 0).
    if ( p.mPositionSize.w <= 0 || p.mColor.a <= 0 ) {
        o.mPosition = float4( 0, 0, 0, 1 );
        o.mLocal = 0;
        o.mColor = 0;
        o.mSoftness = 1;
        o.mTextureIndex = kInvalidParticleTextureIndex;
        o.mBloomIntensity = 0;
        return o;
    }

    sincos( p.mRotationSoftness.x, s, c );
    float2 rotated = float2( c * local.x - s * local.y, s * local.x + c * local.y );

    // Offset in view space: billboards face either a perspective or orthographic
    // camera without an inverse-view basis or a vertex buffer per emitter.
    float4 viewPosition = mul( camera[0].mViewMatrix, float4( p.mPositionSize.xyz, 1 ) );
    viewPosition.xy += rotated * ( p.mPositionSize.w * 0.5f );

    o.mPosition = mul( camera[0].mProjection, viewPosition );
    o.mLocal = local;
    o.mColor = p.mColor;
    o.mSoftness = p.mRotationSoftness.y;
    o.mTextureIndex = p.mTexture.x;
    o.mBloomIntensity = asfloat( p.mTexture.y );

    return o;
}
