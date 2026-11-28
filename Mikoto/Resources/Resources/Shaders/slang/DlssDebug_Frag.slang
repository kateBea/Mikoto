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

#include "base/Base.slang"
#include "base/Memory_Helpers.slang"

struct Parameters {
    uint mTexture;
    uint mSampler;
    uint mMode;
};
struct Input {
    float2 mUv : TEXCOORD0;
};

MKT_DECLARE_TEXTURE_SRV( Texture2D<float4> ) textures;
MKT_DECLARE_SAMPLERS samplers;
MKT_DECLARE_PUSH_CONSTANT( Parameters, parameters );

static const uint DLSS_NOISY_COLOR = 0;
static const uint DLSS_NORMAL_ROUGHNESS = 3;
static const uint DLSS_DEPTH = 4;
static const uint DLSS_MOTION = 5;
static const float MOTION_PREVIEW_RANGE_PIXELS = 32.0;

[shader( "fragment" )]
float4 main( Input input ) : SV_Target0 {
    float4 value = textures[parameters.mTexture].SampleLevel( samplers[parameters.mSampler], input.mUv, 0 );
    float3 color = value.rgb;
    if ( parameters.mMode == DLSS_NORMAL_ROUGHNESS ) {
        color = value.xyz * .5 + .5;
    }
    else if ( parameters.mMode == DLSS_DEPTH ) {
        color = float3( pow( saturate( 1.0 - value.x ), .2 ) );
    }
    else if ( parameters.mMode == DLSS_MOTION ) {
        color = float3( .5 + value.xy / MOTION_PREVIEW_RANGE_PIXELS, .5 );
    }
    else {
        if ( parameters.mMode == DLSS_NOISY_COLOR ) {
            color = max( color, 0 ) / ( 1.0 + max( color, 0 ) );
        }
        color = pow( max( color, 0 ), float3( 1.0 / 2.2 ) );
    }
    return float4( color, 1 );
}
