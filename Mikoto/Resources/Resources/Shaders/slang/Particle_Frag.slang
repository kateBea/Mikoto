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

// Evaluate particle texture coverage, soft intersection fading and emissive tint. The output
// follows the configured particle blending path.

#pragma once

#include "base/Particle.slang"

struct DrawParams {
    Pointer mParticles;
    Pointer mCamera;
    uint mSampler;
};

MKT_DECLARE_PUSH_CONSTANT( DrawParams, parameters );
MKT_DECLARE_TEXTURE_SRV( Texture2D )
textures;
MKT_DECLARE_SAMPLERS samplers;

struct FSInput {
    float4 mPosition : SV_Position;
    float2 mLocal : TEXCOORD0;
    float4 mColor : COLOR0;
    nointerpolation float mSoftness : TEXCOORD1;
    nointerpolation uint mTextureIndex : TEXCOORD2;
    nointerpolation float mBloomIntensity : TEXCOORD3;
};

struct ParticleOutput {
    float4 mColor : SV_Target0;
    float4 mEmission : SV_Target1;
};

[shader( "fragment" )]
ParticleOutput main( FSInput input ) {
    float4 color = input.mColor;
    if ( input.mTextureIndex != kInvalidParticleTextureIndex ) {
        float2 uv = input.mLocal * float2( 0.5f, -0.5f ) + 0.5f;
        const uint index = NonUniformResourceIndex( input.mTextureIndex );
        color *= textures[index].Sample( samplers[parameters.mSampler], uv );
    }
    else {
        // Emitters without a texture keep the soft procedural billboard.
        float radius = length( input.mLocal );
        float feather = max( input.mSoftness, fwidth( radius ) );
        color.a *= 1.0f - smoothstep( max( 0.0f, 1.0f - feather ), 1.0f, radius );
    }

    if ( color.a <= 0.001f ) {
        discard;
    }

    // The RHI uses SRC_ALPHA / ONE_MINUS_SRC_ALPHA: output straight alpha.
    ParticleOutput output;
    // Feed the existing HDR bloom chain. The same emission also brightens the
    // visible core, so whole-image postprocess bloom sees the same radiance.
    // Alpha is shared with the billboard/fade mask and blended exactly once.
    output.mColor = float4( min( color.rgb * max( 1, input.mBloomIntensity ), 60000 ), color.a );
    output.mEmission = float4( min( color.rgb * max( input.mBloomIntensity, 0 ), 60000 ), color.a );
    return output;
}
