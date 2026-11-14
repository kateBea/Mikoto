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

#include "base/Camera_Helpers.slang"
#include "base/TerrainRender_Helpers.slang"

struct TerrainVaryings {
    // Match the fragment registers before appending the rasterizer position.
    // https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-signatures
    float height : TEXCOORD0;
    float2 grid : TEXCOORD1;
    float4 position : SV_Position;
};

[shader("vertex")]
TerrainVaryings main( uint vertexID : SV_VertexID ) {
    BufferReference<uint> indices = (BufferReference<uint>)parameters.indices;
    BufferReference<float4> vertices = (BufferReference<float4>)parameters.vertices;
    BufferReference<CameraInfo> camera = (BufferReference<CameraInfo>)parameters.camera;

    // Indices are local to each landscape; packed draw offsets select its grid.
    uint localIndex = indices[parameters.firstIndex + vertexID];
    uint index = parameters.firstVertex + localIndex;
    float4 vertex = vertices[index];
    float4 worldPosition = mul( parameters.world, float4( vertex.xyz, 1.0f ) );

    TerrainVaryings output;
    output.position = CameraClipPosition( worldPosition, camera[0] );
    output.height = vertex.w;

    // Integer coordinates mark actual grid vertices even after height displacement.
    output.grid = float2( localIndex % parameters.columns, localIndex / parameters.columns );
    return output;
}
