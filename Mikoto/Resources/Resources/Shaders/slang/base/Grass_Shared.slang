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

// CPU-mirrored grass/tile layouts and stable root hashing. Geometry LOD changes blade
// complexity while hierarchical root ranks keep placement consistent between tile levels.

#pragma once

#include "GrassFlow_Helpers.slang"

// Mirrors kGrassLodCount in Renderer/Grass/Grass.hh.
static const uint kGrassLodCount = 2;
static const uint kGrassHighBladeVertexCount = 15;
static const uint kGrassLowBladeVertexCount = 7;
static const uint kGrassTileSide = 32;
static const uint kGrassTileCandidateCount = kGrassTileSide * kGrassTileSide;
static const uint kGrassMaxTileLevel = 16;
static const uint kGrassGenerateGroupSize = 128;

// DrawIndirectCommand stores four uints: vertex count, instance count, first
// vertex and first instance. Byte-addressed counters target its second member.
static const uint kGrassDrawCommandStride = 4 * sizeof( uint );
static const uint kGrassInstanceCountOffset = sizeof( uint );

// CPU mirrors: GrassTile and TerrainGrassDescription. Placement tiles and the
// two geometry LODs are independent. Tiling.x is the finest spacing; .y is the
// number of leaves addressed by mTiles. Integer origins keep roots world-anchored.
struct GrassTile {
    int2 mCellOrigin;
    uint mStride;
    uint mLevel;
};

struct GrassDescription {
    float4x4 mWorld;
    float4 mTiling;
    float4 mShape;
    float4 mView;
    float4 mWind;
    float4 mRootColor;
    float4 mMidColor;
    float4 mTipColor;
    float4 mDryColor;
    float4 mLighting;
    float4 mDetail;
    float4 mLimits;
    float4 mBudgets;
    // Cast enabled, density multiplier, contact-shadow enabled, reserved.
    float4 mShadows;
    uint4 mGrid;
    uint4 mRanges;
    uint4 mControl;
    uint4 mMesh;
    float4 mMaterialColor;
    float4 mMaterialOptions;
    float4 mPalette;
    uint64_t mSurface;
    uint64_t mWindField;
    uint64_t mTiles;
};

struct GrassBlade {
    float4 mRootHeight;
    float4 mFacingWidthBend;
    float4 mNormalColor;
    float4 mWind;
};

// Imported triangles are normalized once on the CPU. Position.w and Normal.w
// retain authored UV.y/UV.x; physical height controls deformation independently.
struct GrassMeshVertex {
    float4 mPosition;
    float4 mNormal;
};

uint GrassHash( uint value ) {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    return value ^ ( value >> 16 );
}
float GrassRandom( uint value ) {
    return float( GrassHash( value ) >> 8 ) / 16777216.0;
}
uint GrassCellHash( int2 cell, uint seed ) {
    return GrassHash( uint( cell.x ) * 73856093u ^ uint( cell.y ) * 19349663u ^ seed );
}

// A hierarchical rank is uniform across the fine grid. Roots present in a
// level-L tile own ranks below 1/4^L; skipped finer roots can never pass a density
// threshold below that value. Tile subdivision therefore changes work, not the
// visible set of roots. Signed coordinates use the same divisibility test.
float GrassRootRank( int2 cell, uint seed ) {
    uint combined = uint( cell.x ) | uint( cell.y );
    uint level = combined == 0 ? kGrassMaxTileLevel : min( uint( firstbitlow( combined ) ), kGrassMaxTileLevel );
    float maximum = exp2( -2.0 * float( level ) );
    float minimum = level == kGrassMaxTileLevel ? 0 : maximum * .25;
    return lerp( minimum, maximum, GrassRandom( seed + 3 ) );
}

// At full morph the 15-vertex strip collapses onto exactly the same seven
// positions as the low strip. Repeated pairs form degenerate triangles.
void GrassBladeCoordinates( uint vertex, uint lod, float morph, out float height, out float side ) {
    uint highPairs = ( kGrassHighBladeVertexCount - 1 ) / 2;
    uint lowPairs = ( kGrassLowBladeVertexCount - 1 ) / 2;
    uint pairs = lod == 0 ? highPairs : lowPairs;
    uint row = vertex / 2;
    bool tip = vertex == pairs * 2;
    float highHeight = tip ? 1 : float( row ) / float( pairs );
    float lowHeight = tip ? 1 : float( row * lowPairs / highPairs ) / float( lowPairs );
    height = lod == 0 ? lerp( highHeight, lowHeight, morph ) : highHeight;
    side = tip ? 0 : ( ( vertex & 1 ) != 0 ? 1 : -1 );
}

