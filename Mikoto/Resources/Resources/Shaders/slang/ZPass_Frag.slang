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
#include "base/Lighting_Helpers.slang"
#include "base/Material_Helpers.slang"
#include "base/Memory_Helpers.slang"

struct FSOutput {
    float4 Color : SV_Target0;
};

struct FSInput {
    nointerpolation uint MaterialIndex : TEXCOORD0;
    nointerpolation uint FaceFlags : TEXCOORD4;
    float2 Uv0 : TEXCOORD1;
    float2 Uv1 : TEXCOORD2;
    float VertexAlpha : TEXCOORD3;
    float4 ScreenPos : SV_Position;
};

struct DrawParams {
    Pointer mGeometryBufferID;
    Pointer mSkinningBufferID;
    Pointer mGeometryAllocationBufferID;

    Pointer mCameraBda;
    Pointer mMaterialsPtr;
    uint mBasicSamplerID;
};

MKT_DECLARE_STRUCTURED_SRV buffers;
MKT_DECLARE_TEXTURE_SRV( Texture2D ) textures;
MKT_DECLARE_SAMPLERS samplers;
MKT_DECLARE_PUSH_CONSTANT( DrawParams, parameters );

[shader( "fragment" )]
FSOutput main( FSInput input, bool frontFacing : SV_IsFrontFace ) {
    FSOutput o;
    if ( MaterialBackfaceDiscard( input.FaceFlags, frontFacing ) ) {
        discard;
    }

    MaterialParameters material =
            ( ( BufferReference<MaterialParameters> )parameters.mMaterialsPtr )[input.MaterialIndex];
    // Include only the fully opaque coverage of BLEND materials. Fractional
    // coverage must not hide surfaces behind glass in the later blend pass.
    if ( material.AlphaMask == PBR_ALPHA_MASK || material.AlphaMask == PBR_ALPHA_BLEND ) {
        int textureIndex = MaterialAlphaTexture( material );

        float alpha = textureIndex == INVALID_TEXTURE_INDEX
                              ? 1.0f
                              : textures[NonUniformResourceIndex( textureIndex )]
                                        .Sample( samplers[parameters.mBasicSamplerID],
                                                 MaterialAlphaUV( material, input.Uv0, input.Uv1 ) )
                                        .a;

        if ( MaterialSurfaceDiscard( material, MaterialAlpha( material, alpha, input.VertexAlpha ), false ) ) {
            discard;
        }
    }

    CameraInfo camera = ( ( BufferReference<CameraInfo> )parameters.mCameraBda )[0];

    // Linear depth visualization
    float depth = LinearDepth( input.ScreenPos.z, camera.mPlanes.xy );

    float near = camera.mPlanes.x;
    float far = camera.mPlanes.y;

    float depthNormalized = ( depth - near ) / ( far - near );

    // Visualize inverted depth for better visibility
    o.Color = float4( abs( 1 - depthNormalized ), abs( 1 - depthNormalized ), abs( 1 - depthNormalized ), 1.0 );

    return o;
}
