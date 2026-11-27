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

#include "base/Animation_Helpers.slang"
#include "base/Base.slang"
#include "base/Camera_Helpers.slang"
#include "base/Material_Helpers.slang"

[[vk::binding( Slot_0, BUFFER_VIEWS_SET_INDEX )]]
ConstantBuffer<CameraInfo, ScalarDataLayout> u_Camera;

[[vk::binding( Slot_0, UAV_SET_INDEX )]]
StructuredBuffer<MeshParameters, ScalarDataLayout> Meshes;

[[vk::binding( Slot_2, UAV_SET_INDEX )]]
StructuredBuffer<BoneData, ScalarDataLayout> Bones;

struct VSInput {
    float3 mPosition : ATTRIBUTE0;
    float3 mNormal : ATTRIBUTE1;
    float4 mColor : ATTRIBUTE2;
    float2 mTexCoord0 : ATTRIBUTE3;
    float2 mTexCoord1 : ATTRIBUTE4;

    float4 mJoint : ATTRIBUTE5;
    float4 mWeight : ATTRIBUTE6;

    uint mInstanceIndex : SV_InstanceID;
    uint mBaseInstanceIndex : SV_StartInstanceLocation;
};

struct VSOutput {
    float3 mFragmentViewPos : TEXCOORD0;
    float3 mFragmentWorldPos : TEXCOORD1;
    float3 mNormal : TEXCOORD2;
    float2 mTexCoord0 : TEXCOORD3;
    float4 mColor : TEXCOORD4;

    float2 mTexCoord1 : TEXCOORD5;

    int mMaterialIndex : TEXCOORD6;

    float4 mPositionH : SV_Position;
};

[shader( "vertex" )]
VSOutput main( VSInput input ) {
    VSOutput o;

    MeshParameters meshInfo = Meshes[input.mBaseInstanceIndex + input.mInstanceIndex];

    float4x4 model = meshInfo.mTransform;

    float4 skinnedPos = float4( 0, 0, 0, 0 );
    float3 skinnedNormal = float3( 0, 0, 0 );

    if ( meshInfo.mAnimatorID != -1 ) {
        uint baseIndex = meshInfo.mAnimatorID;

        for ( uint i = 0; i < MAX_BONE_INFLUENCE; i++ ) {
            int joint = uint( input.mJoint[i] );
            float weight = input.mWeight[i];

            if ( weight <= 0.0 || joint < 0 ) {
                continue;
            }

            if ( joint >= MAX_BONES ) {
                // Fallback to rigid
                skinnedPos = float4( input.mPosition, 1.0 );
                skinnedNormal = input.mNormal;
                break;
            }

            float4x4 bone = Bones[baseIndex].BoneMatrices[joint];

            skinnedPos += mul( bone, float4( input.mPosition, 1.0 ) ) * weight;
            skinnedNormal += mul( ( float3x3 )bone, input.mNormal ) * weight;
        }
    } else {
        skinnedPos = float4( input.mPosition, 1.0 );
        skinnedNormal = input.mNormal;
    }

    float4 worldPos = mul( model, skinnedPos );
    float3 worldNormal = normalize( mul( ( float3x3 )model, skinnedNormal ) );

    o.mTexCoord0 = input.mTexCoord0;
    o.mTexCoord1 = input.mTexCoord1;
    o.mColor = input.mColor;

    o.mFragmentWorldPos = worldPos.xyz;
    o.mFragmentViewPos = mul( u_Camera.ViewMatrix, worldPos ).xyz;
    o.mNormal = worldNormal;
    o.mMaterialIndex = input.mBaseInstanceIndex + input.mInstanceIndex;

    o.mPositionH = mul( u_Camera.mProjection, mul( u_Camera.ViewMatrix, worldPos ) );

    return o;
}