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

// Generate terrain vertices, normals and triangle indices from the height source. Height
// texture sampling and procedural noise share the uploaded terrain dimensions.

#include "base/Memory_Helpers.slang"

// Matches TerrainBuildDescription in TerrainGenerationModule.cc (64 bytes).
// Matches the CPU terrain geometry dispatch divisor.
static const uint kTerrainGeometryGroupSize = 128;

struct TerrainBuildDescription {
    float2 mOrigin;
    float mVertexSpacing;
    float mHeightScale;
    float mBaseElevation;
    uint mColumns;
    uint mRows;
    uint mFirstVertex;
    uint mFirstIndex;
    uint mIndexCount;
    uint mHeightmapWidth;
    uint mHeightmapHeight;
    uint64_t mHeightmapOffset;
    uint mHeightmapSource;
    uint mPadding;
};

struct TerrainGeometryParameters {
    RWBufferReference<float4> mVertices;
    RWBufferReference<uint> mIndices;
    BufferReference<TerrainBuildDescription> mDescriptions;
    Pointer mHeightmapPixels;
    uint mTerrain;
    uint mNoiseTexture;
};
MKT_DECLARE_PUSH_CONSTANT( TerrainGeometryParameters, parameters );
MKT_DECLARE_TEXTURE_SRV( Texture2D<float4> ) textures;

// Red supplies numerical height. Imported pixels retain their original RGBA8
// or RGBA32F representation; generated noise can be read directly from its target.
float TerrainHeightTexel( TerrainBuildDescription description, uint2 pixel ) {
    if ( description.mHeightmapSource == 3u ) {
        return textures[parameters.mNoiseTexture].Load( int3( pixel, 0 ) ).r;
    }

    uint index = pixel.y * description.mHeightmapWidth + pixel.x;
    Pointer address = parameters.mHeightmapPixels + description.mHeightmapOffset;
    if ( description.mHeightmapSource == 2u ) {
        BufferReference<float4> pixels = (BufferReference<float4>)address;
        return pixels[index].r;
    }
    BufferReference<uint> pixels = (BufferReference<uint>)address;
    return float( pixels[index] & 255u ) / 255.0f;
}

float SampleTerrainHeight( TerrainBuildDescription description, float2 uv ) {
    if ( description.mHeightmapSource == 0u ) {
        return 0.0f;
    }

    // Preserve the previous CPU sampling convention: UV endpoints map exactly
    // to image endpoints, including rectangular sources and grids.
    float2 pixel = saturate( uv ) * float2( description.mHeightmapWidth - 1, description.mHeightmapHeight - 1 );
    uint2 lower = uint2( pixel );
    uint2 upper = min( lower + 1u, uint2( description.mHeightmapWidth - 1, description.mHeightmapHeight - 1 ) );
    float2 blend = pixel - float2( lower );

    float a = TerrainHeightTexel( description, lower );
    float b = TerrainHeightTexel( description, uint2( upper.x, lower.y ) );
    float c = TerrainHeightTexel( description, uint2( lower.x, upper.y ) );
    float d = TerrainHeightTexel( description, upper );
    return lerp( lerp( a, b, blend.x ), lerp( c, d, blend.x ), blend.y );
}

[shader("compute")]
[numthreads( kTerrainGeometryGroupSize, 1, 1 )]
// Each invocation addresses one terrain-grid element. Height samples establish position and
// finite-difference slopes; the generated index stream connects neighboring grid vertices.
void main( uint3 thread : SV_DispatchThreadID ) {
    TerrainBuildDescription description = parameters.mDescriptions[parameters.mTerrain];
    uint vertex = thread.x;
    if ( vertex >= description.mColumns * description.mRows ) {
        return;
    }

    uint row = vertex / description.mColumns;
    uint column = vertex % description.mColumns;
    float2 uv = float2( column, row ) / float2( description.mColumns - 1, description.mRows - 1 );
    float height = SampleTerrainHeight( description, uv );
    parameters.mVertices[description.mFirstVertex + vertex] = float4(
        description.mOrigin.x + float( column ) * description.mVertexSpacing,
        description.mBaseElevation + height * description.mHeightScale,
        description.mOrigin.y + float( row ) * description.mVertexSpacing,
        height );

    // Each quad owns six output indices. Vertex and index writes cover separate
    // elements, so threads need no atomics or synchronization within this dispatch.
    uint quadsPerRow = description.mColumns - 1;
    uint quadCount = quadsPerRow * (description.mRows - 1);
    if ( vertex < quadCount ) {
        uint quadRow = vertex / quadsPerRow;
        uint quadColumn = vertex % quadsPerRow;
        uint a = quadRow * description.mColumns + quadColumn;
        uint c = a + description.mColumns;
        uint index = description.mFirstIndex + vertex * kVerticesPerQuad;
        // Same counter-clockwise topology as the original CPU grid.
        parameters.mIndices[index + 0] = a;
        parameters.mIndices[index + 1] = c;
        parameters.mIndices[index + 2] = a + 1;
        parameters.mIndices[index + 3] = a + 1;
        parameters.mIndices[index + 4] = c;
        parameters.mIndices[index + 5] = c + 1;
    }
}

