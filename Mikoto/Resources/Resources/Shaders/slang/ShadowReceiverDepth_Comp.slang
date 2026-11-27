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

// Initialize directional-shadow receiver data when grass contacts are unavailable. RGBA
// contains opaque depth, neutral ground/grass visibility and the same opaque depth for the
// grass slot.

#include "base/Memory_Helpers.slang"

struct ReceiverDepthParameters {
    uint mDepth;
    uint mOutput;
};
MKT_DECLARE_PUSH_CONSTANT( ReceiverDepthParameters, parameters );
MKT_DECLARE_TEXTURE_SRV( Texture2D<float4> ) depths;
MKT_DECLARE_TEXTURE_UAV( RWTexture2D<float4> ) receivers;

// The fallback has no grass occlusion. Keeping R as hardware depth preserves
// existing filtered-shadow receiver tests; neutral G/B make contact optional.
[shader( "compute" )]
[numthreads( kImageGroupWidth, kImageGroupHeight, 1 )]
void main( uint3 thread : SV_DispatchThreadID ) {
    uint width;
    uint height;
    receivers[parameters.mOutput].GetDimensions( width, height );
    if ( any( thread.xy >= uint2( width, height ) ) ) {
        return;
    }
    float depth = depths[parameters.mDepth].Load( int3( thread.xy, 0 ) ).r;
    receivers[parameters.mOutput][thread.xy] = float4( depth, 1, 1, depth );
}
