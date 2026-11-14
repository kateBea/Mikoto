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

#include "base/Animation_Helpers.slang"
#include "base/Camera_Helpers.slang"
#include "base/SubsurfaceCompositeMSAA.slang"

struct VSInput {
    uint VertexID : SV_VertexID;
    uint InstanceIndex : SV_InstanceID;
    uint BaseInstanceIndex : SV_StartInstanceLocation;
};

[shader( "vertex" )]
SubsurfaceCompositeVertex main( VSInput input ) {
    uint instanceIndex = input.BaseInstanceIndex + input.InstanceIndex;
    MeshGeometryInfo mesh = LoadMeshGeometry( parameters.Geometry, instanceIndex );
    GeometryAllocation allocation = GetGeometryAllocation( mesh, parameters.Allocations );
    VertexDescription vertex = PullShadingVertex( mesh, allocation, PullVertexIndex( allocation, input.VertexID ) );

    // Match opaque PBR transforms, including cached skinning.
    float4 worldPosition = ApplySkinning( vertex, mesh, parameters.Skinning ).mPosition;
    CameraInfo camera = ( ( BufferReference<CameraInfo> )parameters.Camera )[0];

    SubsurfaceCompositeVertex output;
    output.Position = CameraClipPosition( worldPosition, camera );
    output.Uv0 = vertex.uv0;
    output.Uv1 = vertex.uv1;
    output.VertexAlpha = vertex.color.a;
    output.MaterialIndex = mesh.MaterialIndex;
    output.FaceFlags = mesh.FaceFlags;
    output.DrawIdentity = instanceIndex + 1;

    return output;
}
