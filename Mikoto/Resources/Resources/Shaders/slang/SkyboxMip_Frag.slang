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

// Filter cubemap radiance into a smaller mip while reconstructing the destination face
// direction. The face mapping remains consistent across cubemap seams.

#pragma once

#include "base/Base.slang"
#include "base/Memory_Helpers.slang"

struct DrawParams {
    uint mCubeID;
    uint mSamplerID;
    uint mSourceMip;
    uint mFace;
    uint mSourceDimension;
    uint mDestinationDimension;
};

MKT_DECLARE_SAMPLERS samplers;
MKT_DECLARE_TEXTURE_SRV( TextureCube ) textures;
MKT_DECLARE_PUSH_CONSTANT( DrawParams, parameters );

float3 CubeDirection( uint face, float2 uv ) {
    float2 p = uv * 2.0 - 1.0;
    // Inverse of cube sampling's face-coordinate mapping: +X,-X,+Y,-Y,+Z,-Z.
    switch ( face ) {
        case 0:
            return float3( 1.0, -p.y, -p.x );
        case 1:
            return float3( -1.0, -p.y, p.x );
        case 2:
            return float3( p.x, 1.0, p.y );
        case 3:
            return float3( p.x, -1.0, -p.y );
        case 4:
            return float3( p.x, -p.y, 1.0 );
        default:
            return float3( -p.x, -p.y, -1.0 );
    }
}

[shader( "fragment" )]
float4 main( float2 unusedUV : TEXCOORD0, float4 position : SV_Position ) : SV_Target0 {
    // Area-average the previous mip in linear HDR. Fractional overlap matters:
    // the 2540-wide source eventually has odd-sized levels (e.g. 5 -> 2).
    // Four point samples would lose/overweight edge texels at those levels.
    float ratio = float( parameters.mSourceDimension ) / float( parameters.mDestinationDimension );
    float2 start = floor( position.xy ) * ratio;
    float2 end = min( start + ratio, float( parameters.mSourceDimension ) );
    int2 first = int2( floor( start ) );
    int2 last = min( int2( ceil( end ) ), int2( parameters.mSourceDimension ) );
    float3 color = 0.0;
    float totalWeight = 0.0;
    for ( int y = first.y; y < last.y; ++y ) {
        for ( int x = first.x; x < last.x; ++x ) {
            float2 overlap = max( 0.0, min( end, float2( x + 1, y + 1 ) ) - max( start, float2( x, y ) ) );
            float weight = overlap.x * overlap.y;
            float2 uv = ( float2( x, y ) + 0.5 ) / float( parameters.mSourceDimension );
            float3 direction = CubeDirection( parameters.mFace, uv );
            color += textures[parameters.mCubeID]
                             .SampleLevel( samplers[parameters.mSamplerID], direction, float( parameters.mSourceMip ) )
                             .rgb *
                     weight;
            totalWeight += weight;
        }
    }
    return float4( color / max( totalWeight, 1e-6 ), 1.0 );
}
