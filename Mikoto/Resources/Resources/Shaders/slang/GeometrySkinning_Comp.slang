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

#pragma once

#include "base/Base.slang"
#include "base/Animation_Helpers.slang"

struct SkinningParameters {
    uint64_t sourcePositions;
    uint64_t sourceNormals;
    BufferReference<float4> joints0;
    BufferReference<float4> weights0;
    BufferReference<float4> joints1;
    BufferReference<float4> weights1;
    BufferReference<BoneData> palette;
    RWBufferReference<float4> positions;
    RWBufferReference<float4> normals;
    uint vertexCount;
    uint hasPalette;
    uint64_t uv0, uv1, materials;
    uint materialIndex, samplerIndex;
};
MKT_DECLARE_PUSH_CONSTANT( SkinningParameters, parameters );
MKT_DECLARE_TEXTURE_SRV( Texture2D ) textures;
MKT_DECLARE_SAMPLERS samplers;

[shader( "compute" )]
[numthreads( 128, 1, 1 )]
void main( uint3 thread : SV_DispatchThreadID ) {
    uint vertexID = thread.x;
    if ( vertexID >= parameters.vertexCount ) {
        return;
    }

    // Visit unique vertices, not indices: repeated triangle corners and every
    // subsequent raster pass share these two output streams.
    VertexDescription vertex = ( VertexDescription )0;
    vertex.position = PullFloat3( parameters.sourcePositions, vertexID );
    vertex.normal = PullFloat3( parameters.sourceNormals, vertexID );
    // Displace once in mesh space before skinning. Every raster pass reads the
    // same cached position, so depth, shadow and picking silhouettes agree.
    // https://developer.nvidia.com/gpugems/gpugems2/part-ii-shading-lighting-and-shadows/chapter-18-using-vertex-texture-displacement
    MaterialParameters material = LoadBufferValue<MaterialParameters>(parameters.materials,parameters.materialIndex*sizeof(MaterialParameters));
    if ( material.HeightIndex>=0 && uint(material.HeightMapping.z)==3 ) {
        float2 uv = LoadBufferValue<float2>(material.HeightUVSet==0 ? parameters.uv0 : parameters.uv1,vertexID*8);
        uv = uv*material.UvTransform.xy+material.UvTransform.zw;
        float height = textures[NonUniformResourceIndex(material.HeightIndex)].SampleLevel(samplers[parameters.samplerIndex],uv,0).r;
        vertex.position += normalize(vertex.normal)*(height-material.HeightMapping.y)*material.HeightMapping.x;
    }
    SkinningResult local;
    local.mPosition=float4(vertex.position,1);
    local.mNormal=vertex.normal;
    if ( parameters.hasPalette!=0 ) {
        vertex.joints0 = parameters.joints0[vertexID];
        vertex.weights0 = parameters.weights0[vertexID];
        vertex.joints1 = parameters.joints1[vertexID];
        vertex.weights1 = parameters.weights1[vertexID];
        local = ComputeSkinLocalVertex(vertex,0,parameters.palette.address);
    }
    parameters.positions[vertexID] = local.mPosition;
    parameters.normals[vertexID] = float4( local.mNormal, 0.0 );
}
