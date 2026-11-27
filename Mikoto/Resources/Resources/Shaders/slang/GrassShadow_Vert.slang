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

// Raise the terrain mesh into a grass-canopy envelope in the light view. The fragment stage
// turns this inexpensive proxy into a distribution of grass blocker depths.

#include "base/GrassShadow_Helpers.slang"

// Only the terrain grid is rasterized into each directional cascade. Raising
// it to the canopy envelope replaces drawing thousands of individual blades.
[shader( "vertex" )]
GrassShadowVaryings main( uint vertex : SV_VertexID ) {
    GrassDescription source = LoadBufferValue<GrassDescription>( parameters.mDescriptions,
        parameters.mDescription * sizeof( GrassDescription ) );

    // Expand each terrain quad into two triangles. SV_VertexID selects the quad and then one of
    // its six triangle-list vertices; the corner code resolves the terrain-grid X/Y coordinate.
    const uint corners[kVerticesPerQuad] = { 0, 2, 1, 1, 2, 3 };
    uint quad = vertex / kVerticesPerQuad;
    uint2 cell = uint2( quad % ( source.mGrid.x - 1 ), quad / ( source.mGrid.x - 1 ) );
    uint corner = corners[vertex % kVerticesPerQuad];
    uint2 grid = cell + uint2( corner & 1, corner / 2 );

    // Fetch the matching terrain height and transform the base into world space. The vertical
    // transform length converts the authored blade height into a world-space canopy height.
    float4 point = LoadBufferValue<float4>( parameters.mVertices,
        ( source.mGrid.z + grid.y * source.mGrid.x + grid.x ) * sizeof( float4 ) );
    float3 base = mul( source.mWorld, float4( point.xyz, 1 ) ).xyz;
    float scale = length( mul( source.mWorld, float4( 0, 1, 0, 0 ) ).xyz );

    // Use low-frequency clump noise to vary the canopy envelope. The height multiplier leaves
    // room for bent/tall blades while avoiding a flat proxy roof.
    float variation = lerp( source.mBudgets.w, 1,
        saturate( GrassFlowFbm( point.xz / max( source.mShape.w * kGrassCanopyNoiseScale,
            kGrassCanopyMinimumNoiseScale ) ) * .5 + .5 ) );
    float height = source.mShape.x * scale * kGrassCanopyHeightScale * variation;
    float3 canopy = base + float3( 0, height, 0 );

    // Select this light record and cascade matrix. Project the canopy envelope into shadow-map
    // depth; the fragment stage replaces it with a sampled blocker height below the envelope.
    float4x4 matrix = LoadBufferValue<float4x4>( parameters.mRecords,
        parameters.mRecord * sizeof( ShadowRecord ) + parameters.mFace * sizeof( float4x4 ) );

    // Retain base, canopy and terrain coordinates for placement masks and depth dithering in
    // the fragment stage.
    GrassShadowVaryings output;
    output.mBase = base;
    output.mCanopy = canopy;
    output.mLocal = point.xyz;
    output.mGridHeight = float3( float2( grid ) / float2( source.mGrid.xy - 1 ), point.w );
    output.mPosition = mul( matrix, float4( canopy, 1 ) );
    return output;
}

