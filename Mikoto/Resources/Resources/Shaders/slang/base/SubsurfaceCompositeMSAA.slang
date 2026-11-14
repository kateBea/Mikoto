// Copyright 2026 ケイト. Licensed under the Apache License, Version 2.0.
#pragma once
#include "Memory_Helpers.slang"

// Geometry application of the diffuse-only compositor, before MSAA resolve.
// https://bentobaux.github.io/posts/separable-subsurface-scattering-in-unity-6/#3-reconstructing-the-final-image
struct SubsurfaceCompositeParameters {
    Pointer Geometry, Skinning, Allocations, Camera, Materials;
    uint Original, Filtered, Specular, Ambient, Metadata, Sampler;
};
MKT_DECLARE_PUSH_CONSTANT( SubsurfaceCompositeParameters, parameters );

struct SubsurfaceCompositeVertex {
    float4 Position : SV_Position;
    float2 Uv0 : TEXCOORD0;
    float2 Uv1 : TEXCOORD1;
    float VertexAlpha : TEXCOORD2;
    nointerpolation uint MaterialIndex : TEXCOORD3;
    nointerpolation uint FaceFlags : TEXCOORD4;
    nointerpolation uint DrawIdentity : TEXCOORD5;
};
