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

// Evaluate material coverage and surface guides for screen-space effects. Outputs encode
// material color, normals, positions and related guide channels rather than final scene
// lighting.

#pragma once

#include "base/Base.slang"
#include "base/SurfaceShading_Helpers.slang"
#include "base/Camera_Helpers.slang"
#include "base/Lighting_Helpers.slang"
#include "base/Material_Helpers.slang"
#include "base/Memory_Helpers.slang"

struct FSInput {
    float3 mViewPos : TEXCOORD0;
    float3 mWorldPos : TEXCOORD1;
    float3 mNormal : TEXCOORD2;
    float2 mUv0 : TEXCOORD3;
    float2 mUv1 : TEXCOORD4;
    float4 mColor : TEXCOORD5;

    nointerpolation uint mMaterialIndex : TEXTCOORD6;
    nointerpolation uint mFaceFlags : TEXCOORD7;

    float4 mScreenPos : SV_Position;
};

struct FSOutput {
    float4 mPosition : SV_Target0;
    float4 mNormal : SV_Target1;
    float4 mColor : SV_Target2;
    float4 mEmissive : SV_Target3;
    float4 mContour : SV_Target4;
};

struct DrawParams {
    Pointer mGeometryBufferID;
    Pointer mMaterialsBufferID;
    Pointer mSkinningBufferID;
    Pointer mGeometryAllocationBufferID;
    Pointer mCameraDataID;

    uint mBasicSamplerID;
};

MKT_DECLARE_STRUCTURED_SRV buffers;

MKT_DECLARE_SAMPLERS samplers;
MKT_DECLARE_TEXTURE_SRV( Texture2D ) textures;

MKT_DECLARE_PUSH_CONSTANT( DrawParams, parameters );

