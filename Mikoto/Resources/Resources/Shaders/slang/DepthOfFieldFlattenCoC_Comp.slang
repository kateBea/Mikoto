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

// Reduce each workgroup tile into foreground/background CoC bounds. These bounds allow later
// gathers to choose a blur radius without rescanning every source pixel.

#include "base/DepthOfField_Helpers.slang"

MKT_DECLARE_PUSH_CONSTANT( DepthOfFieldParameters, parameters );
groupshared float4 tileBounds[kDoFTileLaneCount];

[shader( "compute" )]
[numthreads( kImageGroupWidth, kImageGroupHeight, 1 )]
void main( uint3 group : SV_GroupID, uint3 local : SV_GroupThreadID ) {
    uint index = local.y * kDoFTileSize + local.x;
    float coc = DoFLoad( parameters.mInputs[0], int2( group.xy * kDoFTileSize + local.xy ) ).a;
    // x/y: signed foreground min/max; z/w: background min/max.
    // Include zero for pixels outside a layer. Otherwise a foreground/in-focus
    // edge could be mistaken for uniform foreground and use an unsafe coarse mip.
    tileBounds[index] = float4( min( coc, 0.0 ), min( coc, 0.0 ), max( coc, 0.0 ), max( coc, 0.0 ) );
    // All lanes publish their local bounds before the shared-memory reduction. Every
    // participating lane must reach each barrier before the next reduction step reads its
    // neighbors.
    GroupMemoryBarrierWithGroupSync();
    for ( uint stride = kDoFTileLaneCount / 2; stride > 0; stride >>= 1 ) {
        if ( index < stride ) {
            float4 a = tileBounds[index];
            float4 b = tileBounds[index + stride];
            tileBounds[index] = float4( min( a.x, b.x ), max( a.y, b.y ), min( a.z, b.z ), max( a.w, b.w ) );
        }
        GroupMemoryBarrierWithGroupSync();
    }
    if ( index == 0 ) {
        dofOutputs[parameters.mOutputs[0]][group.xy] = tileBounds[0];
    }
}
