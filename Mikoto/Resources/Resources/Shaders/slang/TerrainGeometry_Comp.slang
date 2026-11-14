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

#include "base/Memory_Helpers.slang"

// Matches TerrainBuildDescription in TerrainGenerationModule.cc (64 bytes).
struct TerrainBuildDescription {
    float2 origin;
    float vertexSpacing;
    float heightScale;
    float baseElevation;
    uint columns;
    uint rows;
    uint firstVertex;
    uint firstIndex;
    uint indexCount;
    uint heightmapWidth;
    uint heightmapHeight;
    uint64_t heightmapOffset;
    uint heightmapSource;
    uint padding;
};

struct TerrainGeometryParameters {
    RWBufferReference<float4> vertices;
    RWBufferReference<uint> indices;
    BufferReference<TerrainBuildDescription> descriptions;
    Pointer heightmapPixels;
    uint terrain;
    uint noiseTexture;
};
MKT_DECLARE_PUSH_CONSTANT( TerrainGeometryParameters, parameters );
MKT_DECLARE_TEXTURE_SRV( Texture2D<float4> ) textures;

// Red supplies numerical height. Imported pixels retain their original RGBA8
// or RGBA32F representation; generated noise can be read directly from its target.
float TerrainHeightTexel( TerrainBuildDescription description, uint2 pixel ) {
    if ( description.heightmapSource == 3u ) {
        return textures[parameters.noiseTexture].Load( int3( pixel, 0 ) ).r;
    }

    uint index = pixel.y * description.heightmapWidth + pixel.x;
    Pointer address = parameters.heightmapPixels + description.heightmapOffset;
    if ( description.heightmapSource == 2u ) {
        BufferReference<float4> pixels = (BufferReference<float4>)address;
        return pixels[index].r;
    }
    BufferReference<uint> pixels = (BufferReference<uint>)address;
    return float( pixels[index] & 255u ) / 255.0f;
}

float SampleTerrainHeight( TerrainBuildDescription description, float2 uv ) {
    if ( description.heightmapSource == 0u ) {
        return 0.0f;
    }

    // Preserve the previous CPU sampling convention: UV endpoints map exactly
    // to image endpoints, including rectangular sources and grids.
    float2 pixel = saturate( uv ) * float2( description.heightmapWidth - 1, description.heightmapHeight - 1 );
    uint2 lower = uint2( pixel );
    uint2 upper = min( lower + 1u, uint2( description.heightmapWidth - 1, description.heightmapHeight - 1 ) );
    float2 blend = pixel - float2( lower );

    float a = TerrainHeightTexel( description, lower );
    float b = TerrainHeightTexel( description, uint2( upper.x, lower.y ) );
    float c = TerrainHeightTexel( description, uint2( lower.x, upper.y ) );
    float d = TerrainHeightTexel( description, upper );
    return lerp( lerp( a, b, blend.x ), lerp( c, d, blend.x ), blend.y );
}

[shader("compute")]
[numthreads(128, 1, 1)]
void main( uint3 thread : SV_DispatchThreadID ) {
    TerrainBuildDescription description = parameters.descriptions[parameters.terrain];
    uint vertex = thread.x;
    if ( vertex >= description.columns * description.rows ) {
        return;
    }

    uint row = vertex / description.columns;
    uint column = vertex % description.columns;
    float2 uv = float2( column, row ) / float2( description.columns - 1, description.rows - 1 );
    float height = SampleTerrainHeight( description, uv );
    parameters.vertices[description.firstVertex + vertex] = float4(
        description.origin.x + float( column ) * description.vertexSpacing,
        description.baseElevation + height * description.heightScale,
        description.origin.y + float( row ) * description.vertexSpacing,
        height );

    // Each quad owns six output indices. Vertex and index writes cover separate
    // elements, so threads need no atomics or synchronization within this dispatch.
    uint quadsPerRow = description.columns - 1;
    uint quadCount = quadsPerRow * (description.rows - 1);
    if ( vertex < quadCount ) {
        uint quadRow = vertex / quadsPerRow;
        uint quadColumn = vertex % quadsPerRow;
        uint a = quadRow * description.columns + quadColumn;
        uint c = a + description.columns;
        uint index = description.firstIndex + vertex * 6;
        // Same counter-clockwise topology as the original CPU grid.
        parameters.indices[index + 0] = a;
        parameters.indices[index + 1] = c;
        parameters.indices[index + 2] = a + 1;
        parameters.indices[index + 3] = a + 1;
        parameters.indices[index + 4] = c;
        parameters.indices[index + 5] = c + 1;
    }
}

