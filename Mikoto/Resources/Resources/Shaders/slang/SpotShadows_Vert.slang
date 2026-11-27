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

// The spot record supplies the perspective projection for its outer cone.

#pragma once

#include "base/Animation_Helpers.slang"
#include "base/Base.slang"
#include "base/Memory_Helpers.slang"
#include "base/ShadowMap_Helpers.slang"

MKT_DECLARE_PUSH_CONSTANT( ShadowDrawParams, parameters );

[shader( "vertex" )]
ShadowVertexOutput main( uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID, uint baseInstance : SV_StartInstanceLocation ) {

    uint instance = instanceID + baseInstance;
    MeshGeometryInfo mesh = LoadMeshGeometry( parameters.mGeometry, instance );
    GeometryAllocation allocation = GetGeometryAllocation( mesh, parameters.mAllocation );
    uint vertexIndex = PullVertexIndex( allocation, vertexID );

    // Opaque casters need positions and optional skinning only. Reusing the
    // per-instance alpha mode avoids another dependent material-buffer lookup.
    VertexDescription vertex;
    if ( mesh.mAlphaMode != PBR_ALPHA_OPAQUE ) {
        vertex = PullDepthVertex( mesh, allocation, vertexIndex );
    } else {
        vertex = PullPositionVertex( mesh, allocation, vertexIndex );
    }

    float4 worldPosition = ApplySkinningPosition( vertex, mesh, parameters.mSkinning );
    uint recordOffset = parameters.mRecord * sizeof( ShadowRecord );

    float4x4 viewProjection = LoadBufferValue<float4x4>( parameters.mRecords, recordOffset + parameters.mFace * sizeof( float4x4 ) );
    ShadowMetadata metadata = LoadBufferValue<ShadowMetadata>( parameters.mRecords, recordOffset + ShadowMetadataOffset );

    ShadowVertexOutput o;

    o.mPosition = mul( viewProjection, worldPosition );
    o.mMaterialIndex = mesh.mMaterialIndex;
    o.mAlphaMode = mesh.mAlphaMode;
    o.mFaceFlags = mesh.mFaceFlags;
    o.mUv0 = vertex.mUv0;
    o.mUv1 = vertex.mUv1;
    o.mVertexAlpha = vertex.mColor.a;
    o.mWorldPosition = worldPosition.xyz;
    o.mLightPositionRange = metadata.mPositionRange;
    o.mLightType = metadata.mLightType;

    return o;
}
