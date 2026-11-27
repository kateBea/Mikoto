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

// Convolve the environment with GGX directions for the requested roughness. Each destination
// mip represents a progressively broader specular reflection footprint.

#pragma once

#include "base/Base.slang"
#include "base/Memory_Helpers.slang"

struct DrawParams {
    PushConstantMatrix4x4 mMVP;
    float mRoughness;
    uint mNumSamples;

    uint mCubeID;
    uint mSamplerID;

    uint mVertexBufferID;
    uint mIndexBufferID;
};

struct FSInput {
    float3 mPos : TEXCOORD0;
};

struct FSOutput {
    float4 mColor : SV_Target0;
};

MKT_DECLARE_PUSH_CONSTANT( DrawParams, parameters );

MKT_DECLARE_TEXTURE_SRV( TextureCube ) textures;
MKT_DECLARE_SAMPLERS samplers;

#include "base/CubemapPrefilter_Helpers.slang"

[shader( "fragment" )]
FSOutput main( FSInput input ) {
    FSOutput o;

    float3 N = normalize( input.mPos );
    float3 result = PrefilterEnvMap( textures[parameters.mCubeID], samplers[parameters.mSamplerID], N, parameters.mRoughness, parameters.mNumSamples );

    o.mColor = float4( result, 1.0 );
    return o;
}
