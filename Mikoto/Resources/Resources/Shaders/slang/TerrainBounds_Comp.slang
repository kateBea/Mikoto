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

// Reduce terrain vertices into height bounds used by terrain acceleration. The configured phase
// selects the intermediate or final reduction stage.

#include "base/TerrainSampling_Helpers.slang"
// CPU reduction dispatch uses 256 groups per terrain; each scratch/result record stores lower and upper float4 bounds.
static const uint kTerrainBoundsGroupSize = 256;
static const uint kTerrainBoundsGroupsPerTerrain = 256;
static const uint kTerrainBoundsRecordStride = 2 * sizeof( float4 );

struct Parameters {
    Pointer mVertices;
    Pointer mRecords;
    Pointer mScratch;
    Pointer mBounds;
    uint mPhase;
};
MKT_DECLARE_PUSH_CONSTANT( Parameters, parameters );
groupshared float3 lower[kTerrainBoundsGroupSize];
groupshared float3 upper[kTerrainBoundsGroupSize];

[shader( "compute" )]
[numthreads( kTerrainBoundsGroupSize, 1, 1 )]
void main( uint3 group : SV_GroupID, uint lane : SV_GroupIndex ) {
    uint terrainIndex = parameters.mPhase == 0 ? group.y : group.x;
    TerrainSamplingRecord terrain = LoadBufferValue<TerrainSamplingRecord>( parameters.mRecords, sizeof( uint4 ) + terrainIndex * sizeof( TerrainSamplingRecord ) );
    float3 lo = 1e30;
    float3 hi = -1e30;
    // Phase zero reduces a strided set of terrain vertices into one scratch bound per
    // workgroup. Phase one reduces those scratch records into the terrain-wide bound.
    if ( parameters.mPhase == 0 ) {
        uint count = terrain.mGrid.x * terrain.mGrid.y;
        for ( uint i = group.x * kTerrainBoundsGroupSize + lane; i < count; i += kTerrainBoundsGroupSize * kTerrainBoundsGroupsPerTerrain ) {
            float3 point = LoadBufferValue<float4>( parameters.mVertices, ( terrain.mGrid.z + i ) * sizeof( float4 ) ).xyz;
            lo = min( lo, point );
            hi = max( hi, point );
        }
    }
    else {
        lo = LoadBufferValue<float4>( parameters.mScratch, ( terrainIndex * kTerrainBoundsGroupsPerTerrain + lane ) * kTerrainBoundsRecordStride ).xyz;
        hi = LoadBufferValue<float4>( parameters.mScratch, ( terrainIndex * kTerrainBoundsGroupsPerTerrain + lane ) * kTerrainBoundsRecordStride + sizeof( float4 ) ).xyz;
    }
    // Publish each lane minimum/maximum before the tree reduction. Lane zero writes the
    // completed lower/upper pair after all reduction barriers.
    lower[lane] = lo;
    upper[lane] = hi;
    GroupMemoryBarrierWithGroupSync();
    for ( uint stride = kTerrainBoundsGroupSize / 2; stride > 0; stride >>= 1 ) {
        if ( lane < stride ) {
            lower[lane] = min( lower[lane], lower[lane + stride] );
            upper[lane] = max( upper[lane], upper[lane + stride] );
        }
        GroupMemoryBarrierWithGroupSync();
    }
    if ( lane == 0 ) {
        Pointer output = parameters.mPhase == 0 ? parameters.mScratch : parameters.mBounds;
        uint offset = parameters.mPhase == 0 ? ( terrainIndex * kTerrainBoundsGroupsPerTerrain + group.x ) * kTerrainBoundsRecordStride : terrainIndex * kTerrainBoundsRecordStride;
        StoreBufferValue<float4>( output, offset, float4( lower[0], 0 ) );
        StoreBufferValue<float4>( output, offset + sizeof( float4 ), float4( upper[0], 0 ) );
    }
}
