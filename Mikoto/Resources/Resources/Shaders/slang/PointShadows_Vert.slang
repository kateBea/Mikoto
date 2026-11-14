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

// A point-light record selects one of the six cubemap face projections.

#pragma once

#include "base/Animation_Helpers.slang"
#include "base/Base.slang"
#include "base/Memory_Helpers.slang"
#include "base/ShadowMap_Helpers.slang"

MKT_DECLARE_PUSH_CONSTANT( ShadowDrawParams, parameters );

[shader( "vertex" )]
ShadowVertexOutput main( uint vertexID : SV_VertexID,
    uint instanceID : SV_InstanceID,
    uint baseInstance : SV_StartInstanceLocation ) {

    uint instance = instanceID + baseInstance;
    MeshGeometryInfo mesh = LoadMeshGeometry( parameters.Geometry, instance );
    GeometryAllocation allocation = GetGeometryAllocation( mesh, parameters.Allocation );
    uint vertexIndex = PullVertexIndex( allocation, vertexID );

    // Opaque casters need positions and optional skinning only. Reusing the
    // per-instance alpha mode avoids another dependent material-buffer lookup.
    VertexDescription vertex;
    if ( mesh.AlphaMode != PBR_ALPHA_OPAQUE ) {
        vertex = PullDepthVertex( mesh, allocation, vertexIndex );
    } else {
        vertex = PullPositionVertex( mesh, allocation, vertexIndex );
    }

    float4 worldPosition = ApplySkinningPosition( vertex, mesh, parameters.Skinning );
    uint recordOffset = parameters.Record * sizeof( ShadowRecord );

    float4x4 viewProjection = LoadBufferValue<float4x4>( parameters.Records, recordOffset + parameters.Face * sizeof( float4x4 ) );
    ShadowMetadata metadata = LoadBufferValue<ShadowMetadata>( parameters.Records, recordOffset + ShadowMetadataOffset );

    ShadowVertexOutput o;

    o.Position = mul( viewProjection, worldPosition );
    o.MaterialIndex = mesh.MaterialIndex;
    o.AlphaMode = mesh.AlphaMode;
    o.FaceFlags = mesh.FaceFlags;
    o.Uv0 = vertex.uv0;
    o.Uv1 = vertex.uv1;
    o.VertexAlpha = vertex.color.a;
    o.WorldPosition = worldPosition.xyz;
    o.LightPositionRange = metadata.PositionRange;
    o.LightType = metadata.LightType;

    return o;
}