[shader( "fragment" )]
FSOutput main( FSInput input, bool frontFacing : SV_IsFrontFace ) {
    FSOutput o;
    float3 flatNormal = SurfaceFaceNormal( ddx( input.mWorldPos ), ddy( input.mWorldPos ), input.mNormal );
    if ( MaterialBackfaceDiscard( input.mFaceFlags, frontFacing ) ) {
        discard;
    }

    BufferReference<MaterialParameters> materialsBuffer =
            ( BufferReference<MaterialParameters> )parameters.mMaterialsBufferID;
    MaterialParameters materialInfo = materialsBuffer[input.mMaterialIndex];
    // Only visible opaque toon/cel pixels participate in crease contours.
    o.mContour = float4( 0 );
    if ( SurfaceStylized( uint( materialInfo.mSubsurface.z ) ) && materialInfo.mAlphaMask == PBR_ALPHA_OPAQUE ) {
        o.mContour = float4( materialInfo.mSubsurfaceColorOpacity.rgb, saturate( materialInfo.mSubsurfaceColorOpacity.a / 12.0 ) );
    }
    // SSAO and other depth consumers need the same nearest opaque surface as
    // the depth prepass, including opaque pixels authored in BLEND materials.
    if ( materialInfo.mAlphaMask == PBR_ALPHA_MASK || materialInfo.mAlphaMask == PBR_ALPHA_BLEND ) {
        int textureIndex = MaterialAlphaTexture( materialInfo );
        float alpha = textureIndex == INVALID_TEXTURE_INDEX
                              ? 1.0f
                              : textures[NonUniformResourceIndex( textureIndex )]
                                        .Sample( samplers[parameters.mBasicSamplerID],
                                                 MaterialAlphaUV( materialInfo, input.mUv0, input.mUv1 ) )
                                        .a;
        if ( MaterialSurfaceDiscard( materialInfo, MaterialAlpha( materialInfo, alpha, input.mColor.a ), false ) ) {
            discard;
        }
    }

    input.mUv0 = input.mUv0 * materialInfo.mUvTransform.xy + materialInfo.mUvTransform.zw;
    input.mUv1 = input.mUv1 * materialInfo.mUvTransform.xy + materialInfo.mUvTransform.zw;

    CameraInfo camera = ( ( BufferReference<CameraInfo> )parameters.mCameraDataID )[0];

    // NORMAL
    float2 normalUvs = materialInfo.mNormalTextureSet == 0 ? input.mUv0 : input.mUv1;
    float3 N = ( uint( materialInfo.mSubsurface.z ) != SURFACE_FLAT && materialInfo.mNormalIndex != INVALID_TEXTURE_INDEX )
                       ? GetNormalFromMap( textures[materialInfo.mNormalIndex], samplers[parameters.mBasicSamplerID],
                                           normalUvs, input.mNormal, input.mWorldPos, materialInfo.mNormalScale )
                       : normalize( input.mNormal );

    if ( !MaterialFrontFacing( input.mFaceFlags, frontFacing ) ) {
        N = -N;
    }

    if ( uint( materialInfo.mSubsurface.z ) == SURFACE_FLAT ) {
        N = MaterialFrontFacing( input.mFaceFlags, frontFacing ) ? flatNormal : -flatNormal;
    }
    o.mNormal = float4( normalize( N ) * 0.5 + 0.5, 1.0 );

    // POSITION (view-space + depth)
    // SSAO needs distances in view-space units. Normalizing this vector
    // destroyed both depth and radius, and made occlusion camera-dependent.
    o.mPosition = float4( input.mViewPos, -input.mViewPos.z );

    // ALBEDO
    // Store linear base color for the G-buffer debug view.
    o.mColor = float4( 0 );
    if ( materialInfo.mWorkflow == PBR_WORKFLOW_METALLIC_ROUGHNESS ) {
        float2 baseColorUvs = materialInfo.mBaseColorTextureSet == 0 ? input.mUv0 : input.mUv1;

        float4 albedo = ( materialInfo.mAlbedoIndex != INVALID_TEXTURE_INDEX )
                                ? SRGBtoLINEAR( textures[materialInfo.mAlbedoIndex].Sample( samplers[parameters.mBasicSamplerID],
                                                                                  baseColorUvs ) ) * materialInfo.mBaseColorFactor
                                : materialInfo.mBaseColorFactor;

        o.mColor = float4( albedo.xyz * input.mColor.rgb, 1.0 );
    }

    if ( materialInfo.mWorkflow == PBR_WORKFLOW_SPECULAR_GLOSSINESS ) {
        float2 diffuseUvs = materialInfo.mBaseColorTextureSet == 0 ? input.mUv0 : input.mUv1;

        float4 diffuse = ( materialInfo.mDiffuseIndex != INVALID_TEXTURE_INDEX )
                                 ? SRGBtoLINEAR( textures[materialInfo.mDiffuseIndex].Sample(
                                                samplers[parameters.mBasicSamplerID], diffuseUvs ) ) * materialInfo.mDiffuseFactor
                                 : materialInfo.mDiffuseFactor;

        o.mColor = float4( diffuse.xyz * input.mColor.rgb, 1.0 );
    }

    // Emisive
    o.mEmissive = float4( 0, 0, 0, 0 );
    if ( materialInfo.mIsBloomy == MKT_SHADER_TRUE ) {
        float3 emissiveContrib = materialInfo.mEmissiveFactor.xyz * materialInfo.mEmissiveStrength;

        if ( materialInfo.mEmissiveIndex != INVALID_TEXTURE_INDEX ) {
            Texture2D emissiveMap = textures[materialInfo.mEmissiveIndex];
            float2 emissiveUvSet = materialInfo.mEmissiveTextureSet == 0 ? input.mUv0 : input.mUv1;

            float3 emissive =
                    SRGBtoLINEAR( emissiveMap.Sample( samplers[parameters.mBasicSamplerID], emissiveUvSet ) ).rgb;

            emissiveContrib *= emissive;
        }

        if ( uint( materialInfo.mSubsurface.z ) == SURFACE_CLEAR_COAT ) {
            float3 view = normalize( camera.mViewPosition.xyz - input.mWorldPos );
            emissiveContrib *= 1 - materialInfo.mSurfaceShading.x * SurfaceCoatFresnel( dot( N, view ) );
        }
        o.mEmissive = float4( emissiveContrib, 1.0f );
    }

    return o;
}
