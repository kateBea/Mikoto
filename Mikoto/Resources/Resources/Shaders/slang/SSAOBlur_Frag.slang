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
#include "base/Memory_Helpers.slang"
struct DrawParams {
    uint mSamplerID;
    uint mSsaoColorTargetID;
    uint mPositionID;
};

MKT_DECLARE_PUSH_CONSTANT( DrawParams, parameters );
MKT_DECLARE_TEXTURE_SRV( Texture2D ) textures;

[shader( "fragment" )]
float main( float2 uv : TEXCOORD0 ) : SV_Target0 {
    uint width, height;
    textures[parameters.mSsaoColorTargetID].GetDimensions( width, height );

    int2 pixel = clamp( int2( uv * float2( width, height ) ), int2( 0 ), int2( width, height ) - 1 );
    float center = textures[parameters.mPositionID].Load( int3( pixel, 0 ) ).z;
    if ( center >= -1e-5 ) {
        return 1;
    }

    // Bilateral filtering removes noise without darkening unrelated depths
    // across silhouettes. Background pixels are excluded from the blur.
    float result = 0;
    float weights = 0;
    for ( int y = -2; y <= 2; ++y ) {
        for ( int x = -2; x <= 2; ++x ) {
            int2 samplePixel = clamp( pixel + int2( x, y ), int2( 0 ), int2( width, height ) - 1 );
            float depth = textures[parameters.mPositionID].Load( int3( samplePixel, 0 ) ).z;

            if ( depth >= -1e-5 ) {
                continue;
            }

            float weight = exp( -0.5 * ( x * x + y * y ) ) * exp( -abs( depth - center ) / 0.1 );
            result += weight * textures[parameters.mSsaoColorTargetID].Load( int3( samplePixel, 0 ) ).r;
            weights += weight;
        }
    }

    return result / max( weights, 1e-5 );
}
