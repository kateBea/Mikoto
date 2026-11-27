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

// Generate world-anchored grass roots from terrain tiles, select geometry detail and append
// indirect instances. The finalization dispatch clamps reserved draw counts to their allocated
// capacities.

#include "base/Grass_Helpers.slang"

MKT_DECLARE_STRUCTURED_UAV grassBuffers;

[shader( "compute" )]
[numthreads( kGrassGenerateGroupSize, 1, 1 )]
void main( uint3 thread : SV_DispatchThreadID ) {
    // One description represents a terrain/mesh choice and owns two geometry LOD
    // draws. Each 16-byte indirect command stores its instance count at +4.
    GrassDescription source = LoadBufferValue<GrassDescription>( parameters.mDescriptions, parameters.mTerrain * sizeof( GrassDescription ) );
    RWByteAddressBuffer arguments = grassBuffers[parameters.mArguments];
    uint argument = source.mRanges.z * kGrassDrawCommandStride;

    // A second dispatch finalizes counts after placement finishes. Reservation
    // counters may exceed capacity, but neither storage nor draw counts may do so.
    if ( parameters.mFinalize != 0 ) {
        if ( thread.x < kGrassLodCount ) {
            uint offset = argument + thread.x * kGrassDrawCommandStride + kGrassInstanceCountOffset;
            arguments.Store( offset, min( arguments.Load( offset ), source.mRanges.y ) );
        }
        return;
    }

    // Flatten the selected quadtree leaves into one dispatch. Every leaf has
    // the same candidate count, while its stride determines its world-space size.
    uint candidate = thread.x + thread.y * parameters.mDispatchWidth;
    uint tileIndex = candidate / kGrassTileCandidateCount;
    if ( tileIndex >= uint( source.mTiling.y ) ) {
        return;
    }
    GrassTile tile = LoadBufferValue<GrassTile>( source.mTiles, tileIndex * sizeof( GrassTile ) );
    uint cellIndex = candidate % kGrassTileCandidateCount;
    uint2 cell = uint2( cellIndex % kGrassTileSide, cellIndex / kGrassTileSide );

    // Reconstruct a canonical fine-grid coordinate in every tile level. Shared
    // roots retain identical seeds, jitter and clump offsets after subdivision.
    float fineSpacing = source.mTiling.x;
    int2 rootCell = tile.mCellOrigin + int2( cell ) * int( tile.mStride );
    float2 local = float2( rootCell ) * fineSpacing;
    uint seed = GrassCellHash( rootCell, source.mRanges.w );

    // Palette intervals are disjoint: each root chooses exactly one mesh from
    // the weighted mix, rather than spawning every model on the same root.
    float choice = GrassRandom( seed + 17 ) * source.mPalette.z;
    if ( choice < source.mPalette.x || choice >= source.mPalette.y ) {
        return;
    }

    // Stable fine-cell jitter hides rows. Low-frequency offsets gather roots
    // into clumps; every tile level uses the same offsets for a shared root.
    local += float2( GrassRandom( seed ), GrassRandom( seed + 1 ) ) * fineSpacing;
    float2 clump = local / source.mShape.w;
    local += ( float2( GrassFlowNoise( clump ), GrassFlowNoise( clump + 100 ) ) * 2 - 1 ) * source.mLimits.z;

    // Sample the generated terrain triangles rather than the source height image:
    // GPU noise, rectangular grids and live sculpt edits all share this surface.
    float4 first = LoadBufferValue<float4>( parameters.mVertices, source.mGrid.z * sizeof( float4 ) );
    float4 last = LoadBufferValue<float4>( parameters.mVertices, ( source.mGrid.z + source.mGrid.x - 1 ) * sizeof( float4 ) );
    float vertexSpacing = ( last.x - first.x ) / ( source.mGrid.x - 1 );
    float2 grid = ( local - first.xz ) / vertexSpacing;
    if ( any( grid < 0 ) || any( grid >= float2( source.mGrid.xy - 1 ) ) ) {
        return;
    }

    // Reconstruct the point on the same triangle diagonal used by the terrain
    // mesh, then transform its tangents to obtain a world-space surface normal.
    uint2 lo = uint2( grid );
    float2 f = frac( grid );
    uint base = source.mGrid.z + lo.y * source.mGrid.x + lo.x;
    float4 a = LoadBufferValue<float4>( parameters.mVertices, base * sizeof( float4 ) );
    float4 b = LoadBufferValue<float4>( parameters.mVertices, ( base + 1 ) * sizeof( float4 ) );
    float4 c = LoadBufferValue<float4>( parameters.mVertices, ( base + source.mGrid.x ) * sizeof( float4 ) );
    float4 d = LoadBufferValue<float4>( parameters.mVertices, ( base + source.mGrid.x + 1 ) * sizeof( float4 ) );
    float4 point = f.x + f.y <= 1 ? a + ( b - a ) * f.x + ( c - a ) * f.y : d + ( c - d ) * ( 1 - f.x ) + ( b - d ) * ( 1 - f.y );
    float3 tangent = mul( source.mWorld, float4( ( f.x + f.y <= 1 ? b - a : d - c ).xyz, 0 ) ).xyz;
    float3 bitangent = mul( source.mWorld, float4( ( f.x + f.y <= 1 ? c - a : d - b ).xyz, 0 ) ).xyz;
    float3 normal = normalize( cross( bitangent, tangent ) );
    float3 world = mul( source.mWorld, float4( point.xyz, 1 ) ).xyz;

    // Elevation/slope limits reject unsuitable ground. Painted/procedural layer
    // weights then thin the roots so grass follows the selected surface layer.
    if ( normal.y < source.mLimits.w || world.y < source.mLimits.x || world.y > source.mLimits.y ) {
        return;
    }

    TerrainSurfaceData surface = LoadBufferValue<TerrainSurfaceData>( source.mSurface, 0 );
    float4 weights = TerrainWeights( surface, grid / float2( source.mGrid.xy - 1 ), point.w, normal.y );
    if ( GrassRandom( seed + 2 ) > weights[source.mControl.x] ) {
        return;
    }

    // All tiers evaluate distance at the identical reconstructed root. The view
    // distance is a hard upper bound; the final 15% fades out below that bound.
    CameraInfo camera = LoadBufferValue<CameraInfo>( parameters.mCamera, 0 );
    float distance = length( world - camera.mViewPosition.xyz );
    if ( distance >= source.mView.x ) {
        return;
    }

    // A smooth density target is independent of tile boundaries. Hierarchical
    // ranks ensure a coarser leaf contains every root that could pass this test;
    // leaf changes cannot introduce holes or a new ring of surviving roots.
    float ratio = source.mDetail.y / max( distance, source.mDetail.y );
    float fade = 1 - smoothstep( source.mView.x * .85, source.mView.x, distance );
    float thinning = lerp( 1, source.mDetail.x, smoothstep( source.mView.y, source.mView.x, distance ) );
    float density = ratio * ratio * thinning * source.mBudgets.x;
    if ( GrassRootRank( rootCell, seed ) >= fade * density ) {
        return;
    }

    // Geometry changes only after the high strip has collapsed to the low
    // strip's shape. Both draws use exactly the same roots and density budget.
    float lodEnd = max( source.mView.z, source.mView.y + .001 );
    uint lod = distance < lodEnd ? 0 : 1;

    // Clump-scale height variation preserves patches of similar height while a
    // smaller per-root variation avoids identical silhouettes within each patch.
    float scale = length( mul( source.mWorld, float4( 0, 1, 0, 0 ) ).xyz );
    float heightVariation = lerp( source.mBudgets.w, 1, saturate( GrassFlowFbm( local / max( source.mShape.w * 3, .1 ) ) * .5 + .5 ) );
    float height = source.mShape.x * scale * lerp( .8, 1.2, GrassRandom( seed + 5 ) ) * heightVariation;

    // Cull a conservative bound covering height, bend and width compensation.
    // Current-frame Hi-Z contains terrain/opaque geometry, never grass itself.
    float radius = max( height * 1.6, source.mShape.y * scale * 4 );
    float4 clip = CameraClipPosition( float4( world, 1 ), camera );
    float margin = radius * 3 + .05 * abs( clip.w );
    if ( clip.w < -margin || abs( clip.x ) > clip.w + margin || abs( clip.y ) > clip.w + margin || clip.z < -margin || clip.z > clip.w + margin ) {
        return;
    }
    if ( source.mControl.z != 0 && GrassOccluded( camera, world, radius ) ) {
        return;
    }

    // Compact survivors into this LOD's fixed buffer slice. Overflow reservations
    // skip the write; the finalize dispatch clamps the indirect instance count.
    uint slot;
    arguments.InterlockedAdd( argument + lod * kGrassDrawCommandStride + kGrassInstanceCountOffset, 1, slot );
    if ( slot >= source.mRanges.y ) {
        return;
    }

    float facing = GrassRandom( seed + 4 ) * 6.2831853;
    // Width follows the continuous fine-grid density target, independently of
    // the tile stride and selected geometry draw.
    float compensation = lerp( 1, min( 4, rcp( max( density, .01 ) ) ), source.mLighting.w );

    // Vertex shading consumes this compact record: world root/height, facing,
    // compensated width, bend, surface normal, colour variation and wind.
    GrassBlade blade;
    blade.mRootHeight = float4( world - normal * .015 * scale, height );
    blade.mFacingWidthBend = float4( cos( facing ), sin( facing ), source.mShape.y * scale * compensation, source.mShape.z );
    blade.mNormalColor = float4( normal, GrassFlowHash21( world.xz * .3 ) );

    // Advect broad fronts along the prevailing wind. Cross-wind domain
    // warping and a slower gust envelope break up periodic, synchronized waves.
    // Per-blade stiffness changes the response, not the phase of the field wave.
    float3 field = SampleWindField( source.mWindField, world );
    float2 velocity = field.xz + source.mWind.xy * source.mWind.z;
    float speed = length( velocity );
    float2 windDirection = speed > .001 ? velocity / speed : blade.mFacingWidthBend.xy;

    // Local gusts can change the bend direction/strength, but not the advection
    // clock. Multiplying time by instantaneous gust velocity makes fronts jump.
    float2 prevailing = source.mWind.xy * source.mWind.z;
    float advectionSpeed = length( prevailing );
    float2 patternDirection = advectionSpeed > .001 ? prevailing / advectionSpeed : float2( 1, 0 );
    float2 patternAcross = float2( -patternDirection.y, patternDirection.x );
    float2 acrossDirection = float2( -windDirection.y, windDirection.x );

    // Fade wind displacement at range, then sample a broad advected gust pattern.
    // Cross-wind warping keeps the fronts from forming straight stripes.
    float windDistance = asfloat( source.mControl.w );
    float windFade = 1 - smoothstep( windDistance * .8, windDistance, distance );
    float frequency = max( source.mDetail.z, .001 );
    float2 fieldPosition = float2( dot( world.xz, patternDirection ) - source.mView.w * advectionSpeed,
        dot( world.xz, patternAcross ) );
    float warp = GrassFlowFbm( fieldPosition * frequency * float2( .35, .65 ) );
    float phase = fieldPosition.x * frequency * 6.2831853 + warp * 2;
    float front = pow( saturate( sin( phase ) * .5 + .5 ), 2.5 );
    float envelope = smoothstep( .2, .85, GrassFlowNoise( fieldPosition * frequency * .35 + float2( 17, 41 ) ) );
    float gust = .15 + front * lerp( .45, 1.3, envelope );
    float lateral = ( GrassFlowNoise( fieldPosition * frequency * .55 + 65 ) - .5 ) * .3;
    float2 flowDirection = normalize( windDirection + acrossDirection * lateral );

    // Turn the shared wind field into a slightly different bend response per
    // root. The vertex shader applies this with zero displacement at the base.
    float stiffness = lerp( .85, 1.15, GrassRandom( seed + 10 ) );
    float response = min( speed * .25, 2 ) * source.mWind.w * windFade / stiffness;
    float impact = gust * response;

    // Flutter is a small independent tip displacement. Keeping it out of the
    // main bend leaves roots/lower stems quiet while gusts roll through the field.
    float flutter = sin( source.mView.w * lerp( 7, 10, GrassRandom( seed + 11 ) ) + GrassRandom( seed + 9 ) * 6.2831853 ) *
        source.mDetail.w * min( speed * .2, 1 ) * windFade * lerp( .25, 1, front );
    blade.mWind = float4( flowDirection, impact, flutter );

    // Each mesh description owns two capacity-sized slices. Its matching draw
    // reads exactly the compacted records written for this LOD.
    StoreBufferValue<GrassBlade>( parameters.mBlades, ( source.mRanges.x + lod * source.mRanges.y + slot ) * sizeof( GrassBlade ), blade );
}
