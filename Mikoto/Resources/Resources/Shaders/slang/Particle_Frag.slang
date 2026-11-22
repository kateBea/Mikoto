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

#include "base/Particle.slang"

struct DrawParams {
    Pointer mParticles;
    Pointer mCamera;
    uint mSampler;
};

MKT_DECLARE_PUSH_CONSTANT( DrawParams, parameters );
MKT_DECLARE_TEXTURE_SRV( Texture2D ) textures;
MKT_DECLARE_SAMPLERS samplers;

struct FSInput {
    float4 Position : SV_Position;
    float2 Local : TEXCOORD0;
    float4 Color : COLOR0;
    nointerpolation float Softness : TEXCOORD1;
    nointerpolation uint TextureIndex : TEXCOORD2;
};

[shader( "fragment" )]
float4 main( FSInput input ) : SV_Target0 {
    float4 color = input.Color;
    if ( input.TextureIndex != kInvalidParticleTextureIndex ) {
        float2 uv = input.Local * float2( 0.5f, -0.5f ) + 0.5f;
        const uint index = NonUniformResourceIndex( input.TextureIndex );
        color *= textures[index].Sample( samplers[parameters.mSampler], uv );
    }
    else {
        // Emitters without a texture keep the soft procedural billboard.
        float radius = length( input.Local );
        float feather = max( input.Softness, fwidth( radius ) );
        color.a *= 1.0f - smoothstep( max( 0.0f, 1.0f - feather ), 1.0f, radius );
    }

    if ( color.a <= 0.001f ) {
        discard;
    }

    // The RHI uses SRC_ALPHA / ONE_MINUS_SRC_ALPHA: output straight alpha.
    return color;
}
