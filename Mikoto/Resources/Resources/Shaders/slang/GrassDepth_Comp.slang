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

// Build the grass occlusion depth hierarchy from scene depth. Conservative reductions let
// placement reject a grass bound only when the covered region is occluded.

#include "base/Memory_Helpers.slang"

MKT_DECLARE_STRUCTURED_UAV grassPyramid;
MKT_DECLARE_TEXTURE_SRV( Texture2D<float> ) grassDepth;

struct Parameters {
    uint mDepth;
    uint mOutput;
    uint mWidth;
    uint mHeight;
    uint mSourceWidth;
    uint mSourceHeight;
    uint mSourceOffset;
    uint mOutputOffset;
    uint mFirstLevel;
};

MKT_DECLARE_PUSH_CONSTANT( Parameters, parameters );

[shader( "compute" )]
[numthreads( kImageGroupWidth, kImageGroupHeight, 1 )]
void main( uint3 thread : SV_DispatchThreadID ) {
    if ( thread.x >= parameters.mWidth || thread.y >= parameters.mHeight ) {
        return;
    }
    
    float farthest = 0;
    RWByteAddressBuffer pyramid = grassPyramid[parameters.mOutput];

    for ( uint y = 0; y < kLinearFilterSide; ++y ) {
        for ( uint x = 0; x < kLinearFilterSide; ++x ) {
            uint2 source = min( thread.xy * 2 + uint2( x, y ), uint2( parameters.mSourceWidth, parameters.mSourceHeight ) - 1 );
            float depth = parameters.mFirstLevel != 0 ? 
                grassDepth[parameters.mDepth].Load( int3( source, 0 ) ) :
                asfloat( pyramid.Load( ( parameters.mSourceOffset + source.y * parameters.mSourceWidth + source.x ) * 4 ) );
            farthest = max( farthest, depth );
        }
    }

    pyramid.Store( ( parameters.mOutputOffset + thread.y * parameters.mWidth + thread.x ) * 4, asuint( farthest ) );
}
