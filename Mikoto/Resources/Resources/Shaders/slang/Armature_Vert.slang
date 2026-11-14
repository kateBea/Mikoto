// Copyright 2026 ケイト
// Licensed under the Apache License, Version 2.0.

#include "base/Base.slang"
#include "base/Camera_Helpers.slang"
#include "base/Memory_Helpers.slang"

struct ArmatureBone {
    float4 AxisX;
    float4 AxisY;
    float4 AxisZ;
    float4 Origin;
    float4 Color;
};

struct Parameters {
    Pointer mVertices;
    Pointer mBones;
    Pointer mCamera;
};

MKT_DECLARE_PUSH_CONSTANT( Parameters, parameters );

struct VSOutput {
    float4 Position : SV_Position;
    float3 WorldPosition : TEXCOORD0;
    float4 Color : COLOR0;
};

[shader( "vertex" )]
VSOutput main( uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID ) {
    BufferReference<float4> vertices = ( BufferReference<float4> )parameters.mVertices;
    BufferReference<ArmatureBone> bones = ( BufferReference<ArmatureBone> )parameters.mBones;
    BufferReference<CameraInfo> camera = ( BufferReference<CameraInfo> )parameters.mCamera;
    ArmatureBone bone = bones[instanceID];
    float3 local = vertices[vertexID].xyz;

    VSOutput output;
    output.WorldPosition = bone.Origin.xyz + local.x * bone.AxisX.xyz + local.y * bone.AxisY.xyz + local.z * bone.AxisZ.xyz;
    output.Position = mul( camera[0].mProjection, mul( camera[0].mViewMatrix, float4( output.WorldPosition, 1 ) ) );
    output.Color = bone.Color;
    return output;
}
