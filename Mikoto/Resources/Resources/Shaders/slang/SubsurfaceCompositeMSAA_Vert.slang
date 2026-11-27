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

// Generate the fullscreen primitive for multisample subsurface composition. The fragment stage
// selects and combines the corresponding scene samples.

#include "base/Animation_Helpers.slang"
#include "base/Camera_Helpers.slang"
#include "base/SubsurfaceCompositeMSAA.slang"

struct VSInput {
    uint mVertexID : SV_VertexID;
    uint mInstanceIndex : SV_InstanceID;
    uint mBaseInstanceIndex : SV_StartInstanceLocation;
};

[shader( "vertex" )]
SubsurfaceCompositeVertex main( VSInput input ) {
    uint instanceIndex = input.mBaseInstanceIndex + input.mInstanceIndex;
    MeshGeometryInfo mesh = LoadMeshGeometry( parameters.mGeometry, instanceIndex );
    GeometryAllocation allocation = GetGeometryAllocation( mesh, parameters.mAllocations );
    VertexDescription vertex = PullShadingVertex( mesh, allocation, PullVertexIndex( allocation, input.mVertexID ) );

    // Match opaque PBR transforms, including cached skinning.
    float4 worldPosition = ApplySkinning( vertex, mesh, parameters.mSkinning ).mPosition;
    CameraInfo camera = ( ( BufferReference<CameraInfo> )parameters.mCamera )[0];

    SubsurfaceCompositeVertex output;
    output.mPosition = CameraClipPosition( worldPosition, camera );
    output.mUv0 = vertex.mUv0;
    output.mUv1 = vertex.mUv1;
    output.mVertexAlpha = vertex.mColor.a;
    output.mMaterialIndex = mesh.mMaterialIndex;
    output.mFaceFlags = mesh.mFaceFlags;
    output.mDrawIdentity = instanceIndex + 1;

    return output;
}
