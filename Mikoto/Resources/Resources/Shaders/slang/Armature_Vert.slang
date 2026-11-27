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

// Pull joint-debug vertices and project them with the scene camera. The fragment stage uses the
// interpolated joint color for the armature overlay.

#include "base/Base.slang"
#include "base/Camera_Helpers.slang"
#include "base/Memory_Helpers.slang"

struct ArmatureBone {
    float4 mAxisX;
    float4 mAxisY;
    float4 mAxisZ;
    float4 mOrigin;
    float4 mColor;
};

struct Parameters {
    Pointer mVertices;
    Pointer mBones;
    Pointer mCamera;
};

MKT_DECLARE_PUSH_CONSTANT( Parameters, parameters );

struct VSOutput {
    float4 mPosition : SV_Position;
    float3 mWorldPosition : TEXCOORD0;
    float4 mColor : COLOR0;
};

[shader( "vertex" )]
VSOutput main( uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID ) {
    BufferReference<float4> vertices = ( BufferReference<float4> )parameters.mVertices;
    BufferReference<ArmatureBone> bones = ( BufferReference<ArmatureBone> )parameters.mBones;
    BufferReference<CameraInfo> camera = ( BufferReference<CameraInfo> )parameters.mCamera;
    ArmatureBone bone = bones[instanceID];
    float3 local = vertices[vertexID].xyz;

    VSOutput output;
    output.mWorldPosition = bone.mOrigin.xyz + local.x * bone.mAxisX.xyz + local.y * bone.mAxisY.xyz + local.z * bone.mAxisZ.xyz;
    output.mPosition = mul( camera[0].mProjection, mul( camera[0].mViewMatrix, float4( output.mWorldPosition, 1 ) ) );
    output.mColor = bone.mColor;
    return output;
}
