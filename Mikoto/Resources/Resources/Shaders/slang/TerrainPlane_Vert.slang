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

// Pull the generated terrain mesh and transform it into world and clip space. Surface
// coordinates identify paint weights and material layers in the fragment stage.

#include "base/Camera_Helpers.slang"
#include "base/TerrainRender_Helpers.slang"
#include "base/TerrainSurface_Helpers.slang"

struct TerrainVaryings {
    // Match the fragment registers before appending the rasterizer position.
    // https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-signatures
    float mHeight : TEXCOORD0;
    float2 mGrid : TEXCOORD1;
    float3 mWorld : TEXCOORD2;
    float3 mNormal : TEXCOORD3;
    float2 mUv : TEXCOORD4;
    float4 mPosition : SV_Position;
};

[shader( "vertex" )]
TerrainVaryings main( uint vertexID : SV_VertexID ) {
    BufferReference<uint> indices = ( BufferReference<uint> )parameters.mIndices;
    BufferReference<float4> vertices = ( BufferReference<float4> )parameters.mVertices;
    BufferReference<CameraInfo> camera = ( BufferReference<CameraInfo> )parameters.mCamera;

    // Indices are local to each landscape; packed draw offsets select its grid.
    uint localIndex = indices[parameters.mFirstIndex + vertexID];
    uint index = parameters.mFirstVertex + localIndex;
    float4 vertex = vertices[index];
    float4 worldPosition = mul( parameters.mWorld, float4( vertex.xyz, 1.0f ) );

    TerrainVaryings output;
    output.mPosition = CameraClipPosition( worldPosition, camera[0] );
    output.mHeight = vertex.w;
    output.mWorld = worldPosition.xyz;
    uint count;
    uint stride;
    // Packed grids have explicit row counts in their surface record.
    TerrainSurfaceData surface = LoadBufferValue<TerrainSurfaceData>( parameters.mSurface, 0 );
    uint rows = surface.mControls.y;
    uint column = localIndex % parameters.mColumns;
    uint row = localIndex / parameters.mColumns;
    uint left = parameters.mFirstVertex + row * parameters.mColumns + ( column > 0 ? column - 1 : column );
    uint right = parameters.mFirstVertex + row * parameters.mColumns + min( column + 1, parameters.mColumns - 1 );
    uint back = parameters.mFirstVertex + ( row > 0 ? row - 1 : row ) * parameters.mColumns + column;
    uint front = parameters.mFirstVertex + min( row + 1, rows - 1 ) * parameters.mColumns + column;
    float3 tangent = mul( parameters.mWorld, float4( vertices[right].xyz - vertices[left].xyz, 0 ) ).xyz;
    float3 bitangent = mul( parameters.mWorld, float4( vertices[front].xyz - vertices[back].xyz, 0 ) ).xyz;
    output.mNormal = normalize( cross( bitangent, tangent ) );
    output.mUv = float2( column, row ) / float2( parameters.mColumns - 1, rows - 1 );

    // Integer coordinates mark actual grid vertices even after height displacement.
    output.mGrid = float2( localIndex % parameters.mColumns, localIndex / parameters.mColumns );
    return output;
}
