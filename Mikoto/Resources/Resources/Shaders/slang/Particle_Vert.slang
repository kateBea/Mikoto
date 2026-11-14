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

#include "base/Camera_Helpers.slang"
#include "base/Memory_Helpers.slang"
#include "base/Particle.slang"

struct DrawParams {
    Pointer mParticles;
    Pointer mCamera;
};

struct VSOutput {
    float4 Position : SV_Position;
    float2 Local : TEXCOORD0;
    float4 Color : COLOR0;
    nointerpolation float Softness : TEXCOORD1;
};

MKT_DECLARE_PUSH_CONSTANT( DrawParams, parameters );

[shader( "vertex" )]
VSOutput main( uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID ) {
    static const float2 corners[6] = { float2( -1, -1 ), float2( 1, -1 ), float2( 1, 1 ),
                                       float2( -1, -1 ), float2( 1, 1 ),  float2( -1, 1 ) };
    VSOutput o;

    float s, c;
    float2 local = corners[vertexID];
    BufferReference<CameraInfo> camera = ( BufferReference<CameraInfo> )parameters.mCamera;
    ParticleRenderData p = ( ( BufferReference<ParticleRenderData> )parameters.mParticles )[instanceID];

    // GPU simulation draws its bounded slot range without a CPU count readback.
    // Dead slots must not project through the camera origin (w == 0).
    if ( p.mPositionSize.w <= 0 || p.mColor.a <= 0 ) {
        o.Position = float4( 0, 0, 0, 1 );
        o.Local = 0;
        o.Color = 0;
        o.Softness = 1;
        return o;
    }

    sincos( p.mRotationSoftness.x, s, c );
    float2 rotated = float2( c * local.x - s * local.y, s * local.x + c * local.y );

    // Offset in view space: billboards face either a perspective or orthographic
    // camera without an inverse-view basis or a vertex buffer per emitter.
    float4 viewPosition = mul( camera[0].mViewMatrix, float4( p.mPositionSize.xyz, 1 ) );
    viewPosition.xy += rotated * ( p.mPositionSize.w * 0.5f );

    o.Position = mul( camera[0].mProjection, viewPosition );
    o.Local = local;
    o.Color = p.mColor;
    o.Softness = p.mRotationSoftness.y;

    return o;
}
