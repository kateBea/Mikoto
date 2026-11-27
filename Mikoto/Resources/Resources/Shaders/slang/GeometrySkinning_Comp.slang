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

// Evaluate bone-weighted deformation into the shared skinned-vertex stream. Later depth, shadow
// and color passes pull the same deformed geometry.

#pragma once

#include "base/Base.slang"
#include "base/Animation_Helpers.slang"

// Matches the CPU skinning dispatch divisor.
static const uint kSkinningGroupSize = 128;

struct SkinningParameters {
    uint64_t mSourcePositions;
    uint64_t mSourceNormals;
    BufferReference<float4> mJoints0;
    BufferReference<float4> mWeights0;
    BufferReference<float4> mJoints1;
    BufferReference<float4> mWeights1;
    BufferReference<BoneData> mPalette;
    RWBufferReference<float4> mPositions;
    RWBufferReference<float4> mNormals;
    uint mVertexCount;
    uint mHasPalette;
    uint64_t mUv0;
    uint64_t mUv1;
    uint64_t mMaterials;
    uint mMaterialIndex;
    uint mSamplerIndex;
};
MKT_DECLARE_PUSH_CONSTANT( SkinningParameters, parameters );
MKT_DECLARE_TEXTURE_SRV( Texture2D ) textures;
MKT_DECLARE_SAMPLERS samplers;

[shader( "compute" )]
[numthreads( kSkinningGroupSize, 1, 1 )]
void main( uint3 thread : SV_DispatchThreadID ) {
    uint vertexID = thread.x;
    if ( vertexID >= parameters.mVertexCount ) {
        return;
    }

    // Visit unique vertices, not indices: repeated triangle corners and every
    // subsequent raster pass share these two output streams.
    VertexDescription vertex = ( VertexDescription )0;
    vertex.mPosition = PullFloat3( parameters.mSourcePositions, vertexID );
    vertex.mNormal = PullFloat3( parameters.mSourceNormals, vertexID );
    // Displace once in mesh space before skinning. Every raster pass reads the
    // same cached position, so depth, shadow and picking silhouettes agree.
    // https://developer.nvidia.com/gpugems/gpugems2/part-ii-shading-lighting-and-shadows/chapter-18-using-vertex-texture-displacement
    MaterialParameters material = LoadBufferValue<MaterialParameters>(parameters.mMaterials,parameters.mMaterialIndex*sizeof(MaterialParameters));
    if ( material.mHeightIndex>=0 && uint(material.mHeightMapping.z)==3 ) {
        float2 uv = LoadBufferValue<float2>(material.mHeightUVSet==0 ? parameters.mUv0 : parameters.mUv1,vertexID * sizeof( float2 ));
        uv = uv*material.mUvTransform.xy+material.mUvTransform.zw;
        float height = textures[NonUniformResourceIndex(material.mHeightIndex)].SampleLevel(samplers[parameters.mSamplerIndex],uv,0).r;
        vertex.mPosition += normalize(vertex.mNormal)*(height-material.mHeightMapping.y)*material.mHeightMapping.x;
    }
    SkinningResult local;
    local.mPosition=float4(vertex.mPosition,1);
    local.mNormal=vertex.mNormal;
    if ( parameters.mHasPalette!=0 ) {
        vertex.mJoints0 = parameters.mJoints0[vertexID];
        vertex.mWeights0 = parameters.mWeights0[vertexID];
        vertex.mJoints1 = parameters.mJoints1[vertexID];
        vertex.mWeights1 = parameters.mWeights1[vertexID];
        local = ComputeSkinLocalVertex(vertex,0,parameters.mPalette.address);
    }
    parameters.mPositions[vertexID] = local.mPosition;
    parameters.mNormals[vertexID] = float4( local.mNormal, 0.0 );
}
